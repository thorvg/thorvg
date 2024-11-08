/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

#ifndef _TVG_TTF_LOADER_H_
#define _TVG_TTF_LOADER_H_

#include "tvgLoader.h"
#include "tvgTaskScheduler.h"
#include "tvgTtfReader.h"


struct TtfLoader : public FontLoader
{
#if defined(_WIN32) && (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
    void* mapping = nullptr;
#endif
    TtfReader reader;
    char* text = nullptr;
    Shape* shape = nullptr;
    bool nomap = false;
    bool freeData = false;

    TtfLoader();
    ~TtfLoader();

    using FontLoader::open;
    bool open(const char* path) override;
    bool open(const char *data, uint32_t size, const char* rpath, bool copy) override;
    bool transform(Paint* paint, float fontSize, bool italic) override;
    bool request(Shape* shape, char* text) override;
    bool read() override;
    void clear();
};

#endif //_TVG_PNG_LOADER_H_
