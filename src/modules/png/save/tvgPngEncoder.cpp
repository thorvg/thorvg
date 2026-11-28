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

#include <cstdio>
#include <cstring>
#include "tvgAllocator.h"
#include "tvgPngEncoder.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

namespace {

constexpr uint32_t WINDOW_SIZE = 32768;
constexpr uint32_t WINDOW_MASK = WINDOW_SIZE - 1;
constexpr uint32_t HASH_BITS = 15;
constexpr uint32_t MIN_MATCH = 3;
constexpr uint32_t MAX_MATCH = 258;
constexpr uint32_t MAX_STORED = 65535;
constexpr uint32_t IDAT_SIZE = 65536;

constexpr uint16_t LEN_BASE[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr uint8_t LEN_EXTRA[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr uint16_t DIST_BASE[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr uint8_t DIST_EXTRA[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};


/************************************************************************/
/* Checksums                                                            */
/************************************************************************/

struct Crc32
{
    uint32_t table[256];

    Crc32()
    {
        for (uint32_t i = 0; i < 256; ++i) {
            auto c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
    }

    uint32_t update(uint32_t crc, const uint8_t* data, size_t size) const
    {
        for (size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
        return crc;
    }
};


static uint32_t _adler32(const uint8_t* data, size_t size)
{
    uint32_t a = 1, b = 0;
    while (size > 0) {
        //5552 is the largest block that keeps the sums from overflowing before the modulo
        auto n = size < 5552 ? size : 5552;
        size -= n;
        while (n--) {
            a += *data++;
            b += a;
        }
        a %= 65521;
        b %= 65521;
    }
    return (b << 16) | a;
}


/************************************************************************/
/* Deflate                                                              */
/************************************************************************/

struct BitWriter
{
    uint8_t* buf;
    size_t pos = 0;
    uint64_t acc = 0;
    uint32_t cnt = 0;

    //The least significant bit goes first.
    void put(uint32_t bits, uint32_t len)
    {
        acc |= (uint64_t)bits << cnt;
        cnt += len;
        while (cnt >= 8) {
            buf[pos++] = acc & 0xff;
            acc >>= 8;
            cnt -= 8;
        }
    }

    //Huffman codes are packed starting from the most significant bit.
    void code(uint32_t code, uint32_t len)
    {
        uint32_t rev = 0;
        for (uint32_t i = 0; i < len; ++i) {
            rev = (rev << 1) | (code & 1);
            code >>= 1;
        }
        put(rev, len);
    }

    void flush()
    {
        if (cnt > 0) put(0, 8 - cnt);
    }
};


//The fixed Huffman code of the literal/length alphabet, see RFC 1951 3.2.6
static void _symbol(BitWriter& bw, uint32_t sym)
{
    if (sym < 144) bw.code(0x30 + sym, 8);
    else if (sym < 256) bw.code(0x190 + sym - 144, 9);
    else if (sym < 280) bw.code(sym - 256, 7);
    else bw.code(0xc0 + sym - 280, 8);
}


static void _match(BitWriter& bw, uint32_t len, uint32_t dist)
{
    int i = 28;
    while (LEN_BASE[i] > len) --i;
    _symbol(bw, 257 + i);
    if (LEN_EXTRA[i]) bw.put(len - LEN_BASE[i], LEN_EXTRA[i]);

    i = 29;
    while (DIST_BASE[i] > dist) --i;
    bw.code(i, 5);
    if (DIST_EXTRA[i]) bw.put(dist - DIST_BASE[i], DIST_EXTRA[i]);
}


static uint32_t _hash(const uint8_t* p)
{
    return ((uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16)) * 0x9e3779b1u) >> (32 - HASH_BITS);
}


//Compresses with a greedy LZ77 and the fixed Huffman codes as a single deflate block.
static bool _compress(BitWriter& bw, const uint8_t* src, size_t size, uint32_t quality)
{
    //head[]: the latest position (+1) of a hash, prev[]: the previous position (+1) with the same hash
    auto head = tvg::calloc<uint32_t>(1u << HASH_BITS, sizeof(uint32_t));
    auto prev = tvg::calloc<uint32_t>(WINDOW_SIZE, sizeof(uint32_t));
    if (!head || !prev) {
        tvg::free(head);
        tvg::free(prev);
        return false;
    }

    const uint32_t maxChain = (quality < 50) ? 8 : 64;

    auto insert = [&](size_t i) {
        if (i + MIN_MATCH > size) return;
        auto h = _hash(src + i);
        prev[i & WINDOW_MASK] = head[h];
        head[h] = uint32_t(i + 1);
    };

    bw.put(1, 1);  //the final block
    bw.put(1, 2);  //the fixed Huffman codes

    size_t i = 0;
    while (i < size) {
        uint32_t bestLen = 0, bestDist = 0;

        if (i + MIN_MATCH <= size) {
            auto limit = (size - i < MAX_MATCH) ? uint32_t(size - i) : MAX_MATCH;
            auto cand = head[_hash(src + i)];
            auto chain = maxChain;
            while (cand && chain--) {
                auto pos = size_t(cand - 1);
                auto dist = i - pos;
                if (dist > WINDOW_SIZE) break;
                uint32_t len = 0;
                while (len < limit && src[pos + len] == src[i + len]) ++len;
                if (len > bestLen) {
                    bestLen = len;
                    bestDist = uint32_t(dist);
                    if (len == limit) break;
                }
                cand = prev[pos & WINDOW_MASK];
            }
        }

        if (bestLen >= MIN_MATCH) {
            _match(bw, bestLen, bestDist);
            for (uint32_t k = 0; k < bestLen; ++k) insert(i + k);
            i += bestLen;
        } else {
            _symbol(bw, src[i]);
            insert(i);
            ++i;
        }
    }
    _symbol(bw, 256);  //the end of the block
    bw.flush();

    tvg::free(head);
    tvg::free(prev);
    return true;
}


//Stores the data without compression.
static void _store(BitWriter& bw, const uint8_t* src, size_t size)
{
    do {
        auto len = size < MAX_STORED ? uint32_t(size) : MAX_STORED;
        size -= len;
        bw.put(size == 0 ? 1 : 0, 1);
        bw.flush();
        bw.put(len, 16);
        bw.put(~len & 0xffff, 16);
        memcpy(bw.buf + bw.pos, src, len);
        bw.pos += len;
        src += len;
    } while (size > 0);
}


/************************************************************************/
/* PNG Filters                                                          */
/************************************************************************/

static uint8_t _paeth(uint8_t a, uint8_t b, uint8_t c)
{
    auto p = int(a) + int(b) - int(c);
    auto pa = abs(p - int(a));
    auto pb = abs(p - int(b));
    auto pc = abs(p - int(c));
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}


//Filters a scanline with the given filter type. The sum of the absolute values of the result is returned as a cost.
static uint32_t _filter(uint8_t type, const uint8_t* cur, const uint8_t* prev, uint8_t* out, size_t size, uint32_t bpp)
{
    uint32_t cost = 0;
    for (size_t i = 0; i < size; ++i) {
        auto a = (i >= bpp) ? cur[i - bpp] : 0;
        auto b = prev[i];
        auto c = (i >= bpp) ? prev[i - bpp] : 0;
        uint8_t pred = 0;
        switch (type) {
            case 1: pred = a; break;
            case 2: pred = b; break;
            case 3: pred = uint8_t((a + b) >> 1); break;
            case 4: pred = _paeth(a, b, c); break;
            default: break;
        }
        out[i] = uint8_t(cur[i] - pred);
        cost += (out[i] < 128) ? out[i] : 256 - out[i];
    }
    return cost;
}


/************************************************************************/
/* PNG Chunks                                                           */
/************************************************************************/

static void _be32(uint8_t* p, uint32_t v)
{
    p[0] = v >> 24;
    p[1] = (v >> 16) & 0xff;
    p[2] = (v >> 8) & 0xff;
    p[3] = v & 0xff;
}


static bool _chunk(FILE* f, const Crc32& crc, const char* type, const uint8_t* data, uint32_t size)
{
    uint8_t head[8];
    _be32(head, size);
    memcpy(head + 4, type, 4);

    auto c = crc.update(0xffffffffu, head + 4, 4);
    c = crc.update(c, data, size) ^ 0xffffffffu;

    uint8_t tail[4];
    _be32(tail, c);

    if (fwrite(head, 1, 8, f) != 8) return false;
    if (size > 0 && fwrite(data, 1, size, f) != size) return false;
    return fwrite(tail, 1, 4, f) == 4;
}

} //anonymous namespace


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool pngEncode(const char* filename, const uint32_t* pixels, uint32_t w, uint32_t h, uint32_t quality)
{
    if (!filename || !pixels || w == 0 || h == 0) return false;

    //The IHDR dimensions and the IDAT sizes are limited to 31bits.
    if ((uint64_t)w * h * 4 + h > 0x7fffffffu) return false;

    //Is there any transparency?
    auto opaque = true;
    for (size_t i = 0, n = (size_t)w * h; i < n; ++i) {
        if ((pixels[i] >> 24) != 0xff) {
            opaque = false;
            break;
        }
    }

    const uint32_t bpp = opaque ? 3 : 4;
    const size_t stride = (size_t)w * bpp;
    const size_t rawSize = (stride + 1) * h;

    //Scanline filtering
    auto raw = tvg::malloc<uint8_t>(rawSize);
    auto lines = tvg::calloc<uint8_t>(4, stride);  //[0]: the previous line, [1]: the current line, [2]: a trial, [3]: the best trial
    if (!raw || !lines) {
        tvg::free(raw);
        tvg::free(lines);
        return false;
    }

    auto prev = lines;
    auto cur = lines + stride;
    auto trial = cur + stride;
    auto best = trial + stride;

    for (uint32_t y = 0; y < h; ++y) {
        auto src = pixels + (size_t)y * w;
        for (uint32_t x = 0; x < w; ++x) {
            auto c = cur + (size_t)x * bpp;
            c[0] = src[x] & 0xff;
            c[1] = (src[x] >> 8) & 0xff;
            c[2] = (src[x] >> 16) & 0xff;
            if (bpp == 4) c[3] = src[x] >> 24;
        }

        auto dst = raw + y * (stride + 1);
        if (quality == 0) {
            dst[0] = 0;
            memcpy(dst + 1, cur, stride);
        } else {
            uint8_t type = 0;
            auto cost = _filter(0, cur, prev, best, stride, bpp);
            for (uint8_t t = 1; t <= 4; ++t) {
                auto c = _filter(t, cur, prev, trial, stride, bpp);
                if (c < cost) {
                    cost = c;
                    type = t;
                    auto tmp = best;
                    best = trial;
                    trial = tmp;
                }
            }
            dst[0] = type;
            memcpy(dst + 1, best, stride);
        }
        auto tmp = prev;
        prev = cur;
        cur = tmp;
    }
    tvg::free(lines);

    //Zlib stream: a header, a deflate stream, an adler32 checksum.
    //The upper bound of the deflate stream: a literal costs 9 bits at most while a match never costs more than the literals it replaces.
    auto cap = rawSize + (rawSize >> 3) + 5 * (rawSize / MAX_STORED + 1) + 32;
    auto zlib = tvg::malloc<uint8_t>(cap);
    if (!zlib) {
        tvg::free(raw);
        return false;
    }

    BitWriter bw;
    bw.buf = zlib;
    bw.put(0x78, 8);  //CMF: deflate, 32K window
    bw.put(0x01, 8);  //FLG: no dictionary, the fastest algorithm, the check bits
    auto ok = true;
    if (quality == 0) _store(bw, raw, rawSize);
    else ok = _compress(bw, raw, rawSize, quality);

    if (ok) {
        uint8_t adler[4];
        _be32(adler, _adler32(raw, rawSize));
        memcpy(bw.buf + bw.pos, adler, 4);
        bw.pos += 4;
    }
    tvg::free(raw);

    if (!ok) {
        tvg::free(zlib);
        return false;
    }

    //Write out the file
    FILE* f = nullptr;
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
    fopen_s(&f, filename, "wb");
#else
    f = fopen(filename, "wb");
#endif
    if (!f) {
        tvg::free(zlib);
        return false;
    }

    static const uint8_t SIGNATURE[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};

    uint8_t ihdr[13];
    _be32(ihdr, w);
    _be32(ihdr + 4, h);
    ihdr[8] = 8;                //bit depth
    ihdr[9] = opaque ? 2 : 6;   //color type: truecolor or truecolor with alpha
    ihdr[10] = ihdr[11] = ihdr[12] = 0;  //deflate, adaptive filtering, no interlace

    Crc32 crc;
    ok = fwrite(SIGNATURE, 1, 8, f) == 8;
    ok = ok && _chunk(f, crc, "IHDR", ihdr, 13);
    for (size_t pos = 0; ok && pos < bw.pos; pos += IDAT_SIZE) {
        auto len = bw.pos - pos;
        ok = _chunk(f, crc, "IDAT", zlib + pos, len < IDAT_SIZE ? uint32_t(len) : IDAT_SIZE);
    }
    ok = ok && _chunk(f, crc, "IEND", nullptr, 0);

    tvg::free(zlib);
    if (fclose(f) != 0) ok = false;
    return ok;
}
