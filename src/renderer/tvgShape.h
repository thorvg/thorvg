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

#include "tvgCommon.h"
#include "tvgMath.h"
#include "tvgPaint.h"

#define SHAPE(A) static_cast<ShapeImpl*>(A)
#define CONST_SHAPE(A) static_cast<const ShapeImpl*>(A)

struct ShapeImpl : Shape
{
    Paint::Impl impl;
    RenderShape rs;
    uint8_t compFlag = CompositionFlag::Invalid;
    uint8_t opacity;    //for composition

    ShapeImpl() : impl(Paint::Impl(this))
    {
    }

    bool render(RenderMethod* renderer)
    {
        if (!impl.rd) return false;

        RenderCompositor* cmp = nullptr;

        renderer->blend(impl.blendMethod);

        if (compFlag) {
            cmp = renderer->target(bounds(renderer), renderer->colorSpace(), static_cast<CompositionFlag>(compFlag));
            renderer->beginComposite(cmp, MaskMethod::None, opacity);
        }

        auto ret = renderer->renderShape(impl.rd);
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
        auto method = PAINT(this)->mask(&target);
        if (!target) return false;

        if ((target->pImpl->opacity == 255 || target->pImpl->opacity == 0) && target->type() == tvg::Type::Shape) {
            auto shape = static_cast<const Shape*>(target);
            if (!shape->fill()) {
                uint8_t r, g, b, a;
                shape->fill(&r, &g, &b, &a);
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
        if ((pFlag | impl.renderFlag) == RenderUpdateFlag::None) return impl.rd;

        if (needComposition(opacity)) {
            /* Overriding opacity value. If this scene is half-translucent,
               It must do intermediate composition with that opacity value. */ 
            this->opacity = opacity;
            opacity = 255;
        }

        impl.rd = renderer->prepare(rs, impl.rd, transform, clips, opacity, (pFlag | impl.renderFlag), clipper);
        return impl.rd;
    }

    RenderRegion bounds(RenderMethod* renderer)
    {
        if (!impl.rd) return {0, 0, 0, 0};
        return renderer->region(impl.rd);
    }

    Result bounds(Point* pt4, Matrix& m, bool obb, bool stroking)
    {
        float x, y, w, h;
        if (!rs.path.bounds(obb ? nullptr : &m, &x, &y, &w, &h)) return Result::InsufficientCondition;

        //Stroke feathering
        if (stroking && rs.stroke) {
            x -= rs.stroke->width * 0.5f;
            y -= rs.stroke->width * 0.5f;
            w += rs.stroke->width;
            h += rs.stroke->width;
        }

        pt4[0] = {x, y};
        pt4[1] = {x + w, y};
        pt4[2] = {x + w, y + h};
        pt4[3] = {x, y + h};

        if (obb) {
            pt4[0] *= m;
            pt4[1] *= m;
            pt4[2] *= m;
            pt4[3] *= m;
        }

        return Result::Success;
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
        impl.mark(RenderUpdateFlag::Path);
    }

    void cubicTo(float cx1, float cy1, float cx2, float cy2, float x, float y)
    {
        rs.path.cmds.push(PathCommand::CubicTo);
        rs.path.pts.push({cx1, cy1});
        rs.path.pts.push({cx2, cy2});
        rs.path.pts.push({x, y});

        impl.mark(RenderUpdateFlag::Path);
    }

    void close()
    {
        //Don't close multiple times.
        if (rs.path.cmds.count > 0 && rs.path.cmds.last() == PathCommand::Close) return;
        rs.path.cmds.push(PathCommand::Close);
        impl.mark(RenderUpdateFlag::Path);
    }

    void strokeWidth(float width)
    {
        if (!rs.stroke) rs.stroke = new RenderStroke();
        rs.stroke->width = width;
        impl.mark(RenderUpdateFlag::Stroke);
    }

    void trimpath(const RenderTrimPath& trim)
    {
        if (!rs.stroke) {
            if (trim.begin == 0.0f && trim.end == 1.0f) return;
            rs.stroke = new RenderStroke();
        }

        if (tvg::equal(rs.stroke->trim.begin, trim.begin) && tvg::equal(rs.stroke->trim.end, trim.end) && rs.stroke->trim.simultaneous == trim.simultaneous) return;

        rs.stroke->trim = trim;
        impl.mark(RenderUpdateFlag::Path);
    }

    bool trimpath(float* begin, float* end)
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
        impl.mark(RenderUpdateFlag::Stroke);
    }

    void strokeJoin(StrokeJoin join)
    {
        if (!rs.stroke) rs.stroke = new RenderStroke();
        rs.stroke->join = join;
        impl.mark(RenderUpdateFlag::Stroke);
    }

    Result strokeMiterlimit(float miterlimit)
    {
        // https://www.w3.org/TR/SVG2/painting.html#LineJoin
        // - A negative value for stroke-miterlimit must be treated as an illegal value.
        if (miterlimit < 0.0f) return Result::InvalidArguments;
        if (!rs.stroke) rs.stroke = new RenderStroke();
        rs.stroke->miterlimit = miterlimit;
        impl.mark(RenderUpdateFlag::Stroke);

        return Result::Success;
    }

    void strokeFill(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        if (!rs.stroke) rs.stroke = new RenderStroke();
        if (rs.stroke->fill) {
            delete(rs.stroke->fill);
            rs.stroke->fill = nullptr;
            impl.mark(RenderUpdateFlag::GradientStroke);
        }

        rs.stroke->color = {r, g, b, a};

        impl.mark(RenderUpdateFlag::Stroke);
    }

    Result strokeFill(Fill* f)
    {
        if (!f) return Result::InvalidArguments;

        if (!rs.stroke) rs.stroke = new RenderStroke();
        if (rs.stroke->fill && rs.stroke->fill != f) delete(rs.stroke->fill);
        rs.stroke->fill = f;
        rs.stroke->color.a = 0;

        impl.mark(RenderUpdateFlag::Stroke | RenderUpdateFlag::GradientStroke);

        return Result::Success;
    }

    Result strokeDash(const float* pattern, uint32_t cnt, float offset)
    {
        if ((cnt == 1) || (!pattern && cnt > 0) || (pattern && cnt == 0)) return Result::InvalidArguments;
        if (!rs.stroke) rs.stroke = new RenderStroke;
        //Reset dash
        auto& dash = rs.stroke->dash;
        if (dash.count != cnt) {
            tvg::free(dash.pattern);
            dash.pattern = nullptr;
        }
        if (cnt > 0) {
            if (!dash.pattern) dash.pattern = tvg::malloc<float*>(sizeof(float) * cnt);
            dash.length = 0.0f;
            for (uint32_t i = 0; i < cnt; ++i) {
                if (pattern[i] < DASH_PATTERN_THRESHOLD) {
                    dash.count = 0;
                    return Result::InvalidArguments;
                }
                dash.pattern[i] = pattern[i];
                dash.length += dash.pattern[i];
            }
        }
        rs.stroke->dash.count = cnt;
        rs.stroke->dash.offset = offset;
        impl.mark(RenderUpdateFlag::Stroke);

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
        impl.mark(RenderUpdateFlag::Stroke);
    }

    Result fill(Fill* f)
    {
        if (!f) return Result::InvalidArguments;

        if (rs.fill && rs.fill != f) delete(rs.fill);
        rs.fill = f;
        impl.mark(RenderUpdateFlag::Gradient);

        return Result::Success;
    }

    void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        if (rs.fill) {
            delete(rs.fill);
            rs.fill = nullptr;
            impl.mark(RenderUpdateFlag::Gradient);
        }

        if (r == rs.color.r && g == rs.color.g && b == rs.color.b && a == rs.color.a) return;

        rs.color = {r, g, b, a};
        impl.mark(RenderUpdateFlag::Color);
    }

    void resetPath()
    {
        rs.path.cmds.clear();
        rs.path.pts.clear();
        impl.mark(RenderUpdateFlag::Path);
    }

    Result appendPath(const PathCommand *cmds, uint32_t cmdCnt, const Point* pts, uint32_t ptsCnt)
    {
        if (cmdCnt == 0 || ptsCnt == 0 || !cmds || !pts) return Result::InvalidArguments;

        grow(cmdCnt, ptsCnt);
        append(cmds, cmdCnt, pts, ptsCnt);
        impl.mark(RenderUpdateFlag::Path);

        return Result::Success;
    }

    void appendCircle(float cx, float cy, float rx, float ry, bool cw)
    {
        auto rxKappa = rx * PATH_KAPPA;
        auto ryKappa = ry * PATH_KAPPA;

        rs.path.cmds.grow(6);
        auto cmds = rs.path.cmds.end();

        cmds[0] = PathCommand::MoveTo;
        cmds[1] = PathCommand::CubicTo;
        cmds[2] = PathCommand::CubicTo;
        cmds[3] = PathCommand::CubicTo;
        cmds[4] = PathCommand::CubicTo;
        cmds[5] = PathCommand::Close;

        rs.path.cmds.count += 6;

        int table[2][13] = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}, {0, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 12}};
        int* idx = cw ? table[0] : table[1];

        rs.path.pts.grow(13);
        auto pts = rs.path.pts.end();

        pts[idx[0]] = {cx, cy - ry}; //moveTo
        pts[idx[1]] = {cx + rxKappa, cy - ry}; pts[idx[2]] = {cx + rx, cy - ryKappa}; pts[idx[3]] = {cx + rx, cy}; //cubicTo
        pts[idx[4]] = {cx + rx, cy + ryKappa}; pts[idx[5]] = {cx + rxKappa, cy + ry}; pts[idx[6]] = {cx, cy + ry}; //cubicTo
        pts[idx[7]] = {cx - rxKappa, cy + ry}; pts[idx[8]] = {cx - rx, cy + ryKappa}; pts[idx[9]] = {cx - rx, cy}; //cubicTo
        pts[idx[10]] = {cx - rx, cy - ryKappa}; pts[idx[11]] = {cx - rxKappa, cy - ry}; pts[idx[12]] = {cx, cy - ry}; //cubicTo

        rs.path.pts.count += 13;

        impl.mark(RenderUpdateFlag::Path);
    }

    void appendRect(float x, float y, float w, float h, float rx, float ry, bool cw)
    {
        //sharp rect
        if (tvg::zero(rx) && tvg::zero(ry)) {
            rs.path.cmds.grow(5);
            rs.path.pts.grow(4);

            auto cmds = rs.path.cmds.end();
            auto pts = rs.path.pts.end();

            cmds[0] = PathCommand::MoveTo;
            cmds[1] = cmds[2] = cmds[3] = PathCommand::LineTo;
            cmds[4] = PathCommand::Close;

            pts[0] = {x + w, y};
            pts[2] = {x, y + h};
            if (cw) {
                pts[1] = {x + w, y + h};
                pts[3] = {x, y};
            } else {
                pts[1] = {x, y};
                pts[3] = {x + w, y + h};
            }

            rs.path.cmds.count += 5;
            rs.path.pts.count += 4;
        //round rect
        } else {
            auto hsize = Point{w * 0.5f, h * 0.5f};
            rx = (rx > hsize.x) ? hsize.x : rx;
            ry = (ry > hsize.y) ? hsize.y : ry;
            auto hr = Point{rx * PATH_KAPPA, ry * PATH_KAPPA};

            rs.path.cmds.grow(10);
            rs.path.pts.grow(17);

            auto cmds = rs.path.cmds.end();
            auto pts = rs.path.pts.end();

            cmds[0] = PathCommand::MoveTo;
            cmds[9] = PathCommand::Close;
            pts[0] = {x + w, y + ry}; //move

            if (cw) {
                cmds[1] = cmds[3] = cmds[5] = cmds[7] = PathCommand::LineTo;
                cmds[2] = cmds[4] = cmds[6] = cmds[8] = PathCommand::CubicTo;

                pts[1] = {x + w, y + h - ry}; //line
                pts[2] = {x + w, y + h - ry + hr.y}; pts[3] = {x + w - rx + hr.x, y + h}; pts[4] = {x + w - rx, y + h};  //cubic
                pts[5] = {x + rx, y + h}, //line
                pts[6] = {x + rx - hr.x, y + h}; pts[7] = {x, y + h - ry + hr.y}; pts[8] = {x, y + h - ry}; //cubic
                pts[9] = {x, y + ry}, //line
                pts[10] = {x, y + ry - hr.y}; pts[11] = {x + rx - hr.x, y}; pts[12] = {x + rx, y}; //cubic
                pts[13] = {x + w - rx, y}; //line
                pts[14] = {x + w - rx + hr.x, y}; pts[15] = {x + w, y + ry - hr.y}; pts[16] = {x + w, y + ry}; //cubic
            } else {
                cmds[1] = cmds[3] = cmds[5] = cmds[7] = PathCommand::CubicTo;
                cmds[2] = cmds[4] = cmds[6] = cmds[8] = PathCommand::LineTo;

                pts[1] = {x + w, y + ry - hr.y}; pts[2] = {x + w - rx + hr.x, y}; pts[3] = {x + w - rx, y}; //cubic
                pts[4] = {x + rx, y}; //line
                pts[5] = {x + rx - hr.x, y}; pts[6] = {x, y + ry - hr.y}; pts[7] = {x, y + ry}; //cubic
                pts[8] = {x, y + h - ry}; //line
                pts[9] = {x, y + h - ry + hr.y}; pts[10] = {x + rx - hr.x, y + h}; pts[11] = {x + rx, y + h}; //cubic
                pts[12] = {x + w - rx, y + h}; //line
                pts[13] = {x + w - rx + hr.x, y + h}; pts[14] = {x + w, y + h - ry + hr.y}; pts[15] = {x + w, y + h - ry}; //cubic
                pts[16] = {x + w, y + ry}; //line
            }

            rs.path.cmds.count += 10;
            rs.path.pts.count += 17;
        }
        impl.mark(RenderUpdateFlag::Path);
    }

    Paint* duplicate(Paint* ret)
    {
        auto shape = static_cast<Shape*>(ret);
        if (shape) shape->reset();
        else shape = Shape::gen();

        auto dup = SHAPE(shape);
        delete(dup->rs.fill);

        //Default Properties
        dup->impl.mark(RenderUpdateFlag::All);
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
        PAINT(this)->reset();
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
