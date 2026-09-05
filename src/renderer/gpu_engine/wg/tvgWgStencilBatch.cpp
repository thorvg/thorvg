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

#include "tvgMath.h"
#include "tvgWgStencilBatch.h"

static bool eligible(const WgRenderShape* rdata, BlendMethod blendMethod, RenderRegion& bounds)
{
    if (blendMethod != BlendMethod::Normal || !rdata->shape.setting.valid) return false;

    if (rdata->convex || rdata->viewport.invalid() || !rdata->clips.empty()) return false;
    if (rdata->shape.mesh.vbuffer.empty() || rdata->shape.mesh.ibuffer.empty() || rdata->meshBBox.vbuffer.empty() || rdata->meshBBox.ibuffer.empty()) return false;
    if (rdata->stroke.setting.valid && !rdata->stroke.mesh.ibuffer.empty()) return false;

    // Clip finite geometry bounds to the viewport before rounding to the integer overlap region.
    const auto& aabb = rdata->aabb;
    if (!std::isfinite(aabb.min.x) || !std::isfinite(aabb.min.y) || !std::isfinite(aabb.max.x) || !std::isfinite(aabb.max.y)) return false;

    const auto minX = tvg::clamp(static_cast<double>(aabb.min.x), static_cast<double>(rdata->viewport.min.x), static_cast<double>(rdata->viewport.max.x));
    const auto minY = tvg::clamp(static_cast<double>(aabb.min.y), static_cast<double>(rdata->viewport.min.y), static_cast<double>(rdata->viewport.max.y));
    const auto maxX = tvg::clamp(static_cast<double>(aabb.max.x), static_cast<double>(rdata->viewport.min.x), static_cast<double>(rdata->viewport.max.x));
    const auto maxY = tvg::clamp(static_cast<double>(aabb.max.y), static_cast<double>(rdata->viewport.min.y), static_cast<double>(rdata->viewport.max.y));
    if (maxX <= minX || maxY <= minY) return false;

    bounds = {{static_cast<int32_t>(std::floor(minX)), static_cast<int32_t>(std::floor(minY))}, {static_cast<int32_t>(std::ceil(maxX)), static_cast<int32_t>(std::ceil(maxY))}};

    return true;
}

static bool intersects(const WgStencilBatch& batch, const RenderRegion& bounds)
{
    const auto min = batch.ySorted ? bounds.min.y : bounds.min.x;
    const auto max = batch.ySorted ? bounds.max.y : bounds.max.x;
    ARRAY_FOREACH(p, batch.bounds) {
        const auto pMin = batch.ySorted ? (*p).min.y : (*p).min.x;
        if ((batch.ySorted ? (*p).max.y : (*p).max.x) <= min) continue;
        if (pMin >= max) break;
        if ((*p).intersected(bounds)) return true;
    }
    return false;
}

static void addBounds(WgStencilBatch& batch, const RenderRegion& bounds)
{
    auto p = batch.bounds.count;
    const auto min = batch.ySorted ? bounds.min.y : bounds.min.x;
    batch.bounds.push(bounds);
    while (p > 0 && (batch.ySorted ? batch.bounds[p - 1].min.y : batch.bounds[p - 1].min.x) > min) {
        batch.bounds[p] = batch.bounds[p - 1];
        --p;
    }
    batch.bounds[p] = bounds;
}

static bool appendable(const WgStencilBatch& batch, WgSceneTask* sceneTask, WgRenderShape* rdata, const RenderRegion& bounds, const Array<WgRenderTask*>& renderTaskList)
{
    if (batch.sceneTask != sceneTask || sceneTask->children.last() != batch.task || renderTaskList.last() != batch.task) return false;
    if (!(batch.viewport == rdata->viewport) || batch.fillRule != rdata->fillRule) return false;
    return !intersects(batch, bounds);
}

static void emitSingle(WgStencilBatch& batch, WgSceneTask* sceneTask, WgRenderShape* rdata, const RenderRegion& bounds, Array<WgRenderTask*>& renderTaskList)
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

static void promote(WgStencilBatch& batch, WgRenderShape* rdata, const RenderRegion& bounds, Array<WgRenderTask*>& renderTaskList)
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

static void append(WgStencilBatch& batch, WgRenderShape* rdata, const RenderRegion& bounds)
{
    assert(batch.sceneTask && !batch.first && batch.task);
    static_cast<WgBatchTask*>(batch.task)->shapes.push(rdata);
    addBounds(batch, bounds);
}

bool WgStencilBatch::draw(WgSceneTask* sceneTask, WgRenderShape* rdata, BlendMethod blendMethod, Array<WgRenderTask*>& renderTaskList)
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
