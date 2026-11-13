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

#include "tvgMath.h"
#include "tvgStr.h"
#include "tvgLock.h"
#include "tvgShape.h"
#include "tvgScene.h"
#include "tvgPicture.h"
#include "tvgText.h"
#include "tvgSvgSaver.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#define SIGMA_TO_DEVIATION 0.8f

static constexpr const char* capTags[] = {"butt", "round", "square"};
static constexpr const char* joinTags[] = {"miter", "round", "bevel"};
static constexpr const char* spreadTags[] = {"pad", "reflect", "repeat"};
static constexpr const char* blendTags[] = {"normal", "multiply", "screen", "overlay", "darken", "lighten", "color-dodge", "color-burn", "hard-light", "soft-light", "difference", "exclusion", "hue", "saturation", "color", "luminosity"};

static bool _strokeVisible(const RenderShape& rs, float width, bool clipper)
{
    if (!rs.stroke || !std::isfinite(width) || tvg::zero(width)) return false;
    if (!clipper && !rs.stroke->fill && rs.stroke->color.a == 0) return false;
    return !tvg::zero(rs.stroke->trim.begin - rs.stroke->trim.end);
}

static const RenderPath* _path(const RenderShape& rs, RenderPath& trimmed)
{
    const RenderPath* path = &rs.path;
    if (rs.trimpath()) {
        if (!rs.stroke->trim.trim(rs.path, trimmed)) return nullptr;
        path = &trimmed;
    }
    if (path->cmds.empty() || path->cmds.first() == PathCommand::CubicTo) return nullptr;
    return path;
}

static void _stroke(string& out, const RenderStroke& stroke, float width)
{
    SvgWriter::attr(out, "stroke-width", width);
    if (stroke.cap != StrokeCap::Butt) SvgWriter::attr(out, "stroke-linecap", capTags[int(stroke.cap)]);
    if (stroke.join != StrokeJoin::Miter) SvgWriter::attr(out, "stroke-linejoin", joinTags[int(stroke.join)]);
    else if (stroke.miterlimit != 4.0f) SvgWriter::attr(out, "stroke-miterlimit", stroke.miterlimit);
    if (stroke.dash.count > 0 && stroke.dash.length > 0.0f) {
        out += " stroke-dasharray=\"";
        for (uint32_t i = 0; i < stroke.dash.count; ++i) {
            if (i > 0) out += ' ';
            SvgWriter::number(out, stroke.dash.pattern[i]);
        }
        out += '"';
        if (stroke.dash.offset != 0.0f) SvgWriter::attr(out, "stroke-dashoffset", stroke.dash.offset);
    }
}

static string _blend(const Paint* paint)
{
    auto method = PAINT(paint)->blendMethod;
    if (method == BlendMethod::Normal) return {};
    if (method > BlendMethod::Luminosity) {
        TVGLOG("SVG_SAVER", "Blend method(%d) is not supported.", int(method));
        return {};
    }
    return string(" style=\"mix-blend-mode:") + blendTags[int(method)] + "\"";
}

static void _prepare(Paint* paint)
{
    switch (paint->type()) {
        case Type::Scene: {
            for (auto child : to<SceneImpl>(paint)->paints) _prepare(child);
            break;
        }
        case Type::Picture: {
            auto picture = to<PictureImpl>(paint);
            picture->impl.mark(picture->load());
            if (picture->vector) {
                picture->resize();
                _prepare(picture->vector);
            }
            break;
        }
        case Type::Text: {
            to<TextImpl>(paint)->load();
            break;
        }
        default: break;
    }

    const Paint* target = nullptr;
    if (paint->mask(&target) != MaskMethod::None) _prepare(const_cast<Paint*>(target));
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void SvgSaver::region(const Matrix& ctm, Point& min, Point& max)
{
    Matrix inv;
    tvg::inverse(&ctm, &inv);
    Point pt4[4] = {{0.0f, 0.0f}, {vsize[0], 0.0f}, {vsize[0], vsize[1]}, {0.0f, vsize[1]}};
    min = max = pt4[0] * inv;
    for (int i = 1; i < 4; ++i) {
        auto pt = pt4[i] * inv;
        min = tvg::min(min, pt);
        max = tvg::max(max, pt);
    }
}

uint32_t SvgSaver::writeGradient(const Fill* fill, float scale)
{
    string out;
    auto id = writer.id();

    if (fill->type() == Type::LinearGradient) {
        float x1, y1, x2, y2;
        static_cast<const LinearGradient*>(fill)->linear(&x1, &y1, &x2, &y2);
        out += "<linearGradient id=\"i" + to_string(id) + "\"";
        SvgWriter::attr(out, "x1", x1 * scale);
        SvgWriter::attr(out, "y1", y1 * scale);
        SvgWriter::attr(out, "x2", x2 * scale);
        SvgWriter::attr(out, "y2", y2 * scale);
    } else if (fill->type() == Type::RadialGradient) {
        float cx, cy, r, fx, fy, fr;
        static_cast<const RadialGradient*>(fill)->radial(&cx, &cy, &r, &fx, &fy, &fr);
        out += "<radialGradient id=\"i" + to_string(id) + "\"";
        SvgWriter::attr(out, "cx", cx * scale);
        SvgWriter::attr(out, "cy", cy * scale);
        SvgWriter::attr(out, "r", r * scale);
        SvgWriter::attr(out, "fx", fx * scale);
        SvgWriter::attr(out, "fy", fy * scale);
        SvgWriter::attr(out, "fr", fr * scale);
    } else {
        TVGLOG("SVG_SAVER", "Gradient type(%d) is not supported.", int(fill->type()));
        return 0;
    }
    out += " gradientUnits=\"userSpaceOnUse\"";
    if (fill->spread() != FillSpread::Pad) SvgWriter::attr(out, "spreadMethod", spreadTags[int(fill->spread())]);
    SvgWriter::matrix(out, "gradientTransform", fill->transform());
    out += '>';

    const Fill::ColorStop* stops;
    auto cnt = fill->colorStops(&stops);
    for (uint32_t i = 0; i < cnt; ++i) {
        out += "<stop";
        SvgWriter::attr(out, "offset", stops[i].offset);
        SvgWriter::color(out, "stop-color", stops[i].r, stops[i].g, stops[i].b);
        SvgWriter::alpha(out, "stop-opacity", stops[i].a);
        out += "/>";
    }
    out += fill->type() == Type::LinearGradient ? "</linearGradient>" : "</radialGradient>";

    writer.defs += out;
    return id;
}

uint32_t SvgSaver::writeEffects(const Array<RenderEffect*>& effects, const Matrix& ctm)
{
    string primitives;
    ARRAY_FOREACH(p, effects) {
        if ((*p)->type != SceneEffect::GaussianBlur) {
            TVGLOG("SVG_SAVER", "Scene effect(%d) is not supported.", int((*p)->type));
            continue;
        }
        auto blur = static_cast<RenderEffectGaussianBlur*>(*p);
        if (tvg::zero(blur->sigma)) continue;
        auto deviation = blur->sigma * SIGMA_TO_DEVIATION;
        primitives += "<feGaussianBlur stdDeviation=\"";
        SvgWriter::number(primitives, blur->direction == 2 ? 0.0f : deviation);
        if (blur->direction != 0) {
            primitives += ' ';
            SvgWriter::number(primitives, blur->direction == 1 ? 0.0f : deviation);
        }
        primitives += '"';
        SvgWriter::attr(primitives, "edgeMode", blur->border == 1 ? "wrap" : "duplicate");
        primitives += "/>";
    }
    if (primitives.empty()) return 0;

    Point min, max;
    region(ctm, min, max);

    auto id = writer.id();
    auto out = "<filter id=\"i" + to_string(id) + "\" filterUnits=\"userSpaceOnUse\" color-interpolation-filters=\"sRGB\"";
    SvgWriter::rect(out, min, max);
    out += '>' + primitives + "</filter>";

    writer.defs += out;
    return id;
}

string SvgSaver::writeClip(const Shape* clipper, const Matrix& ctm)
{
    auto simple = true;
    for (auto p = clipper; p; p = p->clip()) {
        auto& rs = to<ShapeImpl>(p)->rs;
        if (rs.rule == FillRule::EvenOdd || _strokeVisible(rs, rs.strokeWidth(), true)) {
            simple = false;
            break;
        }
    }

    auto& rs = to<ShapeImpl>(clipper)->rs;
    auto inner = clipper->clip() ? writeClip(clipper->clip(), ctm) : string();
    auto& m = PAINT(clipper)->transform();
    auto id = writer.id();

    RenderPath trimmed;
    auto path = _path(rs, trimmed);

    string out;
    if (simple) {
        out += "<clipPath id=\"i" + to_string(id) + "\" clipPathUnits=\"userSpaceOnUse\"" + inner + ">";
        if (path) {
            out += "<path";
            SvgWriter::matrix(out, "transform", m);
            SvgWriter::path(out, path->cmds.data, path->cmds.count, path->pts.data);
            out += "/>";
        }
        out += "</clipPath>";
        writer.defs += out;
        return " clip-path=\"url(#i" + to_string(id) + ")\"";
    }

    Point min, max;
    region(ctm, min, max);
    out += "<mask id=\"i" + to_string(id) + "\" mask-type=\"alpha\" maskUnits=\"userSpaceOnUse\"";
    SvgWriter::rect(out, min, max);
    out += '>';
    if (path) {
        if (!inner.empty()) out += "<g" + inner + ">";
        out += "<path";
        SvgWriter::matrix(out, "transform", m);
        SvgWriter::path(out, path->cmds.data, path->cmds.count, path->pts.data);
        if (_strokeVisible(rs, rs.strokeWidth(), true)) {
            out += " fill=\"none\" stroke=\"#fff\"";
            _stroke(out, *rs.stroke, rs.stroke->width);
        } else {
            out += " fill=\"#fff\"";
            if (rs.rule == FillRule::EvenOdd) out += " fill-rule=\"evenodd\"";
        }
        out += "/>";
        if (!inner.empty()) out += "</g>";
    }
    out += "</mask>";

    writer.defs += out;
    return " mask=\"url(#i" + to_string(id) + ")\"";
}

uint32_t SvgSaver::writeMask(const Paint* target, MaskMethod method, const Matrix& ctm, bool masking)
{
    Point min, max;
    region(ctm, min, max);

    string rect;
    SvgWriter::rect(rect, min, max);

    string content;
    switch (method) {
        case MaskMethod::Alpha: {
            writePaint(content, target, ctm, true, true, masking);
            break;
        }
        case MaskMethod::Luma: {
            writePaint(content, target, ctm, false, true, masking);
            break;
        }
        case MaskMethod::InvAlpha: {
            auto alpha = writeMask(target, MaskMethod::Alpha, ctm, masking);
            content = "<rect" + rect + " fill=\"#fff\"/><rect" + rect + " fill=\"#000\"";
            SvgWriter::url(content, "mask", alpha);
            content += "/>";
            break;
        }
        case MaskMethod::InvLuma: {
            content = "<rect" + rect + " fill=\"#000\"/>";
            writePaint(content, target, ctm, false, true, masking);
            content += "<rect" + rect + " fill=\"#fff\" style=\"mix-blend-mode:difference\"/>";
            break;
        }
        default: return 0;
    }

    auto id = writer.id();
    auto out = "<mask id=\"i" + to_string(id) + "\"";
    if (method == MaskMethod::Alpha) out += " mask-type=\"alpha\"";
    out += " maskUnits=\"userSpaceOnUse\"" + rect + ">" + content + "</mask>";

    writer.defs += out;
    return id;
}

void SvgSaver::writeCompound(string& out, const Paint* paint, const Paint* target, MaskMethod method, const Matrix& ctm)
{
    switch (method) {
        case MaskMethod::Add: {
            out += "<g>";
            writePaint(out, target, ctm, true);
            writePaint(out, paint, ctm, true, true, false);
            out += "</g>";
            return;
        }
        case MaskMethod::Subtract:
        case MaskMethod::Intersect: {
            out += "<g";
            SvgWriter::url(out, "mask", writeMask(target, method == MaskMethod::Subtract ? MaskMethod::InvAlpha : MaskMethod::Alpha, ctm));
            out += ">";
            writePaint(out, paint, ctm, true, true, false);
            out += "</g>";
            return;
        }
        default: break;
    }

    const char* blend = "exclusion";
    if (method == MaskMethod::Lighten) blend = "lighten";
    else if (method == MaskMethod::Darken) blend = "darken";

    Point min, max;
    region(ctm, min, max);
    string rect;
    SvgWriter::rect(rect, min, max);

    auto layer = [&](string& out, uint32_t alpha) {
        out += "<rect" + rect + " fill=\"#000\"/><rect" + rect + " fill=\"#fff\"";
        SvgWriter::url(out, "mask", alpha);
        out += "/>";
    };

    auto s = writeMask(paint, MaskMethod::Alpha, ctm, false);
    auto d = writeMask(target, MaskMethod::Alpha, ctm);
    auto id = writer.id();

    auto mask = "<mask id=\"i" + to_string(id) + "\" maskUnits=\"userSpaceOnUse\"" + rect + ">";
    layer(mask, s);
    mask += string("<g style=\"mix-blend-mode:") + blend + "\">";
    layer(mask, d);
    mask += "</g></mask>";
    writer.defs += mask;

    out += "<rect" + rect + " fill=\"#fff\"";
    SvgWriter::url(out, "mask", id);
    out += "/>";
}

void SvgSaver::writeShape(string& out, const Shape* shape, const string& extra, float gradScale, float strokeWidth)
{
    auto& rs = to<ShapeImpl>(shape)->rs;

    RenderPath trimmed;
    auto path = _path(rs, trimmed);
    if (!path) return;

    out += "<path";
    SvgWriter::matrix(out, "transform", PAINT(shape)->transform());
    SvgWriter::alpha(out, "opacity", shape->opacity());
    out += extra;
    SvgWriter::path(out, path->cmds.data, path->cmds.count, path->pts.data);

    uint32_t fill = rs.fill ? writeGradient(rs.fill, gradScale) : 0;
    if (fill) SvgWriter::url(out, "fill", fill);
    else if (rs.fill || rs.color.a == 0) out += " fill=\"none\"";
    else {
        SvgWriter::color(out, "fill", rs.color.r, rs.color.g, rs.color.b);
        SvgWriter::alpha(out, "fill-opacity", rs.color.a);
    }
    if (rs.rule == FillRule::EvenOdd) out += " fill-rule=\"evenodd\"";

    if (strokeWidth < 0.0f) strokeWidth = rs.strokeWidth();
    if (_strokeVisible(rs, strokeWidth, false)) {
        uint32_t stroke = rs.stroke->fill ? writeGradient(rs.stroke->fill, 1.0f) : 0;
        if (stroke) SvgWriter::url(out, "stroke", stroke);
        else if (rs.stroke->fill) out += " stroke=\"none\"";
        else {
            SvgWriter::color(out, "stroke", rs.stroke->color.r, rs.stroke->color.g, rs.stroke->color.b);
            SvgWriter::alpha(out, "stroke-opacity", rs.stroke->color.a);
        }
        _stroke(out, *rs.stroke, strokeWidth);
        if (rs.stroke->first) out += " paint-order=\"stroke\"";
    }
    out += "/>";
}

void SvgSaver::writeScene(string& out, const Scene* scene, const Matrix& ctm, const string& extra, uint32_t filter, bool alpha)
{
    auto& m = PAINT(scene)->transform();
    auto inner = ctm * m;

    out += "<g";
    SvgWriter::matrix(out, "transform", m);
    SvgWriter::alpha(out, "opacity", scene->opacity());
    out += extra + ">";

    if (filter) {
        out += "<g";
        SvgWriter::url(out, "filter", filter);
        out += ">";
    }

    for (auto child : scene->paints()) writePaint(out, child, inner, alpha);

    if (filter) out += "</g>";
    out += "</g>";
}

void SvgSaver::writePicture(string& out, const Picture* picture, const Matrix& ctm, const string& extra, bool alpha)
{
    auto p = to<PictureImpl>(picture);
    auto m = PAINT(picture)->transform();
    translateR(&m, {-p->origin.x * p->w, -p->origin.y * p->h});

    if (p->vector) {
        out += "<g";
        SvgWriter::matrix(out, "transform", m);
        SvgWriter::alpha(out, "opacity", picture->opacity());
        out += extra + ">";
        writePaint(out, p->vector, ctm * m, alpha, false);
        out += "</g>";
        return;
    }

    auto bitmap = p->bitmap;
    if (!bitmap || bitmap->channelSize != sizeof(uint32_t) || bitmap->w == 0 || bitmap->h == 0) {
        TVGLOG("SVG_SAVER", "Picture(%p) has no exportable data.", picture);
        return;
    }

    auto scale = std::min(p->w / p->loader->w, p->h / p->loader->h);
    scaleR(&m, {scale, scale});

    auto rgba = tvg::malloc<uint8_t>(sizeof(uint32_t) * bitmap->w * bitmap->h);
    {
        ScopedLock lock(bitmap->key);
        auto abgr = bitmap->cs == ColorSpace::ABGR8888 || bitmap->cs == ColorSpace::ABGR8888S || bitmap->cs == ColorSpace::XBGR8888;
        auto opaque = bitmap->opaque || bitmap->cs == ColorSpace::XBGR8888 || bitmap->cs == ColorSpace::XRGB8888;
        auto dst = rgba;
        for (uint32_t y = 0; y < bitmap->h; ++y) {
            auto src = bitmap->buf32 + y * bitmap->stride;
            for (uint32_t x = 0; x < bitmap->w; ++x, dst += 4) {
                auto c = src[x];
                dst[0] = abgr ? uint8_t(c) : uint8_t(c >> 16);
                dst[1] = uint8_t(c >> 8);
                dst[2] = abgr ? uint8_t(c >> 16) : uint8_t(c);
                dst[3] = opaque ? 255 : uint8_t(c >> 24);
                if (bitmap->premultiplied && dst[3] > 0 && dst[3] < 255) {
                    for (int i = 0; i < 3; ++i) dst[i] = uint8_t(std::min(dst[i] * 255 / dst[3], 255));
                }
            }
        }
    }

    auto translucent = picture->opacity() < 255;
    if (translucent) {
        out += "<g";
        SvgWriter::alpha(out, "opacity", picture->opacity());
        out += extra + ">";
    }
    out += "<image";
    SvgWriter::matrix(out, "transform", m);
    if (!translucent) out += extra;
    SvgWriter::attr(out, "width", float(bitmap->w));
    SvgWriter::attr(out, "height", float(bitmap->h));
    if (p->filter == FilterMethod::Nearest) out += " image-rendering=\"optimizeSpeed\"";
    SvgWriter::png(out, rgba, bitmap->w, bitmap->h);
    out += "/>";
    if (translucent) out += "</g>";

    tvg::free(rgba);
}

void SvgSaver::writeText(string& out, const Text* text, const string& extra)
{
    auto t = to<TextImpl>(text);
    auto shape = to<ShapeImpl>(t->shape);

    auto gradScale = (shape->rs.fill && shape->impl.marked(RenderUpdateFlag::Gradient)) ? t->fm.scale : 1.0f;
    auto strokeWidth = (t->outlineWidth > 0.0f && t->impl.marked(RenderUpdateFlag::Stroke)) ? t->outlineWidth * t->fm.scale : -1.0f;

    out += "<g";
    SvgWriter::matrix(out, "transform", PAINT(text)->transform());
    SvgWriter::alpha(out, "opacity", text->opacity());
    out += extra + ">";
    writeShape(out, t->shape, {}, gradScale, strokeWidth);
    out += "</g>";
}

void SvgSaver::writePaint(string& out, const Paint* paint, const Matrix& ctm, bool alpha, bool blending, bool masking)
{
    if (!paint->visible() || paint->opacity() == 0) return;

    Matrix inv;
    auto m = ctm * PAINT(paint)->transform();
    if (!tvg::inverse(&m, &inv)) return;

    const Paint* target = nullptr;
    auto method = masking ? paint->mask(&target) : MaskMethod::None;
    if (method >= MaskMethod::Add) {
        if (alpha && (paint->type() == Type::Shape || paint->type() == Type::Text)) writeCompound(out, paint, target, method, ctm);
        return;
    }

    uint32_t filter = 0;
    if (paint->type() == Type::Scene) {
        if (auto effects = to<SceneImpl>(paint)->effects) filter = writeEffects(*effects, m);
    }

    auto extra = blending ? _blend(paint) : string();
    auto wraps = 0;
    auto wrap = [&](const string& attr) {
        out += "<g" + extra + attr + ">";
        extra.clear();
        ++wraps;
    };

    if (paint->clip()) wrap(writeClip(paint->clip(), ctm));
    if (method != MaskMethod::None) {
        string attr;
        SvgWriter::url(attr, "mask", writeMask(target, method, ctm));
        wrap(attr);
    }

    switch (paint->type()) {
        case Type::Shape: writeShape(out, static_cast<const Shape*>(paint), extra); break;
        case Type::Scene: writeScene(out, static_cast<const Scene*>(paint), ctm, extra, filter, alpha); break;
        case Type::Picture: writePicture(out, static_cast<const Picture*>(paint), ctm, extra, alpha); break;
        case Type::Text: writeText(out, static_cast<const Text*>(paint), extra); break;
        default: break;
    }

    while (wraps--) out += "</g>";
}

void SvgSaver::run(TVG_UNUSED unsigned tid)
{
    if (!writer.write(path, vsize[0], vsize[1])) TVGERR("SVG_SAVER", "Failed to write %s", path);
}

SvgSaver::~SvgSaver()
{
    close();
}

bool SvgSaver::close()
{
    this->done();

    tvg::free(path);
    path = nullptr;

    writer.clear();

    return true;
}

bool SvgSaver::save(Paint* paint, Paint* bg, const char* filename, TVG_UNUSED uint32_t quality)
{
    close();

    if (!filename) return false;

    _prepare(paint);
    if (bg) _prepare(bg);

    auto x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    paint->bounds(&x, &y, &w, &h);

    //cut off the negative space
    vsize[0] = x + w;
    vsize[1] = y + h;

    if (vsize[0] < FLOAT_EPSILON || vsize[1] < FLOAT_EPSILON) {
        TVGLOG("SVG_SAVER", "Saving paint(%p) has zero view size.", paint);
        return false;
    }

    this->path = duplicate(filename);

    paint->ref();

    auto identity = tvg::identity();
    if (bg) writePaint(writer.body, bg, identity, false);

    auto pm = PAINT(paint)->ptransform();
    if (tvg::identity((const Matrix*)&pm)) {
        writePaint(writer.body, paint, identity, false);
    } else {
        writer.body += "<g";
        SvgWriter::matrix(writer.body, "transform", pm);
        writer.body += ">";
        writePaint(writer.body, paint, pm, false);
        writer.body += "</g>";
    }

    paint->unref();

    TaskScheduler::request(this);

    return true;
}

bool SvgSaver::save(Animation* animation, Paint* bg, const char* filename, uint32_t quality, TVG_UNUSED uint32_t fps)
{
    auto picture = animation->picture();
    if (!save(picture, bg, filename, quality)) return false;
    if (picture->refCnt() <= 1) delete(animation);
    return true;
}
