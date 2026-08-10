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

#ifndef _TVG_AVF_LOADER_H_
#define _TVG_AVF_LOADER_H_

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <dispatch/dispatch.h>

#include "tvgMediaLoader.h"

static constexpr uint32_t BUFFER_COUNT = 3;

struct AvfMediaLoader : MediaLoader
{
    // AVFoundation execution and playback state
    dispatch_queue_t queue = nullptr;

    AVAsset* asset = nil;
    AVAssetTrack* track = nil;
    AVVideoComposition* composition = nil;
    AVQueuePlayer* player = nil;
    AVPlayerLooper* looper = nil;
    id endObserver = nil;
    NSString* tempPath = nil;
    dispatch_source_t timer = nullptr;
    bool started = false;  // true after play(), including pause; false after stop() or EOS

    // Decoded frame ring buffer and synchronized publication state
    uint32_t* frames[BUFFER_COUNT] = {};
    StrictKey key;

    float latestTime = 0.0f;
    uint32_t write = 0;
    uint32_t latest = 0;
    bool frameUpdated = false;
    bool eosPending = false;

    // Lifecycle
    AvfMediaLoader();
    ~AvfMediaLoader();

    // Loader interface
    bool open(const char* data, uint32_t size, const LoaderOps* ops, bool copy) override;
    bool open(const char* path, const LoaderOps* ops) override;
    bool read() override;
    bool sync() override;

    // Playback controls
    Result play() override;
    Result pause() override;
    Result stop() override;
    Result seek(float seconds) override;
    Result loop(bool on) override;
    Result volume(float volume) override;
    Result mute(bool on) override;
};

#endif  //_TVG_AVF_LOADER_H_
