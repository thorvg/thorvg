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

#ifndef _TVG_SVG_WRITER_H_
#define _TVG_SVG_WRITER_H_

#include "tvgCommon.h"

struct SvgWriter
{
    string defs;
    string body;
    uint32_t uid = 0;

    uint32_t id()
    {
        return ++uid;
    }

    void clear()
    {
        defs.clear();
        body.clear();
        uid = 0;
    }

    static void number(string& out, float v);
    static void attr(string& out, const char* name, float v);
    static void attr(string& out, const char* name, const char* v);
    static void color(string& out, const char* name, uint8_t r, uint8_t g, uint8_t b);
    static void alpha(string& out, const char* name, uint8_t a);
    static void matrix(string& out, const char* name, const Matrix& m);
    static void url(string& out, const char* name, uint32_t id);
    static void rect(string& out, const Point& min, const Point& max);
    static void path(string& out, const PathCommand* cmds, uint32_t cmdCnt, const Point* pts);
    static void png(string& out, const uint8_t* rgba, uint32_t w, uint32_t h);

    bool write(const char* filename, float w, float h) const;
};

#endif  //_TVG_SVG_WRITER_H_
