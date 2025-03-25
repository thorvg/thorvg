/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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

#ifndef _TVG_TTF_READER_H
#define _TVG_TTF_READER_H

#include <atomic>
#include "tvgCommon.h"
#include "tvgArray.h"

#define INVALID_GLYPH ((uint32_t)-1)

struct TtfGlyphMetrics
{
    uint32_t outline;    //glyph outline table offset

    float advanceWidth;
    float leftSideBearing;
    float yOffset;
    float minw;
    float minh;
};


struct TtfReader
{
public:
    uint8_t* data = nullptr;
    uint32_t size = 0;

    struct
    {
        //horizontal header info
        struct {
            float ascent;
            float descent;
            float lineGap;
        } hhea;

        uint16_t unitsPerEm;
        uint16_t numHmtx;      //the number of Horizontal metrics table
        uint8_t locaFormat;    //0 for short offsets, 1 for long
    } metrics;

    bool header();
    uint32_t glyph(uint32_t codepoint, TtfGlyphMetrics& gmetrics);
    void kerning(uint32_t lglyph, uint32_t rglyph, Point& out);
    bool convert(Shape* shape, TtfGlyphMetrics& gmetrics, const Point& offset, const Point& kerning, uint16_t componentDepth);

private:
    //table offsets
    atomic<uint32_t> cmap{};
    atomic<uint32_t> hmtx{};
    atomic<uint32_t> loca{};
    atomic<uint32_t> glyf{};
    atomic<uint32_t> kern{};
    atomic<uint32_t> maxp{};

    uint32_t cmap_12_13(uint32_t table, uint32_t codepoint, int which) const;
    uint32_t cmap_4(uint32_t table, uint32_t codepoint) const;
    uint32_t cmap_6(uint32_t table, uint32_t codepoint) const;
    bool validate(uint32_t offset, uint32_t margin) const;
    uint32_t table(const char* tag);
    uint32_t outlineOffset(uint32_t glyph);
    uint32_t glyph(uint32_t codepoint);
    bool glyphMetrics(uint32_t glyphIndex, TtfGlyphMetrics& gmetrics);
    bool convertComposite(Shape* shape, TtfGlyphMetrics& gmetrics, const Point& offset, const Point& kerning, uint16_t componentDepth);
    bool genPath(uint8_t* flags, uint16_t basePoint, uint16_t count);
    bool genSimpleOutline(Shape* shape, uint32_t outline, uint32_t cntrsCnt);
    bool points(uint32_t outline, uint8_t* flags, Point* pts, uint32_t ptsCnt, const Point& offset);
    bool flags(uint32_t *outline, uint8_t* flags, uint32_t flagsCnt);
};

#endif //_TVG_TTF_READER_H