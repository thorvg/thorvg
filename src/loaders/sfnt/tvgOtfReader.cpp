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
    return dataStart + (lastOffset - 1);  // CFF index offsets are 1 - based
};

bool OtfReader::parseName(uint32_t idx)
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
        for (uint32_t j = 0; j < length; ++j) {
            char c = (char)u8(start + j);
            printf("%c", c);
        }
        printf("\n");
    }

    return true;
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
bool OtfReader::parseCFF()
{
    cff = table("CFF ");  // PostScript outlines
    auto cff2  = false;
    if (!cff) {
        cff = table("CFF2");  // variable font
        cff2 = true;
    }
    if (!cff || !validate(cff, 4)) return false;

    auto p = cff + u8(cff + 2);  // skip the header size

    //parseName(p);
    p = skip(p);  // name

    // parse Top DICT INDEX
    auto count = u16(p);
    if (count == 0) return false;

    auto size  = u8(p + 2);
    auto base  = (p + 3);
    auto data  = base + (count + 1) * size;
    auto start = data + (offset(base, size) - 1);
    auto end   = data + (offset(base + size, size) - 1);
    int32_t operand = 0;

    for (auto i = start; i < end; ) {
        auto b = u8(i);
        // operand decoding
        if (b >= 32) {
            if (b <= 246) {
                operand = b - 139;
                i += 1;
            } else if (b <= 250) {
                operand = ((b - 247) * 256) + u8(i + 1) + 108;
                i += 2;
            } else if (b <= 254) {
                operand = -((b - 251) * 256) - u8(i + 1) - 108;
                i += 2;
            } else {
                // b == 255: 16.16 fixed-point, not used as dict offset → skip
                i += 5;
            }
        } else if (b == 28) {
            operand = i16(i + 1);
            i += 3;
        } else if (b == 29) {
            operand = i32(i + 1);
            i += 5;
        // operator
        } else {
            if (b == 12) {
                // 2-byte escape operator
                auto b1 = u8(i + 1);
                // 12 0: Copyright, 12 7: FontMatrix, etc. — skip all for POC
                (void)b1;
                i += 2;
            } else {
                if (b == 17) {  // CharStrings
                    toCharStrings = operand;
                    break;
                }
                // other 1-byte operators: ignore, advance
                ++i;
            }
        }
    }
    if (toCharStrings == 0) return false;
    toCharStrings += cff;

    p = skip(p);      // jump to the end of INDEX
    if (!cff2) p = skip(p);        // only cff1 has the String INDEX

    gsubrs = p;

    return true;
}

bool OtfReader::parseCharStrings(uint32_t glyph)
{
    // get the glyph data offset
    auto count = u16(toCharStrings);
    if (glyph >= count) return false;

    auto size = u8(toCharStrings + 2);
    auto base = toCharStrings + 3;
    auto data = base + (count + 1) * size;
    auto offset1 = offset(base + glyph * size, size);
    auto offset2 = offset(base + (glyph + 1) * size, size);

    auto len = offset2 - offset1;
    if (len <= 0) return false;
    auto p = data + (offset1 - 1);
    auto end = (p + len);

    float stack[48];
    int sp = 0;  // stack pointer
    float x = 0, y = 0;  // current point (absolute)
    float v;
    bool inHint = true;  // in hint phase?

    #define SUBROUTINE_MAX 10
    struct SubRoutine {
        uint32_t p, end;
    } subStack[SUBROUTINE_MAX];
    int ssp = 0;  // subroutine stack pointer

    auto push = [&](float v) {
        printf("push = %f\n", v);
        stack[sp++] = v;
    };

    auto pop = [&]() {
        printf("pop = %f\n", stack[sp - 1]);
        return stack[--sp];
    };

    while (p < end) {
        auto b = u8(p);
        printf("b = %d, sp = %d\n", b, sp);

        switch (b) {
            case 22: {  // hmoveto
                x += pop();
                printf("hmoveto (%f, %f)\n", x, y);
                inHint = false;
                sp = 0;
                break;
            }
            case 29: {  // call global subroutine
                if (ssp >= SUBROUTINE_MAX) return false;
                v = pop();  // subroutine index from operand stack
                subStack[ssp++] = {p, end};  // save current execution state

                auto count = u16(gsubrs);
                if (count == 0) return false;  // no available subroutines

                auto size = u8(gsubrs + 2);    // offset size in bytes
                auto base = gsubrs + 3;        // start of offset array
                auto data = base + (count + 1) * size;  // start of subroutine data

                // compute subroutine bias (CFF specification)
                int bias = (count < 1240) ? 107 : (count < 33900) ? 1131 : 32768;

                // apply bias to get actual subroutine index
                auto subrIdx = (int)round(v) + bias;

                // read subroutine byte range from offset array
                auto off1 = offset(base + subrIdx * size, size);
                auto off2 = offset(base + (subrIdx + 1) * size, size);
                if (off2 <= off1) return false;

                // jump to subroutine program
                p = data + (off1 - 1);
                end = data + (off2 - 1);
                printf("call global subroutine\n");
                break;
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
                sp = 0;
                p += 2;
                break;
            }
            case 21: {  // rmoveto
                auto dy = pop();
                auto dx = pop();
                x += dx; y += dy;
                printf("rmoveto (%f, %f)\n", x, y);
                sp = 0;
                break;
            }
            case 4: {  // vmoveto
                y += pop();
                printf("vmoveto (%f, %f)\n", x, y);
                sp = 0;
                break;
            }
            case 5: {  // rlineto
                for (int i = 0; i + 1 < sp; i += 2) {
                    x += stack[i]; y += stack[i + 1];
                    printf("rlineto (%f, %f)\n", x, y);
                }
                sp = 0;
                break;
            }
            case 6: {  // hlineto: dx1 dy2 dx3 ...
                for (int i = 0; i < sp; i++) {
                    if (i % 2 == 0) x += stack[i];
                    else            y += stack[i];
                    printf("hlineto (%f, %f)\n", x, y);
                }
                sp = 0;
                break;
            }
            case 7: {  // vlineto: dy1 dx2 dy3 ...
                for (int i = 0; i < sp; i++) {
                    if (i % 2 == 0) y += stack[i];
                    else            x += stack[i];
                    printf("vlineto (%f, %f)\n", x, y);
                }
                sp = 0;
                break;
            }
            case 8: {  // rrcurveto: dx1 dy1 dx2 dy2 dx3 dy3 ...
                for (int i = 0; i + 5 < sp; i += 6) {
                    float cx1 = x  + stack[i],     cy1 = y  + stack[i + 1];
                    float cx2 = cx1 + stack[i + 2], cy2 = cy1 + stack[i + 3];
                    x = cx2 + stack[i + 4];
                    y = cy2 + stack[i + 5];
                    printf("rrcurveto (%f,%f) (%f,%f) (%f,%f)\n", cx1, cy1, cx2, cy2, x, y);
                }
                sp = 0;
                break;
            }
            case 14: {  // endchar
                printf("endchar\n");
                return true;
            }
            default: {
                // operand decoding (number decoding)
                if (b >= 32) {
                    // 1 byte integer [-107 - +107]
                    if (b <= 246) {
                        push(b - 139);
                        ++p;
                    // 2 byte positive integer [108 - 1131]
                    } else if (b <= 250) {
                        push((b - 247) * 256 + u8(p + 1) + 108);
                        p += 2;
                    // 2 byte negative integer [-1131 - -108]
                    } else if (b <= 254) {
                        push(-((b - 251) * 256) - u8(p + 1) - 108);
                        p += 2;
                    // (b == 255) 5 byte signed 16.16 fixed-point number
                    } else {
                        push(i32(p + 1) / 65536.0f);
                        p += 5;
                    }
                } else if (b == 28) {
                    push(i16(p + 1));
                    p += 3;
                } else if (b == 29) {
                    push(i32(p + 1));
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

    return parseCFF();
}

uint32_t OtfReader::glyph(uint32_t codepoint, SfntGlyphMetrics* tgm)
{
    tgm->idx = SfntReader::glyph(codepoint);
    if (tgm->idx == INVALID_GLYPH) return 0;

    return parseCharStrings(tgm->idx);
}

bool OtfReader::positioning(uint32_t lglyph, uint32_t rglyph, Point& out)
{
    //TODO:
    return false;
}

bool OtfReader::convert(RenderPath& path, SfntGlyph& glyph, uint32_t glyphOffset, const Point& offset, uint16_t depth)
{
    //TODO:
    return false;
}