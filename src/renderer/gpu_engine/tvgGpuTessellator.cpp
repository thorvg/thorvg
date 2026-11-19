/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

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

#include "tvgGpuTessellator.h"

namespace tvg
{

// Reserve a whole primitive at once and keep growth amortized across curves.
template<typename T>
static inline void _growFillBuffer(Array<T>& buffer, uint32_t extra)
{
    if (extra <= buffer.reserved - buffer.count) return;
    auto slack = std::min(buffer.count / 2, UINT32_MAX - buffer.count);
    buffer.grow(std::max(extra, slack));
}

GpuFillResult gpuTessellateFill(const RenderPath& path, Array<Point>& vertices, Array<uint32_t>& indices)
{
    GpuFillResult result;
    if (path.pts.count <= 2) return result;

    vertices.reserve(path.pts.count);
    indices.reserve((path.pts.count - 2) * 3);

    GpuConvexProbe probe;
    uint32_t firstIndex = 0;
    uint32_t prevIndex = 0;
    Point firstPt = {};
    Point prevPt = {};
    bool contourClosed = false;

    auto pushVertex = [&](const Point& pt) {
        auto index = vertices.count;
        vertices.data[vertices.count++] = pt;
        if (index == 0) result.bbox.min = result.bbox.max = pt;
        else {
            result.bbox.min = tvg::min(result.bbox.min, pt);
            result.bbox.max = tvg::max(result.bbox.max, pt);
        }
        return index;
    };

    auto addEdge = [&](const Point& pt) {
        probe.addEdge(pt - prevPt);
        auto index = pushVertex(pt);
        if (prevIndex != 0) {
            indices.data[indices.count++] = firstIndex;
            indices.data[indices.count++] = prevIndex;
            indices.data[indices.count++] = index;
        }
        prevIndex = index;
        prevPt = pt;
    };

    auto finishContour = [&]() {
        if (prevIndex == 0 || contourClosed) return;
        probe.addContourClose(firstPt - prevPt);
        contourClosed = true;
    };

    auto pts = path.pts.data;
    for (auto cmd : path.cmds) {
        switch (cmd) {
            case PathCommand::MoveTo: {
                finishContour();
                probe.nextContour();
                _growFillBuffer(vertices, 1);
                firstIndex = pushVertex(*pts);
                firstPt = prevPt = *pts++;
                prevIndex = 0;
                contourClosed = false;
            } break;
            case PathCommand::LineTo: {
                _growFillBuffer(vertices, 1);
                _growFillBuffer(indices, prevIndex ? 3 : 0);
                addEdge(*pts++);
            } break;
            case PathCommand::CubicTo: {
                Bezier curve{pts[-1], pts[0], pts[1], pts[2]};
                if (probe.convex && gpuEdgesCross(curve.start, curve.ctrl1, curve.ctrl2, curve.end)) probe.convex = false;

                auto count = std::max(2u, curve.segments());
                _growFillBuffer(vertices, count);
                _growFillBuffer(indices, count * 3);
                float step = 1.f / count;
                for (uint32_t i = 1; i <= count; ++i) {
                    addEdge(curve.at(step * i));
                }
                prevPt = curve.end;
                pts += 3;
            } break;
            case PathCommand::Close: {
                finishContour();
            } break;
        }
    }
    finishContour();
    result.convex = probe.convex;
    return result;
}

}  // namespace tvg
