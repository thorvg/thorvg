/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <dispatch/dispatch.h>
#include <math.h>

#include "tvgAvfMediaLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

// CMTime ticks/sec; 600 is a multiple of standard fps (24/30/25) for exact frame times.
// https://developer.apple.com/library/archive/documentation/AudioVideo/Conceptual/AVFoundationPG/Articles/06_MediaRepresentations.html
static constexpr int32_t TIMESCALE = 600;
static constexpr float TIME_EPSILON = 1.0f / static_cast<float>(TIMESCALE);

static dispatch_queue_t _queue = nullptr;
static uint32_t _queueRefCnt = 0;
static tvg::StrictKey _queueKey;

static dispatch_queue_t _refQueue()
{
    ScopedLock lock(_queueKey);

    if (!_queue) _queue = dispatch_queue_create("org.thorvg.media.avfoundation", DISPATCH_QUEUE_SERIAL);
    ++_queueRefCnt;
    return _queue;
}

static void _unrefQueue()
{
    ScopedLock lock(_queueKey);
    if (_queueRefCnt == 0 || --_queueRefCnt > 0) return;

#if !OS_OBJECT_USE_OBJC
    dispatch_release(_queue);
#endif
    _queue = nullptr;
}

static void _sync(AvfMediaLoader& loader, dispatch_block_t block)
{
    dispatch_sync(loader.queue, block);
}

static float _seconds(CMTime time)
{
    if (!CMTIME_IS_NUMERIC(time)) return 0.0f;

    auto seconds = CMTimeGetSeconds(time);
    if (!isfinite(seconds) || seconds < 0.0) return 0.0f;
    return static_cast<float>(seconds);
}

static NSDictionary* _pixelAttrs()
{
    return @{(__bridge NSString*)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)};
}

static AVPlayerItemVideoOutput* _videoOutput(AVPlayerItem* item)
{
    for (AVPlayerItemOutput* output in item.outputs) {
        if ([output isKindOfClass:[AVPlayerItemVideoOutput class]]) return (AVPlayerItemVideoOutput*)output;
    }

    auto output = [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:_pixelAttrs()];
    if (!output) return nil;
    [item addOutput:output];
    [output release];
    return output;
}

static AVVideoComposition* _composition(AVAsset* asset, AVAssetTrack* track, CGSize& displaySize)
{
    auto transform = track.preferredTransform;
    if (transform.a == 1.0 && transform.b == 0.0 && transform.c == 0.0 && transform.d == 1.0 && transform.tx == 0.0 && transform.ty == 0.0) return nil;

    __block AVVideoComposition* composition = nil;
    auto semaphore = dispatch_semaphore_create(0);

    [AVVideoComposition videoCompositionWithPropertiesOfAsset:asset completionHandler:^(AVVideoComposition* videoComposition, TVG_UNUSED NSError* error) {
        composition = [videoComposition retain];
        dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
#if !OS_OBJECT_USE_OBJC
    dispatch_release(semaphore);
#endif

    if (!composition) return nil;
    displaySize = composition.renderSize;
    return composition;
}

// Copy a decoded CVPixelBuffer into ThorVG-owned frame ring buffer.
static bool _push(AvfMediaLoader& loader, CVPixelBufferRef pixelBuffer, float seconds)
{
    auto width = static_cast<uint32_t>(CVPixelBufferGetWidth(pixelBuffer));
    auto height = static_cast<uint32_t>(CVPixelBufferGetHeight(pixelBuffer));
    if (width == 0 || height == 0 || width != loader.surface.w || height != loader.surface.h) return false;

    // Copy the decoded pixel buffer into the loader-owned ring buffer.
    if (CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) return false;

    auto src = static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer));
    auto srcStride = CVPixelBufferGetBytesPerRow(pixelBuffer);
    auto rowBytes = width * sizeof(uint32_t);

    auto ret = false;
    if (src) {
        auto dst = loader.frames[loader.write];
        if (!dst) dst = loader.frames[loader.write] = tvg::malloc<uint32_t>(rowBytes * height);
        if (srcStride == rowBytes) memcpy(dst, src, rowBytes * height);
        else {
            for (auto y = 0U; y < height; ++y) {
                memcpy(reinterpret_cast<uint8_t*>(dst) + y * rowBytes, src + y * srcStride, rowBytes);
            }
        }
        ScopedLock lock(loader.key);
        loader.latestTime = seconds;
        loader.latest = loader.write;
        loader.write = (loader.write + 1) % BUFFER_COUNT;
        loader.frameUpdated = true;
        ret = true;
    }

    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    return ret;
}

static void _clearItems(AvfMediaLoader& loader)
{
    [loader.looper disableLooping];
    [loader.looper release];
    loader.looper = nil;

    [loader.player removeAllItems];
}

static void _stopTimer(AvfMediaLoader& loader)
{
    if (!loader.timer) return;

    dispatch_source_cancel(loader.timer);
#if !OS_OBJECT_USE_OBJC
    dispatch_release(loader.timer);
#endif
    loader.timer = nullptr;
}

static bool _readStillFrame(AvfMediaLoader& loader, float seconds)
{
    // Keep the target just before the end so at least one frame remains to read.
    auto readTime = seconds;
    if (readTime >= loader.totalTime) readTime = loader.totalTime - TIME_EPSILON;

    auto reader = [[AVAssetReader alloc] initWithAsset:loader.asset error:nil];
    if (!reader) {
        TVGLOG("AVF", "Failed to create asset reader.");
        return false;
    }

    AVAssetReaderOutput* output = nil;
    if (loader.composition) {
        auto videoOutput = [[AVAssetReaderVideoCompositionOutput alloc] initWithVideoTracks:@[loader.track] videoSettings:_pixelAttrs()];
        videoOutput.videoComposition = loader.composition;
        output = videoOutput;
    } else {
        output = [[AVAssetReaderTrackOutput alloc] initWithTrack:loader.track outputSettings:_pixelAttrs()];
    }

    if (!output || ![reader canAddOutput:output]) {
        [output release];
        [reader release];
        return false;
    }

    output.alwaysCopiesSampleData = NO;
    [reader addOutput:output];
    //only the first sample is read, so the range end is irrelevant
    reader.timeRange = CMTimeRangeMake(CMTimeMakeWithSeconds(static_cast<double>(readTime), TIMESCALE), kCMTimePositiveInfinity);

    auto ret = false;
    if ([reader startReading]) {
        if (auto sample = [output copyNextSampleBuffer]) {
            if (auto pixelBuffer = CMSampleBufferGetImageBuffer(sample)) ret = _push(loader, pixelBuffer, seconds);
            CFRelease(sample);
        }
    }

    [reader cancelReading];
    [output release];
    [reader release];
    return ret;
}

static void _finishPlayback(AvfMediaLoader& loader)
{
    [loader.player pause];
    _stopTimer(loader);
    loader.paused = true;
    if (!_readStillFrame(loader, loader.totalTime)) {
        ScopedLock lock(loader.key);
        loader.latestTime = loader.totalTime;
    }
}

static bool _buildQueue(AvfMediaLoader& loader, float start)
{
    _clearItems(loader);

    auto item = [[AVPlayerItem alloc] initWithAsset:loader.asset];
    if (!item) return false;
    if (loader.composition) item.videoComposition = loader.composition;

    auto seek = start > 0.0f;
    auto time = CMTimeMakeWithSeconds(static_cast<double>(start), TIMESCALE);

    // Keep playback looper-backed; loop state only decides whether the end observer stops.
    loader.looper = [[AVPlayerLooper alloc] initWithPlayer:loader.player templateItem:item timeRange:kCMTimeRangeInvalid];
    for (AVPlayerItem* loopItem in loader.looper.loopingPlayerItems) _videoOutput(loopItem);
    if (seek) [loader.player seekToTime:time toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];

    [item release];
    return loader.player.currentItem != nil;
}

static void _startEndObserver(AvfMediaLoader& loader)
{
    if (loader.endObserver) return;

    auto end = CMTimeMakeWithSeconds(static_cast<double>(loader.totalTime), TIMESCALE);
    auto state = &loader;
    loader.endObserver = [[loader.player addBoundaryTimeObserverForTimes:@[[NSValue valueWithCMTime:end]] queue:loader.queue usingBlock:^{
        if (!state->player || state->paused || state->looping) return;
        _finishPlayback(*state);
    }] retain];
}

static void _tick(AvfMediaLoader& loader)
{
    if (loader.paused) return;

    auto item = loader.player.currentItem;
    if (!item) {
        if (!loader.looping) _finishPlayback(loader);
        return;
    }

    auto output = _videoOutput(item);
    if (!output) return;

    auto time = item.currentTime;
    if (!CMTIME_IS_NUMERIC(time)) return;

    // Poll only if AVFoundation has a decoded frame for this item's current time.
    if (![output hasNewPixelBufferForItemTime:time]) return;

    if (auto buffer = [output copyPixelBufferForItemTime:time itemTimeForDisplay:nullptr]) {
        _push(loader, buffer, _seconds(time));
        CVPixelBufferRelease(buffer);
    }
}

static void _startTimer(AvfMediaLoader& loader)
{
    auto created = false;
    if (!loader.timer) {
        auto state = &loader;
        loader.timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, loader.queue);
        dispatch_source_set_event_handler(loader.timer, ^{ _tick(*state); });
        created = true;
    }

    // Derive the poll interval from the track frame rate.
    constexpr auto DefaultFps = 30.0;
    auto track = loader.track;
    auto fps = DefaultFps;
    if (track.nominalFrameRate > 0.0f) {
        fps = track.nominalFrameRate;
    } else {
        auto seconds = _seconds(track.minFrameDuration);
        if (seconds > 0.0) fps = 1.0 / seconds;
    }
    if (!isfinite(fps) || fps <= 0.0) fps = DefaultFps;

    auto interval = static_cast<uint64_t>(NSEC_PER_SEC / fps);
    if (interval == 0) interval = 1;

    dispatch_source_set_timer(loader.timer, dispatch_time(DISPATCH_TIME_NOW, 0), interval, NSEC_PER_MSEC);

    if (created) dispatch_resume(loader.timer);
}

static void _close(AvfMediaLoader& loader)
{
    _stopTimer(loader);
    [loader.player pause];
    _clearItems(loader);

    if (loader.endObserver) {
        [loader.player removeTimeObserver:loader.endObserver];
        [loader.endObserver release];
        loader.endObserver = nil;
    }

    [loader.player release];
    [loader.composition release];
    [loader.track release];
    [loader.asset release];

    loader.player = nil;
    loader.composition = nil;
    loader.track = nil;
    loader.asset = nil;
}

// Run a playback command on the avf queue: succeeds only when a player exists.
static Result _run(AvfMediaLoader& loader, Result (^block)())
{
    __block auto ret = Result::InsufficientCondition;
    auto state = &loader;

    _sync(loader, ^{
        if (!state->player) return;
        ret = block();
    });

    return ret;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

AvfMediaLoader::AvfMediaLoader()
{
    queue = _refQueue();
}

AvfMediaLoader::~AvfMediaLoader()
{
    _sync(*this, ^{
        _close(*this);
    });

    for (auto data : frames) tvg::free(data);
    tvg::free(surface.data);
    _unrefQueue();
}

bool AvfMediaLoader::open(const char* path, TVG_UNUSED const LoaderOps* ops)
{
    if (!path) return false;

    // Open the media asset and keep the first video track.
    auto url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
    auto asset = [[AVURLAsset alloc] initWithURL:url options:nil];
    if (!asset || !asset.playable) {
        TVGLOG("AVF", "Failed to open media: %s", path);
        [asset release];
        return false;
    }

    // Use the async track loader to avoid the deprecated sync API.
    __block AVAssetTrack* track = nil;
    auto semaphore = dispatch_semaphore_create(0);

    [asset loadTracksWithMediaType:AVMediaTypeVideo completionHandler:^(NSArray<AVAssetTrack*>* tracks, TVG_UNUSED NSError* error) {
        track = [tracks.firstObject retain];
        dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
#if !OS_OBJECT_USE_OBJC
    dispatch_release(semaphore);
#endif

    if (!track) {
        TVGLOG("AVF", "No video track found: %s", path);
        [asset release];
        return false;
    }

    auto duration = _seconds(asset.duration);
    if (duration <= TIME_EPSILON) {
        TVGLOG("AVF", "Invalid media duration: %s", path);
        [track release];
        [asset release];
        return false;
    }

    this->asset = asset;
    this->track = track;

    // Publish the display video size to Picture.
    auto displaySize = track.naturalSize;
    composition = _composition(asset, track, displaySize);
    w = static_cast<float>(fabs(displaySize.width));
    h = static_cast<float>(fabs(displaySize.height));

    totalTime = duration;
    curTime = 0.0f;

    return true;
}

bool AvfMediaLoader::read()
{
    if (!Loader::read()) return true;
    if (!asset || !track || w == 0 || h == 0) return false;

    surface.cs = ColorSpace::ARGB8888S;
    surface.w = static_cast<uint32_t>(w);
    surface.h = static_cast<uint32_t>(h);
    surface.stride = surface.w;
    surface.channelSize = sizeof(uint32_t);
    surface.premultiplied = false;
    surface.alphaIgnored = true;

    // Prime the first frame so Picture can render immediately after load().
    _readStillFrame(*this, 0.0f);
    sync();

    // Prepare playback; the timer starts only when play() is called.
    player = [[AVQueuePlayer alloc] init];
    if (!player) return false;

    player.automaticallyWaitsToMinimizeStalling = NO;
    player.volume = audioVolume;
    player.muted = muted;
    _startEndObserver(*this);
    paused = true;

    return _buildQueue(*this, 0.0f);
}

bool AvfMediaLoader::close()
{
    if (!Loader::close()) return false;

    _sync(*this, ^{
        _close(*this);
    });

    return true;
}

RenderSurface* AvfMediaLoader::bitmap()
{
    return ImageLoader::bitmap();
}

bool AvfMediaLoader::sync()
{
    ScopedLock lock(key);

    curTime = latestTime;
    if (!frameUpdated) return false;

    auto frame = frames[latest];
    if (!frame) return false;

    auto size = surface.stride * surface.h * surface.channelSize;
    if (!surface.data) surface.data = tvg::malloc<pixel_t>(size);
    memcpy(surface.data, frame, size);
    surface.cs = ColorSpace::ARGB8888S;   // rasterConvertCS() can update this.
    surface.premultiplied = false;        // rasterPremultiply() can update this.
    frameUpdated = false;

    return true;
}

Result AvfMediaLoader::play()
{
    return _run(*this, ^{
        paused = false;
        [player play];
        _startTimer(*this);
        return Result::Success;
    });
}

Result AvfMediaLoader::pause()
{
    return _run(*this, ^{
        paused = true;
        [player pause];
        _stopTimer(*this);
        return Result::Success;
    });
}

Result AvfMediaLoader::stop()
{
    return _run(*this, ^{
        [player pause];
        _stopTimer(*this);
        paused = true;
        curTime = 0.0f;

        [player seekToTime:kCMTimeZero toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];
        _readStillFrame(*this, 0.0f);
        return Result::Success;
    });
}

Result AvfMediaLoader::seek(float seconds)
{
    if (seconds < 0.0f || (totalTime > 0.0f && seconds > totalTime)) return Result::InvalidArguments;

    return _run(*this, ^{
        auto end = totalTime > 0.0f && seconds >= totalTime;

        if (end) {
            [player.currentItem cancelPendingSeeks];
            [player seekToTime:kCMTimeZero toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];
            curTime = seconds;
            if (!looping) _finishPlayback(*this);
            else _readStillFrame(*this, seconds);
            return Result::Success;
        }

        if (paused) _readStillFrame(*this, seconds);
        [player.currentItem cancelPendingSeeks];
        [player seekToTime:CMTimeMakeWithSeconds(static_cast<double>(seconds), TIMESCALE) toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];

        curTime = seconds;
        return Result::Success;
    });
}

Result AvfMediaLoader::loop(bool on)
{
    return _run(*this, ^{
        looping = on;
        return Result::Success;
    });
}

Result AvfMediaLoader::volume(float volume)
{
    return _run(*this, ^{
        audioVolume = volume;
        player.volume = volume;
        return Result::Success;
    });
}

Result AvfMediaLoader::mute(bool on)
{
    return _run(*this, ^{
        muted = on;
        player.muted = on;
        return Result::Success;
    });
}

MediaLoader* MediaLoader::gen()
{
    return new AvfMediaLoader;
}