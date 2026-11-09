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

#include "tvgTvgParser.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#define KEY_AS(val) !strcmp(key, val)

void TvgParser::trim(const char*& p, const char* end)
{
    while (p < end && (*p == ' ' || (*p >= '\t' && *p <= '\r')))
        ++p;
}

const char* TvgParser::next()
{
    auto p = this->cursor;
    auto end = this->end;

    idOffset = 0;
    valueOffset = 0;

    // Skip the current structure unless the caller entered it.
    if (pending) {
        uint32_t depth = 1;
        pending = false;
        while (p < end && depth) {
            if (*p == '{') ++depth;
            else if (*p == '}') --depth;
            ++p;
        }
        if (depth) {
            cursor = end;
            return nullptr;
        }
    }

    trim(p, end);
    cursor = p;
    if (p == end) return nullptr;
    if (*p == '}') {
        ++cursor;
        return nullptr;
    }

    auto begin = p;
    while (p < end && *p != ':' && *p != '{' && *p != '}' && *p != '"' && *p != ' ' && !(*p >= '\t' && *p <= '\r'))
        ++p;
    auto length = p - begin;
    const char* idBegin = nullptr;
    size_t idLength = 0;
    while (p < end && (*p == ' ' || *p == '\t'))
        ++p;
    if (length && (p == end || *p == '}' || *p == '\n' || *p == '\r')) {
        buffer.assign(begin, length);
        cursor = p;
        return buffer.c_str();
    }
    if (length && p < end && *p == ':') {
        auto valueBegin = ++p;
        while (p < end && *p != '}' && *p != '\n' && *p != '\r')
            ++p;
        buffer.reserve(length + 1 + (p - valueBegin));
        buffer.assign(begin, length);
        valueOffset = length + 1;
        buffer.append(1, '\0').append(valueBegin, p - valueBegin);
        cursor = p;
        return buffer.c_str();
    }
    if (p < end && *p == '"') {
        idBegin = ++p;
        while (p < end && *p != '"' && *p != '\n' && *p != '\r') {
            if (*p == '\\' && p + 1 < end && p[1] != '\n' && p[1] != '\r') ++p;
            ++p;
        }
        if (p == end || *p != '"') {
            cursor = end;
            return nullptr;
        }
        idLength = p - idBegin;
        ++p;
        while (p < end && (*p == ' ' || *p == '\t'))
            ++p;
    }
    if (!length || p == end || *p != '{') {
        cursor = end;
        return nullptr;
    }

    buffer.reserve(length + (idBegin ? idLength + 1 : 0));
    buffer.assign(begin, length);

    if (idBegin) {
        idOffset = length + 1;
        buffer.append(1, '\0').append(idBegin, idLength);
    }
    cursor = p + 1;
    pending = true;
    return buffer.c_str();
}

const char* TvgParser::getId()
{
    return idOffset ? buffer.c_str() + idOffset : nullptr;
}

bool TvgParser::enter()
{
    if (!pending) {
        malformed = true;
        return false;
    }
    pending = false;
    return true;
}

// read a float value
bool TvgParser::read(const char*& p, const char* end, float& out, bool separator)
{
    if (separator) {
        if (p == end || *p != ',') return false;
        ++p;
    }
    char* tail;
    out = strtof(p, &tail);
    if (tail == p) return false;
    p = tail;
    trim(p, end);
    return true;
}

void TvgParser::getFloat(float* out, uint32_t cnt)
{
    auto p = buffer.c_str() + valueOffset;
    auto end = buffer.c_str() + buffer.size();
    for (uint32_t i = 0; i < cnt; ++i) {
        if (!read(p, end, out[i], i != 0)) {
            TVGERR("TVG", "Invalid number of values: input(%u), expected(%u)", i, cnt);
            malformed = true;
            return;
        }
    }
}

void TvgParser::getProperty(TvgPaint* obj, TvgPoint& out)
{
    float value[2];

    if (valueOffset) {
        getFloat(value, 2);
        out.value = {value[0], value[1]};
        return;
    }

    if (!enter()) return;

    Array<TvgPoint::Keyframe> frames(4);

    while (auto key = next()) {
        char* tail;
        auto progress = strtof(key, &tail);
        if (tail == key || *tail) {
            malformed = true;
            return;
        }
        getFloat(value, 2);
        frames.push({progress, {value[0], value[1]}});
    }

    frames.compact();
    out.frames = frames.data;
    out.cnt = frames.count;
    frames.data = nullptr;

    obj->statical = false;
}

// read an arbitrary number of floats, up to the max
void TvgParser::getFloats(float* out, uint32_t max)
{
    float data[8];  // 8 is temporary
    uint32_t n = 0;

    auto p = buffer.c_str() + valueOffset;
    auto end = buffer.c_str() + buffer.size();
    while (n < max && n < 10) {
        if (!read(p, end, data[n], n != 0)) break;
        ++n;
        if (p == end) break;
    }
    if (n > 0) {
        memcpy(out, data, n * sizeof(float));
        return;
    }
    TVGERR("TVG", "Invalid number of values");
    malformed = true;
}

int TvgParser::getInteger()
{
    return valueOffset ? atoi(buffer.c_str() + valueOffset) : 0;
}

float TvgParser::getFloat()
{
    return valueOffset ? strtof(buffer.c_str() + valueOffset, nullptr) : 0.0f;
}

bool TvgParser::getBool()
{
    return getInteger() != 0;
}

void TvgParser::getFill(RGBA& out)
{
    auto p = buffer.c_str() + valueOffset;
    auto end = buffer.c_str() + buffer.size();
    trim(p, end);
    if (p < end && *p++ == '#') {
        uint32_t color = 0;
        uint32_t digits = 0;
        while (p < end && digits < 8) {
            auto c = static_cast<unsigned char>(*p);
            uint32_t digit;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if ((c | 0x20) >= 'a' && (c | 0x20) <= 'f') digit = (c | 0x20) - 'a' + 10;
            else break;
            color = (color << 4) | digit;
            ++digits;
            ++p;
        }
        trim(p, end);
        if (p == end) {
            if (digits == 6) out = {uint8_t(color >> 16), uint8_t(color >> 8), uint8_t(color), 255};
            else if (digits == 8) out = {uint8_t(color >> 24), uint8_t(color >> 16), uint8_t(color >> 8), uint8_t(color)};
            else malformed = true;
        }
    }
}

bool TvgParser::parsePaint(TvgPaint* paint, const char* key)
{
    if (KEY_AS("b")) paint->obj->blend((BlendMethod)getInteger());
    else if (KEY_AS("o")) paint->o = (uint8_t)getInteger();
    else if (KEY_AS("r")) paint->transform()->r = getFloat();
    else if (KEY_AS("s")) getFloats(paint->transform()->s, 2);
    else if (KEY_AS("t")) getProperty(paint, paint->transform()->t);
    else if (KEY_AS("a")) getProperty(paint, paint->transform()->a);
    else if (KEY_AS("k")) getProperty(paint, paint->transform()->k);
    else return false;
    return true;
}

TvgScene* TvgParser::parseScene(TvgScene* parent)
{
    if (!enter()) return nullptr;

    auto scene = new TvgScene(parent, getId());

    while (auto key = next()) {
        if (parsePaint(scene, key)) continue;
        if (KEY_AS("i")) getFloat(scene->i, 2);
        else if (KEY_AS("z")) getFloat(scene->z, 2);
        else if (KEY_AS("scene")) printf("scene\n");
        else if (KEY_AS("shape")) parseShape(scene);
        else if (KEY_AS("picture")) printf("picture\n");
        else if (KEY_AS("text")) printf("text\n");
        else if (KEY_AS("animation")) printf("animation\n");
        else if (KEY_AS("effect")) printf("effect\n");
    }
    return scene;
}

void TvgParser::parseRect(TvgShape* shape)
{
    if (!enter()) return;

    auto rect = new TvgShape::Rect;
    shape->list.push(rect);

    while (auto key = next()) {
        if (KEY_AS("p")) getProperty(shape, rect->p);
        else if (KEY_AS("z")) getProperty(shape, rect->z);
        else if (KEY_AS("r")) getProperty(shape, rect->r);
        else if (KEY_AS("d")) rect->d = (uint8_t)getInteger();
    }
}

void TvgParser::parseCircle(TvgShape* shape)
{
    if (!enter()) return;

    auto circle = new TvgShape::Circle;
    shape->list.push(circle);

    while (auto key = next()) {
        if (KEY_AS("c")) getProperty(shape, circle->c);
        else if (KEY_AS("r")) getProperty(shape, circle->r);
        else if (KEY_AS("d")) circle->d = (uint8_t)getInteger();
    }
}

void TvgParser::parsePath(TvgShape* shape)
{
    if (!enter()) return;

    auto custom = new TvgShape::Custom;
    shape->list.push(custom);

    float p[6];

    while (auto key = next()) {
        if (KEY_AS("m")) {
            getFloat(p, 2);
            custom->path.moveTo({p[0], p[1]});
        } else if (KEY_AS("l")) {
            getFloat(p, 2);
            custom->path.lineTo({p[0], p[1]});
        } else if (KEY_AS("c")) {
            getFloat(p, 6);
            custom->path.cubicTo({p[0], p[1]}, {p[2], p[3]}, {p[4], p[5]});
        } else if (KEY_AS("z")) {
            custom->path.close();
        } else if (KEY_AS("u")) {
            // TODO:
        }
    }

    custom->path.cmds.compact();
    custom->path.pts.compact();
}

TvgShape* TvgParser::parseShape(TvgScene* parent)
{
    if (!enter()) return nullptr;

    auto shape = new TvgShape(parent, getId());

    while (auto key = next()) {
        if (parsePaint(shape, key)) continue;
        if (KEY_AS("f")) getFill(shape->f);
        else if (KEY_AS("rect")) parseRect(shape);
        else if (KEY_AS("circle")) parseCircle(shape);
        else if (KEY_AS("path")) parsePath(shape);
        else if (KEY_AS("w")) shape->shape()->fillRule((FillRule)getInteger());
    }

    shape->list.compact();

    if (shape->statical) shape->build(0.0f);

    return shape;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

TvgParser::TvgParser(const char* content, uint32_t size) : content(content), end(content + size), size(size), cursor(content)
{
}

TvgScene* TvgParser::parse()
{
    // start from the root scene
    auto key = next();
    if (!key || !KEY_AS("scene")) return nullptr;

    main = parseScene(nullptr);
    if (malformed) {
        delete (main);
        return nullptr;
    }
    return main;
}

Result TvgParser::header(uint16_t& ver, float& w, float& h, float& dur)
{
    auto key = next();
    if (!key || !KEY_AS("tvg") || !enter()) return Result::InvalidArguments;

    // version
    key = next();
    if (!key || !KEY_AS("v")) return Result::InvalidArguments;
    ver = static_cast<uint16_t>(getInteger());

    // size
    key = next();
    if (!key || !KEY_AS("z")) return Result::InvalidArguments;

    float size[2];
    getFloat(size, 2);
    w = size[0];
    h = size[1];

    // duration
    key = next();
    if (!key || !KEY_AS("d")) return Result::InvalidArguments;
    dur = getFloat();

    if (malformed) return Result::InvalidArguments;

    return Result::Success;
}
