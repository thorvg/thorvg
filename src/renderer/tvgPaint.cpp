/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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
#include "tvgPaint.h"
#include "tvgShape.h"
#include "tvgPicture.h"
#include "tvgScene.h"
#include "tvgText.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#define PAINT_METHOD(ret, METHOD) \
    switch (paint->type()) { \
        case Type::Shape: ret = SHAPE(paint)->METHOD; break; \
        case Type::Scene: ret = SCENE(paint)->METHOD; break; \
        case Type::Picture: ret = PICTURE(paint)->METHOD; break; \
        case Type::Text: ret = TEXT(paint)->METHOD; break; \
        default: ret = {}; \
    }


static Result _clipRect(RenderMethod* renderer, const Point* pts, const Matrix& pm, const Matrix& rm, RenderRegion& before)
{
    //sorting
    Point tmp[4];
    Point min = {FLT_MAX, FLT_MAX};
    Point max = {0.0f, 0.0f};

    for (int i = 0; i < 4; ++i) {
        tmp[i] = pts[i];
        tmp[i] *= rm;
        tmp[i] *= pm;
        if (tmp[i].x < min.x) min.x = tmp[i].x;
        if (tmp[i].x > max.x) max.x = tmp[i].x;
        if (tmp[i].y < min.y) min.y = tmp[i].y;
        if (tmp[i].y > max.y) max.y = tmp[i].y;
    }

    float region[4] = {float(before.min.x), float(before.max.x), float(before.min.y), float(before.max.y)};

    //figure out if the clipper is a superset of the current viewport(before) region
    if (min.x <= region[0] && max.x >= region[1] && min.y <= region[2] && max.y >= region[3]) {
        //viewport region is same, nothing to do.
        return Result::Success;
    //figure out if the clipper is totally outside of the viewport
    } else if (max.x <= region[0] || min.x >= region[1] || max.y <= region[2] || min.y >= region[3]) {
        renderer->viewport({});
        return Result::Success;
    }
    return Result::InsufficientCondition;
}


static Result _compFastTrack(RenderMethod* renderer, Paint* cmpTarget, const Matrix& pm, RenderRegion& before)
{
    /* Access Shape class by Paint is bad... but it's ok still it's an internal usage. */
    auto shape = static_cast<Shape*>(cmpTarget);

    //Trimming likely makes the shape non-rectangular
    if (SHAPE(shape)->rs.trimpath()) return Result::InsufficientCondition;

    //Rectangle Candidates?
    const Point* pts;
    uint32_t ptsCnt;
    shape->path(nullptr, nullptr, &pts, &ptsCnt);

    //nothing to clip
    if (ptsCnt == 0) return Result::InvalidArguments;
    if (ptsCnt != 4) return Result::InsufficientCondition;

    auto& rm = cmpTarget->transform();

    //No rotation and no skewing, still can try out clipping the rect region.
    auto tryClip = false;

    if ((!rightAngle(pm) || skewed(pm))) tryClip = true;
    if ((!rightAngle(rm) || skewed(rm))) tryClip = true;

    if (tryClip) return _clipRect(renderer, pts, pm, rm, before);

    //Perpendicular Rectangle?
    auto pt1 = pts + 0;
    auto pt2 = pts + 1;
    auto pt3 = pts + 2;
    auto pt4 = pts + 3;

    if ((tvg::equal(pt1->x, pt2->x) && tvg::equal(pt2->y, pt3->y) && tvg::equal(pt3->x, pt4->x) && tvg::equal(pt1->y, pt4->y)) ||
        (tvg::equal(pt2->x, pt3->x) && tvg::equal(pt1->y, pt2->y) && tvg::equal(pt1->x, pt4->x) && tvg::equal(pt3->y, pt4->y))) {

        RenderRegion after;

        auto v1 = *pt1;
        auto v2 = *pt3;
        v1 *= rm;
        v2 *= rm;
        v1 *= pm;
        v2 *= pm;

        //sorting
        if (v1.x > v2.x) std::swap(v1.x, v2.x);
        if (v1.y > v2.y) std::swap(v1.y, v2.y);

        after.min.x = static_cast<int32_t>(nearbyint(v1.x));
        after.min.y = static_cast<int32_t>(nearbyint(v1.y));
        after.max.x = static_cast<int32_t>(nearbyint(v2.x));
        after.max.y = static_cast<int32_t>(nearbyint(v2.y));

        if (after.max.x < after.min.x) after.max.x = after.min.x;
        if (after.max.y < after.min.y) after.max.y = after.min.y;

        after.intersect(before);
        renderer->viewport(after);

        return Result::Success;
    }
    return Result::InsufficientCondition;
}


RenderRegion Paint::Impl::bounds(RenderMethod* renderer) const
{
    RenderRegion ret;
    PAINT_METHOD(ret, bounds(renderer));
    return ret;
}


Iterator* Paint::Impl::iterator()
{
    Iterator* ret;
    PAINT_METHOD(ret, iterator());
    return ret;
}


Paint* Paint::Impl::duplicate(Paint* ret)
{
    if (ret) ret->mask(nullptr, MaskMethod::None);

    PAINT_METHOD(ret, duplicate(ret));

    //duplicate Transform
    ret->pImpl->tr = tr;
    ret->pImpl->mark(RenderUpdateFlag::Transform);

    ret->pImpl->opacity = opacity;

    if (maskData) ret->mask(maskData->target->duplicate(), maskData->method);
    if (clipper) ret->clip(static_cast<Shape*>(clipper->duplicate()));

    return ret;
}


bool Paint::Impl::render(RenderMethod* renderer)
{
    if (opacity == 0) return true;

    RenderCompositor* cmp = nullptr;

    //OPTIMIZE: bounds(renderer) calls could dismiss the parallelization
    if (maskData && !(maskData->target->pImpl->ctxFlag & ContextFlag::FastTrack)) {
        RenderRegion region;
        PAINT_METHOD(region, bounds(renderer));

        auto mData = maskData;
        while (mData) {
            if (MASK_REGION_MERGING(mData->method)) region.add(PAINT(mData->target)->bounds(renderer));
            if (region.invalid()) return true;
            mData = PAINT(mData->target)->maskData;
        }
        cmp = renderer->target(region, MASK_TO_COLORSPACE(renderer, maskData->method), CompositionFlag::Masking);
        if (renderer->beginComposite(cmp, MaskMethod::None, 255)) {
            maskData->target->pImpl->render(renderer);
        }
    }

    if (cmp) renderer->beginComposite(cmp, maskData->method, maskData->target->pImpl->opacity);

    bool ret;
    PAINT_METHOD(ret, render(renderer));

    if (cmp) renderer->endComposite(cmp);

    return ret;
}


RenderData Paint::Impl::update(RenderMethod* renderer, const Matrix& pm, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flag, bool clipper)
{
    bool ret;
    PAINT_METHOD(ret, skip((flag | renderFlag)));

    if (ret) return rd;

    cmpFlag = CompositionFlag::Invalid;  //must clear after the rendering

    if (this->renderer != renderer) {
        if (this->renderer) TVGERR("RENDERER", "paint's renderer has been changed!");
        renderer->ref();
        this->renderer = renderer;
    }

    if (renderFlag & RenderUpdateFlag::Transform) tr.update();

    /* 1. Composition Pre Processing */
    RenderData trd = nullptr;                 //composite target render data
    RenderRegion viewport;
    Result compFastTrack = Result::InsufficientCondition;

    if (maskData) {
        auto target = maskData->target;
        auto method = maskData->method;
        PAINT(target)->ctxFlag &= ~ContextFlag::FastTrack;   //reset

        /* If the transformation has no rotational factors and the Alpha(InvAlpha) Masking involves a simple rectangle,
           we can optimize by using the viewport instead of the regular Alphaing sequence for improved performance. */
        if (target->type() == Type::Shape) {
            auto shape = static_cast<Shape*>(target);
            uint8_t a;
            shape->fill(nullptr, nullptr, nullptr, &a);
            //no gradient fill & no maskings of the masking target.
            if (!shape->fill() && !(PAINT(shape)->maskData)) {
                if ((method == MaskMethod::Alpha && a == 255 && PAINT(shape)->opacity == 255) || (method == MaskMethod::InvAlpha && (a == 0 || PAINT(shape)->opacity == 0))) {
                    viewport = renderer->viewport();
                    if ((compFastTrack = _compFastTrack(renderer, target, pm, viewport)) == Result::Success) {
                        PAINT(target)->ctxFlag |= ContextFlag::FastTrack;
                    }
                }
            }
        }
        if (compFastTrack == Result::InsufficientCondition) {
            trd = PAINT(target)->update(renderer, pm, clips, 255, flag, false);
        }
    }

    /* 2. Clipping */
    if (this->clipper) {
        auto pclip = PAINT(this->clipper);
        if (pclip->renderFlag) mark(RenderUpdateFlag::Clip);
        pclip->ctxFlag &= ~ContextFlag::FastTrack;   //reset
        viewport = renderer->viewport();
        /* TODO: Intersect the clipper's clipper, if both are FastTrack.
           Update the subsequent clipper first and check its ctxFlag. */
        if (!pclip->clipper && SHAPE(this->clipper)->rs.strokeWidth() == 0.0f && _compFastTrack(renderer, this->clipper, pm, viewport) == Result::Success) {
            pclip->ctxFlag |= ContextFlag::FastTrack;
            compFastTrack = Result::Success;
        } else {
            trd = pclip->update(renderer, pm, clips, 255, flag, true);
            clips.push(trd);
            compFastTrack = Result::InsufficientCondition;
        }
    }

    /* 3. Main Update */
    opacity = MULTIPLY(opacity, this->opacity);
    PAINT_METHOD(ret, update(renderer, pm * tr.m, clips, opacity, (flag | renderFlag), clipper));

    /* 4. Composition Post Processing */
    if (compFastTrack == Result::Success) renderer->viewport(viewport);
    else if (this->clipper) clips.pop();

    renderFlag = RenderUpdateFlag::None;

    return rd;
}


Result Paint::Impl::bounds(float* x, float* y, float* w, float* h, Matrix* pm, bool stroking)
{
    Point pts[4];
    if (bounds(pts, pm, false, stroking) != Result::Success) return Result::InsufficientCondition;

    Point min = {FLT_MAX, FLT_MAX};
    Point max = {-FLT_MAX, -FLT_MAX};

    for (int i = 0; i < 4; ++i) {
        if (pts[i].x < min.x) min.x = pts[i].x;
        if (pts[i].x > max.x) max.x = pts[i].x;
        if (pts[i].y < min.y) min.y = pts[i].y;
        if (pts[i].y > max.y) max.y = pts[i].y;
    }

    if (x) *x = min.x;
    if (y) *y = min.y;
    if (w) *w = max.x - min.x;
    if (h) *h = max.y - min.y;
    return Result::Success;
}


Result Paint::Impl::bounds(Point* pt4, Matrix* pm, bool obb, bool stroking)
{
    auto m = this->transform();
    if (pm) m = *pm * m;

    Result ret;
    PAINT_METHOD(ret, bounds(pt4, m, obb, stroking));
    return ret;
}


bool Paint::Impl::intersects(const RenderRegion& region)
{
    if (!renderer) return false;
    bool ret;
    PAINT_METHOD(ret, intersects(region));
    return ret;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Paint :: Paint() = default;
Paint :: ~Paint() = default;


Result Paint::rotate(float degree) noexcept
{
    if (pImpl->rotate(degree)) return Result::Success;
    return Result::InsufficientCondition;
}


Result Paint::scale(float factor) noexcept
{
    if (pImpl->scale(factor)) return Result::Success;
    return Result::InsufficientCondition;
}


Result Paint::translate(float x, float y) noexcept
{
    if (pImpl->translate(x, y)) return Result::Success;
    return Result::InsufficientCondition;
}


Result Paint::transform(const Matrix& m) noexcept
{
    if (pImpl->transform(m)) return Result::Success;
    return Result::InsufficientCondition;
}


Matrix& Paint::transform() noexcept
{
    return pImpl->transform();
}


Result Paint::bounds(float* x, float* y, float* w, float* h) const noexcept
{
    auto pm = pImpl->ptransform();
    return pImpl->bounds(x, y, w, h, &pm, true);
}


Result Paint::bounds(Point* pt4) const noexcept
{
    if (!pt4) return Result::InvalidArguments;
    auto pm = pImpl->ptransform();
    return pImpl->bounds(pt4, &pm, true, true);
}


bool Paint::intersects(int32_t x, int32_t y, int32_t w, int32_t h) noexcept
{
    if (w <= 0 || h <= 0) return false;
    return pImpl->intersects({x, y, x + w, y + h});
}


Paint* Paint::duplicate() const noexcept
{
    return pImpl->duplicate();
}


Result Paint::clip(Shape* clipper) noexcept
{
    return pImpl->clip(clipper);
}


Shape* Paint::clip() const noexcept
{
    return pImpl->clipper;
}


Result Paint::mask(Paint* target, MaskMethod method) noexcept
{
    if (method > MaskMethod::Darken) return Result::InvalidArguments;
    return pImpl->mask(target, method);
}


MaskMethod Paint::mask(const Paint** target) const noexcept
{
    return pImpl->mask(target);
}


Result Paint::opacity(uint8_t o) noexcept
{
    if (pImpl->opacity == o) return Result::Success;

    pImpl->opacity = o;
    pImpl->mark(RenderUpdateFlag::Color);

    return Result::Success;
}


uint8_t Paint::opacity() const noexcept
{
    return pImpl->opacity;
}


Result Paint::blend(BlendMethod method) noexcept
{
    //Composition is only allowed to Scene.
    if (method <= BlendMethod::Add || (method == BlendMethod::Composition && type() == Type::Scene)) {
        pImpl->blend(method);
        return Result::Success;
    }
    return Result::InvalidArguments;
}


uint16_t Paint::ref() noexcept
{
    return pImpl->ref();
}


uint16_t Paint::unref(bool free) noexcept
{
    return pImpl->unrefx(free);
}


uint16_t Paint::refCnt() const noexcept
{
    return pImpl->refCnt;
}


const Paint* Paint::parent() const noexcept
{
    return pImpl->parent;
}