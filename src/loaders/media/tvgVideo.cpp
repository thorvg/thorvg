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

#include "thorvg_media.h"
#include "tvgMediaLoader.h"
#include "tvgPicture.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Video::Impl
{
    Picture* picture;
    float volume = 0.5f;
    bool muted = false;

    Impl()
    {
        picture = Picture::gen();
        picture->ref();
    }

    ~Impl()
    {
        picture->unref();
    }
};

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Video::Video() :
    pImpl(new Impl)
{
}

Video::~Video()
{
    delete (pImpl);
}

Result Video::play() noexcept
{
    //TODO:

    return Result::InsufficientCondition;
}

Result Video::pause() noexcept
{
    //TODO:

    return Result::InsufficientCondition;
}

Result Video::stop() noexcept
{
    //TODO:

    return Result::InsufficientCondition;
}

Result Video::seek(float seconds) noexcept
{
    if (seconds < 0.0f) return Result::InvalidArguments;

    //TODO:

    return Result::InsufficientCondition;
}

Result Video::loop(bool on) noexcept
{
    //TODO:

    return Result::InsufficientCondition;
}

Picture* Video::picture() const noexcept
{
    return pImpl->picture;
}

float Video::time() const noexcept
{
    //TODO:

    return 0.0f;
}

float Video::duration() const noexcept
{
    //TODO:

    return 0.0f;
}

Result Video::volume(float volume) noexcept
{
    if (volume < 0.0f || volume > 1.0f) return Result::InvalidArguments;

    //TODO:

    pImpl->volume = volume;

    return Result::Success;
}

float Video::volume() const noexcept
{
    return pImpl->volume;
}

Result Video::mute(bool on) noexcept
{
    //TODO:

    pImpl->muted = on;
    return Result::Success;
}

bool Video::muted() const noexcept
{
    return pImpl->muted;
}

Video* Video::gen() noexcept
{
    return new Video;
}