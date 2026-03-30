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

#ifndef _TVG_OTF_READER_H_
#define _TVG_OTF_READER_H_

#include "tvgSfntReader.h"

struct OtfReader : SfntReader
{
    OtfReader(uint8_t* data, uint32_t size) :
        SfntReader(data, size) {}

    bool header() override;
    uint32_t glyph(uint32_t codepoint, SfntGlyphMetrics* tgm) override;
    bool positioning(uint32_t lglyph, uint32_t rglyph, Point& out) override;
    bool convert(RenderPath& path, SfntGlyph& glyph, uint32_t glyphOffset, const Point& offset, uint16_t depth) override;

private:
    uint32_t cff = 0;  // postscript outlines
    uint32_t toCharStrings;  // CharStrings offset in Top DICT INDEX in the cff table

    uint32_t skip(uint32_t idx);
    int32_t offset(uint32_t base, uint8_t size);

    bool parseCFF();
    bool parseName(uint32_t idx);
    bool parseCharStrings(uint32_t glyph);
};

#endif  //_TVG_OTF_READER_H_