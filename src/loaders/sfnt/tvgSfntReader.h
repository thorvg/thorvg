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

#ifndef _TVG_SFNT_READER_H
#define _TVG_SFNT_READER_H

#include <atomic>
#include "tvgRender.h"

#define INVALID_GLYPH ((uint32_t)-1)

static int _cmpu32(const void *a, const void *b)
{
    return memcmp(a, b, 4);
}

static inline uint32_t _u32(uint8_t* data, uint32_t offset)
{
    auto base = data + offset;
    return (base[0] << 24 | base[1] << 16 | base[2] << 8 | base[3]);
}

static inline uint16_t _u16(uint8_t* data, uint32_t offset)
{
    auto base = data + offset;
    return (base[0] << 8 | base[1]);
}

static inline int16_t _i16(uint8_t* data, uint32_t offset)
{
    return (int16_t)_u16(data, offset);
}

static inline uint8_t _u8(uint8_t* data, uint32_t offset)
{
    return *(data + offset);
}

static inline int8_t _i8(uint8_t* data, uint32_t offset)
{
    return (int8_t)_u8(data, offset);
}

struct SfntGlyph
{
    uint32_t idx;        //glyph index
    float advance;       //advance width/height
    float lsb;           //left side bearing
    float x, y, w, h;    //bounding box
};

struct SfntGlyphMetrics : SfntGlyph
{
    RenderPath path;     //outline path
};

struct SfntReader
{
    uint8_t* data = nullptr;
    uint32_t size = 0;

    struct
    {
        TextMetrics hhea;      //horizontal header info
        uint16_t unitsPerEm;
        uint16_t numHmtx;      //the number of Horizontal metrics table
        uint8_t locaFormat;    //0 for short offsets, 1 for long
    } metrics;

    SfntReader(uint8_t* data, uint32_t size) :
        data(data), size(size) {}
    virtual ~SfntReader() {}

    virtual bool header();
    virtual uint32_t glyph(uint32_t codepoint, SfntGlyphMetrics* tgm) = 0;
    virtual bool positioning(uint32_t lglyph, uint32_t rglyph, Point& out) = 0;
    bool convert(RenderPath& path, SfntGlyph& glyph, uint32_t glyphOffset, const Point& offset, uint16_t depth);

protected:
    //table offsets
    atomic<uint32_t> cmap{};  // character map
    atomic<uint32_t> hmtx{};  // horizontal metrics
    atomic<uint32_t> maxp{};  // maximum profile

    uint32_t cmap_12_13(uint32_t table, uint32_t codepoint, int which) const;
    uint32_t cmap_4(uint32_t table, uint32_t codepoint) const;
    uint32_t cmap_6(uint32_t table, uint32_t codepoint) const;
    bool validate(uint32_t offset, uint32_t margin) const;
    uint32_t table(const char* tag);

    bool glyphMetrics(SfntGlyph& glyph);
    bool convertComposite(RenderPath& path, SfntGlyph& glyph, uint32_t glyphOffset, const Point& offset, uint16_t depth);
    bool genPath(uint8_t* flags, uint16_t basePoint, uint16_t count);
    bool points(uint32_t outline, uint8_t* flags, Point* pts, uint32_t ptsCnt, const Point& offset);
    bool flags(uint32_t *outline, uint8_t* flags, uint32_t flagsCnt);
};

#endif  //_TVG_SFNT_READER_H