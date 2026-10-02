/*
 * Copyright (c) 2024 - 2026 ThorVG project. All rights reserved.
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

#include "webp/decode.h"
#include "tvgWebpLoader.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void WebpLoader::clear()
{
    if (owner != Ownership::Borrow) tvg::free(data);
    data = nullptr;
    owner = Ownership::Borrow;
}


void WebpLoader::run(unsigned tid)
{
    ColorSpace cs;
    uint8_t* buf8;

    // static loader WebPDecodeRGBA/WebPDecodeBGRA returns a premultiplied version.
    if (surface.cs == ColorSpace::ARGB8888 || surface.cs == ColorSpace::ARGB8888S) {
        buf8 = WebPDecodeBGRA(data, size, nullptr, nullptr);
        cs = ColorSpace::ARGB8888;
    } else  {
        buf8 = WebPDecodeRGBA(data, size, nullptr, nullptr);
        cs = ColorSpace::ABGR8888;
    }
    surface.setup((pixel_t*)buf8, static_cast<uint32_t>(w), static_cast<uint32_t>(w), static_cast<uint32_t>(h), sizeof(uint32_t), cs);
    clear();
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

WebpLoader::~WebpLoader()
{
    done();
    clear();
    tvg::free(surface.buf8);
}

Result WebpLoader::open(const char* path, const LoaderOps& ops)
{
#ifdef THORVG_FILE_IO_SUPPORT
    if (!(data = (uint8_t*)Loader::open(path, size))) return Result::InvalidArguments;
    owner = Ownership::Transfer;

    WebPBitstreamFeatures features;
    if (WebPGetFeatures(data, size, &features)) return Result::InvalidArguments;
    w = static_cast<float>(features.width);
    h = static_cast<float>(features.height);
    surface.alphaIgnored = !features.has_alpha;
    return Result::Success;
#else
    return Result::NonSupport;
#endif
}

Result WebpLoader::open(const char* data, uint32_t size, const LoaderOps& ops)
{
    if (ops.owner == Ownership::Copy) {
        this->data = tvg::malloc<uint8_t>(size);
        memcpy((uint8_t*)this->data, data, size);
    } else {
        this->data = (uint8_t*) data;
    }
    owner = ops.owner;

    WebPBitstreamFeatures features;
    if (WebPGetFeatures(this->data, size, &features)) return Result::InvalidArguments;
    w = static_cast<float>(features.width);
    h = static_cast<float>(features.height);
    surface.alphaIgnored = !features.has_alpha;
    this->size = size;

    return Result::Success;
}


bool WebpLoader::read()
{
    if (!Loader::read()) return true;

    if (!data || w == 0 || h == 0) return false;

    surface.cs = BitmapLoader::cs;

    TaskScheduler::request(this);

    return true;
}


bool WebpLoader::close()
{
    if (!Loader::close()) return false;
    this->done();
    return true;
}


RenderSurface* WebpLoader::bitmap()
{
    this->done();
    return BitmapLoader::bitmap();
}
