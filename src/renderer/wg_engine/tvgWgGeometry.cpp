/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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

#include "tvgWgGeometry.h"

void WgVertexList::computeTriFansIndexes()
{
    assert(mVertexList.count > 2);
    mIndexList.reserve((mVertexList.count - 2) * 3);
    for (size_t i = 0; i < mVertexList.count - 2; i++) {
        mIndexList.push(0);
        mIndexList.push(i + 1);
        mIndexList.push(i + 2);
    }
}


void WgVertexList::appendCubic(WgPoint p1, WgPoint p2, WgPoint p3)
{
    WgPoint p0 = mVertexList.count > 0 ? mVertexList.last() : WgPoint(0.0f, 0.0f);
    const size_t segs = 16;
    for (size_t i = 1; i <= segs; i++) {
        float t = i / (float)segs;
        // get cubic spline interpolation coefficients
        float t0 = 1 * (1.0f - t) * (1.0f - t) * (1.0f - t);
        float t1 = 3 * (1.0f - t) * (1.0f - t) * t;
        float t2 = 3 * (1.0f - t) * t * t;
        float t3 = 1 * t * t * t;
        mVertexList.push(p0 * t0 + p1 * t1 + p2 * t2 + p3 * t3);
    }
}


void WgVertexList::appendRect(WgPoint p0, WgPoint p1, WgPoint p2, WgPoint p3)
{
    uint32_t index = mVertexList.count;
    mVertexList.push(p0); // +0
    mVertexList.push(p1); // +1
    mVertexList.push(p2); // +2
    mVertexList.push(p3); // +3
    mIndexList.push(index + 0);
    mIndexList.push(index + 1);
    mIndexList.push(index + 2);
    mIndexList.push(index + 1);
    mIndexList.push(index + 3);
    mIndexList.push(index + 2);
}


// TODO: optimize vertex and index count 
void WgVertexList::appendCircle(WgPoint center, float radius)
{
    uint32_t index = mVertexList.count;
    uint32_t nSegments = 32;
    for (uint32_t i = 0; i < nSegments; i++) {
        float angle0 = (float)(i + 0) / nSegments * 3.141593f * 2.0f;
        float angle1 = (float)(i + 1) / nSegments * 3.141593f * 2.0f;
        WgPoint p0 = center + WgPoint(sin(angle0) * radius, cos(angle0) * radius);
        WgPoint p1 = center + WgPoint(sin(angle1) * radius, cos(angle1) * radius);
        mVertexList.push(center); // +0
        mVertexList.push(p0);     // +1
        mVertexList.push(p1);     // +2
        mIndexList.push(index + 0);
        mIndexList.push(index + 1);
        mIndexList.push(index + 2);
        index += 3;
    }
}


void WgVertexList::close()
{
    if (mVertexList.count > 1) {
        mVertexList.push(mVertexList[0]);
    }
}