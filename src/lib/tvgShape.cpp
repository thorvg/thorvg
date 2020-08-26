/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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
#include <limits>

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
    if (cmdCnt == 0 || ptsCnt == 0 || !pts || !ptsCnt) return Result::InvalidArguments;

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

Result Shape::appendArc(float cx, float cy, float radius, float startAngle, float sweep, bool pie) noexcept
{
    const float M_PI_HALF = M_PI * 0.5f;

    //just circle
    if (sweep >= 360) return appendCircle(cx, cy, radius, radius);

    auto impl = pImpl.get();

    startAngle = (startAngle * M_PI) / 180;
    sweep = sweep * M_PI / 180;

    auto nCurves = ceil(abs(sweep / M_PI_HALF));
    auto fract = fmod(sweep, M_PI_HALF);
    fract = (fract < std::numeric_limits<float>::epsilon()) ? M_PI_HALF : fract;

    //Start from here
    Point start = {radius * cos(startAngle), radius * sin(startAngle)};

    if (pie) {
        impl->path->moveTo(cx, cy);
        impl->path->lineTo(start.x + cx, start.y + cy);
    }else {
        impl->path->moveTo(start.x + cx, start.y + cy);
    }

    for (int i = 0; i < nCurves; ++i) {

        auto endAngle = startAngle + ((i != nCurves - 1) ? M_PI_HALF : fract);
        Point end = {radius * cos(endAngle), radius * sin(endAngle)};

        //variables needed to calculate bezier control points

        //get bezier control points using article:
        //(http://itc.ktu.lt/index.php/ITC/article/view/11812/6479)
        auto ax = start.x;
        auto ay = start.y;
        auto bx = end.x;
        auto by = end.y;
        auto q1 = ax * ax + ay * ay;
        auto q2 = ax * bx + ay * by + q1;
        auto k2 = static_cast<float> (4.0/3.0) * ((sqrt(2 * q1 * q2) - q2) / (ax * by - ay * bx));

        start = end; //Next start point is the current end point

        end.x += cx;
        end.y += cy;

        Point ctrl1 = {ax - k2 * ay + cx, ay + k2 * ax + cy};
        Point ctrl2 = {bx + k2 * by + cx, by - k2 * bx + cy};

        impl->path->cubicTo(ctrl1.x, ctrl1.y, ctrl2.x, ctrl2.y, end.x, end.y);

        startAngle = endAngle;
    }

    if (pie) {
        impl->path->moveTo(cx, cy);
        impl->path->close();
    }

    IMPL->flag |= RenderUpdateFlag::Path;

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

    if (impl->fill && impl->fill != p) delete(impl->fill);
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