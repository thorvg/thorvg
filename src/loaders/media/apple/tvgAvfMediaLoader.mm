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
static StrictKey _queueKey;

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

    dispatch_release(_queue);
    _queue = nullptr;
}

static float _seconds(CMTime time)
{
    if (!CMTIME_IS_NUMERIC(time)) return 0.0f;
    auto seconds = CMTimeGetSeconds(time);
    return static_cast<float>((seconds < 0.0) ? 0.0 : seconds);
}

static NSDictionary* _pixelAttrs()
{
    return @{(__bridge NSString*)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)};
}

static AVPlayerItemVideoOutput* _videoOutput(AVPlayerItem* item)
{
    for (AVPlayerItemOutput* output in item.outputs) {
        if ([output isKindOfClass:[AVPlayerItemVideoOutput class]])
            return static_cast<AVPlayerItemVideoOutput*>(output);
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
    if (transform.a == 1.0 && transform.b == 0.0 && transform.c == 0.0 && transform.d == 1.0 && transform.tx == 0.0 && transform.ty == 0.0)
        return nil;

    __block AVVideoComposition* composition = nil;
    auto semaphore = dispatch_semaphore_create(0);

    [AVVideoComposition videoCompositionWithPropertiesOfAsset:asset completionHandler:^(AVVideoComposition* videoComposition, TVG_UNUSED NSError* error) {
        composition = [videoComposition retain];
        dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(semaphore);

    if (!composition) return nil;
    displaySize = composition.renderSize;
    return composition;
}

static void _resetFrame(AvfMediaLoader& loader, float seconds, bool eos = false)
{
    ScopedLock lock(loader.key);
    loader.latestTime = seconds;
    loader.frameUpdated = false;
    loader.eosPending = eos;
}

static void _clearEos(AvfMediaLoader& loader)
{
    ScopedLock lock(loader.key);
    loader.eosPending = false;
}

// Copy a decoded CVPixelBuffer into ThorVG-owned frame ring buffer.
static bool _push(AvfMediaLoader& loader, CVPixelBufferRef pixelBuffer, float seconds)
{
    auto width = static_cast<uint32_t>(CVPixelBufferGetWidth(pixelBuffer));
    auto height = static_cast<uint32_t>(CVPixelBufferGetHeight(pixelBuffer));
    if (width != loader.surface.w || height != loader.surface.h) return false;

    // Copy the decoded pixel buffer into the loader-owned ring buffer.
    if (CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) return false;

    auto src = static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer));
    auto srcStride = CVPixelBufferGetBytesPerRow(pixelBuffer);
    auto rowBytes = width * sizeof(uint32_t);

    auto ret = false;
    if (src) {
        auto dst = loader.frames[loader.write];
        if (!dst) dst = loader.frames[loader.write] = tvg::malloc<uint32_t>(rowBytes * height);
        // CVPixelBuffer rows may include hardware-specific padding.
        // https://developer.apple.com/library/archive/qa/qa1829/_index.html
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

static void _tick(AvfMediaLoader& loader)
{
    auto item = loader.player.currentItem;
    if (!item) return;

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
    if (loader.timer) return;

    auto state = &loader;
    loader.timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, loader.queue);
    if (!loader.timer) return;
    dispatch_source_set_event_handler(loader.timer, ^{
        _tick(*state);
    });

    // Derive the poll interval from the track frame rate.
    auto fps = static_cast<double>(loader.track.nominalFrameRate);
    if (fps <= 0.0) {
        auto seconds = _seconds(loader.track.minFrameDuration);
        if (seconds > 0.0f) fps = 1.0 / seconds;
    }
    // Match AVVideoComposition's 30 FPS fallback when the track frame rate is unknown.
    // https://developer.apple.com/documentation/avfoundation/avvideocomposition/videocomposition%28withpropertiesof%3Acompletionhandler%3A%29
    if (!isfinite(fps) || fps <= 0.0) fps = 30.0;

    auto interval = static_cast<uint64_t>(NSEC_PER_SEC / fps);

    dispatch_source_set_timer(loader.timer, dispatch_time(DISPATCH_TIME_NOW, 0), interval, NSEC_PER_MSEC);
    dispatch_resume(loader.timer);
}

static void _cancelTimer(AvfMediaLoader& loader)
{
    if (!loader.timer) return;

    dispatch_source_cancel(loader.timer);
    dispatch_release(loader.timer);
    loader.timer = nullptr;
}

static bool _readStillFrame(AvfMediaLoader& loader, float seconds)
{
    // Keep the target just before the end so at least one frame remains to read.
    auto readTime = seconds;
    if (readTime >= loader.totalTime) readTime = loader.totalTime - TIME_EPSILON;

    auto reader = [[AVAssetReader alloc] initWithAsset:loader.asset error:nil];
    if (!reader) {
        TVGERR("AVF", "Failed to create asset reader.");
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
    _cancelTimer(loader);
    _resetFrame(loader, loader.totalTime, true);
    _readStillFrame(loader, loader.totalTime);
}

static bool _buildQueue(AvfMediaLoader& loader)
{
    auto item = [[AVPlayerItem alloc] initWithAsset:loader.asset];
    if (!item) return false;
    if (loader.composition) item.videoComposition = loader.composition;

    // Keep playback looper-backed; the end observer stops playback when looping is off.
    loader.looper = [[AVPlayerLooper alloc] initWithPlayer:loader.player templateItem:item timeRange:kCMTimeRangeInvalid];
    for (AVPlayerItem* loopItem in loader.looper.loopingPlayerItems) {
        _videoOutput(loopItem);
    }

    [item release];
    return loader.player.currentItem != nil;
}

static void _removeEndObserver(AvfMediaLoader& loader)
{
    if (!loader.endObserver) return;
    [loader.player removeTimeObserver:loader.endObserver];
    [loader.endObserver release];
    loader.endObserver = nil;
}

static void _startEndObserver(AvfMediaLoader& loader)
{
    if (loader.endObserver) return;

    auto end = CMTimeMakeWithSeconds(static_cast<double>(loader.totalTime), TIMESCALE);
    auto state = &loader;
    loader.endObserver = [[loader.player addBoundaryTimeObserverForTimes:@[[NSValue valueWithCMTime:end]] queue:loader.queue usingBlock:^{
        _finishPlayback(*state);
    }] retain];
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
    dispatch_sync(queue, ^{
        _removeEndObserver(*this);
        _cancelTimer(*this);
        [player pause];
        [looper disableLooping];
        [looper release];
        [player removeAllItems];
        [player release];
        [composition release];
        [track release];
        [asset release];
    });

    for (auto data : frames) {
        tvg::free(data);
    }
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
        TVGERR("AVF", "Failed to open media: %s", path);
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
    dispatch_release(semaphore);

    if (!track) {
        TVGERR("AVF", "No video track found: %s", path);
        [asset release];
        return false;
    }

    auto duration = _seconds(asset.duration);
    if (duration <= TIME_EPSILON) {
        TVGERR("AVF", "Invalid media duration: %s", path);
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
    if (w == 0 || h == 0) return false;

    totalTime = duration;

    player = [[AVQueuePlayer alloc] init];
    if (!player) return false;

    // Prepare playback; the timer starts only when play() is called.
    player.automaticallyWaitsToMinimizeStalling = NO;
    player.volume = audioVolume;
    player.muted = muted;
    _startEndObserver(*this);

    return _buildQueue(*this);
}

bool AvfMediaLoader::read()
{
    if (!Loader::read()) return true;

    surface.w = static_cast<uint32_t>(w);
    surface.h = static_cast<uint32_t>(h);
    surface.stride = surface.w;
    surface.channelSize = sizeof(uint32_t);
    surface.alphaIgnored = true;

    // Prime the first frame so Picture can render immediately after load().
    return _readStillFrame(*this, 0.0f) && sync();
}

bool AvfMediaLoader::sync()
{
    ScopedLock lock(key);

    curTime = latestTime;
    if (eosPending) {
        started = false;
        paused = true;
        eosPending = false;
    }
    if (!frameUpdated) return false;

    auto frame = frames[latest];
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
    auto restart = curTime >= totalTime;
    _clearEos(*this);
    started = true;
    paused = false;
    dispatch_async(queue, ^{
        if (restart) [player seekToTime:kCMTimeZero toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];
        [player play];
        _startTimer(*this);
    });
    return Result::Success;
}

Result AvfMediaLoader::pause()
{
    if (!started) return Result::InsufficientCondition;

    paused = true;
    dispatch_async(queue, ^{
        [player pause];
        _cancelTimer(*this);
    });
    return Result::Success;
}

Result AvfMediaLoader::stop()
{
    _clearEos(*this);
    curTime = 0.0f;
    started = false;
    paused = true;
    dispatch_async(queue, ^{
        [player pause];
        _cancelTimer(*this);
        _resetFrame(*this, 0.0f);

        [player seekToTime:kCMTimeZero toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];
        _readStillFrame(*this, 0.0f);
    });
    return Result::Success;
}

Result AvfMediaLoader::seek(float seconds)
{
    _clearEos(*this);
    curTime = seconds;
    dispatch_async(queue, ^{
        auto end = seconds >= totalTime;
        _resetFrame(*this, seconds);

        if (end) {
            [player.currentItem cancelPendingSeeks];
            [player seekToTime:kCMTimeZero toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];
            if (endObserver) _finishPlayback(*this);
            else _readStillFrame(*this, seconds);
            return;
        }

        if (!timer) _readStillFrame(*this, seconds);
        [player.currentItem cancelPendingSeeks];
        [player seekToTime:CMTimeMakeWithSeconds(static_cast<double>(seconds), TIMESCALE) toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];
    });
    return Result::Success;
}

Result AvfMediaLoader::loop(bool on)
{
    looping = on;
    dispatch_async(queue, ^{
        if (on) _removeEndObserver(*this);
        else _startEndObserver(*this);
    });
    return Result::Success;
}

Result AvfMediaLoader::volume(float volume)
{
    audioVolume = volume;
    player.volume = volume;
    return Result::Success;
}

Result AvfMediaLoader::mute(bool on)
{
    muted = on;
    player.muted = on;
    return Result::Success;
}

MediaLoader* MediaLoader::gen()
{
    return new AvfMediaLoader;
}