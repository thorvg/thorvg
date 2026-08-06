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

int WebMediaLoader::sync(uint32_t* buf, uint32_t* size, float* time, float* duration)
{
    auto state = js.call<WebMediaState>("sync");
    if (state.isNull()) return 0;

    if (size) {
        size[0] = state["width"].as<uint32_t>();
        size[1] = state["height"].as<uint32_t>();
    }
    if (duration) *duration = state["duration"].as<float>();

    auto data = state["data"];
    if (!buf || data.isUndefined()) return 1;

    val(typed_memory_view(data["length"].as<uint32_t>(), reinterpret_cast<uint8_t*>(buf))).call<void>("set", data);
    if (time) *time = state["time"].as<float>();
    return 2;
}

WebMediaLoader::~WebMediaLoader()
{
    if (!js.isUndefined() && !js.isNull()) js.call<void>("dispose");
    tvg::free(surface.buf32);
}

bool WebMediaLoader::open(const char* data, uint32_t size, const LoaderOps* ops, bool copy)
{
    auto generator = val::module_property("createMediaPlayer");
    if (generator.isUndefined()) return false;

    auto bytes = val::global("Uint8Array").new_(val(typed_memory_view(size, reinterpret_cast<const uint8_t*>(data))));
    js = WebMediaPlayer(generator(reinterpret_cast<uintptr_t>(this), bytes));
    return !js.isNull();
}

bool WebMediaLoader::sync()
{
    float time;
    if (sync(surface.buf32, nullptr, &time, nullptr) < 2) return false;

    curTime = time;
    surface.premultiplied = false;
    return true;
}

RenderSurface* WebMediaLoader::bitmap()
{
    if (surface.buf32) return BitmapLoader::bitmap();

    uint32_t size[2];
    float duration, time;
    if (sync(nullptr, size, &time, &duration)) {
        surface.buf32 = tvg::calloc<uint32_t>(size[0] * size[1], sizeof(uint32_t));
        surface.stride = size[0];
        surface.w = size[0];
        surface.h = size[1];
        surface.cs = ColorSpace::ABGR8888S;
        surface.channelSize = sizeof(uint32_t);
        surface.premultiplied = false;
        w = static_cast<float>(size[0]);
        h = static_cast<float>(size[1]);
        totalTime = duration;
    }
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
