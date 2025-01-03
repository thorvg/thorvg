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

#ifndef _TVG_SHAPE_H_
#define _TVG_SHAPE_H_

#include <memory.h>
#include "tvgMath.h"
#include "tvgPaint.h"

#define SHAPE(A) PIMPL(A, Shape)

struct Shape::Impl : Paint::Impl
{
    RenderShape rs;
    RenderData rd = nullptr;
    uint8_t compFlag = CompositionFlag::Invalid;
    uint8_t opacity;    //for composition

    Impl(Shape* s) : Paint::Impl(s)
    {
    }

    ~Impl()
    {
        if (renderer) renderer->dispose(rd);
    }

    bool render(RenderMethod* renderer)
    {
        if (!rd) return false;

        RenderCompositor* cmp = nullptr;

        renderer->blend(blendMethod);

        if (compFlag) {
            cmp = renderer->target(bounds(renderer), renderer->colorSpace(), static_cast<CompositionFlag>(compFlag));
            renderer->beginComposite(cmp, MaskMethod::None, opacity);
        }

        auto ret = renderer->renderShape(rd);
        if (cmp) renderer->endComposite(cmp);
        return ret;
    }

    bool needComposition(uint8_t opacity)
    {
        compFlag = CompositionFlag::Invalid;

        if (opacity == 0) return false;

        //Shape composition is only necessary when stroking & fill are valid.
        if (!rs.stroke || rs.stroke->width < FLOAT_EPSILON || (!rs.stroke->fill && rs.stroke->color.a == 0)) return false;
        if (!rs.fill && rs.color.a == 0) return false;

        //translucent fill & stroke
        if (opacity < 255) {
            compFlag = CompositionFlag::Opacity;
            return true;
        }

        //Composition test
        const Paint* target;
        auto method = paint->mask(&target);
        if (!target) return false;

        if ((target->pImpl->opacity == 255 || target->pImpl->opacity == 0) && target->type() == Type::Shape) {
            auto shape = static_cast<const Shape*>(target);
            if (!shape->fill()) {
                uint8_t r, g, b, a;
                shape->fillColor(&r, &g, &b, &a);
                if (a == 0 || a == 255) {
                    if (method == MaskMethod::Luma || method == MaskMethod::InvLuma) {
                        if ((r == 255 && g == 255 && b == 255) || (r == 0 && g == 0 && b == 0)) return false;
                    } else return false;
                }
            }
        }

        compFlag = CompositionFlag::Masking;
        return true;
    }

    RenderData update(RenderMethod* renderer, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag pFlag, bool clipper)
    {
        if (static_cast<RenderUpdateFlag>(pFlag | renderFlag) == RenderUpdateFlag::None) return rd;

        if (needComposition(opacity)) {
            /* Overriding opacity value. If this scene is half-translucent,
               It must do intermediate composition with that opacity value. */ 
            this->opacity = opacity;
            opacity = 255;
        }

        rd = renderer->prepare(rs, rd, transform, clips, opacity, static_cast<RenderUpdateFlag>(pFlag | renderFlag), clipper);
        return rd;
    }

    RenderRegion bounds(RenderMethod* renderer)
    {
        if (!rd) return {0, 0, 0, 0};
        return renderer->region(rd);
    }

    bool bounds(float* x, float* y, float* w, float* h, bool stroking)
    {
        //Path bounding size
        if (rs.path.pts.count > 0 ) {
            auto pts = rs.path.pts.begin();
            Point min = { pts->x, pts->y };
            Point max = { pts->x, pts->y };

            for (auto pts2 = pts + 1; pts2 < rs.path.pts.end(); ++pts2) {
                if (pts2->x < min.x) min.x = pts2->x;
                if (pts2->y < min.y) min.y = pts2->y;
                if (pts2->x > max.x) max.x = pts2->x;
                if (pts2->y > max.y) max.y = pts2->y;
            }

            if (x) *x = min.x;
            if (y) *y = min.y;
            if (w) *w = max.x - min.x;
            if (h) *h = max.y - min.y;
        }

        //Stroke feathering
        if (stroking && rs.stroke) {
            if (x) *x -= rs.stroke->width * 0.5f;
            if (y) *y -= rs.stroke->width * 0.5f;
            if (w) *w += rs.stroke->width;
            if (h) *h += rs.stroke->width;
        }
        return rs.path.pts.count > 0 ? true : false;
    }

    void reserveCmd(uint32_t cmdCnt)
    {
        rs.path.cmds.reserve(cmdCnt);
    }

    void reservePts(uint32_t ptsCnt)
    {
        rs.path.pts.reserve(ptsCnt);
    }

    void grow(uint32_t cmdCnt, uint32_t ptsCnt)
    {
        rs.path.cmds.grow(cmdCnt);
        rs.path.pts.grow(ptsCnt);
    }

    void append(const PathCommand* cmds, uint32_t cmdCnt, const Point* pts, uint32_t ptsCnt)
    {
        memcpy(rs.path.cmds.end(), cmds, sizeof(PathCommand) * cmdCnt);
        memcpy(rs.path.pts.end(), pts, sizeof(Point) * ptsCnt);
        rs.path.cmds.count += cmdCnt;
        rs.path.pts.count += ptsCnt;
    }

    void moveTo(float x, float y)
    {
        rs.path.cmds.push(PathCommand::MoveTo);
        rs.path.pts.push({x, y});
    }

    void lineTo(float x, float y)
    {
        rs.path.cmds.push(PathCommand::LineTo);
        rs.path.pts.push({x, y});
        renderFlag |= RenderUpdateFlag::Path;
    }

    void cubicTo(float cx1, float cy1, float cx2, float cy2, float x, float y)
    {
        rs.path.cmds.push(PathCommand::CubicTo);
        rs.path.pts.push({cx1, cy1});
        rs.path.pts.push({cx2, cy2});
        rs.path.pts.push({x, y});

        renderFlag |= RenderUpdateFlag::Path;
    }

    void close()
    {
        //Don't close multiple times.
        if (rs.path.cmds.count > 0 && rs.path.cmds.last() == PathCommand::Close) return;
        rs.path.cmds.push(PathCommand::Close);
        renderFlag |= RenderUpdateFlag::Path;
    }

    void strokeWidth(float width)
    {
        if (!rs.stroke) rs.stroke = new RenderStroke();
        rs.stroke->width = width;
        renderFlag |= RenderUpdateFlag::Stroke;
    }

    void strokeTrim(float begin, float end, bool simultaneous)
    {
        if (!rs.stroke) {
            if (begin == 0.0f && end == 1.0f) return;
            rs.stroke = new RenderStroke();
        }

        if (tvg::equal(rs.stroke->trim.begin, begin) && tvg::equal(rs.stroke->trim.end, end) && rs.stroke->trim.simultaneous == simultaneous) return;

        rs.stroke->trim.begin = begin;
        rs.stroke->trim.end = end;
        rs.stroke->trim.simultaneous = simultaneous;
        renderFlag |= RenderUpdateFlag::Stroke;
    }

    bool strokeTrim(float* begin, float* end)
    {
        if (rs.stroke) {
            if (begin) *begin = rs.stroke->trim.begin;
            if (end) *end = rs.stroke->trim.end;
            return rs.stroke->trim.simultaneous;
        } else {
            if (begin) *begin = 0.0f;
            if (end) *end = 1.0f;
            return false;
        }
    }

    void strokeCap(StrokeCap cap)
    {
        if (!rs.stroke) rs.stroke = new RenderStroke();
        rs.stroke->cap = cap;
        renderFlag |= RenderUpdateFlag::Stroke;
    }

    void strokeJoin(StrokeJoin join)
    {
        if (!rs.stroke) rs.stroke = new RenderStroke();
        rs.stroke->join = join;
        renderFlag |= RenderUpdateFlag::Stroke;
    }

    Result strokeMiterlimit(float miterlimit)
    {
        // https://www.w3.org/TR/SVG2/painting.html#LineJoin
        // - A negative value for stroke-miterlimit must be treated as an illegal value.
        if (miterlimit < 0.0f) return Result::InvalidArguments;
        if (!rs.stroke) rs.stroke = new RenderStroke();
        rs.stroke->miterlimit = miterlimit;
        renderFlag |= RenderUpdateFlag::Stroke;

        return Result::Success;
    }

    void strokeFill(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        if (!rs.stroke) rs.stroke = new RenderStroke();
        if (rs.stroke->fill) {
            delete(rs.stroke->fill);
            rs.stroke->fill = nullptr;
            renderFlag |= RenderUpdateFlag::GradientStroke;
        }

        rs.stroke->color = {r, g, b, a};

        renderFlag |= RenderUpdateFlag::Stroke;
    }

    Result strokeFill(Fill* f)
    {
        if (!f) return Result::InvalidArguments;

        if (!rs.stroke) rs.stroke = new RenderStroke();
        if (rs.stroke->fill && rs.stroke->fill != f) delete(rs.stroke->fill);
        rs.stroke->fill = f;
        rs.stroke->color.a = 0;

        renderFlag |= RenderUpdateFlag::Stroke;
        renderFlag |= RenderUpdateFlag::GradientStroke;

        return Result::Success;
    }

    Result strokeDash(const float* pattern, uint32_t cnt, float offset)
    {
        if ((cnt == 1) || (!pattern && cnt > 0) || (pattern && cnt == 0)) {
            return Result::InvalidArguments;
        }

        for (uint32_t i = 0; i < cnt; i++) {
            if (pattern[i] < FLOAT_EPSILON) return Result::InvalidArguments;
        }

        //Reset dash
        if (!pattern && cnt == 0) {
            free(rs.stroke->dashPattern);
            rs.stroke->dashPattern = nullptr;
        } else {
            if (!rs.stroke) rs.stroke = new RenderStroke();
            if (rs.stroke->dashCnt != cnt) {
                free(rs.stroke->dashPattern);
                rs.stroke->dashPattern = nullptr;
            }
            if (!rs.stroke->dashPattern) {
                rs.stroke->dashPattern = static_cast<float*>(malloc(sizeof(float) * cnt));
                if (!rs.stroke->dashPattern) return Result::FailedAllocation;
            }
            for (uint32_t i = 0; i < cnt; ++i) {
                rs.stroke->dashPattern[i] = pattern[i];
            }
        }
        rs.stroke->dashCnt = cnt;
        rs.stroke->dashOffset = offset;
        renderFlag |= RenderUpdateFlag::Stroke;

        return Result::Success;
    }

    bool strokeFirst()
    {
        if (!rs.stroke) return true;
        return rs.stroke->strokeFirst;
    }

    void strokeFirst(bool strokeFirst)
    {
        if (!rs.stroke) rs.stroke = new RenderStroke();
        rs.stroke->strokeFirst = strokeFirst;
        renderFlag |= RenderUpdateFlag::Stroke;
    }

    Result fill(Fill* f)
    {
        if (!f) return Result::InvalidArguments;

        if (rs.fill && rs.fill != f) delete(rs.fill);
        rs.fill = f;
        renderFlag |= RenderUpdateFlag::Gradient;

        return Result::Success;
    }

    void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        if (rs.fill) {
            delete(rs.fill);
            rs.fill = nullptr;
            renderFlag |= RenderUpdateFlag::Gradient;
        }

        if (r == rs.color.r && g == rs.color.g && b == rs.color.b && a == rs.color.a) return;

        rs.color = {r, g, b, a};
        renderFlag |= RenderUpdateFlag::Color;
    }

    void resetPath()
    {
        rs.path.cmds.clear();
        rs.path.pts.clear();
        renderFlag |= RenderUpdateFlag::Path;
    }

    Result appendPath(const PathCommand *cmds, uint32_t cmdCnt, const Point* pts, uint32_t ptsCnt)
    {
        if (cmdCnt == 0 || ptsCnt == 0 || !cmds || !pts) return Result::InvalidArguments;

        grow(cmdCnt, ptsCnt);
        append(cmds, cmdCnt, pts, ptsCnt);
        renderFlag |= RenderUpdateFlag::Path;

        return Result::Success;
    }

    void appendCircle(float cx, float cy, float rx, float ry)
    {
        auto rxKappa = rx * PATH_KAPPA;
        auto ryKappa = ry * PATH_KAPPA;

        grow(6, 13);
        moveTo(cx + rx, cy);
        cubicTo(cx + rx, cy + ryKappa, cx + rxKappa, cy + ry, cx, cy + ry);
        cubicTo(cx - rxKappa, cy + ry, cx - rx, cy + ryKappa, cx - rx, cy);
        cubicTo(cx - rx, cy - ryKappa, cx - rxKappa, cy - ry, cx, cy - ry);
        cubicTo(cx + rxKappa, cy - ry, cx + rx, cy - ryKappa, cx + rx, cy);
        close();
    }

    void appendRect(float x, float y, float w, float h, float rx, float ry)
    {
        auto halfW = w * 0.5f;
        auto halfH = h * 0.5f;

        //clamping cornerRadius by minimum size
        if (rx > halfW) rx = halfW;
        if (ry > halfH) ry = halfH;

        //rectangle
        if (rx == 0 && ry == 0) {
            grow(5, 4);
            moveTo(x, y);
            lineTo(x + w, y);
            lineTo(x + w, y + h);
            lineTo(x, y + h);
            close();
        //rounded rectangle or circle
        } else {
            auto hrx = rx * PATH_KAPPA;
            auto hry = ry * PATH_KAPPA;
            grow(10, 17);
            moveTo(x + rx, y);
            lineTo(x + w - rx, y);
            cubicTo(x + w - rx + hrx, y, x + w, y + ry - hry, x + w, y + ry);
            lineTo(x + w, y + h - ry);
            cubicTo(x + w, y + h - ry + hry, x + w - rx + hrx, y + h, x + w - rx, y + h);
            lineTo(x + rx, y + h);
            cubicTo(x + rx - hrx, y + h, x, y + h - ry + hry, x, y + h - ry);
            lineTo(x, y + ry);
            cubicTo(x, y + ry - hry, x + rx - hrx, y, x + rx, y);
            close();
        }
    }

    Paint* duplicate(Paint* ret)
    {
        auto shape = static_cast<Shape*>(ret);
        if (shape) shape->reset();
        else shape = Shape::gen();

        auto dup = SHAPE(shape);
        delete(dup->rs.fill);

        //Default Properties
        dup->renderFlag = RenderUpdateFlag::All;
        dup->rs.rule = rs.rule;
        dup->rs.color = rs.color;

        //Path
        dup->rs.path.cmds.push(rs.path.cmds);
        dup->rs.path.pts.push(rs.path.pts);

        //Stroke
        if (rs.stroke) {
            if (!dup->rs.stroke) dup->rs.stroke = new RenderStroke;
            *dup->rs.stroke = *rs.stroke;
        } else {
            delete(dup->rs.stroke);
            dup->rs.stroke = nullptr;
        }

        //Fill
        if (rs.fill) dup->rs.fill = rs.fill->duplicate();
        else dup->rs.fill = nullptr;

        return shape;
    }

    void reset()
    {
        PAINT(paint)->reset();
        rs.path.cmds.clear();
        rs.path.pts.clear();

        rs.color.a = 0;
        rs.rule = FillRule::NonZero;

        delete(rs.stroke);
        rs.stroke = nullptr;

        delete(rs.fill);
        rs.fill = nullptr;
    }

    Iterator* iterator()
    {
        return nullptr;
    }
};

#endif //_TVG_SHAPE_H_
