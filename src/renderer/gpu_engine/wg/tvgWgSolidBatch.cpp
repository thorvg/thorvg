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

#include "tvgWgSolidBatch.h"

struct WgSolidBatchTask : WgRenderTask
{
    Array<WgRenderDataShape*> shapes;
    WgSolidBatchRange range{};

    WgSolidBatchTask(WgRenderDataShape* first, WgRenderDataShape* second) : shapes{2}
    {
        shapes.push(first);
        shapes.push(second);
    }

    void stage(WgCompositor& compositor) override
    {
        compositor.requestSolidBatch(shapes, range);
    }

    void run(WgContext&, WgCompositor& compositor, WGPUCommandEncoder) override
    {
        compositor.renderSolidBatch(range);
    }
};

static inline bool eligible(const WgRenderDataShape* renderData, BlendMethod blendMethod)
{
    if (blendMethod != BlendMethod::Normal) return false;
    if (renderData->renderSettingsShape.skip || renderData->renderSettingsShape.fillType != WgRenderSettingsType::Solid) return false;
    if (!renderData->convex || renderData->viewport.invalid() || !renderData->clips.empty()) return false;
    if (renderData->meshShape.vbuffer.empty() || renderData->meshShape.ibuffer.empty()) return false;
    if (!renderData->renderSettingsStroke.skip && !renderData->meshStrokes.ibuffer.empty()) return false;
    return true;
}

static inline bool appendable(WgSceneTask* batchSceneTask, WgRenderTask* batchTask, const RenderRegion& batchViewport, WgSceneTask* sceneTask, const WgRenderDataShape* renderData, const Array<WgRenderTask*>& renderTaskList)
{
    // Any task submitted after the candidate is an implicit batch boundary.
    if (batchSceneTask != sceneTask) return false;
    if (sceneTask->children.last() != batchTask) return false;
    if (renderTaskList.last() != batchTask) return false;
    if (!(batchViewport == renderData->viewport)) return false;
    return true;
}

static inline WgRenderTask* emitSingle(WgSceneTask* sceneTask, WgRenderDataShape* renderData, Array<WgRenderTask*>& renderTaskList)
{
    auto task = new WgPaintTask(renderData, BlendMethod::Normal);
    sceneTask->children.push(task);
    renderTaskList.push(task);
    return task;
}

static inline WgRenderTask* promote(WgSceneTask* sceneTask, WgRenderTask* task, WgRenderDataShape* first, WgRenderDataShape* renderData, Array<WgRenderTask*>& renderTaskList)
{
    // Tasks are staged only after the tree is complete, so replacing its tail is safe.
    auto batchTask = new WgSolidBatchTask(first, renderData);
    sceneTask->children.last() = batchTask;
    renderTaskList.last() = batchTask;
    delete task;

    return batchTask;
}

static inline void append(WgRenderTask* task, WgRenderDataShape* renderData)
{
    static_cast<WgSolidBatchTask*>(task)->shapes.push(renderData);
}

bool WgSolidBatch::draw(WgSceneTask* sceneTask, WgRenderDataShape* renderData, BlendMethod blendMethod, Array<WgRenderTask*>& renderTaskList)
{
    if (!eligible(renderData, blendMethod)) return false;

    if (!appendable(this->sceneTask, task, viewport, sceneTask, renderData, renderTaskList)) {
        task = emitSingle(sceneTask, renderData, renderTaskList);
        this->sceneTask = sceneTask;
        first = renderData;
        viewport = renderData->viewport;
        return true;
    }

    if (first) {
        task = promote(this->sceneTask, task, first, renderData, renderTaskList);
        first = nullptr;
    } else append(task, renderData);
    return true;
}
