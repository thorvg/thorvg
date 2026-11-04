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

#include "tvgPngEncoder.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static void _u32(string& out, uint32_t v)
{
    char buf[4] = {char(v >> 24), char(v >> 16), char(v >> 8), char(v)};
    out.append(buf, 4);
}

static uint32_t _crc32(const char* data, size_t len, uint32_t crc)
{
    for (size_t i = 0; i < len; ++i) {
        crc ^= uint8_t(data[i]);
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xedb88320 & (0 - (crc & 1)));
    }
    return crc;
}

static void _chunk(string& png, const char* type, const string& data)
{
    _u32(png, data.size());
    png.append(type, 4);
    png += data;
    _u32(png, ~_crc32(data.data(), data.size(), _crc32(type, 4, 0xffffffff)));
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void PngEncoder::encode(string& out, const uint8_t* rgba, uint32_t w, uint32_t h)
{
    string raw;
    raw.reserve(size_t(h) * (w * 4 + 1));
    for (uint32_t y = 0; y < h; ++y) {
        raw += '\0';
        raw.append((const char*)(rgba + size_t(y) * w * 4), size_t(w) * 4);
    }

    string zlib = "\x78\x01";
    uint32_t a = 1, b = 0;
    for (auto c : raw) {
        a = (a + uint8_t(c)) % 65521;
        b = (b + a) % 65521;
    }
    size_t pos = 0;
    do {
        auto n = std::min(raw.size() - pos, size_t(65535));
        char header[5] = {char(pos + n == raw.size()), char(n), char(n >> 8), char(~n), char(~n >> 8)};
        zlib.append(header, 5);
        zlib.append(raw, pos, n);
        pos += n;
    } while (pos < raw.size());
    _u32(zlib, (b << 16) | a);

    string ihdr;
    _u32(ihdr, w);
    _u32(ihdr, h);
    ihdr.append("\x08\x06\x00\x00\x00", 5);

    out += "\x89PNG\r\n\x1a\n";
    _chunk(out, "IHDR", ihdr);
    _chunk(out, "IDAT", zlib);
    _chunk(out, "IEND", "");
}
