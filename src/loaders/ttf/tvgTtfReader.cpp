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


static inline int8_t _i8(uint8_t* data, uint32_t offset)
{
    return (int8_t) _u8(data, offset);
}


static int _cmpu32(const void *a, const void *b)
{
    return memcmp(a, b, 4);
}


bool TtfReader::validate(uint32_t offset, uint32_t margin) const
{
    if ((offset > size) || (size - offset < margin)) {
        TVGERR("TTF", "Invalidate data");
        return false;
    }
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

    auto loca = this->loca.load();
    if (loca == 0) this->loca = loca = table("loca");

    auto glyf = this->glyf.load();
    if (glyf == 0) this->glyf = glyf = table("glyf");

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
    metrics.hhea.advance = metrics.hhea.ascent - metrics.hhea.descent + metrics.hhea.lineGap;
    metrics.numHmtx = _u16(data, hhea + 34);

    //kerning
    auto kern = this->kern = table("kern");
    if (kern) {
        if (!validate(kern, 4)) return false;
        if (_u16(data, kern) != 0) return false;
    }

    return true;
}


uint32_t TtfReader::glyph(uint32_t codepoint)
{
    auto cmap = this->cmap.load();
    if (cmap == 0) {
        this->cmap = cmap = table("cmap");
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


uint32_t TtfReader::glyph(uint32_t codepoint, TtfGlyphMetrics* tgm)
{
    tgm->idx = glyph(codepoint);
    if (tgm->idx == INVALID_GLYPH) return 0;
    return glyphMetrics(*tgm);
}


uint32_t TtfReader::glyphMetrics(TtfGlyph& glyph)
{
    //horizontal metrics
    auto hmtx = this->hmtx.load();
    if (hmtx == 0) this->hmtx = hmtx = table("hmtx");

    //glyph is inside long metrics segment.
    if (glyph.idx < metrics.numHmtx) {
        auto offset = hmtx + 4 * glyph.idx;
        if (!validate(offset, 4)) return 0;
        glyph.advance = _u16(data, offset);
        glyph.lsb = _i16(data, offset + 2);
    /* glyph is inside short metrics segment. */
    } else {
        auto boundary = hmtx + 4U * (uint32_t) metrics.numHmtx;
        if (boundary < 4) return 0;

        auto offset = boundary - 4;
        if (!validate(offset, 4)) return 0;
        glyph.advance = _u16(data, offset);
        offset = boundary + 2 * (glyph.idx - metrics.numHmtx);
        if (!validate(offset, 2)) return 0;
        glyph.lsb = _i16(data, offset);
    }

    auto glyphOffset = outlineOffset(glyph.idx);
    // glyph without outline
    if (glyphOffset == 0) {
        glyph.y = glyph.w = glyph.h = 0.0f;
        return 0;
    }
    if (!validate(glyphOffset, 10)) return 0;

    //read the bounding box from the font file verbatim.
    float bbox[4];
    bbox[0] = static_cast<float>(_i16(data, glyphOffset + 2));
    bbox[1] = static_cast<float>(_i16(data, glyphOffset + 4));
    bbox[2] = static_cast<float>(_i16(data, glyphOffset + 6));
    bbox[3] = static_cast<float>(_i16(data, glyphOffset + 8));

    glyph.w = bbox[2] - bbox[0] + 1;
    glyph.h = bbox[3] - bbox[1] + 1;
    glyph.y = bbox[3];

    return glyphOffset;
}


bool TtfReader::convert(RenderPath& path, TtfGlyph& glyph, uint32_t glyphOffset, const Point& offset, uint16_t depth)
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


bool TtfReader::convertComposite(RenderPath& path, TtfGlyph& glyph, uint32_t glyphOffset, const Point& offset, uint16_t depth)
{
    #define ARG_1_AND_2_ARE_WORDS 0x0001
    #define ARGS_ARE_XY_VALUES 0x0002
    #define WE_HAVE_A_SCALE 0x0008
    #define MORE_COMPONENTS 0x0020
    #define WE_HAVE_AN_X_AND_Y_SCALE 0x0040
    #define WE_HAVE_A_TWO_BY_TWO 0x0080

    TtfGlyph compGlyph;
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


bool TtfReader::kerning(uint32_t lglyph, uint32_t rglyph, Point& out)
{
    #define HORIZONTAL_KERNING 0x01
    #define MINIMUM_KERNING 0x02
    #define CROSS_STREAM_KERNING 0x04

    if (!kern) return false;

    auto kern = this->kern.load();

    //kern tables
    auto tableCnt = _u16(data, kern + 2);
    kern += 4;

    while (tableCnt > 0) {
        //read subtable header.
        if (!validate(kern, 6)) return false;
        auto length = _u16(data, kern + 2);
        auto format = _u8(data, kern + 4);
        auto flags = _u8(data, kern + 5);
        kern += 6;

        if (format == 0 && (flags & HORIZONTAL_KERNING) && !(flags & MINIMUM_KERNING)) {
            //read format 0 header.
            if (!validate(kern, 8)) return false;
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

    return true;
}
