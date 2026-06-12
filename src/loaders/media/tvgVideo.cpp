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

#define FETCH_LOADER(RET_VAL)                                                              \
    auto loader = static_cast<MediaLoader*>(tvg::to<PictureImpl>(pImpl->picture)->loader); \
    if (!loader) return RET_VAL

struct Video::Impl
{
    Picture* picture;

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
    FETCH_LOADER(Result::InsufficientCondition);
    return loader->play();
}

Result Video::pause() noexcept
{
    FETCH_LOADER(Result::InsufficientCondition);
    return loader->pause();
}

Result Video::stop() noexcept
{
    FETCH_LOADER(Result::InsufficientCondition);
    return loader->stop();
}

Result Video::seek(float seconds) noexcept
{
    if (seconds < 0.0f) return Result::InvalidArguments;
    FETCH_LOADER(Result::InsufficientCondition);
    return loader->seek(seconds);
}

Result Video::loop(bool on) noexcept
{
    FETCH_LOADER(Result::InsufficientCondition);
    return loader->loop(on);
}

Picture* Video::picture() const noexcept
{
    return pImpl->picture;
}

float Video::time() const noexcept
{
    FETCH_LOADER(0.0f);
    return loader->curTime;
}

float Video::duration() const noexcept
{
    FETCH_LOADER(0.0f);
    return loader->totalTime;
}

Result Video::volume(float volume) noexcept
{
    if (volume < 0.0f || volume > 1.0f) return Result::InvalidArguments;
    FETCH_LOADER(Result::InsufficientCondition);
    return loader->volume(volume);
}

float Video::volume() const noexcept
{
    FETCH_LOADER(0.0f);
    return loader->audioVolume;
}

Result Video::mute(bool on) noexcept
{
    FETCH_LOADER(Result::InsufficientCondition);
    return loader->mute(on);
}

bool Video::muted() const noexcept
{
    FETCH_LOADER(false);
    return loader->muted;
}

Video* Video::gen() noexcept
{
    return new Video;
}
