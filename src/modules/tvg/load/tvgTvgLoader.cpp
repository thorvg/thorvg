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

#include "tvgTvgLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

TvgLoader::~TvgLoader()
{
    release();
    delete (resource);
    delete (main);
}

void TvgLoader::release()
{
    delete (parser);
    parser = nullptr;

    if (owner != Ownership::Borrow) {
        tvg::free((char*)content);
        content = nullptr;
    }
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void TvgLoader::run(unsigned tid)
{
    if (parser) {
        main = parser->parse(resource);
        release();
    }

    if (main) main->build(progress);
}

bool TvgLoader::read()
{
    // the loading has been already completed
    if (!Loader::read()) return true;

    if (!content || size == 0) return false;

    TaskScheduler::request(this);

    return true;
}

bool TvgLoader::resize(Paint* paint, float w, float h)
{
    auto sx = w / this->w;
    auto sy = h / this->h;
    Matrix m = {sx, 0, 0, 0, sy, 0, 0, 0, 1};
    paint->transform(m);

    return true;
}

Result TvgLoader::open(const char* data, uint32_t size, const LoaderOps& _ops)
{
    auto ops = static_cast<const PictureOps*>(&_ops);
    if (ops->caller != tvg::Type::Picture) return Result::InvalidArguments;

    if (ops->owner == Ownership::Copy) {
        content = tvg::malloc<char>(size + 1);
        memcpy((char*)content, data, size);
        const_cast<char*>(content)[size] = '\0';
    } else content = data;

    parser = new TvgTextParser(content, size);
    auto ret = parser->header(version, w, h, playtime);
    this->size = size;
    this->owner = ops->owner;
    this->segmentEnd = playtime;
    return ret;
}

Result TvgLoader::open(const char* path, const LoaderOps& ops)
{
#ifdef THORVG_FILE_IO_SUPPORT
    if (ops.caller != tvg::Type::Picture) return Result::InvalidArguments;
    if ((content = Loader::open(path, size, true))) {
        parser = new TvgTextParser(content, size);
        auto ret = parser->header(version, w, h, playtime);
        this->owner = Ownership::Transfer;
        this->segmentEnd = playtime;
        return ret;
    }
    return Result::InvalidArguments;
#else
    return Result::NonSupport;
#endif
}

bool TvgLoader::frame(float no)
{
    // skip update if frame diff is too small
    no = nearbyintf(no * 10000.0f) * 0.0001f;
    if (fabsf(this->progress - no) <= 0.00009f) return false;

    done();

    progress = no;

    TaskScheduler::request(this);

    return true;
}

float TvgLoader::duration()
{
    return playtime;
}

float TvgLoader::curFrame()
{
    return progress;
}

bool TvgLoader::sync()
{
    done();

    // TODO:

    return true;
}

Paint* TvgLoader::paint()
{
    done();

    return main ? main->obj : nullptr;
}

bool TvgLoader::cacheable()
{
    return tvg::zero(playtime);
}