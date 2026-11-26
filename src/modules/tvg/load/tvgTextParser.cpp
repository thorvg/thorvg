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

#include "tvgTextParser.h"
#include "tvgCompressor.h"
#include "tvgStr.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#define KEY_AS(val) !strcmp(key, val)
#define ERROR(...)                                             \
    do {                                                       \
        TVGERR("TVG", "Parsing failed: malformed TVG asset."); \
        malformed = true;                                      \
        __VA_ARGS__;                                           \
    } while (0)

void TvgTextParser::trim(const char*& p, const char* end)
{
    while (p < end && (*p == ' ' || (*p >= '\t' && *p <= '\r')))
        ++p;
}

void TvgTextParser::enter()
{
    if (!pending) ERROR(return);
    pending = false;
}

const char* TvgTextParser::next()
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
    const char* extra = nullptr;
    size_t extraLength = 0;
    while (p < end && (*p == ' ' || *p == '\t'))
        ++p;

    auto bare = length && (p == end || *p == '}' || *p == '\n' || *p == '\r');
    if (length && p < end && *p == ':') {
        extra = ++p;
        while (p < end && *p != '}' && *p != '\n' && *p != '\r')
            ++p;
        extraLength = p - extra;
        valueOffset = length + 1;
    } else if (!bare) {
        if (p < end && *p == '"') {
            extra = ++p;
            while (p < end && *p != '"' && *p != '\n' && *p != '\r') {
                if (*p == '\\' && p + 1 < end && p[1] != '\n' && p[1] != '\r') ++p;
                ++p;
            }
            if (p == end || *p != '"') {
                cursor = end;
                return nullptr;
            }
            extraLength = p - extra;
            idOffset = length + 1;
            ++p;
            while (p < end && (*p == ' ' || *p == '\t'))
                ++p;
        }
        if (!length || p == end || *p != '{') {
            idOffset = 0;
            cursor = end;
            return nullptr;
        }
        ++p;
        pending = true;
    }

    buffer.reserve(length + (extra ? extraLength + 1 : 0));
    buffer.assign(begin, length);
    if (extra) buffer.append(1, '\0').append(extra, extraLength);
    cursor = p;
    return buffer.c_str();
}

// read a float value
bool TvgTextParser::read(const char*& p, const char* end, float& out, bool separator)
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

const char* TvgTextParser::getId()
{
    return idOffset ? buffer.c_str() + idOffset : nullptr;
}

bool TvgTextParser::getFloat(float* out, uint32_t cnt)
{
    auto p = buffer.c_str() + valueOffset;
    auto end = buffer.c_str() + buffer.size();
    for (uint32_t i = 0; i < cnt; ++i) {
        if (!read(p, end, out[i], i != 0)) ERROR(return false);
    }
    return true;
}

float TvgTextParser::getFloat()
{
    return valueOffset ? strtof(buffer.c_str() + valueOffset, nullptr) : 0.0f;
}

Point TvgTextParser::getPoint()
{
    float data[2];
    getFloat(data, 2);
    return {data[0], data[1]};
}

int TvgTextParser::getInteger()
{
    return valueOffset ? atoi(buffer.c_str() + valueOffset) : 0;
}

bool TvgTextParser::getBool()
{
    return getInteger() != 0;
}

RGBA TvgTextParser::getFill()
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
            if (digits == 6) return {uint8_t(color >> 16), uint8_t(color >> 8), uint8_t(color), 255};
            else if (digits == 8) return {uint8_t(color >> 24), uint8_t(color >> 16), uint8_t(color >> 8), uint8_t(color)};
            else ERROR();
        }
    }
    return {};
}

Interpolator* TvgTextParser::getInterpolator()
{
    constexpr auto whitespace = " \t\n\v\f\r";
    auto close = buffer.find_last_not_of(whitespace);
    if (close == string::npos || close <= valueOffset) return nullptr;

    auto named = buffer[close] == '"';
    if (!named && (close - valueOffset < 4 || buffer[close] != 'd' || memcmp(buffer.c_str() + close - 3, "hold", 4))) return nullptr;
    auto start = named ? close : close - 3;
    if (named) {
        do {
            start = buffer.rfind('"', start - 1);
            if (start == string::npos || start <= valueOffset) ERROR(return nullptr);
            auto escape = start;
            while (escape > valueOffset && buffer[escape - 1] == '\\') --escape;
            if ((start - escape) % 2 == 0) break;
        } while (true);
    }
    auto separator = buffer.find_last_not_of(whitespace, start - 1);
    if (separator == string::npos || separator < valueOffset || buffer[separator] != ',') {
        if (named) ERROR(return nullptr);
        return nullptr;
    }
    Interpolator* interp = &TvgResource::holdInterp;
    if (named) {
        buffer[close] = '\0';
        auto item = resource->interp.find(djb2Encode(buffer.c_str() + start + 1));
        if (!item || !item->val) ERROR(return nullptr);
        interp = item->val;
    }
    buffer.resize(separator);
    return interp;
}

template<typename T>
void TvgTextParser::getProperty(TvgPaint* obj, TvgProperty<T>& out)
{
    if (valueOffset) {
        getValue(out.value);
        return;
    }

    enter();

    auto& frames = getFrames(static_cast<T*>(nullptr));
    frames.clear();

    while (auto key = next()) {
        char* tail;
        auto progress = strtof(key, &tail);
        if (tail == key || *tail) ERROR(return);
        auto interp = getInterpolator();
        T value{};
        getValue(value);
        frames.push({progress, value, interp});
    }

    using Keyframe = typename TvgProperty<T>::Keyframe;
    out.cnt = frames.count;
    out.frames = tvg::malloc<Keyframe>(out.cnt * sizeof(Keyframe));
    memcpy(out.frames, frames.data, out.cnt * sizeof(Keyframe));
    obj->statical = false;
}

bool TvgTextParser::parseTransform(TvgPaint* paint, TvgTransform*& t, const char* key)
{
    auto transform = [&]() {
        if (!t) t = new TvgTransform;
        return t;
    };
    if (KEY_AS("r")) getProperty(paint, transform()->r);
    else if (KEY_AS("s")) getProperty(paint, transform()->s);
    else if (KEY_AS("t")) getProperty(paint, transform()->t);
    else if (KEY_AS("a")) getProperty(paint, transform()->a);
    else if (KEY_AS("k")) getProperty(paint, transform()->k);
    else return false;

    return true;
}

bool TvgTextParser::parsePaint(TvgPaint* paint, const char* key)
{
    if (KEY_AS("b")) paint->obj->blend((BlendMethod)getInteger());
    else if (KEY_AS("o")) getProperty(paint, paint->o);
    else return parseTransform(paint, paint->t, key);
    return true;
}

void TvgTextParser::parseBlur(TvgScene* scene)
{
    enter();

    auto blur = new TvgScene::Blur;
    scene->effect->list.push(blur);

    while (auto key = next()) {
        if (KEY_AS("s")) getProperty(scene, blur->s);
    }
}

void TvgTextParser::parseShadow(TvgScene* scene)
{
    enter();

    auto shadow = new TvgScene::Shadow;
    scene->effect->list.push(shadow);

    while (auto key = next()) {
        if (KEY_AS("c")) getProperty(scene, shadow->c);
        else if (KEY_AS("a")) getProperty(scene, shadow->a);
        else if (KEY_AS("d")) getProperty(scene, shadow->d);
        else if (KEY_AS("s")) getProperty(scene, shadow->s);
    }
}

void TvgTextParser::parseMotion(TvgScene* scene)
{
    enter();

    auto motion = new TvgScene::Motion;
    scene->effect->list.push(motion);

    while (auto key = next()) {
        if (KEY_AS("d")) getProperty(scene, motion->d);
        else if (KEY_AS("a")) getProperty(scene, motion->a);
    }
}

void TvgTextParser::parseFill(TvgScene* scene)
{
    enter();

    auto fill = new TvgScene::Fill;
    scene->effect->list.push(fill);

    while (auto key = next()) {
        if (KEY_AS("c")) getProperty(scene, fill->c);
    }
}

void TvgTextParser::parseTritone(TvgScene* scene)
{
    enter();

    auto tritone = new TvgScene::Tritone;
    scene->effect->list.push(tritone);

    while (auto key = next()) {
        if (KEY_AS("s")) getProperty(scene, tritone->s);
        else if (KEY_AS("m")) getProperty(scene, tritone->m);
        else if (KEY_AS("h")) getProperty(scene, tritone->h);
        else if (KEY_AS("b")) getProperty(scene, tritone->b);
    }
}

void TvgTextParser::parseTint(TvgScene* scene)
{
    enter();

    auto tint = new TvgScene::Tint;
    scene->effect->list.push(tint);

    while (auto key = next()) {
        if (KEY_AS("b")) getProperty(scene, tint->b);
        else if (KEY_AS("w")) getProperty(scene, tint->w);
        else if (KEY_AS("i")) getProperty(scene, tint->i);
    }
}

void TvgTextParser::parseEffect(TvgScene* scene)
{
    enter();

    if (!scene->effect) scene->effect = new TvgScene::Effects;

    while (auto key = next()) {
        if (KEY_AS("blur")) parseBlur(scene);
        else if (KEY_AS("shadow")) parseShadow(scene);
        else if (KEY_AS("motion")) parseMotion(scene);
        else if (KEY_AS("fill")) parseFill(scene);
        else if (KEY_AS("tint")) parseTint(scene);
        else if (KEY_AS("tritone")) parseTritone(scene);
    }
}

void TvgTextParser::parseClip(TvgScene* scene)
{
    enter();

    scene->clip = new TvgScene::Clip;
    scene->scene()->clip(scene->clip->obj);

    while (auto key = next()) {
        parsePath(scene, scene->clip->list, key);
    }
}

TvgScene* TvgTextParser::parseScene(TvgScene* parent)
{
    enter();

    auto scene = new TvgScene(parent, getId());
    float z[2] = {};  // viewport

    while (auto key = next()) {
        if (parsePaint(scene, key)) continue;
        if (KEY_AS("i")) scene->interval = getFloat(scene->i, 2);
        else if (KEY_AS("z")) getFloat(z, 2);
        else if (KEY_AS("clip")) parseClip(scene);
        else if (KEY_AS("scene")) parseScene(scene);
        else if (KEY_AS("shape")) parseShape(scene);
        else if (KEY_AS("picture")) printf("picture\n");
        else if (KEY_AS("text")) printf("text\n");
        else if (KEY_AS("animation")) printf("animation\n");
        else if (KEY_AS("effect")) parseEffect(scene);
    }

    scene->prepare(parent, z);

    return scene;
}

void TvgTextParser::parseRect(TvgPaint* paint, Array<TvgPath*>& list)
{
    enter();

    auto rect = new TvgRect;
    list.push(rect);

    while (auto key = next()) {
        if (KEY_AS("p")) getProperty(paint, rect->p);
        else if (KEY_AS("z")) getProperty(paint, rect->z);
        else if (KEY_AS("r")) getProperty(paint, rect->r);
        else if (KEY_AS("d")) rect->d = getBool();
    }
}

void TvgTextParser::parseCircle(TvgPaint* paint, Array<TvgPath*>& list)
{
    enter();

    auto circle = new TvgCircle;
    list.push(circle);

    while (auto key = next()) {
        if (KEY_AS("c")) getProperty(paint, circle->c);
        else if (KEY_AS("r")) getProperty(paint, circle->r);
        else if (KEY_AS("d")) circle->d = getBool();
    }
}

void TvgTextParser::parsePath(TvgPaint* paint, Array<TvgPath*>& list)
{
    enter();

    auto path = new TvgCmdPath;
    list.push(path);

    auto& buffer = RenderPath::scratch();

    while (auto key = next()) {
        auto p = this->buffer.c_str() + valueOffset;
        auto end = this->buffer.c_str() + this->buffer.size();
        trim(p, end);
        if (KEY_AS("cmd")) {
            while (p < end) {
                switch (*p++) {
                    case 'm': buffer.cmds.push(PathCommand::MoveTo); break;
                    case 'l': buffer.cmds.push(PathCommand::LineTo); break;
                    case 'c': buffer.cmds.push(PathCommand::CubicTo); break;
                    case 'z': buffer.cmds.push(PathCommand::Close); break;
                    default: ERROR(return);
                }
                trim(p, end);
                if (p == end) break;
                if (*p++ != ',') ERROR(return);
                trim(p, end);
                if (p == end) ERROR(return);
            }
            auto& cmd = path->cmd;
            cmd.cnt = buffer.cmds.count;
            cmd.cmds = tvg::malloc<PathCommand>(cmd.cnt * sizeof(PathCommand));
            memcpy(cmd.cmds, buffer.cmds.data, cmd.cnt * sizeof(PathCommand));
        } else if (KEY_AS("pts")) {
            auto& pts = path->pts;
            auto readPts = [&]() -> Point* {
                buffer.pts.clear();
                while (p < end) {
                    auto separator = !buffer.pts.empty();
                    auto& pt = buffer.pts.next();
                    if (!read(p, end, pt.x, separator) || !read(p, end, pt.y, true)) ERROR(return nullptr);
                }
                if (pts.cnt == 0) pts.cnt = buffer.pts.count;
                else if (pts.cnt != buffer.pts.count) ERROR(return nullptr);
                auto data = tvg::malloc<Point>(pts.cnt * sizeof(Point));
                memcpy(data, buffer.pts.data, pts.cnt * sizeof(Point));
                return data;
            };
            if (!valueOffset) {
                enter();
                pathFrame.clear();
                while (auto key = next()) {
                    char* tail;
                    auto progress = strtof(key, &tail);
                    if (tail == key || *tail || !valueOffset) {
                        for (auto& frame : pathFrame) tvg::free(frame.value);
                        pathFrame.clear();
                        ERROR(return);
                    }
                    auto interp = getInterpolator();
                    p = this->buffer.c_str() + valueOffset;
                    end = this->buffer.c_str() + this->buffer.size();
                    trim(p, end);
                    pathFrame.push({progress, readPts(), interp});
                }
                pts.pts.cnt = pathFrame.count;
                pts.pts.frames = tvg::malloc<TvgPoints::Keyframe>(pathFrame.count * sizeof(TvgPoints::Keyframe));
                memcpy(pts.pts.frames, pathFrame.data, pathFrame.count * sizeof(TvgPoints::Keyframe));
                paint->statical = false;
            } else {
                pts.pts.value = readPts();
            }
        } else ERROR(return);
    }
}

void TvgTextParser::parseStroke(TvgShape* shape)
{
    enter();
    while (auto key = next()) {
        if (KEY_AS("w")) getProperty(shape, shape->stroke()->w);
        else if (KEY_AS("f")) getProperty(shape, shape->stroke()->f);
        else if (KEY_AS("c")) shape->shape()->strokeCap((StrokeCap)getInteger());
        else if (KEY_AS("j")) shape->shape()->strokeJoin((StrokeJoin)getInteger());
        else if (KEY_AS("m")) shape->shape()->strokeMiterlimit(getFloat());
        else if (KEY_AS("o")) shape->shape()->order(getBool());
        else if (KEY_AS("linear")) parseLinear(shape, shape->stroke()->g);
        else if (KEY_AS("radial")) parseRadial(shape, shape->stroke()->g);
        else if (KEY_AS("conic")) parseConic(shape, shape->stroke()->g);
    }
}

void TvgTextParser::parseTrim(TvgShape* shape)
{
    enter();
    while (auto key = next()) {
        if (KEY_AS("b")) getProperty(shape, shape->trim()->b);
        else if (KEY_AS("e")) getProperty(shape, shape->trim()->e);
        else if (KEY_AS("m")) shape->trim()->m = getBool();
    }
}

void TvgTextParser::getValue(ColorStops& value)
{
    auto p = buffer.c_str() + valueOffset;
    auto end = buffer.c_str() + buffer.size();
    trim(p, end);
    while (p < end) {
        auto separator = !stops.empty();
        auto& stop = stops.next();
        if (!read(p, end, stop.offset, separator)) ERROR(return);
        for (auto channel : {&stop.r, &stop.g, &stop.b, &stop.a}) {
            if (p == end || *p != ',') ERROR(return);
            char* tail;
            auto data = strtoul(++p, &tail, 10);
            if (tail == p) ERROR(return);
            *channel = static_cast<uint8_t>(data);
            p = tail;
            trim(p, end);
        }
    }
    value.stops = stops.data;
    value.cnt = stops.count;
    stops.data = nullptr;
    stops.reset();
}

bool TvgTextParser::parseGradient(TvgShape* shape, TvgGradient* g, const char* key)
{
    if (KEY_AS("c")) getProperty(shape, g->c);
    else if (KEY_AS("m")) g->fill->spread((FillSpread)getInteger());
    else return parseTransform(shape, g->t, key);
    return true;
}

void TvgTextParser::parseLinear(TvgShape* shape, TvgGradient*& gradient)
{
    if (gradient) ERROR(return);

    enter();

    auto linear = new TvgLinear;
    gradient = linear;

    while (auto key = next()) {
        if (KEY_AS("p")) getProperty(shape, linear->p);
        else parseGradient(shape, linear, key);
    }
}

void TvgTextParser::parseRadial(TvgShape* shape, TvgGradient*& gradient)
{
    if (gradient) ERROR(return);

    enter();

    auto radial = new TvgRadial;
    gradient = radial;

    while (auto key = next()) {
        if (KEY_AS("p")) getProperty(shape, radial->p);
        else if (KEY_AS("f")) getProperty(shape, radial->f);
        else parseGradient(shape, radial, key);
    }
}

void TvgTextParser::parseConic(TvgShape* shape, TvgGradient*& gradient)
{
    if (gradient) ERROR(return);

    enter();

    auto conic = new TvgConic;
    gradient = conic;

    while (auto key = next()) {
        if (KEY_AS("p")) getProperty(shape, conic->p);
        else if (KEY_AS("d")) getProperty(shape, conic->d);
        else parseGradient(shape, conic, key);
    }
}

bool TvgTextParser::parsePath(TvgPaint* paint, Array<TvgPath*>& list, const char* key)
{
    if (KEY_AS("rect")) parseRect(paint, list);
    else if (KEY_AS("circle")) parseCircle(paint, list);
    else if (KEY_AS("path")) parsePath(paint, list);
    else return false;
    return true;
}

void TvgTextParser::parseShape(TvgScene* parent)
{
    enter();

    auto shape = new TvgShape(parent, getId());

    while (auto key = next()) {
        if (parsePaint(shape, key)) continue;
        if (parsePath(shape, shape->list, key)) continue;
        if (KEY_AS("f")) getProperty(shape, shape->f);
        else if (KEY_AS("stroke")) parseStroke(shape);
        else if (KEY_AS("trim")) parseTrim(shape);
        else if (KEY_AS("w")) shape->shape()->fillRule((FillRule)getInteger());
        else if (KEY_AS("linear")) parseLinear(shape, shape->g);
        else if (KEY_AS("radial")) parseRadial(shape, shape->g);
        else if (KEY_AS("conic")) parseConic(shape, shape->g);
    }

    shape->prepare(parent);
}

void TvgTextParser::parseInterp()
{
    auto id = getId();
    if (!id) ERROR(return);

    auto& interp = resource->interp[djb2Encode(id)];
    if (interp) ERROR(return);

    enter();
    while (auto key = next()) {
        if (!KEY_AS("p")) ERROR(return);
        float p[4];
        if (!getFloat(p, 4)) return;
        delete (interp);
        interp = new BezierInterpolator({p[0], p[1]}, {p[2], p[3]});
    }
}

void TvgTextParser::parseStyle()
{
    enter();
    while (auto key = next()) {
        if (KEY_AS("interp")) parseInterp();
    }
}

bool TvgTextParser::next(const char* expected, bool optional)
{
    auto pos = cursor;
    auto wasPending = pending;
    auto key = next();
    if (key && KEY_AS(expected)) return true;

    if (optional) {
        cursor = pos;
        pending = wasPending;
    }
    return false;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

TvgTextParser::TvgTextParser(const char* content, uint32_t size) : resource(new TvgResource), cursor(content), end(content + size)
{
    pointFrame.reserve(8);
    floatFrame.reserve(8);
    opacityFrame.reserve(8);
    colorFrame.reserve(8);
    pathFrame.reserve(8);
}

TvgScene* TvgTextParser::parse(TvgResource*& out)
{
    // start from the root scene
    if (!next("scene")) return nullptr;

    auto main = parseScene(nullptr);
    if (malformed) {
        delete (main);
        return nullptr;
    }

    out = resource;
    resource = nullptr;

    return main;
}

Result TvgTextParser::header(uint16_t& ver, float& w, float& h, float& dur)
{
    if (!next("tvg")) return Result::InvalidArguments;

    enter();

    // version
    if (!next("v")) return Result::InvalidArguments;
    ver = static_cast<uint16_t>(getInteger());

    // size
    if (!next("z")) return Result::InvalidArguments;

    float size[2];
    getFloat(size, 2);
    w = size[0];
    h = size[1];

    // duration
    if (next("d", true)) dur = getFloat();

    if (next("style", true)) parseStyle();

    return malformed ? Result::InvalidArguments : Result::Success;
}
