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

#include "tvgCompressor.h"
#include "tvgModel.h"
#include "tvgShape.h"

/************************************************************************/
/* TvgProperty                                                          */
/************************************************************************/

void TvgPoints::operator()(float progress, Array<Point>& out, uint32_t ptsCnt) const
{
    out.grow(ptsCnt);
    auto dst = out.end();

    if (cnt == 0) memcpy(dst, value, sizeof(Point) * ptsCnt);
    else if (progress <= frames[0].progress) memcpy(dst, frames[0].value, sizeof(Point) * ptsCnt);
    else if (progress >= frames[cnt - 1].progress) memcpy(dst, frames[cnt - 1].value, sizeof(Point) * ptsCnt);
    else {
        auto idx = tvg::bsearch(frames, cnt, sizeof(Keyframe), progress);
        auto& a = frames[idx];
        auto& b = frames[idx + 1];
        auto t = a.map(b, progress);
        for (uint32_t i = 0; i < ptsCnt; ++i) {
            dst[i] = tvg::lerp(a.value[i], b.value[i], t);
        }
    }
    out.count += ptsCnt;
}

void TvgColorStop::operator()(float progress, Fill* out) const
{
    if (cnt == 0) out->colorStops(value.stops, value.cnt);
    else if (progress <= frames[0].progress) out->colorStops(frames[0].value.stops, frames[0].value.cnt);
    else if (progress >= frames[cnt - 1].progress) out->colorStops(frames[cnt - 1].value.stops, frames[cnt - 1].value.cnt);
    else {
        Array<Fill::ColorStop> dst;
        auto idx = tvg::bsearch(frames, cnt, sizeof(Keyframe), progress);
        auto& a = frames[idx];
        auto& b = frames[idx + 1];
        dst.reserve(a.value.cnt);
        if (a.value.cnt == b.value.cnt) {
            auto t = a.map(b, progress);
            for (uint32_t i = 0; i < a.value.cnt; ++i) {
                dst.push(tvg::lerp(a.value.stops[i], b.value.stops[i], t));
            }
            out->colorStops(dst.data, a.value.cnt);
        } else {
            out->colorStops(a.value.stops, a.value.cnt);
        }
    }
}

/************************************************************************/
/* TvgTransform                                                         */
/************************************************************************/

void TvgTransform::dispatch(float progress, Matrix& out) const
{
    // T(translate) * R(rotation) * K(skew) * S(scale) * T(-anchor)
    auto r = deg2rad(this->r(progress));
    auto cv = cosf(r);
    auto sv = sinf(r);

    auto k = this->k(progress);
    auto kx = tanf(deg2rad(k.x));
    auto ky = tanf(deg2rad(k.y));

    auto s = this->s(progress);

    out = {(cv - sv * ky) * s, (cv * kx - sv) * s, 0.0f,
           (sv + cv * ky) * s, (sv * kx + cv) * s, 0.0f,
           0.0f, 0.0f, 1.0f};

    auto t = this->t(progress);
    auto a = this->a(progress);

    out.e13 = t.x - out.e11 * a.x - out.e12 * a.y;
    out.e23 = t.y - out.e21 * a.x - out.e22 * a.y;
}

/************************************************************************/
/* TvgPaint                                                             */
/************************************************************************/

TvgPaint::TvgPaint(TvgPaint* parent, Paint* obj, const char* id) : obj(obj)
{
    if (parent) static_cast<TvgScene*>(parent)->scene()->add(obj);
    obj->ref();
    obj->id = djb2Encode(id);
    obj->data = this;
}

TvgPaint::~TvgPaint()
{
    delete (t);
    obj->unref();
}

void TvgPaint::prepare(TvgPaint* parent)
{
    if (parent && !statical) parent->statical = false;
}

void TvgPaint::build(float progress)
{
    obj->opacity(o(progress));

    if (t) {
        t->dispatch(progress, obj->transform());
        obj->transform(obj->transform());
    }
}

/************************************************************************/
/* TvgScene                                                             */
/************************************************************************/

void TvgScene::Blur::dispatch(float progress, Scene* out)
{
    out->add(SceneEffect::GaussianBlur, (double)s(progress), 0, 0, 60);
}

void TvgScene::Shadow::dispatch(float progress, Scene* out)
{
    auto color = c(progress);
    out->add(SceneEffect::DropShadow, color.r, color.g, color.b, color.a,
             (double)a(progress), (double)d(progress), (double)s(progress), 60);
}

void TvgScene::Motion::dispatch(float progress, Scene* out)
{
    out->add(SceneEffect::MotionBlur, (double)d(progress), (double)a(progress), 60);
}

void TvgScene::Fill::dispatch(float progress, Scene* out)
{
    auto color = c(progress);
    out->add(SceneEffect::Fill, color.r, color.g, color.b, color.a);
}

void TvgScene::Tint::dispatch(float progress, Scene* out)
{
    auto black = b(progress);
    auto white = w(progress);
    out->add(SceneEffect::Tint, black.r, black.g, black.b, white.r, white.g, white.b, (double)i(progress));
}

void TvgScene::Tritone::dispatch(float progress, Scene* out)
{
    auto shadow = s(progress);
    auto midtone = m(progress);
    auto highlight = h(progress);
    out->add(SceneEffect::Tritone, shadow.r, shadow.g, shadow.b, midtone.r, midtone.g, midtone.b, highlight.r, highlight.g, highlight.b, b(progress));
}

TvgScene::~TvgScene()
{
    delete (clip);
    delete (effect);

    for (auto c : scene()->paints()) {
        delete ((TvgPaint*)(c->data));
    }
}

void TvgScene::prepare(TvgPaint* parent, const float z[])
{
    TvgPaint::prepare(parent);

    // viewport
    if (z[0] > 0.0f && z[1] > 0.0f) {
        if (clip) {
            auto rect = new TvgRect;
            rect->z.value = {z[0], z[1]};
            clip->list.push(rect);
        } else {
            auto viewport = tvg::Shape::gen();
            viewport->appendRect(0.0f, 0.0f, z[0], z[1]);
            obj->clip(viewport);
        }
    }

    if (statical) buildCore(0.0, scene());
}

float TvgScene::buildCore(float progress, Scene* obj)
{
    // inherit the parent interval if current scene doesn't have
    auto remap = interval ? map(progress) : static_cast<TvgScene*>(obj->parent()->data)->map(progress);

    TvgPaint::build(remap);

    if (clip) {
        clip->obj->reset();
        for (auto path : clip->list) {
            path->dispatch(remap, clip->obj);
        }
    }

    if (effect) {
        obj->add(SceneEffect::Clear);
        for (auto e : effect->list) {
            e->dispatch(remap, obj);
        }
    }

    return remap;
}

void TvgScene::build(float progress)
{
    auto obj = scene();

    // out of frame
    if (interval && (progress < i[0] || progress >= i[1])) {
        obj->visible(false);
        return;
    }

    if (statical) return;

    obj->visible(true);

    auto remap = buildCore(progress, obj);

    // children
    for (auto c : obj->paints()) {
        switch (c->type()) {
            case Type::Scene: static_cast<TvgScene*>(c->data)->build(progress); break;
            case Type::Shape: static_cast<TvgShape*>(c->data)->build(remap); break;
            case Type::Picture: break;
            case Type::Text: break;
            default: break;
        }
    }
}

/************************************************************************/
/* TvgGradient                                                          */
/************************************************************************/

TvgGradient::~TvgGradient()
{
    if (c.cnt == 0) {
        tvg::free(c.value.stops);
    } else {
        for (uint32_t i = 0; i < c.cnt; ++i) {
            tvg::free(c.frames[i].value.stops);
        }
    }
    delete t;
}

Fill* TvgLinear::dispatch(float progress)
{
    c(progress, fill);
    auto p = this->p(progress);
    static_cast<LinearGradient*>(fill)->linear(p.v[0], p.v[1], p.v[2], p.v[3]);
    if (t) t->dispatch(progress, static_cast<LinearGradient*>(fill)->transform());
    return fill;
}

Fill* TvgRadial::dispatch(float progress)
{
    c(progress, fill);
    auto p = this->p(progress);
    auto f = this->f(progress);
    static_cast<RadialGradient*>(fill)->radial(p.v[0], p.v[1], p.v[2], f.v[0], f.v[1], f.v[2]);
    if (t) t->dispatch(progress, fill->transform());
    return fill;
}

Fill* TvgConic::dispatch(float progress)
{
    c(progress, fill);
    auto p = this->p(progress);
    static_cast<ConicGradient*>(fill)->conic(p.x, p.y, d(progress));
    if (t) t->dispatch(progress, fill->transform());
    return fill;
}

/************************************************************************/
/* TvgShape                                                             */
/************************************************************************/

void TvgRect::dispatch(float progress, Shape* out)
{
    auto p = this->p(progress);
    auto z = this->z(progress);
    auto r = this->r(progress);
    out->appendRect(p.x, p.y, z.x, z.y, r.x, r.y, d);
}

void TvgCircle::dispatch(float progress, Shape* out)
{
    auto c = this->c(progress);
    auto r = this->r(progress);
    out->appendCircle(c.x, c.y, r.x, r.y, d);
}

TvgCmdPath::Pts::~Pts()
{
    if (pts.cnt == 0) {
        tvg::free(pts.value);
    } else {
        for (uint32_t i = 0; i < pts.cnt; ++i) {
            tvg::free(pts.frames[i].value);
        }
    }
}

void TvgCmdPath::dispatch(float progress, Shape* out)
{
    // direct approach for passing memory copy
    auto& path = to<ShapeImpl>(out)->rs.path;
    to<ShapeImpl>(out)->impl.mark(RenderUpdateFlag::Path);
    // points
    pts.pts(progress, path.pts, pts.cnt);
    // commands
    path.cmds.grow(cmd.cnt);
    memcpy(path.cmds.end(), cmd.cmds, sizeof(PathCommand) * cmd.cnt);
    path.cmds.count += cmd.cnt;
}

TvgShape::~TvgShape()
{
    for (auto p : list) delete p;
    delete (g);
    delete (s);
    delete (t);
}

void TvgShape::buildCore(float progress)
{
    TvgPaint::build(progress);

    auto obj = shape();
    obj->reset();

    // fill
    if (g) {
        obj->fill(g->dispatch(progress));
    } else {
        auto f = this->f(progress);
        obj->fill(f.r, f.g, f.b, f.a);
    }

    // paths
    for (auto path : list) {
        path->dispatch(progress, obj);
    }

    // stroke
    if (s) {
        obj->strokeWidth(s->w(progress));
        if (s->g) {
            obj->strokeFill(s->g->dispatch(progress));
        } else {
            auto f = s->f(progress);
            obj->strokeFill(f.r, f.g, f.b, f.a);
        }
    }

    if (t) obj->trimpath(t->b(progress), t->e(progress), t->m);
}

void TvgShape::build(float progress)
{
    if (!statical) buildCore(progress);
}

TvgShape::Stroke* TvgShape::stroke()
{
    if (!s) s = new Stroke;
    return s;
}

TvgShape::Trim* TvgShape::trim()
{
    if (!t) t = new Trim;
    return t;
}

void TvgShape::prepare(TvgPaint* parent)
{
    TvgPaint::prepare(parent);

    // assign to prevent a dangling fill
    if (g) shape()->fill(g->fill);
    if (s && s->g) shape()->strokeFill(s->g->fill);

    if (statical) buildCore(0.0);
}
