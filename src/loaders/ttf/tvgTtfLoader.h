/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

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

#include <unordered_map>
#include "tvgLoader.h"
#include "tvgTaskScheduler.h"
#include "tvgTtfReader.h"

using namespace std;


struct TtfMetrics : FontMetrics
{
    float baseWidth;  //Use as the reference glyph width for italic transform
};


struct TtfLoader : public FontLoader
{
#if defined(_WIN32) && (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
    void* mapping = nullptr;
#endif
    TtfReader reader;
    unordered_map<uint32_t, TtfGlyphMetrics> glyphs;  //glypha cache. key: codepoint
    char* text = nullptr;
    bool nomap = false;
    bool freeData = false;

    TtfLoader();
    ~TtfLoader();

    using FontLoader::open;
    using FontLoader::read;

    bool open(const char* path) override;
    bool open(const char *data, uint32_t size, const char* rpath, bool copy) override;
    void transform(Paint* paint, FontMetrics& fm, float italicShear) override;
    bool get(FontMetrics& fm, char* text, RenderPath& out) override;
    void copy(const FontMetrics& in, FontMetrics& out) override;
    void release(FontMetrics& fm) override;

private:
    float height(uint32_t loc, float spacing)
    {
        return (reader.metrics.hhea.advance * loc - reader.metrics.hhea.lineGap) * spacing;
    }

    uint32_t feedLine(FontMetrics& fm, float box, float x, uint32_t begin, uint32_t end, Point& cursor, uint32_t& loc, RenderPath& out);
    void wrapNone(FontMetrics& fm, const Point& box, char* utf8, RenderPath& out);
    void wrapChar(FontMetrics& fm, const Point& box, char* utf8, RenderPath& out);
    void wrapWord(FontMetrics& fm, const Point& box, char* utf8, RenderPath& out, bool smart);
    void wrapEllipsis(FontMetrics& fm, const Point& box, char* utf8, RenderPath& out);
    TtfGlyphMetrics* request(uint32_t code);
    void clear();
};

#endif //_TVG_PNG_LOADER_H_
