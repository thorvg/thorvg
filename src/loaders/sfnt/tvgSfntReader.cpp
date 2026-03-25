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

#include "tvgSfntReader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

bool SfntReader::validate(uint32_t offset, uint32_t margin) const
{
    if ((offset > size) || (size - offset < margin)) {
        TVGERR("SFNT", "Invalidate data");
        return false;
    }
    return true;
}

uint32_t SfntReader::table(const char* tag)
{
    auto tableCnt = _u16(data, 4);
    if (!validate(12, (uint32_t) tableCnt * 16)) return 0;

    auto match = bsearch(tag, data + 12, tableCnt, 16, _cmpu32);
    if (!match) {
        TVGLOG("SFNT", "No searching table = %s", tag);
        return 0;
    }

    return _u32(data, (uint32_t) ((uint8_t*) match - data + 8));
}

uint32_t SfntReader::cmap_12_13(uint32_t table, uint32_t codepoint, int fmt) const
{
    //A minimal header is 16 bytes
    auto len = _u32(data, table + 4);
    if (len < 16) return -1;

    if (!validate(table, len)) return -1;

    auto entryCnt = _u32(data, table + 12);

    for (uint32_t i = 0; i < entryCnt; ++i) {
        auto firstCode = _u32(data, table + (i * 12) + 16);
        auto lastCode = _u32(data, table + (i * 12) + 16 + 4);
        if (codepoint < firstCode || codepoint > lastCode) continue;
        auto glyphOffset = _u32(data, table + (i * 12) + 16 + 8);
        if (fmt == 12) return (codepoint - firstCode) + glyphOffset;
        else return glyphOffset;
    }
    return -1;
}

uint32_t SfntReader::cmap_4(uint32_t table, uint32_t codepoint) const
{
    //cmap format 4 only supports the Unicode BMP.
    if (codepoint > 0xffff) return -1;

    auto shortCode = static_cast<uint16_t>(codepoint);

    if (!validate(table, 8)) return -1;

    auto segmentCnt = _u16(data, table);
    if ((segmentCnt & 1) || segmentCnt == 0) return -1;

    //find starting positions of the relevant arrays.
    auto endCodes = table + 8;
    auto startCodes = endCodes + segmentCnt + 2;
    auto idDeltas = startCodes + segmentCnt;
    auto idRangeOffsets = idDeltas + segmentCnt;

    if (!validate(idRangeOffsets, segmentCnt)) return -1;

    //find the segment that contains shortCode by binary searching over the highest codes in the segments.
    //binary search, but returns the next highest element if key could not be found.
    uint8_t* segment = nullptr;
    auto n = segmentCnt / 2;
    if (n > 0) {
        uint8_t key[2] = {(uint8_t)(codepoint >> 8), (uint8_t)codepoint};
        auto bytes = data + endCodes;
        auto low = 0;
        auto high = n - 1;
        while (low != high) {
            auto mid = low + (high - low) / 2;
            auto sample = bytes + mid * 2;
            if (memcmp(key, sample, 2) > 0) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
        segment = (bytes + low * 2);
    }

    auto segmentIdx = static_cast<uint32_t>(segment - (data + endCodes));

    //look up segment info from the arrays & short circuit if the spec requires.
    auto startCode = _u16(data, startCodes + segmentIdx);
    if (startCode > shortCode) return 0;

    auto delta = _u16(data, idDeltas + segmentIdx);
    auto idRangeOffset = _u16(data, idRangeOffsets + segmentIdx);
    //intentional integer under- and overflow.
    if (idRangeOffset == 0) return (shortCode + delta) & 0xffff;

    //calculate offset into glyph array and determine ultimate value.
    auto offset = idRangeOffsets + segmentIdx + idRangeOffset + 2U * (unsigned int)(shortCode - startCode);
    if (!validate(offset, 2)) return -1;

    auto id = _u16(data, offset);
    //intentional integer under- and overflow.
    if (id > 0) return (id + delta) & 0xffff;

    return 0;
}

uint32_t SfntReader::cmap_6(uint32_t table, uint32_t codepoint) const
{
    //cmap format 6 only supports the Unicode BMP.
    if (codepoint > 0xFFFF) return 0;

    auto firstCode  = _u16(data, table);
    auto entryCnt = _u16(data, table + 2);
    if (!validate(table, 4 + 2 * entryCnt)) return -1;

    if (codepoint < firstCode) return -1;
    codepoint -= firstCode;
    if (codepoint >= entryCnt) return -1;
    return _u16(data, table + 4 + 2 * codepoint);
}

bool SfntReader::points(uint32_t outline, uint8_t* flags, Point* pts, uint32_t ptsCnt, const Point& offset)
{
    #define X_CHANGE_IS_SMALL 0x02
    #define X_CHANGE_IS_POSITIVE 0x10
    #define X_CHANGE_IS_ZERO 0x10
    #define Y_CHANGE_IS_SMALL 0x04
    #define Y_CHANGE_IS_ZERO 0x20
    #define Y_CHANGE_IS_POSITIVE 0x20

    long accum = 0L;

    for (uint32_t i = 0; i < ptsCnt; ++i) {
        if (flags[i] & X_CHANGE_IS_SMALL) {
            if (!validate(outline, 1)) {
                return false;
            }
            auto value = (long) _u8(data, outline++);
            auto bit = (uint8_t)!!(flags[i] & X_CHANGE_IS_POSITIVE);
            accum -= (value ^ -bit) + bit;
        } else if (!(flags[i] & X_CHANGE_IS_ZERO)) {
            if (!validate(outline, 2)) return false;
            accum += _i16(data, outline);
            outline += 2;
        }
        pts[i].x = offset.x + (float) accum;
    }

    accum = 0L;

    for (uint32_t i = 0; i < ptsCnt; ++i) {
        if (flags[i] & Y_CHANGE_IS_SMALL) {
            if (!validate(outline, 1)) return false;
            auto value = (long) _u8(data, outline++);
            auto bit = (uint8_t)!!(flags[i] & Y_CHANGE_IS_POSITIVE);
            accum -= (value ^ -bit) + bit;
        } else if (!(flags[i] & Y_CHANGE_IS_ZERO)) {
            if (!validate(outline, 2)) return false;
            accum += _i16(data, outline);
            outline += 2;
        }
        pts[i].y = offset.y - (float) accum;
    }
    return true;
}

bool SfntReader::flags(uint32_t* outline, uint8_t* flags, uint32_t flagsCnt)
{
    #define REPEAT_FLAG 0x08

    auto offset = *outline;
    uint8_t value = 0;
    uint8_t repeat = 0;

    for (uint32_t i = 0; i < flagsCnt; ++i) {
        if (repeat) {
            --repeat;
        } else {
            if (!validate(offset, 1)) return false;
            value = _u8(data, offset++);
            if (value & REPEAT_FLAG) {
                if (!validate(offset, 1)) return false;
                repeat = _u8(data, offset++);
            }
        }
        flags[i] = value;
    }
    *outline = offset;
    return true;
}

bool SfntReader::glyphMetrics(SfntGlyph& glyph)
{
    //glyph is inside long metrics segment.
    if (glyph.idx < metrics.numHmtx) {
        auto offset = hmtx + 4 * glyph.idx;
        if (!validate(offset, 4)) return false;
        glyph.advance = _u16(data, offset);
        glyph.lsb = _i16(data, offset + 2);
    /* glyph is inside short metrics segment. */
    } else {
        auto boundary = hmtx + 4U * (uint32_t) metrics.numHmtx;
        if (boundary < 4) return false;

        auto offset = boundary - 4;
        if (!validate(offset, 4)) return false;
        glyph.advance = _u16(data, offset);
        offset = boundary + 2 * (glyph.idx - metrics.numHmtx);
        if (!validate(offset, 2)) return false;
        glyph.lsb = _i16(data, offset);
    }
    return true;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool SfntReader::header()
{
    if (!validate(0, 12)) return false;

    //header
    auto head = table("head");
    if (!validate(head, 54)) return false;

    metrics.unitsPerEm = _u16(data, head + 18);
    metrics.locaFormat = _u16(data, head + 50);

    //horizontal metrics
    auto hhea = table("hhea");
    if (!validate(hhea, 36)) return false;

    metrics.hhea.ascent = _i16(data, hhea + 4);
    metrics.hhea.descent = _i16(data, hhea + 6);
    metrics.hhea.linegap = _i16(data, hhea + 8);
    metrics.hhea.advance = metrics.hhea.ascent - metrics.hhea.descent + metrics.hhea.linegap;

    metrics.numHmtx = _u16(data, hhea + 34);

    // horizontal metrics
    hmtx = table("hmtx");
    if (!validate(cmap, metrics.numHmtx * 4)) return false;

    // character map
    cmap = table("cmap");
    if (!validate(cmap, 4)) return false;

    return true;
}

bool SfntReader::convert(RenderPath& path, SfntGlyph& glyph, uint32_t glyphOffset, const Point& offset, uint16_t depth)
{
    #define ON_CURVE 0x01

    if (glyphOffset == 0) return true;

    auto outlineCnt = _i16(data, glyphOffset);
    if (outlineCnt == 0) return false;
    if (outlineCnt < 0) {
        uint16_t maxComponentDepth = 1U;
        auto maxp = this->maxp.load();
        if (maxp == 0) this->maxp = maxp = table("maxp");
        if (validate(maxp, 32) && _u32(data, maxp) >= 0x00010000U) { // >= version 1.0
            maxComponentDepth = _u16(data, maxp + 30);
        }
        if (depth > maxComponentDepth) return false;
        return convertComposite(path, glyph, glyphOffset, offset, depth + 1);
    }
    auto cntrsCnt = (uint32_t) outlineCnt;
    auto outline = glyphOffset + 10;
    if (!validate(outline, cntrsCnt * 2 + 2)) return false;

    auto ret = false;
    auto ptsCnt = _u16(data, outline + (cntrsCnt - 1) * 2) + 1;
    auto endPts = tvg::malloc<size_t>(cntrsCnt * sizeof(size_t));  //the index of the contour points.

    for (uint32_t i = 0; i < cntrsCnt; ++i) {
        endPts[i] = (uint32_t) _u16(data, outline);
        outline += 2;
    }
    outline += 2U + _u16(data, outline);

    auto flags = tvg::malloc<uint8_t>(ptsCnt * sizeof(uint8_t));
    if (this->flags(&outline, flags, ptsCnt)) {
        auto pts = tvg::malloc<Point>(ptsCnt * sizeof(Point));
        if (this->points(outline, flags, pts, ptsCnt, offset)) {
            //generate tvg paths
            path.cmds.reserve(ptsCnt);
            path.pts.reserve(ptsCnt);
            uint32_t begin = 0;
            for (uint32_t i = 0; i < cntrsCnt; ++i) {
                //contour must start with move to
                auto offCurve = !(flags[begin] & ON_CURVE);
                auto ptsBegin = offCurve ? (pts[begin] + pts[endPts[i]]) * 0.5f : pts[begin];
                path.moveTo(ptsBegin);
                auto cnt = endPts[i] - begin + 1;
                for (uint32_t x = 1; x < cnt; ++x) {
                    if (flags[begin + x] & ON_CURVE) {
                        if (offCurve) {
                            path.cubicTo(path.pts.last() + (2.0f/3.0f) * (pts[begin + x - 1] - path.pts.last()), pts[begin + x] + (2.0f/3.0f) * (pts[begin + x - 1] - pts[begin + x]), pts[begin + x]);
                            offCurve = false;
                        } else {
                            path.lineTo(pts[begin + x]);
                        }
                    } else {
                        if (offCurve) {
                            auto end = (pts[begin + x] + pts[begin + x - 1]) * 0.5f;
                            path.cubicTo(path.pts.last() + (2.0f/3.0f) * (pts[begin + x - 1] - path.pts.last()), end + (2.0f/3.0f) * (pts[begin + x - 1] - end), end);
                        } else {
                            offCurve = true;
                        }
                    }
                }
                if (offCurve) {
                    path.cubicTo(path.pts.last() + (2.0f/3.0f) * (pts[begin + cnt - 1] - path.pts.last()), ptsBegin + (2.0f/3.0f) * (pts[begin + cnt - 1] - ptsBegin), ptsBegin);
                }
                //contour must end with close
                path.close();
                begin = endPts[i] + 1;
            }
            ret = true;
        }
        tvg::free(pts);
    }
    tvg::free(flags);
    tvg::free(endPts);
    return ret;
}

bool SfntReader::convertComposite(RenderPath& path, SfntGlyph& glyph, uint32_t glyphOffset, const Point& offset, uint16_t depth)
{
    #define ARG_1_AND_2_ARE_WORDS 0x0001
    #define ARGS_ARE_XY_VALUES 0x0002
    #define WE_HAVE_A_SCALE 0x0008
    #define MORE_COMPONENTS 0x0020
    #define WE_HAVE_AN_X_AND_Y_SCALE 0x0040
    #define WE_HAVE_A_TWO_BY_TWO 0x0080

    SfntGlyph compGlyph;
    Point compOffset;
    uint16_t flags;
    auto pointer = glyphOffset + 10;
    do {
        if (!validate(pointer, 4)) return false;
        flags = _u16(data, pointer);
        compGlyph.idx = _u16(data, pointer + 2U);
        if (compGlyph.idx == INVALID_GLYPH) continue;
        pointer += 4U;
        if (flags & ARG_1_AND_2_ARE_WORDS) {
            if (!validate(pointer, 4)) return false;
            // TODO: align to parent point
            compOffset = (flags & ARGS_ARE_XY_VALUES) ? Point{float(_i16(data, pointer)), -float(_i16(data, pointer + 2U))} : Point{0.0f, 0.0f};
            pointer += 4U;
        } else {
            if (!validate(pointer, 2)) return false;
            // TODO: align to parent point
            compOffset = (flags & ARGS_ARE_XY_VALUES) ? Point{float(_i8(data, pointer)), -float(_i8(data, pointer + 1U))} : Point{0.0f, 0.0f};
            pointer += 2U;
        }
        if (flags & WE_HAVE_A_SCALE) {
            if (!validate(pointer, 2)) return false;
            // TODO transformation
            // F2DOT14  scale;    /* Format 2.14 */
            pointer += 2U;
        } else if (flags & WE_HAVE_AN_X_AND_Y_SCALE) {
            if (!validate(pointer, 4)) return false;
            // TODO transformation
            // F2DOT14  xscale;    /* Format 2.14 */
            // F2DOT14  yscale;    /* Format 2.14 */
            pointer += 4U;
        } else if (flags & WE_HAVE_A_TWO_BY_TWO) {
            if (!validate(pointer, 8)) return false;
            // TODO transformation
            // F2DOT14  xscale;    /* Format 2.14 */
            // F2DOT14  scale01;   /* Format 2.14 */
            // F2DOT14  scale10;   /* Format 2.14 */
            // F2DOT14  yscale;    /* Format 2.14 */
            pointer += 8U;
        }
        if (!convert(path, compGlyph, glyphMetrics(compGlyph), offset + compOffset, depth)) return false;
    } while (flags & MORE_COMPONENTS);
    return true;
}