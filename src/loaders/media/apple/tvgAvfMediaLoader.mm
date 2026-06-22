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

// Ring buffer depth for decoded pixels copied from CVPixelBuffer.
static constexpr uint32_t BUFFER_COUNT = 3;

struct AvfImpl
{
    // ThorVG loader state updated from the AVFoundation queue.
    AvfMediaLoader* loader = nullptr;
    dispatch_queue_t queue = nullptr;

    // AVFoundation objects for metadata, playback, and looping.
    AVAsset* asset = nil;
    AVAssetTrack* track = nil;
    AVVideoComposition* composition = nil;
    AVQueuePlayer* player = nil;
    AVPlayerLooper* looper = nil;
    id endObserver = nil;

    // Polling timer and frame availability flag.
    dispatch_source_t timer = nullptr;
    bool frameUpdated = false;

    // Owned frame ring for decoded pixels shared with bitmap().
    uint32_t* frames[BUFFER_COUNT] = {};
    uint32_t* surfaceData = nullptr;
    float latestTime = 0.0f;
    uint32_t write = 0;
    uint32_t latest = 0;

    // Protects frame publication to bitmap().
    tvg::StrictKey key;
};

// CMTime ticks/sec; 600 is a multiple of standard fps (24/30/25) for exact frame times.
// https://developer.apple.com/library/archive/documentation/AudioVideo/Conceptual/AVFoundationPG/Articles/06_MediaRepresentations.html
static constexpr int32_t TIMESCALE = 600;
static constexpr float TIME_EPSILON = 1.0f / static_cast<float>(TIMESCALE);

static dispatch_queue_t _queue = nullptr;
static uint32_t _queueRefCnt = 0;
static tvg::StrictKey _queueKey;

static dispatch_queue_t _refQueue()
{
    tvg::ScopedLock lock(_queueKey);

    if (!_queue) _queue = dispatch_queue_create("org.thorvg.media.avfoundation", DISPATCH_QUEUE_SERIAL);
    ++_queueRefCnt;
    return _queue;
}

static void _unrefQueue(dispatch_queue_t queue)
{
    if (!queue) return;

    tvg::ScopedLock lock(_queueKey);
    if (_queueRefCnt == 0) return;
    if (--_queueRefCnt > 0) return;

#if !OS_OBJECT_USE_OBJC
    dispatch_release(_queue);
#endif
    _queue = nullptr;
}

static void _sync(AvfImpl* impl, dispatch_block_t block)
{
    dispatch_sync(impl->queue, block);
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
        if ([output isKindOfClass:[AVPlayerItemVideoOutput class]]) return static_cast<AVPlayerItemVideoOutput*>(output);
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
static bool _push(AvfImpl* impl, CVPixelBufferRef pixelBuffer, float seconds)
{
    auto width = static_cast<uint32_t>(CVPixelBufferGetWidth(pixelBuffer));
    auto height = static_cast<uint32_t>(CVPixelBufferGetHeight(pixelBuffer));
    if (width == 0 || height == 0 || width != impl->loader->surface.w || height != impl->loader->surface.h) return false;

    // Copy the decoded pixel buffer into the loader-owned ring buffer.
    if (CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) return false;

    auto src = static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer));
    auto srcStride = CVPixelBufferGetBytesPerRow(pixelBuffer);
    auto rowBytes = width * sizeof(uint32_t);

    auto ret = false;
    if (src) {
        auto dst = impl->frames[impl->write];
        if (!dst) dst = impl->frames[impl->write] = tvg::malloc<uint32_t>(rowBytes * height);

        if (dst) {
            if (srcStride == rowBytes) memcpy(dst, src, rowBytes * height);
            else {
                for (auto y = 0U; y < height; ++y) {
                    memcpy(reinterpret_cast<uint8_t*>(dst) + y * rowBytes, src + y * srcStride, rowBytes);
                }
            }

            tvg::ScopedLock lock(impl->key);
            impl->latestTime = seconds;
            impl->latest = impl->write;
            impl->write = (impl->write + 1) % BUFFER_COUNT;
            impl->frameUpdated = true;
            ret = true;
        }
    }

    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    return ret;
}

static void _clearItems(AvfImpl* impl)
{
    [impl->looper disableLooping];
    [impl->looper release];
    impl->looper = nil;

    [impl->player removeAllItems];
}

static void _finishPlayback(AvfImpl* impl)
{
    [impl->player pause];
    impl->loader->paused = true;
    {
        tvg::ScopedLock lock(impl->key);
        impl->latestTime = impl->loader->totalTime;
    }
}

static bool _buildQueue(AvfImpl* impl, float start)
{
    _clearItems(impl);

    auto item = [[AVPlayerItem alloc] initWithAsset:impl->asset];
    if (!item) return false;
    if (impl->composition) item.videoComposition = impl->composition;

    auto seek = start > 0.0f;
    auto time = CMTimeMakeWithSeconds(static_cast<double>(start), TIMESCALE);

    // Keep playback looper-backed; loop state only decides whether the end observer stops.
    impl->looper = [[AVPlayerLooper alloc] initWithPlayer:impl->player templateItem:item timeRange:kCMTimeRangeInvalid];
    for (AVPlayerItem* loopItem in impl->looper.loopingPlayerItems) _videoOutput(loopItem);
    if (seek) [impl->player seekToTime:time toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];

    [item release];
    return impl->player.currentItem != nil;
}

static void _startEndObserver(AvfImpl* impl)
{
    if (impl->endObserver) return;

    auto end = CMTimeMakeWithSeconds(static_cast<double>(impl->loader->totalTime), TIMESCALE);
    impl->endObserver = [[impl->player addBoundaryTimeObserverForTimes:@[[NSValue valueWithCMTime:end]] queue:impl->queue usingBlock:^{
        if (!impl->player || impl->loader->paused || impl->loader->looping) return;
        _finishPlayback(impl);
    }] retain];
}

static bool _readStillFrame(AvfImpl* impl, float seconds)
{
    // Keep the target just before the end so at least one frame remains to read.
    auto readTime = seconds;
    if (readTime >= impl->loader->totalTime) readTime = impl->loader->totalTime - TIME_EPSILON;

    auto reader = [[AVAssetReader alloc] initWithAsset:impl->asset error:nil];
    if (!reader) {
        TVGLOG("AVF", "Failed to create asset reader.");
        return false;
    }

    AVAssetReaderOutput* output = nil;
    if (impl->composition) {
        auto videoOutput = [[AVAssetReaderVideoCompositionOutput alloc] initWithVideoTracks:@[impl->track] videoSettings:_pixelAttrs()];
        videoOutput.videoComposition = impl->composition;
        output = videoOutput;
    } else {
        output = [[AVAssetReaderTrackOutput alloc] initWithTrack:impl->track outputSettings:_pixelAttrs()];
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
            if (auto pixelBuffer = CMSampleBufferGetImageBuffer(sample)) ret = _push(impl, pixelBuffer, seconds);
            CFRelease(sample);
        }
    }

    [reader cancelReading];
    [output release];
    [reader release];
    return ret;
}

static void _tick(AvfImpl* impl)
{
    if (impl->loader->paused) return;

    auto item = impl->player.currentItem;
    if (!item) {
        if (!impl->loader->looping) _finishPlayback(impl);
        return;
    }

    auto output = _videoOutput(item);
    if (!output) return;

    auto time = item.currentTime;
    if (!CMTIME_IS_NUMERIC(time)) return;

    // Poll only if AVFoundation has a decoded frame for this item's current time.
    if (![output hasNewPixelBufferForItemTime:time]) return;

    if (auto buffer = [output copyPixelBufferForItemTime:time itemTimeForDisplay:nullptr]) {
        _push(impl, buffer, _seconds(time));
        CVPixelBufferRelease(buffer);
    }
}

static void _startTimer(AvfImpl* impl)
{
    auto created = false;
    if (!impl->timer) {
        impl->timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, impl->queue);
        dispatch_source_set_event_handler(impl->timer, ^{ _tick(impl); });
        created = true;
    }

    // Derive the poll interval from the track frame rate.
    constexpr auto DefaultFps = 30.0;
    auto track = impl->track;
    auto fps = DefaultFps;
    if (track.nominalFrameRate > 0.0f) {
        fps = track.nominalFrameRate;
    } else {
        auto seconds = _seconds(track.minFrameDuration);
        if (seconds > 0.0) fps = 1.0 / seconds;
    }
    auto interval = static_cast<uint64_t>(NSEC_PER_SEC / fps);

    dispatch_source_set_timer(impl->timer, dispatch_time(DISPATCH_TIME_NOW, 0), interval, NSEC_PER_MSEC);

    if (created) dispatch_resume(impl->timer);
}

static void _close(AvfImpl* impl)
{
    if (impl->timer) {
        dispatch_source_cancel(impl->timer);
#if !OS_OBJECT_USE_OBJC
        dispatch_release(impl->timer);
#endif
        impl->timer = nullptr;
    }

    [impl->player pause];
    _clearItems(impl);

    if (impl->endObserver) {
        [impl->player removeTimeObserver:impl->endObserver];
        [impl->endObserver release];
        impl->endObserver = nil;
    }

    [impl->player release];
    [impl->composition release];
    [impl->track release];
    [impl->asset release];

    impl->player = nil;
    impl->composition = nil;
    impl->track = nil;
    impl->asset = nil;
}

// Run a playback command on the avf queue: succeeds only when a player exists.
static Result _run(AvfImpl* impl, Result (^block)())
{
    __block auto ret = Result::InsufficientCondition;

    _sync(impl, ^{
        if (!impl->player) return;
        ret = block();
    });

    return ret;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

AvfMediaLoader::AvfMediaLoader() :
    MediaLoader(FileType::Media),
    pImpl(new AvfImpl)
{
    pImpl->queue = _refQueue();
    pImpl->loader = this;
}

AvfMediaLoader::~AvfMediaLoader()
{
    _sync(pImpl, ^{
        _close(pImpl);
    });

    for (auto data : pImpl->frames) tvg::free(data);
    tvg::free(pImpl->surfaceData);
    _unrefQueue(pImpl->queue);
    delete pImpl;
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

    pImpl->asset = asset;
    pImpl->track = track;

    // Publish the display video size to Picture.
    auto displaySize = track.naturalSize;
    pImpl->composition = _composition(asset, track, displaySize);
    w = static_cast<float>(fabs(displaySize.width));
    h = static_cast<float>(fabs(displaySize.height));

    totalTime = duration;
    curTime = 0.0f;

    return true;
}

bool AvfMediaLoader::read()
{
    if (!Loader::read()) return true;
    if (!pImpl->asset || !pImpl->track || w == 0 || h == 0) return false;

    surface.cs = ColorSpace::ARGB8888S;
    surface.w = static_cast<uint32_t>(w);
    surface.h = static_cast<uint32_t>(h);
    surface.stride = surface.w;
    surface.channelSize = sizeof(uint32_t);
    surface.premultiplied = false;
    surface.alphaIgnored = true;

    // Prime the first frame so Picture can render immediately after load().
    _readStillFrame(pImpl, 0.0f);

    // Prepare playback; the timer starts only when play() is called.
    pImpl->player = [[AVQueuePlayer alloc] init];
    if (!pImpl->player) return false;

    pImpl->player.automaticallyWaitsToMinimizeStalling = NO;
    pImpl->player.volume = audioVolume;
    pImpl->player.muted = muted;
    _startEndObserver(pImpl);
    paused = true;

    return _buildQueue(pImpl, 0.0f);
}

bool AvfMediaLoader::close()
{
    if (!Loader::close()) return false;

    _sync(pImpl, ^{
        _close(pImpl);
    });

    return true;
}

RenderSurface* AvfMediaLoader::bitmap()
{
    tvg::ScopedLock lock(pImpl->key);

    curTime = pImpl->latestTime;
    if (!pImpl->frameUpdated) return nullptr;

    pImpl->frameUpdated = false;
    std::swap(pImpl->frames[pImpl->latest], pImpl->surfaceData);
    surface.data = pImpl->surfaceData;
    surface.cs = ColorSpace::ARGB8888S;   // rasterConvertCS() can update this.
    surface.premultiplied = false;        // rasterPremultiply() can update this.

    return &surface;
}

Result AvfMediaLoader::play()
{
    return _run(pImpl, ^{
        paused = false;
        [pImpl->player play];
        _startTimer(pImpl);
        return Result::Success;
    });
}

Result AvfMediaLoader::pause()
{
    return _run(pImpl, ^{
        paused = true;
        [pImpl->player pause];
        return Result::Success;
    });
}

Result AvfMediaLoader::stop()
{
    return _run(pImpl, ^{
        [pImpl->player pause];
        paused = true;
        curTime = 0.0f;

        [pImpl->player seekToTime:kCMTimeZero toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];
        _readStillFrame(pImpl, 0.0f);
        return Result::Success;
    });
}

Result AvfMediaLoader::seek(float seconds)
{
    if (seconds < 0.0f || (totalTime > 0.0f && seconds > totalTime)) return Result::InvalidArguments;

    return _run(pImpl, ^{
        auto end = totalTime > 0.0f && seconds >= totalTime;

        if (end) {
            [pImpl->player.currentItem cancelPendingSeeks];
            _readStillFrame(pImpl, seconds);
            [pImpl->player seekToTime:kCMTimeZero toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];
            curTime = seconds;
            if (!looping) _finishPlayback(pImpl);
            return Result::Success;
        }

        if (paused) _readStillFrame(pImpl, seconds);
        [pImpl->player.currentItem cancelPendingSeeks];
        [pImpl->player seekToTime:CMTimeMakeWithSeconds(static_cast<double>(seconds), TIMESCALE) toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];

        curTime = seconds;
        return Result::Success;
    });
}

Result AvfMediaLoader::loop(bool on)
{
    return _run(pImpl, ^{
        looping = on;
        return Result::Success;
    });
}

Result AvfMediaLoader::volume(float volume)
{
    return _run(pImpl, ^{
        audioVolume = volume;
        pImpl->player.volume = volume;
        return Result::Success;
    });
}

Result AvfMediaLoader::mute(bool on)
{
    return _run(pImpl, ^{
        muted = on;
        pImpl->player.muted = on;
        return Result::Success;
    });
}
