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

#define FETCH_LOADER(RET_VAL) \
    auto loader = tvg::to<PictureImpl>(pImpl->picture)->fetch<MediaLoader>(FileType::Media); \
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

Video::Video() : pImpl(new Impl)
{
}

Video::~Video()
{
    delete (pImpl);
}

Result Video::play() noexcept
{
    FETCH_LOADER(Result::InsufficientCondition);
    auto ret = loader->play();
    if (ret == Result::Success) loader->state = MediaLoader::State::Playing;
    return ret;
}

Result Video::pause() noexcept
{
    FETCH_LOADER(Result::InsufficientCondition);
    if (loader->state == MediaLoader::State::Stopped) return Result::InsufficientCondition;
    auto ret = loader->pause();
    if (ret == Result::Success) loader->state = MediaLoader::State::Paused;
    return ret;
}

Result Video::stop() noexcept
{
    FETCH_LOADER(Result::InsufficientCondition);
    auto ret = loader->stop();
    if (ret == Result::Success) {
        loader->curTime = 0.0f;
        loader->state = MediaLoader::State::Stopped;
    }
    return ret;
}

Result Video::seek(float seconds) noexcept
{
    FETCH_LOADER(Result::InsufficientCondition);
    if (seconds < 0.0f || seconds > loader->totalTime) return Result::InvalidArguments;
    auto ret = loader->seek(seconds);
    if (ret == Result::Success) loader->curTime = seconds;
    return ret;
}

Result Video::loop(bool on) noexcept
{
    FETCH_LOADER(Result::InsufficientCondition);
    auto ret = loader->loop(on);
    if (ret == Result::Success) loader->looping = on;
    return ret;
}

bool Video::loop() noexcept
{
    FETCH_LOADER(false);
    return loader->looping;
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
    auto ret = loader->volume(volume);
    if (ret == Result::Success) loader->audioVolume = volume;
    return ret;
}

float Video::volume() const noexcept
{
    FETCH_LOADER(0.0f);
    return loader->audioVolume;
}

Result Video::mute(bool on) noexcept
{
    FETCH_LOADER(Result::InsufficientCondition);
    auto ret = loader->mute(on);
    if (ret == Result::Success) loader->muted = on;
    return ret;
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
