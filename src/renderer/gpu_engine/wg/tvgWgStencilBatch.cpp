/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

#include <cassert>
#include "tvgMath.h"
#include "tvgWgStencilBatch.h"

static bool eligible(const WgShape* rdata, BlendMethod blendMethod, RenderRegion& bounds)
{
    if (blendMethod != BlendMethod::Normal || !rdata->shape.setting.valid) return false;

    if (rdata->convex || rdata->viewport.invalid() || !rdata->clips.empty()) return false;
    if (rdata->shape.mesh.vbuffer.empty() || rdata->shape.mesh.ibuffer.empty() || rdata->bboxMesh.vbuffer.empty() || rdata->bboxMesh.ibuffer.empty()) return false;
    if (rdata->stroke.setting.valid && !rdata->stroke.mesh.ibuffer.empty()) return false;

    // Clip finite geometry bounds to the viewport before rounding to the integer overlap region.
    const auto& bbox = rdata->bbox;
    if (!std::isfinite(bbox.min.x) || !std::isfinite(bbox.min.y) || !std::isfinite(bbox.max.x) || !std::isfinite(bbox.max.y)) return false;

    const auto minX = tvg::clamp(static_cast<double>(bbox.min.x), static_cast<double>(rdata->viewport.xMin), static_cast<double>(rdata->viewport.xMax));
    const auto minY = tvg::clamp(static_cast<double>(bbox.min.y), static_cast<double>(rdata->viewport.yMin), static_cast<double>(rdata->viewport.yMax));
    const auto maxX = tvg::clamp(static_cast<double>(bbox.max.x), static_cast<double>(rdata->viewport.xMin), static_cast<double>(rdata->viewport.xMax));
    const auto maxY = tvg::clamp(static_cast<double>(bbox.max.y), static_cast<double>(rdata->viewport.yMin), static_cast<double>(rdata->viewport.yMax));
    if (maxX <= minX || maxY <= minY) return false;

    bounds = {static_cast<int32_t>(std::floor(minX)), static_cast<int32_t>(std::floor(minY)), static_cast<int32_t>(std::ceil(maxX)), static_cast<int32_t>(std::ceil(maxY))};

    return true;
}

static bool intersects(const WgStencilBatch& batch, const RenderRegion& bounds)
{
    const auto min = batch.ySorted ? bounds.yMin : bounds.xMin;
    const auto max = batch.ySorted ? bounds.yMax : bounds.xMax;
    ARRAY_FOREACH(p, batch.bounds) {
        const auto pMin = batch.ySorted ? (*p).yMin : (*p).xMin;
        if ((batch.ySorted ? (*p).yMax : (*p).xMax) <= min) continue;
        if (pMin >= max) break;
        if ((*p).intersected(bounds)) return true;
    }
    return false;
}

static void addBounds(WgStencilBatch& batch, const RenderRegion& bounds)
{
    auto p = batch.bounds.count;
    const auto min = batch.ySorted ? bounds.yMin : bounds.xMin;
    batch.bounds.push(bounds);
    while (p > 0 && (batch.ySorted ? batch.bounds[p - 1].yMin : batch.bounds[p - 1].xMin) > min) {
        batch.bounds[p] = batch.bounds[p - 1];
        --p;
    }
    batch.bounds[p] = bounds;
}

static bool appendable(const WgStencilBatch& batch, WgSceneTask* sceneTask, WgShape* rdata, const RenderRegion& bounds, const Array<WgRenderTask*>& renderTaskList)
{
    if (batch.sceneTask != sceneTask || sceneTask->children.last() != batch.task || renderTaskList.last() != batch.task) return false;
    if (!(batch.viewport == rdata->viewport) || batch.fillRule != rdata->fillRule) return false;
    return !intersects(batch, bounds);
}

static void emitSingle(WgStencilBatch& batch, WgSceneTask* sceneTask, WgShape* rdata, const RenderRegion& bounds, Array<WgRenderTask*>& renderTaskList)
{
    auto task = new WgPaintTask(rdata, BlendMethod::Normal);
    sceneTask->children.push(task);
    renderTaskList.push(task);

    batch.sceneTask = sceneTask;
    batch.task = task;
    batch.first = rdata;
    batch.viewport = rdata->viewport;
    batch.fillRule = rdata->fillRule;
    batch.ySorted = batch.viewport.sh() > batch.viewport.sw();
    batch.bounds.clear();

    addBounds(batch, bounds);
}

static void promote(WgStencilBatch& batch, WgShape* rdata, const RenderRegion& bounds, Array<WgRenderTask*>& renderTaskList)
{
    assert(batch.sceneTask && batch.first && batch.task);
    assert(batch.sceneTask->children.last() == batch.task && renderTaskList.last() == batch.task);

    auto batchTask = new WgBatchTask(batch.first, rdata, true);
    batch.sceneTask->children.last() = batchTask;
    renderTaskList.last() = batchTask;
    delete (batch.task);

    batch.task = batchTask;
    batch.first = nullptr;
    addBounds(batch, bounds);
}

static void append(WgStencilBatch& batch, WgShape* rdata, const RenderRegion& bounds)
{
    assert(batch.sceneTask && !batch.first && batch.task);
    static_cast<WgBatchTask*>(batch.task)->shapes.push(rdata);
    addBounds(batch, bounds);
}

bool WgStencilBatch::draw(WgSceneTask* sceneTask, WgShape* rdata, BlendMethod blendMethod, Array<WgRenderTask*>& renderTaskList)
{
    RenderRegion bounds;
    if (!eligible(rdata, blendMethod, bounds)) return false;

    if (!appendable(*this, sceneTask, rdata, bounds, renderTaskList)) {
        emitSingle(*this, sceneTask, rdata, bounds, renderTaskList);
        return true;
    }

    if (first) promote(*this, rdata, bounds, renderTaskList);
    else append(*this, rdata, bounds);
    return true;
}
