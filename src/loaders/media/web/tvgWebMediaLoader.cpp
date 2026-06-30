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

#include <cstring>
#include "tvgWebMediaLoader.h"

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

WebMediaLoader::WebMediaLoader() : MediaLoader(FileType::Media)
{
}

WebMediaLoader::~WebMediaLoader()
{
    tvg::free(surface.buf32);
}

Result WebMediaLoader::open(uint32_t w, uint32_t h, float duration, ColorSpace cs)
{
    if (w == 0 || h == 0) return Result::InvalidArguments;

    auto bytes = sizeof(uint32_t) * w * h;
    tvg::free(surface.buf32);
    surface.buf32 = tvg::malloc<uint32_t>(bytes);
    if (!surface.buf32) return Result::FailedAllocation;
    memset(surface.buf32, 0, bytes);

    surface.stride = w;
    surface.w = w;
    surface.h = h;
    surface.cs = cs;
    surface.channelSize = sizeof(uint32_t);
    surface.premultiplied = (cs == ColorSpace::ABGR8888 || cs == ColorSpace::ARGB8888);

    this->w = (float)w;
    this->h = (float)h;
    this->totalTime = duration;

    return Result::Success;
}

Result WebMediaLoader::update(const uint32_t* frame, float time)
{
    if (!surface.buf32) return Result::InsufficientCondition;
    if (!frame) return Result::InvalidArguments;

    memcpy(surface.buf32, frame, sizeof(uint32_t) * surface.w * surface.h);
    curTime = time;

    return Result::Success;
}

Result WebMediaLoader::play()
{
    if (!surface.buf32) return Result::InsufficientCondition;
    paused = false;
    return Result::Success;
}

Result WebMediaLoader::pause()
{
    if (!surface.buf32) return Result::InsufficientCondition;
    paused = true;
    return Result::Success;
}

Result WebMediaLoader::stop()
{
    if (!surface.buf32) return Result::InsufficientCondition;
    paused = true;
    curTime = 0.0f;
    return Result::Success;
}

Result WebMediaLoader::seek(float seconds)
{
    if (!surface.buf32) return Result::InsufficientCondition;
    curTime = seconds;
    return Result::Success;
}

Result WebMediaLoader::loop(bool on)
{
    if (!surface.buf32) return Result::InsufficientCondition;
    looping = on;
    return Result::Success;
}

Result WebMediaLoader::volume(float volume)
{
    if (!surface.buf32) return Result::InsufficientCondition;
    audioVolume = volume;
    return Result::Success;
}

Result WebMediaLoader::mute(bool on)
{
    if (!surface.buf32) return Result::InsufficientCondition;
    muted = on;
    return Result::Success;
}
