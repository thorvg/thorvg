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

#ifndef _TVG_MEDIA_LOADER_H_
#define _TVG_MEDIA_LOADER_H_

#include "tvgLoader.h"

struct MediaLoader : ImageLoader
{
    float curTime = 0.0f;      // current playback position in seconds
    float totalTime = 0.0f;    // media duration in seconds
    float audioVolume = 0.0f;  // [0 - 1]
    bool paused = false;
    bool muted = false;
    bool looping = false;

    MediaLoader(FileType type) :
        ImageLoader(type, true) {}
    virtual ~MediaLoader(){};

    virtual Result play() = 0;                // start or resume playback
    virtual Result pause() = 0;               // pause playback
    virtual Result stop() = 0;                // stop playback
    virtual Result seek(float seconds) = 0;   // set the playback position in seconds
    virtual Result loop(bool on) = 0;         // enable or disable repeated playback
    virtual Result volume(float volume) = 0;  // set the audio volume level
    virtual Result mute(bool on) = 0;         // enable or disable audio muting

    static MediaLoader* gen();
};

#endif  //_TVG_MEDIA_LOADER_H_
