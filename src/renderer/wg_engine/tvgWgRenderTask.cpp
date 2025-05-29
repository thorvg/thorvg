/*
 * Copyright (c) 2025 the ThorVG project. All rights reserved.

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

#include "tvgWgRenderTask.h"
#include <iostream>

//***********************************************************************
// WgPaintTask
//***********************************************************************

void WgPaintTask::run(WgContext& context, WgCompositor& compositor, WGPUCommandEncoder encoder)
{
    if (renderData->type() == tvg::Type::Shape)
        compositor.renderShape(context, (WgRenderDataShape*)renderData, blendMethod);
    if (renderData->type() == tvg::Type::Picture)
        compositor.renderImage(context, (WgRenderDataPicture*)renderData, blendMethod);
    else assert(true);
}

//***********************************************************************
// WgSceneTask
//***********************************************************************

void WgSceneTask::run(WgContext& context, WgCompositor& compositor, WGPUCommandEncoder encoder)
{
    // begin the render pass for the current scene and clear the target content
    WGPUColor color{};
    if ((compose->method == MaskMethod::None) && (compose->blend != BlendMethod::Normal)) color = { 1.0, 1.0, 1.0, 0.0 };
    compositor.beginRenderPass(encoder, renderTarget, true, color);
    // run all childs (scenes and shapes)
    runChildren(context, compositor, encoder);
    // we must to end current render pass for current scene
    compositor.endRenderPass();
    // we must to apply effect for current scene
    if (effect)
        runEffect(context, compositor, encoder);
    // there's no point in continuing if the scene has no destination target (e.g., the root scene)
    if (!renderTargetDst) return;
    // apply scene blending
    if (compose->method == MaskMethod::None) {
        compositor.beginRenderPass(encoder, renderTargetDst, false);
        compositor.renderScene(context, renderTarget, compose);
    // apply scene composition (for scenes, that have a handle to mask)
    } else if (renderTargetMsk) {
        compositor.beginRenderPass(encoder, renderTargetDst, false);
        compositor.composeScene(context, renderTarget, renderTargetMsk, compose);
    }
}


void WgSceneTask::runChildren(WgContext& context, WgCompositor& compositor, WGPUCommandEncoder encoder)
{
    ARRAY_FOREACH(task, children) {
        WgRenderTask* renderTask = *task;
        // we need to restore current render pass without clear
        compositor.beginRenderPass(encoder, renderTarget, false);
        // run children (shape or scene)
        renderTask->run(context, compositor, encoder);
    }
}


void WgSceneTask::runEffect(WgContext& context, WgCompositor& compositor, WGPUCommandEncoder encoder)
{
    assert(effect);
    switch (effect->type) {
        case SceneEffect::GaussianBlur: compositor.gaussianBlur(context, renderTarget, (RenderEffectGaussianBlur*)effect, compose); break;
        case SceneEffect::DropShadow: compositor.dropShadow(context, renderTarget, (RenderEffectDropShadow*)effect, compose); break;
        case SceneEffect::Fill: compositor.fillEffect(context, renderTarget, (RenderEffectFill*)effect, compose); break;
        case SceneEffect::Tint: compositor.tintEffect(context, renderTarget, (RenderEffectTint*)effect, compose); break;
        case SceneEffect::Tritone : compositor.tritoneEffect(context, renderTarget, (RenderEffectTritone*)effect, compose); break;
        default: break;
    }
}
