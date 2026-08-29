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

static inline bool eligible(const WgRenderShape* rdata, BlendMethod blendMethod)
{
    if (blendMethod != BlendMethod::Normal) return false;
    if (!rdata->shape.setting.valid || rdata->shape.setting.fillType != WgRenderSettingsType::Solid) return false;
    if (!rdata->convex || rdata->viewport.invalid() || !rdata->clips.empty()) return false;
    if (rdata->shape.mesh.vbuffer.empty() || rdata->shape.mesh.ibuffer.empty()) return false;
    if (rdata->stroke.setting.valid && !rdata->stroke.mesh.ibuffer.empty()) return false;
    return true;
}

static inline bool appendable(WgSceneTask* batchSceneTask, WgRenderTask* batchTask, const RenderRegion& batchViewport, WgSceneTask* sceneTask, const WgRenderShape* rdata, const Array<WgRenderTask*>& renderTaskList)
{
    // Any task submitted after the candidate is an implicit batch boundary.
    if (batchSceneTask != sceneTask) return false;
    if (sceneTask->children.last() != batchTask) return false;
    if (renderTaskList.last() != batchTask) return false;
    if (!(batchViewport == rdata->viewport)) return false;
    return true;
}

static inline WgRenderTask* emitSingle(WgSceneTask* sceneTask, WgRenderShape* rdata, Array<WgRenderTask*>& renderTaskList)
{
    auto task = new WgPaintTask(rdata, BlendMethod::Normal);
    sceneTask->children.push(task);
    renderTaskList.push(task);
    return task;
}

static inline WgRenderTask* promote(WgSceneTask* sceneTask, WgRenderTask* task, WgRenderShape* first, WgRenderShape* rdata, Array<WgRenderTask*>& renderTaskList)
{
    // Tasks are staged only after the tree is complete, so replacing its tail is safe.
    auto batchTask = new WgBatchTask(first, rdata, false);
    sceneTask->children.last() = batchTask;
    renderTaskList.last() = batchTask;
    delete task;

    return batchTask;
}

static inline void append(WgRenderTask* task, WgRenderShape* rdata)
{
    static_cast<WgBatchTask*>(task)->shapes.push(rdata);
}

bool WgSolidBatch::draw(WgSceneTask* sceneTask, WgRenderShape* rdata, BlendMethod blendMethod, Array<WgRenderTask*>& renderTaskList)
{
    if (!eligible(rdata, blendMethod)) return false;

    if (!appendable(this->sceneTask, task, viewport, sceneTask, rdata, renderTaskList)) {
        task = emitSingle(sceneTask, rdata, renderTaskList);
        this->sceneTask = sceneTask;
        first = rdata;
        viewport = rdata->viewport;
        return true;
    }

    if (first) {
        task = promote(this->sceneTask, task, first, rdata, renderTaskList);
        first = nullptr;
    } else append(task, rdata);
    return true;
}
