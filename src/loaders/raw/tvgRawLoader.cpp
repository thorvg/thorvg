/*
 * Copyright (c) 2020 - 2026 ThorVG project. All rights reserved.

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

#include "tvgLoader.h"
#include "tvgRawLoader.h"


RawLoader::RawLoader() : ImageLoader(FileType::Raw)
{
}


RawLoader::~RawLoader()
{
    if (copy) tvg::free(surface.buf32);
}

bool RawLoader::open(const uint32_t* data, uint32_t w, uint32_t h, ColorSpace cs, bool copy)
{
    if (!Loader::read()) return true;

    if (!data || w == 0 || h == 0) return false;

    this->w = (float)w;
    this->h = (float)h;
    this->copy = copy;

    auto channelSize = CHANNEL_SIZE(cs);

    if (copy) {
        auto size = channelSize * w * h;
        surface.buf8 = tvg::malloc<uint8_t>(size);
        if (!surface.buf8) return false;
        memcpy(surface.buf8, data, size);
    }
    else surface.buf32 = const_cast<uint32_t*>(data);

    //setup the surface
    surface.stride = w;
    surface.w = w;
    surface.h = h;
    surface.cs = cs;
    surface.channelSize = channelSize;
    surface.premultiplied = (cs == ColorSpace::ABGR8888 || cs == ColorSpace::ARGB8888 || cs == ColorSpace::BGR888 || cs == ColorSpace::RGB888);

    return true;
}


bool RawLoader::read()
{
    Loader::read();

    return true;
}
