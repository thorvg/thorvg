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

#include "tvgJpgLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void JpgLoader::clear()
{
    jpgdDelete(decoder);
    if (owner != Ownership::Borrow) tvg::free(data);
    decoder = nullptr;
    data = nullptr;
    owner = Ownership::Borrow;
}


void JpgLoader::run(unsigned tid)
{
    surface.setup((pixel_t*)jpgdDecompress(decoder), static_cast<uint32_t>(w), static_cast<uint32_t>(w), static_cast<uint32_t>(h), sizeof(uint32_t), ColorSpace::ABGR8888, true);
    clear();
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

JpgLoader::~JpgLoader()
{
    done();
    clear();
    tvg::free(surface.buf8);
}

Result JpgLoader::open(const char* path, const LoaderOps& ops)
{
#ifdef THORVG_FILE_IO_SUPPORT
    int width, height;
    if (!(decoder = jpgdHeader(path, &width, &height))) return Result::InvalidArguments;

    w = static_cast<float>(width);
    h = static_cast<float>(height);
    owner = Ownership::Transfer;

    return Result::Success;
#else
    return Result::NonSupport;
#endif
}

Result JpgLoader::open(const char* data, uint32_t size, const LoaderOps& ops)
{
    if (ops.owner == Ownership::Copy) {
        this->data = tvg::malloc<char>(size);
        memcpy((char *)this->data, data, size);
    } else {
        this->data = (char *) data;
    }
    owner = ops.owner;

    int width, height;
    decoder = jpgdHeader(this->data, size, &width, &height);
    if (!decoder) return Result::InvalidArguments;

    w = static_cast<float>(width);
    h = static_cast<float>(height);

    return Result::Success;
}

bool JpgLoader::read()
{
    if (!Loader::read()) return true;

    if (!decoder || w == 0 || h == 0) return false;

    TaskScheduler::request(this);

    return true;
}


bool JpgLoader::close()
{
    if (!Loader::close()) return false;
    this->done();
    return true;
}


RenderSurface* JpgLoader::bitmap()
{
    this->done();
    return BitmapLoader::bitmap();
}
