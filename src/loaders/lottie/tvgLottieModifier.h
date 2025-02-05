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

struct LottieRoundnessModifier
{
    static constexpr float ROUNDNESS_EPSILON = 1.0f;
    float r;

    LottieRoundnessModifier(float r) : r(r) {};

    bool modifyPath(PathCommand* inCmds, uint32_t inCmdsCnt, Point* inPts, uint32_t inPtsCnt, Matrix* transform, RenderPath& out) const;
    bool modifyPolystar(RenderPath& in, RenderPath& out, float outerRoundness, bool hasRoundness) const;
    bool modifyRect(Point& size, float& r) const;
};


struct LottieOffsetModifier
{
    float offset;
    float miterLimit;
    StrokeJoin join;

    LottieOffsetModifier(float offset, float miter = 4.0f, StrokeJoin join = StrokeJoin::Round) : offset(offset), miterLimit(miter), join(join) {}

    bool modifyPath(PathCommand* inCmds, uint32_t inCmdsCnt, Point* inPts, uint32_t inPtsCnt, RenderPath& out) const;
    bool modifyPolystar(RenderPath& in, RenderPath& out) const;
    bool modifyRect(PathCommand* inCmds, uint32_t inCmdsCnt, Point* inPts, uint32_t inPtsCnt, RenderPath& out) const;
    bool modifyEllipse(Point& radius) const;

private:
    struct State
    {
        Line line{};
        Line firstLine{};
        bool moveto = false;
        uint32_t movetoOutIndex = 0;
        uint32_t movetoInIndex = 0;
    };

    void line(RenderPath& out, PathCommand* inCmds, uint32_t inCmdsCnt, Point* inPts, uint32_t& curPt, uint32_t curCmd, State& state, float offset, bool degenerated) const;
    void corner(RenderPath& out, Line& line, Line& nextLine, uint32_t movetoIndex, bool nextClose) const;
};

#endif