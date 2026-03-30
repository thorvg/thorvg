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

#include "tvgOtfReader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

int32_t OtfReader::offset(uint32_t base, uint8_t size)
{
    int32_t ret = 0;
    for (uint8_t i = 0; i < size; ++i) {
        ret = (ret << 8) | u8(base++);
    }
    return ret;
}

uint32_t OtfReader::skip(uint32_t idx)
{
    auto cnt = u16(idx);
    if (cnt == 0) return idx + 2;

    auto offSize = u8(idx + 2);
    auto offsetsBase = idx + 3;
    auto lastOffsetPos = offsetsBase + cnt * offSize;
    uint32_t lastOffset = 0;

    for (uint8_t i = 0; i < offSize; ++i) {
        lastOffset = (lastOffset << 8) | u8(lastOffsetPos + i);
    }

    auto dataStart = offsetsBase + (cnt + 1) * offSize;  // data starts after the offset array
    return dataStart + (lastOffset - 1);                 // CFF index offsets are 1 - based
};

bool OtfReader::name(uint32_t idx)
{
    auto count = u16(idx);
    if (count == 0) return false;

    auto size = u8(idx + 2);
    auto base = idx + 3;
    auto data = base + (count + 1) * size;

    for (uint16_t i = 0; i < count; ++i) {
        auto off1 = offset(base + i * size, size);
        auto off2 = offset(base + (i + 1) * size, size);
        auto start = data + (off1 - 1);
        auto length = off2 - off1;
        printf("Name[%d]: ", i);
        for (int j = 0; j < length; ++j) {
            char c = (char)u8(start + j);
            printf("%c", c);
        }
        printf("\n");
    }

    return true;
}

// common part of DICT interpret
bool OtfReader::DICT(uint8_t b, uint32_t& ptr)
{
    // operand decoding
    if (b >= 32) {
        auto v = 0;
        if (b <= 246) {
            v = b - 139;
            ptr += 1;
        } else if (b <= 250) {
            v = ((b - 247) * 256) + u8(ptr + 1) + 108;
            ptr += 2;
        } else if (b <= 254) {
            v = -((b - 251) * 256) - u8(ptr + 1) - 108;
            ptr += 2;
        } else {
            v = i32(ptr + 1);  // b == 255: 16.16 fixed-point, not used as dict offset → skip
            ptr += 5;
        }
        args.push(v);
        return true;
    // 2 bytes integer
    } else if (b == 28) {
        args.push(i16(ptr + 1));
        ptr += 3;
        return true;
    // 4 bytes integer
    } else if (b == 29) {
        args.push(i32(ptr + 1));
        ptr += 5;
        return true;
    // unnecessary. skip operators (0: Copyright, 7: FontMatrix, etc)
    } else if (b == 12) {
        ptr += 2;
    }
    return false;
}

uint32_t OtfReader::localSubrs(uint32_t offset, uint32_t size)
{
    args.clear();

    auto base = cff + offset;
    auto end = base + size;
    auto ptr = base;

    while (ptr < end) {
        auto b = u8(ptr);
        if (DICT(b, ptr)) continue;
        if (b == 19) return base + args.last();  // sub-routine
        ++ptr;
        args.clear();
    }
    return 0;
}

// table("CFF ")
// ├── Header
// ├── Name INDEX
// ├── Top DICT INDEX
// │   └── CharStrings (op: 17) ← toCharStrings (relative offset within CFF)
// ├── String INDEX
// ├── Global Subr INDEX
// └── CharStrings INDEX ← cff + toCharStrings
//     ├── count (u16)
//     ├── offSize (u8)
//     ├── offset array [(count + 1) * offSize]
//     └── data (CharString bytecode per glyph)
bool OtfReader::CFF()
{
    cff = table("CFF ");  // PostScript outlines
    auto cff2 = false;
    if (!cff) {
        cff = table("CFF2");  // variable font
        cff2 = true;
    }
    if (!cff || !validate(cff, 4)) return false;

    auto p = cff + u8(cff + 2);  // skip the header size

    // parseName(p);
    p = skip(p);  // name

    // parse Top DICT INDEX
    auto count = u16(p);
    if (count == 0) return false;

    args.clear();

    auto size = u8(p + 2);
    auto base = (p + 3);
    auto data = base + (count + 1) * size;
    auto ptr = data + (offset(base, size) - 1);
    auto end = data + (offset(base + size, size) - 1);

    while (ptr < end) {
        auto b = u8(ptr);
        if (DICT(b, ptr)) continue;
        // CharStrings
        if (b == 17) {
            toCharStrings = args.last();
            break;
        // find the subroutine from private DICT
        } else if (b == 18 && args.count >= 2) {
            subrs = localSubrs(args[args.count - 1], args[args.count - 2]);
        }
        ++ptr;
        args.clear();
    }
    if (toCharStrings == 0) return false;
    toCharStrings += cff;

    p = skip(p);             // jump to the end of INDEX
    if (!cff2) p = skip(p);  // only cff1 has the String INDEX

    gsubrs = p;

    return true;
}

bool OtfReader::subRoutine(float operand, uint32_t subrs, uint32_t& p, uint32_t& end, SubRoutine* stack, int& ssp)
{
    if (ssp >= SUBROUTINE_MAX) return false;

    stack[ssp++] = {p, end};  // save current execution state

    auto count = u16(subrs);
    if (count == 0) return false;  // no available subroutines

    auto size = u8(subrs + 2);              // offset size in bytes
    auto base = subrs + 3;                  // start of offset array
    auto data = base + (count + 1) * size;  // start of subroutine data

    // compute subroutine bias (CFF specification)
    int bias = (count < 1240) ? 107 : (count < 33900) ? 1131
                                                      : 32768;

    // apply bias to get actual subroutine index
    auto subrIdx = (int)round(operand) + bias;

    // read subroutine byte range from offset array
    auto off1 = offset(base + subrIdx * size, size);
    auto off2 = offset(base + (subrIdx + 1) * size, size);
    if (off2 <= off1) return false;

    // jump to subroutine program
    p = data + (off1 - 1);
    end = data + (off2 - 1);

    return true;
}

void OtfReader::alternatingCurve(Array<float>& values, Point& pos, RenderPath& path, bool horizontal)
{
    uint32_t i = 0;
    while (i + 3 < values.count) {
        if (horizontal) {
            auto cp1 = pos + Point{values[i++], 0};
            auto cp2 = cp1 + Point{values[i++], -values[i++]};
            pos = cp2 + Point{0, -values[i++]};
            path.cubicTo(cp1, cp2, pos);
        } else {
            auto cp1 = pos + Point{0, -values[i++]};
            auto cp2 = cp1 + Point{values[i++], -values[i++]};
            pos = cp2 + Point{values[i++], 0};
            path.cubicTo(cp1, cp2, pos);
        }
        horizontal = !horizontal;
    }
// edge case
#if 0
    if (i < sp) {
        if (!horizontal) pos.x += stack[i];
        else pos.y += stack[i];
    }
#endif
    i += 4;
    values.clear();
}

void OtfReader::alignedCurve(Array<float>& values, Point& pos, RenderPath& path, bool horizontal)
{
    uint32_t i = 0;
    auto d = 0.0f;  // optional operand (odd count)

    if (values.count % 4 == 1) {
        d = values[i++];
    }

    while (i + 3 < values.count) {
        Point cp1, cp2;
        if (horizontal) {
            cp1 = pos + Point{values[i++], -d};
            cp2 = cp1 + Point{values[i++], -values[i++]};
            pos = cp2 + Point{values[i++], 0};
        } else {
            cp1 = pos + Point{d, -values[i++]};
            cp2 = cp1 + Point{values[i++], -values[i++]};
            pos = cp2 + Point{0, -values[i++]};
        }
        path.cubicTo(cp1, cp2, pos);
        d = 0.0f;
    }
    values.clear();
}

bool OtfReader::charStrings(RenderPath& path, SfntGlyph& glyph)
{
    values.clear();

    // get the glyph data offset
    auto count = u16(toCharStrings);
    if (glyph.idx >= count) return false;

    auto size = u8(toCharStrings + 2);
    auto base = toCharStrings + 3;
    auto data = base + (count + 1) * size;
    auto offset1 = offset(base + glyph.idx * size, size);
    auto offset2 = offset(base + (glyph.idx + 1) * size, size);

    auto len = offset2 - offset1;
    if (len <= 0) return false;
    auto p = data + (offset1 - 1);
    auto end = (p + len);

    Point pos = {0.0f, glyph.h};  // current point (absolute)

    SubRoutine subStack[SUBROUTINE_MAX];
    auto ssp = 0;  // subroutine stack pointer

    while (p < end) {
        auto b = u8(p);
        printf("b = %d, sp = %d\n", b, values.count);
        switch (b) {
            case 4: {  // vmoveto
                pos.y -= values.pick();
                path.moveTo(pos);
                values.clear();
                break;
            }
            case 5: {  // rlineto
                for (uint32_t i = 0; i + 1 < values.count; i += 2) {
                    pos.x += values[i];
                    pos.y -= values[i + 1];
                    path.lineTo(pos);
                }
                values.clear();
                break;
            }
            case 6: {  // hlineto: dx1 dy2 dx3 ...
                for (uint32_t i = 0; i < values.count; i++) {
                    if (i % 2 == 0) pos.x += values[i];
                    else pos.y -= values[i];
                    path.lineTo(pos);
                }
                values.clear();
                break;
            }
            case 7: {  // vlineto: dy1 dx2 dy3 ...
                for (uint32_t i = 0; i < values.count; i++) {
                    if (i % 2 == 0) pos.y -= values[i];
                    else pos.x += values[i];
                    path.lineTo(pos);
                }
                values.clear();
                break;
            }
            case 8: {  // rrcurveto: dx1 dy1 dx2 dy2 dx3 dy3 ...
                for (uint32_t i = 0; i + 5 < values.count; i += 6) {
                    auto cx1 = pos.x + values[i];
                    auto cy1 = pos.y - values[i + 1];
                    auto cx2 = cx1 + values[i + 2];
                    auto cy2 = cy1 - values[i + 3];
                    pos.x = cx2 + values[i + 4];
                    pos.y = cy2 - values[i + 5];
                    printf("rrcurveto (%f,%f) (%f,%f) (%f,%f)\n", cx1, cy1, cx2, cy2, pos.x, pos.y);
                }
                values.clear();
                break;
            }
            case 10: {
                if (this->subRoutine(values.pick(), subrs, p, end, subStack, ssp)) continue;
                return false;
            }
            case 11: {  // return from the subroutine
                if (ssp <= 0) return false;
                auto s = subStack[--ssp];
                p = s.p;
                end = s.end;
                break;
            }
            case 12: {
                // 2-byte escape operator
                auto b1 = u8(p + 1);
                // TODO: handle as needed
                (void)b1;
                values.clear();
                p += 2;
                break;
            }
            case 14: {  // close
                path.close();
                break;
            }
            case 21: {  // rmoveto
                pos.y -= values.pick();
                pos.x += values.pick();
                path.moveTo(pos);
                values.clear();
                break;
            }
            case 22: {  // hmoveto
                pos.x += values.pick();
                path.moveTo(pos);
                values.clear();
                break;
            }
            case 26: {  // vvcurveto
                alignedCurve(values, pos, path, false);
                break;
            }
            case 27: {  // hhcurveto
                alignedCurve(values, pos, path, true);
                break;
            }
            case 29: {
                if (this->subRoutine(values.pick(), gsubrs, p, end, subStack, ssp)) continue;
                return false;
            }
            case 30: {  // vhcurveto
                alternatingCurve(values, pos, path, false);
                break;
            }
            case 31: {  // hvcurveto
                alternatingCurve(values, pos, path, true);
                break;
            }
            default: {
                // operand decoding (number decoding)
                if (b >= 32) {
                    // 1 byte integer [-107 - +107]
                    if (b <= 246) {
                        values.push(b - 139);
                        ++p;
                    // 2 byte positive integer [108 - 1131]
                    } else if (b <= 250) {
                        values.push((b - 247) * 256 + u8(p + 1) + 108);
                        p += 2;
                    // 2 byte negative integer [-1131 - -108]
                    } else if (b <= 254) {
                        values.push(-((b - 251) * 256) - u8(p + 1) - 108);
                        p += 2;
                    // (b == 255) 5 byte signed 16.16 fixed-point number
                    } else {
                        values.push(i32(p + 1) / 65536.0f);
                        p += 5;
                    }
                } else if (b == 28) {
                    values.push(i16(p + 1));
                    p += 3;
                } else if (b == 29) {
                    values.push(i32(p + 1));
                    p += 5;
                }
                continue;
            }
        }
        ++p;
    }
    return true;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool OtfReader::header()
{
    if (!SfntReader::header()) return false;
    return CFF();
}

bool OtfReader::positioning(uint32_t lglyph, uint32_t rglyph, Point& out)
{
    // TODO:
    return false;
}

bool OtfReader::convert(SfntGlyph& glyph, uint32_t codepoint, RenderPath& path)
{
    glyph.idx = SfntReader::glyph(codepoint);
    if (glyph.idx == INVALID_GLYPH) return false;
    if (glyphMetrics(glyph)) return charStrings(path, glyph);
    return false;
}