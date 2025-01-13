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
#include "tvgShape.h"


Shape :: Shape()
{
    pImpl = new Impl(this);
}


Shape* Shape::gen() noexcept
{
    return new Shape;
}


Type Shape::type() const noexcept
{
    return Type::Shape;
}


Result Shape::reset() noexcept
{
    SHAPE(this)->resetPath();
    return Result::Success;
}


Result Shape::path(const PathCommand** cmds, uint32_t* cmdsCnt, const Point** pts, uint32_t* ptsCnt) const noexcept
{
    if (cmds) *cmds = SHAPE(this)->rs.path.cmds.data;
    if (cmdsCnt) *cmdsCnt = SHAPE(this)->rs.path.cmds.count;

    if (pts) *pts = SHAPE(this)->rs.path.pts.data;
    if (ptsCnt) *ptsCnt = SHAPE(this)->rs.path.pts.count;

    return Result::Success;
}


Result Shape::appendPath(const PathCommand *cmds, uint32_t cmdCnt, const Point* pts, uint32_t ptsCnt) noexcept
{
    return SHAPE(this)->appendPath(cmds, cmdCnt, pts, ptsCnt);
}


Result Shape::moveTo(float x, float y) noexcept
{
    SHAPE(this)->moveTo(x, y);
    return Result::Success;
}


Result Shape::lineTo(float x, float y) noexcept
{
    SHAPE(this)->lineTo(x, y);
    return Result::Success;
}


Result Shape::cubicTo(float cx1, float cy1, float cx2, float cy2, float x, float y) noexcept
{
    SHAPE(this)->cubicTo(cx1, cy1, cx2, cy2, x, y);
    return Result::Success;
}


Result Shape::close() noexcept
{
    SHAPE(this)->close();
    return Result::Success;
}


Result Shape::appendCircle(float cx, float cy, float rx, float ry) noexcept
{
    SHAPE(this)->appendCircle(cx, cy, rx, ry);
    return Result::Success;
}


Result Shape::appendRect(float x, float y, float w, float h, float rx, float ry) noexcept
{
    SHAPE(this)->appendRect(x, y, w, h, rx, ry);
    return Result::Success;
}


Result Shape::fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept
{
    SHAPE(this)->fill(r, g, b, a);
    return Result::Success;
}


Result Shape::fill(Fill* f) noexcept
{
    return SHAPE(this)->fill(f);
}


Result Shape::fill(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept
{
    SHAPE(this)->rs.fillColor(r, g, b, a);
    return Result::Success;
}


const Fill* Shape::fill() const noexcept
{
    return SHAPE(this)->rs.fill;
}


Result Shape::order(bool strokeFirst) noexcept
{
    SHAPE(this)->strokeFirst(strokeFirst);
    return Result::Success;
}


Result Shape::strokeWidth(float width) noexcept
{
    SHAPE(this)->strokeWidth(width);
    return Result::Success;
}


float Shape::strokeWidth() const noexcept
{
    return SHAPE(this)->rs.strokeWidth();
}


Result Shape::strokeFill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept
{
    SHAPE(this)->strokeFill(r, g, b, a);
    return Result::Success;
}


Result Shape::strokeFill(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const noexcept
{
    if (!SHAPE(this)->rs.strokeFill(r, g, b, a)) return Result::InsufficientCondition;
    return Result::Success;
}


Result Shape::strokeFill(Fill* f) noexcept
{
    return SHAPE(this)->strokeFill(f);
}


const Fill* Shape::strokeFill() const noexcept
{
    return SHAPE(this)->rs.strokeFill();
}


Result Shape::strokeDash(const float* dashPattern, uint32_t cnt, float offset) noexcept
{
    return SHAPE(this)->strokeDash(dashPattern, cnt, offset);
}


uint32_t Shape::strokeDash(const float** dashPattern, float* offset) const noexcept
{
    return SHAPE(this)->rs.strokeDash(dashPattern, offset);
}


Result Shape::strokeCap(StrokeCap cap) noexcept
{
    SHAPE(this)->strokeCap(cap);
    return Result::Success;
}


Result Shape::strokeJoin(StrokeJoin join) noexcept
{
    SHAPE(this)->strokeJoin(join);
    return Result::Success;
}


Result Shape::strokeMiterlimit(float miterlimit) noexcept
{
    return SHAPE(this)->strokeMiterlimit(miterlimit);
}


StrokeCap Shape::strokeCap() const noexcept
{
    return SHAPE(this)->rs.strokeCap();
}


StrokeJoin Shape::strokeJoin() const noexcept
{
    return SHAPE(this)->rs.strokeJoin();
}


float Shape::strokeMiterlimit() const noexcept
{
    return SHAPE(this)->rs.strokeMiterlimit();
}


Result Shape::strokeTrim(float begin, float end, bool simultaneous) noexcept
{
    SHAPE(this)->strokeTrim(begin, end, simultaneous);
    return Result::Success;
}


Result Shape::fill(FillRule r) noexcept
{
    SHAPE(this)->rs.rule = r;
    return Result::Success;
}


FillRule Shape::fillRule() const noexcept
{
    return SHAPE(this)->rs.rule;
}
