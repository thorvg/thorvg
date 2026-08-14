/*
 * Copyright (c) 2021 - 2026 ThorVG project. All rights reserved.

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

#include <memory.h>
#include "tvgLoader.h"
#include "tvgPngLoader.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void PngLoader::run(unsigned tid)
{
    auto width = static_cast<unsigned>(w);
    auto height = static_cast<unsigned>(h);

    state.info_raw.colortype = LCT_RGBA;   //request this image format

    if (lodepng_decode(&surface.buf8, &width, &height, &state, data, size)) TVGERR("PNG", "Failed to decode image");
    else if (state.info_png.iccp_defined && lodepng_toSrgb(surface.buf8, surface.buf8, width, height, &state)) TVGERR("PNG", "Unsupported ICC color profile");

    surface.setup(surface.buf32, width, width, height, sizeof(uint32_t), ColorSpace::ABGR8888S);
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

PngLoader::PngLoader() : BitmapLoader(FileType::Png)
{
    lodepng_state_init(&state);
}


PngLoader::~PngLoader()
{
    done();
    if (owner != Ownership::Borrow) tvg::free(data);
    tvg::free(surface.buf8);
    lodepng_state_cleanup(&state);
}

bool PngLoader::open(const char* path, const LoaderOps& ops)
{
#ifdef THORVG_FILE_IO_SUPPORT
    if (!(data = (unsigned char*)Loader::open(path, size))) return false;
    owner = Ownership::Transfer;

    lodepng_state_init(&state);

    unsigned int width, height;
    if (lodepng_inspect(&width, &height, &state, data, size) > 0) return false;
    w = static_cast<float>(width);
    h = static_cast<float>(height);
    return true;
#else
    return false;
#endif
}

bool PngLoader::open(const char* data, uint32_t size, const LoaderOps& ops)
{
    unsigned int width, height;
    if (lodepng_inspect(&width, &height, &state, (unsigned char*)(data), size) > 0) return false;

    if (ops.owner == Ownership::Copy) {
        this->data = tvg::malloc<unsigned char>(size);
        memcpy((unsigned char *)this->data, data, size);
    } else {
        this->data = (unsigned char *) data;
    }
    owner = ops.owner;
    w = static_cast<float>(width);
    h = static_cast<float>(height);
    this->size = size;

    return true;
}


bool PngLoader::read()
{
    if (!data || w == 0 || h == 0) return false;

    if (!Loader::read()) return true;

    TaskScheduler::request(this);

    return true;
}


RenderSurface* PngLoader::bitmap()
{
    this->done();
    return BitmapLoader::bitmap();
}
