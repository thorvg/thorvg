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

#ifndef _TVG_ANDROID_MEDIA_LOADER_H_
#define _TVG_ANDROID_MEDIA_LOADER_H_

#include "tvgAndroidMediaDecoder.h"
#include "tvgLock.h"
#include "tvgMediaLoader.h"

struct AndroidMediaLoader : MediaLoader
{
    ~AndroidMediaLoader();

    bool open(const char* path, const LoaderOps& ops) override;
    bool open(const char* data, uint32_t size, const LoaderOps& ops) override;
    bool read() override;
    bool sync() override;

    Result play() override;
    Result pause() override;
    Result stop() override;
    Result seek(float seconds) override;
    Result loop(bool on) override;
    Result volume(float volume) override;
    Result mute(bool on) override;

    int64_t pump();

private:
    bool open(int fd, int64_t length, const char* name);
    bool start();
    float syncTime();

    struct Playback
    {
        enum class State : uint8_t {Stopped, Paused, Playing};

        float time = 0.0f;
        float duration = 0.0f;
        State state = State::Stopped;
        bool looping = false;
    };

    VideoDecoder video;
    AudioDecoder audio;
    StrictKey key;             // guards playback and decoder state
    Playback playback;         // accessed by caller and scheduler threads; guarded by key
    bool audioCovers = false;  // audio spans the whole media
};

#endif  //_TVG_ANDROID_MEDIA_LOADER_H_
