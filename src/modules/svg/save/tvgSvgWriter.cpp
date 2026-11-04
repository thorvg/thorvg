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

#include <cmath>
#include <clocale>
#include <cstdio>
#include "tvgMath.h"
#include "tvgStr.h"
#include "tvgSvgWriter.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static void _delocalize(char* buf)
{
    auto point = localeconv()->decimal_point;
    if (!point || (point[0] == '.' && point[1] == '\0')) return;
    if (auto p = strstr(buf, point)) {
        *p = '.';
        auto len = strlen(point);
        if (len > 1) memmove(p + 1, p + len, strlen(p + len) + 1);
    }
}

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

static void _base64(string& out, const string& in)
{
    static constexpr const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto len = in.size();
    out.reserve(out.size() + (len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = uint8_t(in[i]) << 16;
        if (i + 1 < len) v |= uint8_t(in[i + 1]) << 8;
        if (i + 2 < len) v |= uint8_t(in[i + 2]);
        out += table[(v >> 18) & 63];
        out += table[(v >> 12) & 63];
        out += i + 1 < len ? table[(v >> 6) & 63] : '=';
        out += i + 2 < len ? table[v & 63] : '=';
    }
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void SvgWriter::number(string& out, float v)
{
    if (!std::isfinite(v) || v == 0.0f) {
        out += '0';
        return;
    }

    char buf[32];
    for (int precision = 6; precision <= 9; ++precision) {
        snprintf(buf, sizeof(buf), "%.*g", precision, double(v));
        _delocalize(buf);
        if (toFloat(buf, nullptr) == v) break;
    }
    out += buf;
}

void SvgWriter::attr(string& out, const char* name, float v)
{
    out += ' ';
    out += name;
    out += "=\"";
    number(out, v);
    out += '"';
}

void SvgWriter::attr(string& out, const char* name, const char* v)
{
    out += ' ';
    out += name;
    out += "=\"";
    out += v;
    out += '"';
}

void SvgWriter::color(string& out, const char* name, uint8_t r, uint8_t g, uint8_t b)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    attr(out, name, buf);
}

void SvgWriter::alpha(string& out, const char* name, uint8_t a)
{
    if (a < 255) attr(out, name, a / 255.0f);
}

void SvgWriter::matrix(string& out, const char* name, const Matrix& m)
{
    if (tvg::identity(&m)) return;
    out += ' ';
    out += name;
    out += "=\"matrix(";
    const float v[6] = {m.e11, m.e21, m.e12, m.e22, m.e13, m.e23};
    for (int i = 0; i < 6; ++i) {
        if (i > 0) out += ' ';
        number(out, v[i]);
    }
    out += ")\"";
}

void SvgWriter::url(string& out, const char* name, uint32_t id)
{
    out += ' ';
    out += name;
    out += "=\"url(#i" + to_string(id) + ")\"";
}

void SvgWriter::rect(string& out, const Point& min, const Point& max)
{
    attr(out, "x", min.x);
    attr(out, "y", min.y);
    attr(out, "width", max.x - min.x);
    attr(out, "height", max.y - min.y);
}

void SvgWriter::path(string& out, const PathCommand* cmds, uint32_t cmdCnt, const Point* pts)
{
    auto point = [&](const Point& pt) {
        number(out, pt.x);
        out += ' ';
        number(out, pt.y);
    };

    out += " d=\"";
    for (uint32_t i = 0; i < cmdCnt; ++i) {
        switch (cmds[i]) {
            case PathCommand::MoveTo: {
                out += 'M';
                point(*pts++);
                break;
            }
            case PathCommand::LineTo: {
                out += i == 0 ? 'M' : 'L';
                point(*pts++);
                break;
            }
            case PathCommand::CubicTo: {
                out += 'C';
                point(*pts++);
                out += ' ';
                point(*pts++);
                out += ' ';
                point(*pts++);
                break;
            }
            case PathCommand::Close: {
                out += 'Z';
                break;
            }
        }
    }
    out += '"';
}

void SvgWriter::png(string& out, const uint8_t* rgba, uint32_t w, uint32_t h)
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

    string png = "\x89PNG\r\n\x1a\n";
    _chunk(png, "IHDR", ihdr);
    _chunk(png, "IDAT", zlib);
    _chunk(png, "IEND", "");

    out += " href=\"data:image/png;base64,";
    _base64(out, png);
    out += '"';
}

bool SvgWriter::write(const char* filename, float w, float h) const
{
    auto f = fopen(filename, "wb");
    if (!f) return false;

    string head = "<svg xmlns=\"http://www.w3.org/2000/svg\"";
    attr(head, "width", w);
    attr(head, "height", h);
    head += " viewBox=\"0 0 ";
    number(head, w);
    head += ' ';
    number(head, h);
    head += "\">";
    if (!defs.empty()) head += "<defs>" + defs + "</defs>";

    auto ret = fwrite(head.data(), 1, head.size(), f) == head.size();
    ret &= fwrite(body.data(), 1, body.size(), f) == body.size();
    ret &= fputs("</svg>\n", f) >= 0;
    ret &= fclose(f) == 0;
    return ret;
}
