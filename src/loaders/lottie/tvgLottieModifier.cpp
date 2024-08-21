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

#include "tvgLottieModifier.h"
#include "tvgMath.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static void _roundCorner(Array<PathCommand>& cmds, Array<Point>& pts, const Point& prev, const Point& curr, const Point& next, float r)
{
    auto lenPrev = length(prev - curr);
    auto rPrev = lenPrev > 0.0f ? 0.5f * std::min(lenPrev * 0.5f, r) / lenPrev : 0.0f;
    auto lenNext = length(next - curr);
    auto rNext = lenNext > 0.0f ? 0.5f * std::min(lenNext * 0.5f, r) / lenNext : 0.0f;

    auto dPrev = rPrev * (curr - prev);
    auto dNext = rNext * (curr - next);

    pts.push(curr - 2.0f * dPrev);
    pts.push(curr - dPrev);
    pts.push(curr - dNext);
    pts.push(curr - 2.0f * dNext);
    cmds.push(PathCommand::LineTo);
    cmds.push(PathCommand::CubicTo);
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool LottieRoundnessModifier::modifyPath(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t inPtsCnt, Array<PathCommand>& outCmds, Array<Point>& outPts, Matrix* transform) const
{
    outCmds.reserve(inCmdsCnt * 2);
    outPts.reserve((uint32_t)(inPtsCnt * 1.5));
    auto ptsCnt = outPts.count;

    uint32_t startIndex = 0;
    for (uint32_t iCmds = 0, iPts = 0; iCmds < inCmdsCnt; ++iCmds) {
        switch (inCmds[iCmds]) {
            case PathCommand::MoveTo: {
                startIndex = outPts.count;
                outCmds.push(PathCommand::MoveTo);
                outPts.push(inPts[iPts++]);
                break;
            }
            case PathCommand::CubicTo: {
                auto& prev = inPts[iPts - 1];
                auto& curr = inPts[iPts + 2];
                if (iCmds < inCmdsCnt - 1 &&
                    tvg::zero(inPts[iPts - 1] - inPts[iPts]) &&
                    tvg::zero(inPts[iPts + 1] - inPts[iPts + 2])) {
                    if (inCmds[iCmds + 1] == PathCommand::CubicTo &&
                        tvg::zero(inPts[iPts + 2] - inPts[iPts + 3]) &&
                        tvg::zero(inPts[iPts + 4] - inPts[iPts + 5])) {
                        _roundCorner(outCmds, outPts, prev, curr, inPts[iPts + 5], r);
                        iPts += 3;
                        break;
                    } else if (inCmds[iCmds + 1] == PathCommand::Close) {
                        _roundCorner(outCmds, outPts, prev, curr, inPts[2], r);
                        outPts[startIndex] = outPts.last();
                        iPts += 3;
                        break;
                    }
                }
                outCmds.push(PathCommand::CubicTo);
                outPts.push(inPts[iPts++]);
                outPts.push(inPts[iPts++]);
                outPts.push(inPts[iPts++]);
                break;
            }
            case PathCommand::Close: {
                outCmds.push(PathCommand::Close);
                break;
            }
            default: break;
        }
    }
    if (transform) {
        for (auto i = ptsCnt; i < outPts.count; ++i) {
            outPts[i] *= *transform;
        }
    }
    return true;
}


bool LottieRoundnessModifier::modifyPolystar(TVG_UNUSED const Array<PathCommand>& inCmds, const Array<Point>& inPts, Array<PathCommand>& outCmds, Array<Point>& outPts, float outerRoundness, bool hasRoundness) const
{
    static constexpr auto ROUNDED_POLYSTAR_MAGIC_NUMBER = 0.47829f;

    auto len = length(inPts[1] - inPts[2]);
    auto r = len > 0.0f ? ROUNDED_POLYSTAR_MAGIC_NUMBER * std::min(len * 0.5f, this->r) / len : 0.0f;

    if (hasRoundness) {
        outCmds.grow((uint32_t)(1.5 * inCmds.count));
        outPts.grow((uint32_t)(4.5 * inCmds.count));

        int start = 3 * tvg::zero(outerRoundness);
        outCmds.push(PathCommand::MoveTo);
        outPts.push(inPts[start]);

        for (uint32_t i = 1 + start; i < inPts.count; i += 6) {
            auto& prev = inPts[i];
            auto& curr = inPts[i + 2];
            auto& next = (i < inPts.count - start) ? inPts[i + 4] : inPts[2];
            auto& nextCtrl = (i < inPts.count - start) ? inPts[i + 5] : inPts[3];
            auto dNext = r * (curr - next);
            auto dPrev = r * (curr - prev);

            auto p0 = curr - 2.0f * dPrev;
            auto p1 = curr - dPrev;
            auto p2 = curr - dNext;
            auto p3 = curr - 2.0f * dNext;

            outCmds.push(PathCommand::CubicTo);
            outPts.push(prev); outPts.push(p0); outPts.push(p0);
            outCmds.push(PathCommand::CubicTo);
            outPts.push(p1); outPts.push(p2); outPts.push(p3);
            outCmds.push(PathCommand::CubicTo);
            outPts.push(p3); outPts.push(next); outPts.push(nextCtrl);
        }
    } else {
        outCmds.grow(2 * inCmds.count);
        outPts.grow(4 * inCmds.count);

        auto dPrev = r * (inPts[1] - inPts[0]);
        auto p = inPts[0] + 2.0f * dPrev;
        outCmds.push(PathCommand::MoveTo);
        outPts.push(p);

        for (uint32_t i = 1; i < inPts.count; ++i) {
            auto& curr = inPts[i];
            auto& next = (i == inPts.count - 1) ? inPts[1] : inPts[i + 1];
            auto dNext = r * (curr - next);

            auto p0 = curr - 2.0f * dPrev;
            auto p1 = curr - dPrev;
            auto p2 = curr - dNext;
            auto p3 = curr - 2.0f * dNext;

            outCmds.push(PathCommand::LineTo);
            outPts.push(p0);
            outCmds.push(PathCommand::CubicTo);
            outPts.push(p1); outPts.push(p2); outPts.push(p3);

            dPrev = -1.0f * dNext;
        }
    }
    outCmds.push(PathCommand::Close);
    return true;
}


bool LottieRoundnessModifier::modifyRect(const Point& size, float& r) const
{
    r = std::min(this->r, std::max(size.x, size.y) * 0.5f);
    return true;
}