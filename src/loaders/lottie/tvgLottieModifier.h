/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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

#ifndef _TVG_LOTTIE_MODIFIER_H_
#define _TVG_LOTTIE_MODIFIER_H_

#include "tvgCommon.h"
#include "tvgArray.h"
#include "tvgMath.h"
#include "tvgRender.h"

struct LottieModifier
{
    enum Type : uint8_t {Roundness = 0, Offset};

    LottieModifier* next = nullptr;
    RenderPath* buffer;
    Type type;

    LottieModifier(RenderPath* buffer, Type type) :
        buffer(buffer), type(type)
    {
    }

    virtual ~LottieModifier()
    {
        delete (next);
    }

    LottieModifier* decorate(LottieModifier* next)
    {
        this->next = next;
        return this;
    }

    virtual void path(PathCommand* inCmds, uint32_t inCmdsCnt, Point* inPts, uint32_t inPtsCnt, Matrix* transform, RenderPath& out) = 0;
    virtual void polystar(RenderPath& in, RenderPath& out, float outerRoundness, bool hasRoundness) = 0;
    virtual void rect(RenderPath& in, RenderPath& out, const Point& pos, const Point& size, float r, bool clockwise) = 0;
    virtual void ellipse(RenderPath& in, RenderPath& out, const Point& center, const Point& radius, bool clockwise) = 0;
};

struct LottieRoundnessModifier : LottieModifier
{
    static constexpr float ROUNDNESS_EPSILON = 1.0f;
    float r;

    LottieRoundnessModifier(RenderPath* buffer, float r) :
        LottieModifier(buffer, Roundness), r(r) {}

    void path(PathCommand* inCmds, uint32_t inCmdsCnt, Point* inPts, uint32_t inPtsCnt, Matrix* transform, RenderPath& out) override;
    void polystar(RenderPath& in, RenderPath& out, float outerRoundness, bool hasRoundness) override;
    void rect(RenderPath& in, RenderPath& out, const Point& pos, const Point& size, float r, bool clockwise) override;
    void ellipse(RenderPath& in, RenderPath& out, const Point& center, const Point& radius, bool clockwise) override;

private:
    RenderPath& modify(PathCommand* inCmds, uint32_t inCmdsCnt, Point* inPts, uint32_t inPtsCnt, Matrix* transform, RenderPath& out);
    Point rounding(RenderPath& out, Point& prev, Point& curr, Point& next, float r);
};


struct LottieOffsetModifier : LottieModifier
{
    float offset;
    float miterLimit;
    StrokeJoin join;

    LottieOffsetModifier(RenderPath* buffer, float offset, float miter = 4.0f, StrokeJoin join = StrokeJoin::Round) :
        LottieModifier(buffer, Offset), offset(offset), miterLimit(miter), join(join) {}

    void path(PathCommand* inCmds, uint32_t inCmdsCnt, Point* inPts, uint32_t inPtsCnt, Matrix* transform, RenderPath& out) override;
    void polystar(RenderPath& in, RenderPath& out, float outerRoundness, bool hasRoundness) override;
    void rect(RenderPath& in, RenderPath& out, const Point& pos, const Point& size, float r, bool clockwise) override;
    void ellipse(RenderPath& in, RenderPath& out, const Point& center, const Point& radius, bool clockwise) override;

private:
    struct State
    {
        Line line{};
        Line firstLine{};
        uint32_t movetoOutIndex = 0;
        uint32_t movetoInIndex = 0;
        bool moveto = false;
    };

    RenderPath& modify(PathCommand* inCmds, uint32_t inCmdsCnt, Point* inPts, uint32_t inPtsCnt, Matrix* transform, RenderPath& out);
    bool intersected(Line& line1, Line& line2, Point& intersection, bool& inside);
    Line shift(Point& p1, Point& p2, float offset);
    void line(RenderPath& out, PathCommand* inCmds, uint32_t inCmdsCnt, Point* inPts, uint32_t& curPt, uint32_t curCmd, State& state, float offset, bool degenerated);
    void corner(RenderPath& out, Line& line, Line& nextLine, uint32_t movetoIndex, bool nextClose);
};

#endif