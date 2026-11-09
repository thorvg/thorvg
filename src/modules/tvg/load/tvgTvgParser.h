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

#ifndef _TVG_TVG_READER_H_
#define _TVG_TVG_READER_H_

#include "tvgCommon.h"
#include "tvgColor.h"
#include "tvgTvgModel.h"

struct TvgParser
{
    TvgScene* main = nullptr;

    TvgParser(const char* content, uint32_t size);
    TvgScene* parse();
    Result header(uint16_t& ver, float& w, float& h, float& dur);

private:
    const char* content;  // tvg data
    const char* end;      // tvg data end position
    uint32_t size;        // tvg data size

    string buffer;           // current key and optional ID or value, separated by a null terminator.
    const char* cursor;      // current position in content where parsing resumes.
    uint32_t idOffset = 0;   // byte offset of the ID in buffer, or zero when no ID is present.
    size_t valueOffset = 0;  // byte offset of the property value, or zero for a structure.
    bool pending = false;    // next() skips the current structure unless enter() is called.
    bool malformed = false;  // true if this parsing data is malformed

    // helpers
    bool enter();
    void trim(const char*& p, const char* end);
    const char* next();
    void error(const char* msg);
    bool read(const char*& p, const char* end, float& out, bool separator);

    // parsers
    bool parsePaint(TvgPaint* paint, const char* key);
    TvgScene* parseScene(TvgScene* parent);
    TvgShape* parseShape(TvgScene* parent);
    void parseRect(TvgShape* shape);
    void parseCircle(TvgShape* shape);
    void parsePath(TvgShape* shape);

    // static property
    const char* getId();
    void getFloat(float* out, uint32_t cnt);
    void getFloats(float* out, uint32_t max);
    float getFloat();
    int getInteger();
    void getFill(RGBA& out);
    bool getBool();

    // animatable property
    void getProperty(TvgPaint* obj, TvgPoint& out);
};

#endif  // _TVG_TVG_READER_H_
