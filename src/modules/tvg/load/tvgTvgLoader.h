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

#ifndef _TVG_TVG_LOADER_H_
#define _TVG_TVG_LOADER_H_

#include "tvgCommon.h"
#include "tvgLoader.h"
#include "tvgTaskScheduler.h"
#include "tvgTvgBuilder.h"

struct TvgLoader : AnimLoader, Task
{
    TvgLoader() : AnimLoader(FileType::Tvg) {}
    ~TvgLoader();

    Result open(const char* path, const LoaderOps& ops) override;
    Result open(const char* data, uint32_t size, const LoaderOps& ops) override;
    bool resize(Paint* paint, float w, float h) override;
    bool sync() override;
    bool read() override;
    Paint* paint() override;
    bool frame(float no) override;
    float curFrame() override;
    float duration() override;
    Result segment(float begin, float end) override { /* TODO: */ return Result::NonSupport; }
    void run(unsigned tid) override;

private:
    void release();

    TvgBuilder builder;
    const char* content = nullptr;  // tvg data
    uint32_t size;                  // tvg data size
    float progress = 0.0f;
    uint16_t version;
};

#endif  // _TVG_TVG_LOADER_H_