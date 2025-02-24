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


#ifdef _WIN32
    #include <malloc.h>
#elif defined(__linux__) || defined(__ZEPHYR__)
    #include <alloca.h>
#else
    #include <stdlib.h>
#endif

#include "tvgMath.h"
#include "tvgShape.h"
#include "tvgTtfReader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/


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
    return (int16_t) _u16(data, offset);
}


static inline uint8_t _u8(uint8_t* data, uint32_t offset)
{
    return *(data + offset);
}


static int _cmpu32(const void *a, const void *b)
{
    return memcmp(a, b, 4);
}


bool TtfReader::validate(uint32_t offset, uint32_t margin) const
{
#if 1
    if ((offset > size) || (size - offset < margin)) {
        TVGERR("TTF", "Invalidate data");
        return false;
    }
#endif
    return true;
}


uint32_t TtfReader::table(const char* tag)
{
    auto tableCnt = _u16(data, 4);
    if (!validate(12, (uint32_t) tableCnt * 16)) return 0;

    auto match = bsearch(tag, data + 12, tableCnt, 16, _cmpu32);
    if (!match) {
        TVGLOG("TTF", "No searching table = %s", tag);
        return 0;
    }

    return _u32(data, (uint32_t) ((uint8_t*) match - data + 8));
}


uint32_t TtfReader::cmap_12_13(uint32_t table, uint32_t codepoint, int fmt) const
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


uint32_t TtfReader::cmap_4(uint32_t table, uint32_t codepoint) const
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


uint32_t TtfReader::cmap_6(uint32_t table, uint32_t codepoint) const
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


//Returns the offset into the font that the glyph's outline is stored at
uint32_t TtfReader::outlineOffset(uint32_t glyph)
{
    uint32_t cur, next;

    if (!loca) loca = table("loca");
    if (!glyf) glyf = table("glyf");

    if (metrics.locaFormat == 0) {
        auto base = loca + 2 * glyph;
        if (!validate(base, 4)) {
            TVGERR("TTF", "invalid outline offset");
            return 0;
        }
        cur = 2U * (uint32_t) _u16(data, base);
        next = 2U * (uint32_t) _u16(data, base + 2);
    } else {
        auto base = loca + 4 * glyph;
        if (!validate(base, 8)) return 0;
        cur = _u32(data, base);
        next = _u32(data, base + 4);
    }
    if (cur == next) return 0;
    return glyf + cur;
}


bool TtfReader::points(uint32_t outline, uint8_t* flags, Point* pts, uint32_t ptsCnt, const Point& offset)
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


bool TtfReader::flags(uint32_t *outline, uint8_t* flags, uint32_t flagsCnt)
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


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool TtfReader::header()
{
    if (!validate(0, 12)) return false;

    //verify ttf(scalable font)
    auto type = _u32(data, 0);
    if (type != 0x00010000 && type != 0x74727565) return false;

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
    metrics.hhea.lineGap = _i16(data, hhea + 8);
    metrics.numHmtx = _u16(data, hhea + 34);

    //kerning
    this->kern = table("kern");
    if (this->kern) {
        if (!validate(kern, 4)) return false;
        if (_u16(data, kern) != 0) return false;
    }

    return true;
}


uint32_t TtfReader::glyph(uint32_t codepoint)
{
    if (cmap == 0) {
        cmap = table("cmap");
        if (!validate(cmap, 4)) return -1;
    }

    auto entryCnt = _u16(data, cmap + 2);
    if (!validate(cmap, 4 + entryCnt * 8)) return -1;

    //full repertory (non-BMP map).
    for (auto idx = 0; idx < entryCnt; ++idx) {
        auto entry = cmap + 4 + idx * 8;
        auto type = _u16(data, entry) * 0100 + _u16(data, entry + 2);
        //unicode map
        if (type == 0004 || type == 0312) {
            auto table = cmap + _u32(data, entry + 4);
            if (!validate(table, 8)) return -1;
            //dispatch based on cmap format.
            auto format = _u16(data, table);
            switch (format) {
                case 12: return cmap_12_13(table, codepoint, 12);
                default: return -1;
            }
        }
    }

    //Try looking for a BMP map.
    for (auto idx = 0; idx < entryCnt; ++idx) {
        auto entry = cmap + 4 + idx * 8;
        auto type = _u16(data, entry) * 0100 + _u16(data, entry + 2);
        //Unicode BMP
        if (type == 0003 || type == 0301) {
            auto table = cmap + _u32(data, entry + 4);
            if (!validate(table, 6)) return -1;
            //Dispatch based on cmap format.
            switch (_u16(data, table)) {
                case 4: return cmap_4(table + 6, codepoint);
                case 6: return cmap_6(table + 6, codepoint);
                default: return -1;
            }
        }
    }
    return -1;
}


uint32_t TtfReader::glyph(uint32_t codepoint, TtfGlyphMetrics& gmetrics)
{
    auto glyph = this->glyph(codepoint);
    if (glyph == INVALID_GLYPH || !glyphMetrics(glyph, gmetrics)) {
        TVGERR("TTF", "invalid glyph id, codepoint(0x%x)", codepoint);
        return INVALID_GLYPH;
    }
    return glyph;
}

bool TtfReader::glyphMetrics(uint32_t glyphIndex, TtfGlyphMetrics& gmetrics)
{
    //horizontal metrics
    if (!hmtx) hmtx = table("hmtx");

    //glyph is inside long metrics segment.
    if (glyphIndex < metrics.numHmtx) {
        auto offset = hmtx + 4 * glyphIndex;
        if (!validate(offset, 4)) return false;
        gmetrics.advanceWidth = _u16(data, offset);
        gmetrics.leftSideBearing = _i16(data, offset + 2);
    /* glyph is inside short metrics segment. */
    } else {
        auto boundary = hmtx + 4U * (uint32_t) metrics.numHmtx;
        if (boundary < 4) return false;

        auto offset = boundary - 4;
        if (!validate(offset, 4)) return false;
        gmetrics.advanceWidth = _u16(data, offset);
        offset = boundary + 2 * (glyphIndex - metrics.numHmtx);
        if (!validate(offset, 2)) return false;
        gmetrics.leftSideBearing = _i16(data, offset);
    }

    gmetrics.outline = outlineOffset(glyphIndex);
    // glyph without outline
    if (gmetrics.outline == 0) {
        gmetrics.minw = gmetrics.minh = gmetrics.yOffset = 0;
        return true;
    }
    if (!validate(gmetrics.outline, 10)) return false;

    //read the bounding box from the font file verbatim.
    float bbox[4];
    bbox[0] = static_cast<float>(_i16(data, gmetrics.outline + 2));
    bbox[1] = static_cast<float>(_i16(data, gmetrics.outline + 4));
    bbox[2] = static_cast<float>(_i16(data, gmetrics.outline + 6));
    bbox[3] = static_cast<float>(_i16(data, gmetrics.outline + 8));

    if (bbox[2] <= bbox[0] || bbox[3] <= bbox[1]) return false;

    gmetrics.minw = bbox[2] - bbox[0] + 1;
    gmetrics.minh = bbox[3] - bbox[1] + 1;
    gmetrics.yOffset = bbox[3];

    return true;
}

bool TtfReader::convert(Shape* shape, TtfGlyphMetrics& gmetrics, const Point& offset, const Point& kerning, uint16_t componentDepth)
{
    #define ON_CURVE 0x01

    if (!gmetrics.outline) return true;
    auto outlineCnt = _i16(data, gmetrics.outline);
    if (outlineCnt == 0) return false;
    if (outlineCnt < 0) {
        uint16_t maxComponentDepth = 1U;
        if (!maxp) maxp = table("maxp");
        if (validate(maxp, 32) && _u32(data, maxp) >= 0x00010000U) { // >= version 1.0
            maxComponentDepth = _u16(data, maxp + 30);
        }
        if (componentDepth > maxComponentDepth) return false;
        return convertComposite(shape, gmetrics, offset, kerning, componentDepth + 1);
    }
    auto cntrsCnt = (uint32_t) outlineCnt;

    auto outline = gmetrics.outline + 10;
    if (!validate(outline, cntrsCnt * 2 + 2)) return false;

    auto ptsCnt = _u16(data, outline + (cntrsCnt - 1) * 2) + 1;
    auto endPts = (size_t*)alloca(cntrsCnt * sizeof(size_t));  //the index of the contour points.

    for (uint32_t i = 0; i < cntrsCnt; ++i) {
        endPts[i] = (uint32_t) _u16(data, outline);
        outline += 2;
    }
    outline += 2U + _u16(data, outline);

    auto flags = (uint8_t*)alloca(ptsCnt * sizeof(uint8_t));
    if (!this->flags(&outline, flags, ptsCnt)) return false;

    auto pts = (Point*)alloca(ptsCnt * sizeof(Point));
    if (!this->points(outline, flags, pts, ptsCnt, offset + kerning)) return false;

    //generate tvg paths.
    auto& pathCmds = P(shape)->rs.path.cmds;
    auto& pathPts = P(shape)->rs.path.pts;
    pathCmds.reserve(ptsCnt);
    pathPts.reserve(ptsCnt);

    uint32_t begin = 0;

    for (uint32_t i = 0; i < cntrsCnt; ++i) {
        //contour must start with move to
        bool offCurve = !(flags[begin] & ON_CURVE);
        Point ptsBegin = offCurve ? (pts[begin] + pts[endPts[i]]) * 0.5f : pts[begin];
        pathCmds.push(PathCommand::MoveTo);
        pathPts.push(ptsBegin);

        auto cnt = endPts[i] - begin + 1;
        for (uint32_t x = 1; x < cnt; ++x) {
            if (flags[begin + x] & ON_CURVE) {
                if (offCurve) {
                    pathCmds.push(PathCommand::CubicTo);
                    pathPts.push(pathPts.last() + (2.0f/3.0f) * (pts[begin + x - 1] - pathPts.last()));
                    pathPts.push(pts[begin + x] + (2.0f/3.0f) * (pts[begin + x - 1] - pts[begin + x]));
                    pathPts.push(pts[begin + x]);
                    offCurve = false;
                } else {
                    pathCmds.push(PathCommand::LineTo);
                    pathPts.push(pts[begin + x]);
                }
            } else {
                if (offCurve) {
                    pathCmds.push(PathCommand::CubicTo);
                    auto end = (pts[begin + x] + pts[begin + x - 1]) * 0.5f;
                    pathPts.push(pathPts.last() + (2.0f/3.0f) * (pts[begin + x - 1] - pathPts.last()));
                    pathPts.push(end + (2.0f/3.0f) * (pts[begin + x - 1] - end));
                    pathPts.push(end);
                } else {
                    offCurve = true;
                }
            }
        }
        if (offCurve) {
            pathCmds.push(PathCommand::CubicTo);
            pathPts.push(pathPts.last() + (2.0f/3.0f) * (pts[begin + cnt - 1] - pathPts.last()));
            pathPts.push(ptsBegin + (2.0f/3.0f) * (pts[begin + cnt - 1] - ptsBegin));
            pathPts.push(ptsBegin);
        }
        //contour must end with close
        pathCmds.push(PathCommand::Close);
        begin = endPts[i] + 1;
    }
    return true;
}

bool TtfReader::convertComposite(Shape* shape, TtfGlyphMetrics& gmetrics, const Point& offset, const Point& kerning, uint16_t componentDepth)
{
    #define ARG_1_AND_2_ARE_WORDS 0x0001
    #define ARGS_ARE_XY_VALUES 0x0002
    #define WE_HAVE_A_SCALE 0x0008
    #define MORE_COMPONENTS 0x0020
    #define WE_HAVE_AN_X_AND_Y_SCALE 0x0040
    #define WE_HAVE_A_TWO_BY_TWO 0x0080

    TtfGlyphMetrics componentGmetrics;
    Point componentOffset;
    uint16_t flags, glyphIndex;
    uint32_t pointer = gmetrics.outline + 10;
    do {
        if (!validate(pointer, 4)) return false;
        flags = _u16(data, pointer);
        glyphIndex = _u16(data, pointer + 2U);
        pointer += 4U;
        if (flags & ARG_1_AND_2_ARE_WORDS) {
            if (!validate(pointer, 4)) return false;
            if(flags & ARGS_ARE_XY_VALUES) {
                componentOffset.x = static_cast<float>(_i16(data, pointer));
                componentOffset.y = -static_cast<float>(_i16(data, pointer + 2U));
            } else {
                // TODO align to parent point
                componentOffset.x = 0;
                componentOffset.y = 0;
            }
            pointer += 4U;
        } else {
            if (!validate(pointer, 2)) return false;
            if(flags & ARGS_ARE_XY_VALUES) {
                componentOffset.x = static_cast<float>((int8_t)_u8(data, pointer));
                componentOffset.y = -static_cast<float>((int8_t)_u8(data, pointer + 1U));
            } else {
                // TODO align to parent point
                componentOffset.x = 0;
                componentOffset.y = 0;
            }
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
        if (!glyphMetrics(glyphIndex, componentGmetrics)) return false;
        if (!convert(shape, componentGmetrics, offset + componentOffset, kerning, componentDepth)) return false;
    } while (flags & MORE_COMPONENTS);
    return true;
}


void TtfReader::kerning(uint32_t lglyph, uint32_t rglyph, Point& out)
{
    #define HORIZONTAL_KERNING 0x01
    #define MINIMUM_KERNING 0x02
    #define CROSS_STREAM_KERNING 0x04

    if (!kern) return;

    auto kern = this->kern;

    out.x = out.y = 0.0f;

    //kern tables
    auto tableCnt = _u16(data, kern + 2);
    kern += 4;

    while (tableCnt > 0) {
        //read subtable header.
        if (!validate(kern, 6)) return;
        auto length = _u16(data, kern + 2);
        auto format = _u8(data, kern + 4);
        auto flags = _u8(data, kern + 5);
        kern += 6;

        if (format == 0 && (flags & HORIZONTAL_KERNING) && !(flags & MINIMUM_KERNING)) {
            //read format 0 header.
            if (!validate(kern, 8)) return;
            auto pairCnt = _u16(data, kern);
            kern += 8;

            //look up character code pair via binary search.
            uint8_t key[4];
            key[0] = (lglyph >> 8) & 0xff;
            key[1] = lglyph & 0xff;
            key[2] = (rglyph >> 8) & 0xff;
            key[3] = rglyph & 0xff;

            auto match = bsearch(key, data + kern, pairCnt, 6, _cmpu32);

            if (match) {
                auto value = _i16(data, (uint32_t)((uint8_t *) match - data + 4));
                if (flags & CROSS_STREAM_KERNING) out.y += value;
                else out.x += value;
            }
        }
        kern += length;
        --tableCnt;
    }
}
