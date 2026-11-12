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

static char* _name(const uint8_t* data, uint32_t length)
{
    if (!length || (length & 1)) return nullptr;
    auto text = tvg::malloc<char>((length / 2) * 3 + 1);
    if (!text) return nullptr;
    auto out = text;
    for (uint32_t i = 0; i < length; i += 2) {
        auto cp = (static_cast<uint32_t>(data[i]) << 8) | data[i + 1];
        if (cp >= 0xd800 && cp <= 0xdbff) {
            if (length - i < 4) break;
            auto low = (static_cast<uint32_t>(data[i + 2]) << 8) | data[i + 3];
            if (low < 0xdc00 || low > 0xdfff) break;
            cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
            i += 2;
        } else if (cp >= 0xdc00 && cp <= 0xdfff) break;

        // Embedded NULs cannot be exposed as complete C-string names.
        if (cp == 0) break;
        if (cp < 0x80) *out++ = cp;
        else if (cp < 0x800) {
            *out++ = 0xc0 | (cp >> 6);
            *out++ = 0x80 | (cp & 0x3f);
        } else if (cp < 0x10000) {
            *out++ = 0xe0 | (cp >> 12);
            *out++ = 0x80 | ((cp >> 6) & 0x3f);
            *out++ = 0x80 | (cp & 0x3f);
        } else {
            *out++ = 0xf0 | (cp >> 18);
            *out++ = 0x80 | ((cp >> 12) & 0x3f);
            *out++ = 0x80 | ((cp >> 6) & 0x3f);
            *out++ = 0x80 | (cp & 0x3f);
        }
        if (i + 2 == length) {
            *out = '\0';
            return text;
        }
    }
    tvg::free(text);
    return nullptr;
}

bool SfntReader::validate(uint32_t offset, uint32_t margin) const
{
    if ((offset > size) || (size - offset < margin)) {
        TVGERR("SFNT", "Invalidate data");
        return false;
    }
    return true;
}

uint32_t SfntReader::table(const char* tag, uint32_t* length) const
{
    auto tableCnt = u16(4);
    if (!validate(12, (uint32_t)tableCnt * 16)) return 0;
    auto match = bsearch(tag, data + 12, tableCnt, 16, cmpu32);
    if (!match) {
        TVGLOG("SFNT", "No searching table = %s", tag);
        return 0;
    }
    auto record = static_cast<uint32_t>(static_cast<uint8_t*>(match) - data);
    auto offset = u32(record + 8);
    if (length) {
        *length = u32(record + 12);
        if (!validate(offset, *length)) return 0;
    }
    return offset;
}

uint32_t SfntReader::cmap_12_13(uint32_t table, uint32_t codepoint, int fmt) const
{
    // A minimal header is 16 bytes
    auto len = u32(table + 4);
    if (len < 16) return -1;
    if (!validate(table, len)) return -1;

    auto entryCnt = u32(table + 12);

    for (uint32_t i = 0; i < entryCnt; ++i) {
        auto firstCode = u32(table + (i * 12) + 16);
        auto lastCode = u32(table + (i * 12) + 16 + 4);
        if (codepoint < firstCode || codepoint > lastCode) continue;
        auto glyphOffset = u32(table + (i * 12) + 16 + 8);
        if (fmt == 12) return (codepoint - firstCode) + glyphOffset;
        else return glyphOffset;
    }
    return -1;
}

uint32_t SfntReader::cmap_4(uint32_t table, uint32_t codepoint) const
{
    // cmap format 4 only supports the Unicode BMP.
    if (codepoint > 0xffff || !validate(table, 8)) return -1;

    auto segmentCnt = u16(table);
    if (segmentCnt == 0 || (segmentCnt & 1)) return -1;

    // find starting positions of the relevant arrays.
    auto endCodes = table + 8;
    auto startCodes = endCodes + segmentCnt + 2;
    auto idDeltas = startCodes + segmentCnt;
    auto idRangeOffsets = idDeltas + segmentCnt;

    if (!validate(idRangeOffsets, segmentCnt)) return -1;

    // find the segment that contains shortCode by binary searching over the highest codes in the segments.
    // binary search, but returns the next highest element if key could not be found.
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

    // look up segment info from the arrays & short circuit if the spec requires.
    auto startCode = u16(startCodes + segmentIdx);
    auto shortCode = static_cast<uint16_t>(codepoint);
    if (startCode > shortCode) return 0;

    auto delta = u16(idDeltas + segmentIdx);
    auto idRangeOffset = u16(idRangeOffsets + segmentIdx);
    // intentional integer under- and overflow.
    if (idRangeOffset == 0) return (shortCode + delta) & 0xffff;

    // calculate offset into glyph array and determine ultimate value.
    auto offset = idRangeOffsets + segmentIdx + idRangeOffset + 2U * (unsigned int)(shortCode - startCode);
    if (!validate(offset, 2)) return -1;

    auto id = u16(offset);
    // intentional integer under- and overflow.
    if (id > 0) return (id + delta) & 0xffff;

    return 0;
}

uint32_t SfntReader::cmap_6(uint32_t table, uint32_t codepoint) const
{
    // cmap format 6 only supports the Unicode BMP.
    if (codepoint > 0xFFFF) return 0;

    auto firstCode = u16(table);
    auto entryCnt = u16(table + 2);
    if (!validate(table, 4 + 2 * entryCnt)) return -1;

    if (codepoint < firstCode) return -1;
    codepoint -= firstCode;
    if (codepoint >= entryCnt) return -1;
    return u16(table + 4 + 2 * codepoint);
}

bool SfntReader::glyphMetrics(SfntGlyph& glyph)
{
    // glyph is inside long metrics segment.
    if (glyph.idx < metrics.numHmtx) {
        auto offset = hmtx + 4 * glyph.idx;
        if (!validate(offset, 4)) return false;
        glyph.advance = u16(offset);
        glyph.lsb = i16(offset + 2);
    // glyph is inside short metrics segment.
    } else {
        auto boundary = hmtx + 4U * (uint32_t)metrics.numHmtx;
        if (boundary < 4) return false;
        auto offset = boundary - 4;
        if (!validate(offset, 4)) return false;
        glyph.advance = u16(offset);
        offset = boundary + 2 * (glyph.idx - metrics.numHmtx);
        if (!validate(offset, 2)) return false;
        glyph.lsb = i16(offset);
    }
    return true;
}

uint32_t SfntReader::glyph(uint32_t codepoint)
{
    auto entryCnt = u16(cmap + 2);
    if (!validate(cmap, 4 + entryCnt * 8)) return -1;

    // full repertory (non-BMP map).
    for (auto idx = 0; idx < entryCnt; ++idx) {
        auto entry = cmap + 4 + idx * 8;
        auto type = u16(entry) * 0100 + u16(entry + 2);
        // unicode map
        if (type == 0004 || type == 0312) {
            auto table = cmap + u32(entry + 4);
            if (!validate(table, 8)) return -1;
            // dispatch based on cmap format.
            auto format = u16(table);
            switch (format) {
                case 12: return cmap_12_13(table, codepoint, 12);
                default: return -1;
            }
        }
    }
    // Try looking for a BMP map.
    for (auto idx = 0; idx < entryCnt; ++idx) {
        auto entry = cmap + 4 + idx * 8;
        auto type = u16(entry) * 0100 + u16(entry + 2);
        // Unicode BMP, including legacy Unicode 1.1 maps.
        if (type == 0001 || type == 0003 || type == 0301) {
            auto table = cmap + u32(entry + 4);
            if (!validate(table, 6)) return -1;
            // Dispatch based on cmap format.
            switch (u16(table)) {
                case 4: return cmap_4(table + 6, codepoint);
                case 6: return cmap_6(table + 6, codepoint);
                default: return -1;
            }
        }
    }
    return -1;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Result SfntReader::header(FontFace& info)
{
    if (!validate(0, 12)) return Result::InvalidArguments;

    // header
    auto head = table("head");
    if (!head || !validate(head, 54)) return Result::InvalidArguments;

    metrics.unitsPerEm = u16(head + 18);
    metrics.locaFormat = u16(head + 50);

    // horizontal header
    auto hhea = table("hhea");
    if (!hhea || !validate(hhea, 36)) return Result::InvalidArguments;

    metrics.hhea.ascent = i16(hhea + 4);
    metrics.hhea.descent = i16(hhea + 6);
    metrics.hhea.linegap = i16(hhea + 8);
    metrics.hhea.advance = metrics.hhea.ascent - metrics.hhea.descent + metrics.hhea.linegap;
    metrics.numHmtx = u16(hhea + 34);

    maxp = table("maxp");
    if (!maxp || !validate(maxp, 6)) return Result::InvalidArguments;

    // glyph outlines count
    auto glyphs = u16(maxp + 4);
    if (glyphs == 0) return Result::InvalidArguments;

    // horizontal metrics count
    auto hmtxs = u16(hhea + 34);
    if (hmtxs == 0 || hmtxs > glyphs) return Result::InvalidArguments;

    cmap = table("cmap");
    if (!cmap || !validate(cmap, 4)) return Result::InvalidArguments;

    // horizontal metrics
    hmtx = table("hmtx");
    if (!validate(hmtx, metrics.numHmtx * 4)) return Result::InvalidArguments;

    metadata(info);
    return Result::Success;
}

void SfntReader::metadata(FontFace& info) const
{
    uint32_t length;
    if (auto os2 = table("OS/2", &length)) {
        if (length >= 8) {
            auto weight = u16(os2 + 4);
            auto width = u16(os2 + 6);
            if (weight >= 1 && weight <= 1000) info.style.weight = static_cast<FontWeight>(weight);
            if (width >= 1 && width <= 9) info.style.width = static_cast<FontWidth>(width);
        }
        if (length >= 64) {
            auto flags = u16(os2 + 62);
            if (u16(os2) >= 4 && (flags & (1 << 9))) info.style.slant = FontSlant::Oblique;
            else info.style.slant = (flags & 1) ? FontSlant::Italic : FontSlant::Normal;
        }
    }

    auto names = table("name", &length);
    if (!names || length < 6) return;
    auto version = u16(names);
    auto count = u16(names + 2);
    auto storage = u16(names + 4);
    uint32_t recordsEnd = 6 + count * 12;
    if (version > 1 || recordsEnd > length || storage < recordsEnd || storage > length) return;

    uint16_t tags = 0;
    if (version == 1) {
        if (length - recordsEnd < 2) return;
        tags = u16(names + recordsEnd);
        if (2u + tags * 4u > length - recordsEnd || storage < recordsEnd + 2u + tags * 4u) return;
    }
    auto best = 0;
    for (uint32_t i = 0; i < count; ++i) {
        auto record = names + 6 + i * 12;
        auto platform = u16(record);
        auto encoding = u16(record + 2);
        auto language = u16(record + 4);
        auto id = u16(record + 6);
        if (id != 1 && id != 16) continue;
        auto bytes = u16(record + 8);
        uint32_t offset = u16(record + 10);
        if (platform != 0 && !(platform == 3 && (encoding == 0 || encoding == 1 || encoding == 10))) continue;
        if (offset > length - storage || bytes > length - storage - offset) continue;

        auto english = platform == 3 && (language & 0x3ff) == 9;
        if (language >= 0x8000) {
            auto index = language - 0x8000;
            if (version != 1 || index >= tags) continue;
            auto tag = names + recordsEnd + 2 + index * 4;
            auto tagLength = u16(tag);
            uint32_t tagOffset = u16(tag + 2);
            if (tagOffset > length - storage || tagLength > length - storage - tagOffset || (tagLength & 1)) continue;
            auto text = names + storage + tagOffset;
            english = tagLength >= 4 && u16(text) == 'e' && u16(text + 2) == 'n' && (tagLength == 4 || u16(text + 4) == '-');
        }
        auto rank = (id == 16 ? 32 : 0) + (english ? 8 : 0) + (platform == 0 ? 3 : 2);
        if (rank <= best) continue;
        auto text = _name(data + names + storage + offset, bytes);
        if (!text) continue;
        tvg::free(const_cast<char*>(info.family));
        info.family = text;
        best = rank;
    }
}
