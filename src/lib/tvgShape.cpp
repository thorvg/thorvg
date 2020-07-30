/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#ifndef _TVG_SHAPE_CPP_
#define _TVG_SHAPE_CPP_

#include "tvgCommon.h"
#include "tvgShapeImpl.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
constexpr auto PATH_KAPPA = 0.552284f;

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Shape :: Shape() : pImpl(make_unique<Impl>(this))
{
    Paint::IMPL->method(new PaintMethod<Shape::Impl>(IMPL));
}


Shape :: ~Shape()
{
}


unique_ptr<Shape> Shape::gen() noexcept
{
    return unique_ptr<Shape>(new Shape);
}


Result Shape::reset() noexcept
{
    IMPL->path->reset();

    IMPL->flag |= RenderUpdateFlag::Path;

    return Result::Success;
}


uint32_t Shape::pathCommands(const PathCommand** cmds) const noexcept
{
    if (!cmds) return 0;

    *cmds = IMPL->path->cmds;

    return IMPL->path->cmdCnt;
}


uint32_t Shape::pathCoords(const Point** pts) const noexcept
{
    if (!pts) return 0;

    *pts = IMPL->path->pts;

    return IMPL->path->ptsCnt;
}


Result Shape::appendPath(const PathCommand *cmds, uint32_t cmdCnt, const Point* pts, uint32_t ptsCnt) noexcept
{
    if (cmdCnt < 0 || ptsCnt < 0 || !pts || !ptsCnt) return Result::InvalidArguments;

    IMPL->path->grow(cmdCnt, ptsCnt);
    IMPL->path->append(cmds, cmdCnt, pts, ptsCnt);

    IMPL->flag |= RenderUpdateFlag::Path;

    return Result::Success;
}


Result Shape::moveTo(float x, float y) noexcept
{
    IMPL->path->moveTo(x, y);

    IMPL->flag |= RenderUpdateFlag::Path;

    return Result::Success;
}


Result Shape::lineTo(float x, float y) noexcept
{
    IMPL->path->lineTo(x, y);

    IMPL->flag |= RenderUpdateFlag::Path;

    return Result::Success;
}


Result Shape::cubicTo(float cx1, float cy1, float cx2, float cy2, float x, float y) noexcept
{
    IMPL->path->cubicTo(cx1, cy1, cx2, cy2, x, y);

    IMPL->flag |= RenderUpdateFlag::Path;

    return Result::Success;
}


Result Shape::close() noexcept
{
    IMPL->path->close();

    IMPL->flag |= RenderUpdateFlag::Path;

    return Result::Success;
}


Result Shape::appendCircle(float cx, float cy, float rx, float ry) noexcept
{
    auto impl = pImpl.get();

    auto rxKappa = rx * PATH_KAPPA;
    auto ryKappa = ry * PATH_KAPPA;

    impl->path->grow(6, 13);
    impl->path->moveTo(cx, cy - ry);
    impl->path->cubicTo(cx + rxKappa, cy - ry, cx + rx, cy - ryKappa, cx + rx, cy);
    impl->path->cubicTo(cx + rx, cy + ryKappa, cx + rxKappa, cy + ry, cx, cy + ry);
    impl->path->cubicTo(cx - rxKappa, cy + ry, cx - rx, cy + ryKappa, cx - rx, cy);
    impl->path->cubicTo(cx - rx, cy - ryKappa, cx - rxKappa, cy - ry, cx, cy - ry);
    impl->path->close();

    impl->flag |= RenderUpdateFlag::Path;

    return Result::Success;
}


Result Shape::appendRect(float x, float y, float w, float h, float rx, float ry) noexcept
{
    auto impl = pImpl.get();

    auto halfW = w * 0.5f;
    auto halfH = h * 0.5f;

    //clamping cornerRadius by minimum size
    if (rx > halfW) rx = halfW;
    if (ry > halfH) ry = halfH;

    //rectangle
    if (rx == 0 && ry == 0) {
        impl->path->grow(5, 4);
        impl->path->moveTo(x, y);
        impl->path->lineTo(x + w, y);
        impl->path->lineTo(x + w, y + h);
        impl->path->lineTo(x, y + h);
        impl->path->close();
    //circle
    } else if (fabsf(rx - halfW) < FLT_EPSILON && fabsf(ry - halfH) < FLT_EPSILON) {
        return appendCircle(x + (w * 0.5f), y + (h * 0.5f), rx, ry);
    } else {
        auto hrx = rx * 0.5f;
        auto hry = ry * 0.5f;
        impl->path->grow(10, 17);
        impl->path->moveTo(x + rx, y);
        impl->path->lineTo(x + w - rx, y);
        impl->path->cubicTo(x + w - rx + hrx, y, x + w, y + ry - hry, x + w, y + ry);
        impl->path->lineTo(x + w, y + h - ry);
        impl->path->cubicTo(x + w, y + h - ry + hry, x + w - rx + hrx, y + h, x + w - rx, y + h);
        impl->path->lineTo(x + rx, y + h);
        impl->path->cubicTo(x + rx - hrx, y + h, x, y + h - ry + hry, x, y + h - ry);
        impl->path->lineTo(x, y + ry);
        impl->path->cubicTo(x, y + ry - hry, x + rx - hrx, y, x + rx, y);
        impl->path->close();
    }

    impl->flag |= RenderUpdateFlag::Path;

    return Result::Success;
}


Result Shape::fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept
{
    auto impl = pImpl.get();

    impl->color[0] = r;
    impl->color[1] = g;
    impl->color[2] = b;
    impl->color[3] = a;
    impl->flag |= RenderUpdateFlag::Color;

    if (impl->fill) {
        delete(impl->fill);
        impl->fill = nullptr;
        impl->flag |= RenderUpdateFlag::Gradient;
    }

    return Result::Success;
}


Result Shape::fill(unique_ptr<Fill> f) noexcept
{
    auto impl = pImpl.get();

    auto p = f.release();
    if (!p) return Result::MemoryCorruption;

    if (impl->fill) delete(impl->fill);
    impl->fill = p;
    impl->flag |= RenderUpdateFlag::Gradient;

    return Result::Success;
}


Result Shape::fill(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept
{
    auto impl = pImpl.get();

    if (r) *r = impl->color[0];
    if (g) *g = impl->color[1];
    if (b) *b = impl->color[2];
    if (a) *a = impl->color[3];

    return Result::Success;
}

const Fill* Shape::fill() const noexcept
{
    return IMPL->fill;
}


Result Shape::stroke(float width) noexcept
{
    if (!IMPL->strokeWidth(width)) return Result::FailedAllocation;

    return Result::Success;
}


float Shape::strokeWidth() const noexcept
{
    if (!IMPL->stroke) return 0;
    return IMPL->stroke->width;
}


Result Shape::stroke(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept
{
    if (!IMPL->strokeColor(r, g, b, a)) return Result::FailedAllocation;

    return Result::Success;
}


Result Shape::strokeColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept
{
    auto impl = pImpl.get();

    if (!impl->stroke) return Result::InsufficientCondition;

    if (r) *r = impl->stroke->color[0];
    if (g) *g = impl->stroke->color[1];
    if (b) *b = impl->stroke->color[2];
    if (a) *a = impl->stroke->color[3];

    return Result::Success;
}


Result Shape::stroke(const float* dashPattern, uint32_t cnt) noexcept
{
    if (cnt < 2 || !dashPattern) return Result::InvalidArguments;

    if (!IMPL->strokeDash(dashPattern, cnt)) return Result::FailedAllocation;

    return Result::Success;
}


uint32_t Shape::strokeDash(const float** dashPattern) const noexcept
{
    if (!IMPL->stroke) return 0;

    if (dashPattern) *dashPattern = IMPL->stroke->dashPattern;
    return IMPL->stroke->dashCnt;
}


Result Shape::stroke(StrokeCap cap) noexcept
{
    if (!IMPL->strokeCap(cap)) return Result::FailedAllocation;

    return Result::Success;
}


Result Shape::stroke(StrokeJoin join) noexcept
{
    if (!IMPL->strokeJoin(join)) return Result::FailedAllocation;

    return Result::Success;
}


StrokeCap Shape::strokeCap() const noexcept
{
    if (!IMPL->stroke) return StrokeCap::Square;

    return IMPL->stroke->cap;
}


StrokeJoin Shape::strokeJoin() const noexcept
{
    if (!IMPL->stroke) return StrokeJoin::Bevel;

    return IMPL->stroke->join;
}


#endif //_TVG_SHAPE_CPP_
