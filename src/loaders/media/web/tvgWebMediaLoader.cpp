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

#include <emscripten/bind.h>
#include "tvgWebMediaLoader.h"

using emscripten::val;
using emscripten::typed_memory_view;

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

EMSCRIPTEN_BINDINGS(tvg_web_media)
{
    emscripten::register_type<WebMediaPlayer>("WebMediaPlayer");
    emscripten::register_type<WebMediaState>("WebMediaState | null");
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

WebMediaLoader::~WebMediaLoader()
{
    if (!js.isUndefined() && !js.isNull()) js.call<void>("dispose");
    tvg::free(surface.buf32);
}

bool WebMediaLoader::open(const char* data, uint32_t size, TVG_UNUSED const LoaderOps& ops)
{
    auto generator = val::module_property("createMediaPlayer");
    if (generator.isUndefined()) return false;

    auto bytes = val::global("Uint8Array").new_(val(typed_memory_view(size, reinterpret_cast<const uint8_t*>(data))));
    js = WebMediaPlayer(generator(reinterpret_cast<uintptr_t>(this), bytes));
    return !js.isNull();
}

bool WebMediaLoader::sync()
{
    auto state = js.call<WebMediaState>("sync");
    if (state.isNull()) return false;

    totalTime = state["duration"].as<float>();

    auto width = state["width"].as<uint32_t>();
    auto height = state["height"].as<uint32_t>();
    if (width != surface.w || height != surface.h) {
        surface.setup(tvg::realloc<uint32_t>(surface.buf32, width * height * sizeof(uint32_t)), width, width, height, sizeof(uint32_t), ColorSpace::ABGR8888S);
        w = static_cast<float>(width);
        h = static_cast<float>(height);
    }

    auto data = state["data"];
    if (!surface.buf32 || data.isUndefined()) return false;

    val(typed_memory_view(surface.stride * surface.h * surface.channelSize, reinterpret_cast<uint8_t*>(surface.buf32))).call<void>("set", data);
    surface.premultiplied = false;
    curTime = state["time"].as<float>();
    return true;
}

RenderSurface* WebMediaLoader::bitmap()
{
    sync();
    return BitmapLoader::bitmap();
}

Result WebMediaLoader::play()
{
    paused = false;
    js.call<void>("play");
    return Result::Success;
}

Result WebMediaLoader::pause()
{
    paused = true;
    js.call<void>("pause");
    return Result::Success;
}

Result WebMediaLoader::stop()
{
    paused = true;
    curTime = 0.0f;
    js.call<void>("stop");
    return Result::Success;
}

Result WebMediaLoader::seek(float seconds)
{
    curTime = seconds;
    js.call<void>("seek", seconds);
    return Result::Success;
}

Result WebMediaLoader::loop(bool on)
{
    looping = on;
    js.call<void>("loop", on);
    return Result::Success;
}

Result WebMediaLoader::volume(float volume)
{
    audioVolume = volume;
    js.call<void>("volume", volume);
    return Result::Success;
}

Result WebMediaLoader::mute(bool on)
{
    muted = on;
    js.call<void>("mute", on);
    return Result::Success;
}

MediaLoader* MediaLoader::gen()
{
    return new WebMediaLoader;
}
