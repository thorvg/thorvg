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
#include "tvgTvgModel.h"

/************************************************************************/
/* TvgPaint                                                             */
/************************************************************************/

TvgPaint::TvgPaint(TvgScene* parent, Paint* obj, const char* id) : obj(obj)
{
    if (parent) parent->scene()->add(obj);
    obj->ref();
    obj->id = djb2Encode(id);
    obj->data = this;
}

TvgPaint::~TvgPaint()
{
    delete (t);
    obj->unref();
}

void TvgPaint::build(float progress)
{
    // T(translate) * R(rotation) * K(skew) * S(scale) * T(-anchor)
    auto tr = transform();

    auto r = deg2rad(tr->r);
    auto c = cosf(r);
    auto s = sinf(r);

    auto k = tr->k(progress);
    auto kx = tanf(deg2rad(k.x));
    auto ky = tanf(deg2rad(k.y));

    Matrix m = {(c - s * ky) * tr->s[0], (c * kx - s) * tr->s[1], 0.0f,
                (s + c * ky) * tr->s[0], (s * kx + c) * tr->s[1], 0.0f,
                0.0f, 0.0f, 1.0f};

    auto t = tr->t(progress);
    auto a = tr->a(progress);

    m.e13 = t.x - m.e11 * a.x - m.e12 * a.y;
    m.e23 = t.y - m.e21 * a.x - m.e22 * a.y;

    obj->transform(m);
    obj->opacity(o);
}

/************************************************************************/
/* TvgScene                                                             */
/************************************************************************/

TvgScene::~TvgScene()
{
    for (auto c : scene()->paints()) {
        delete ((TvgPaint*)(c->data));
    }
}

/************************************************************************/
/* TvgShape                                                             */
/************************************************************************/

TvgShape::~TvgShape()
{
    ARRAY_FOREACH(p, list) {
        delete (*p);
    }
}

void TvgShape::build(float progress)
{
    TvgPaint::build(progress);

    auto obj = shape();

    obj->reset();

    // append paths
    for (auto path : list) {
        switch (path->type) {
            case TvgShape::Path::Custom: {
                auto custom = (TvgShape::Custom*)path;
                obj->appendPath(custom->path.cmds.data, custom->path.cmds.count, custom->path.pts.data, custom->path.pts.count);
                break;
            }
            case TvgShape::Path::Rect: {
                auto rect = (TvgShape::Rect*)path;
                auto p = rect->p(progress);
                auto z = rect->z(progress);
                auto r = rect->r(progress);
                obj->appendRect(p.x, p.y, z.x, z.y, r.x, r.y, rect->d);
                break;
            }
            case TvgShape::Path::Circle: {
                auto circle = (TvgShape::Circle*)path;
                auto c = circle->c(progress);
                auto r = circle->r(progress);
                obj->appendCircle(c.x, c.y, r.x, r.y, circle->d);
                break;
            }
            default: break;
        }
    }

    obj->fill(f.r, f.g, f.b, f.a);
}
