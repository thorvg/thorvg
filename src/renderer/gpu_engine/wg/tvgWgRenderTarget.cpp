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

#include "tvgWgRenderTarget.h"

static uint32_t _renderTargetSize(uint32_t value, uint32_t limit)
{
    uint32_t size = 1;
    while (size < value && size < limit) {
        if (size > limit / 2) return limit;
        size <<= 1;
    }
    return size < limit ? size : limit;
}

void WgRenderTarget::initialize(WgContext& context, uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
    texture = context.createTexStorage(width, height, WGPUTextureFormat_RGBA8Unorm);
    textureMS = context.createTexAttachement(width, height, WGPUTextureFormat_RGBA8Unorm, 4);
    texView = context.createTextureView(texture);
    texViewMS = context.createTextureView(textureMS);
    bgRead = context.layouts.createBindGroupStrorage1RO(texView);
    bgWrite = context.layouts.createBindGroupStrorage1WO(texView);
    bgTexture = context.layouts.createBindGroupTexSampled(context.samplerNearestRepeat, texView);
}

WGPUBindGroup WgRenderTarget::getBindGroupTextureLinear(WgContext& context)
{
    if (!bgTextureLinear) {
        bgTextureLinear = context.layouts.createBindGroupTexSampled(context.samplerLinearClamp, texView);
    }
    return bgTextureLinear;
}

void WgRenderTarget::release(WgContext& context)
{
    context.layouts.releaseBindGroup(bgTextureLinear);
    context.layouts.releaseBindGroup(bgTexture);
    context.layouts.releaseBindGroup(bgWrite);
    context.layouts.releaseBindGroup(bgRead);
    context.releaseTextureView(texViewMS);
    context.releaseTexture(textureMS);
    context.releaseTextureView(texView);
    context.releaseTexture(texture);
    height = 0;
    width = 0;
}

//*****************************************************************************
// render target pool
//*****************************************************************************

WgRenderTarget* WgRenderTargetPool::allocate(WgContext& context, const RenderRegion& region)
{
    auto targetWidth = _renderTargetSize(region.sw() > 1 ? region.sw() : 1, width);
    auto targetHeight = _renderTargetSize(region.sh() > 1 ? region.sh() : 1, height);
    for (uint32_t i = 0; i < pool.count; ++i) {
        auto target = pool[i];
        if (target->width != targetWidth || target->height != targetHeight) continue;
        pool[i] = pool.last();
        pool.pop();
        return target;
    }

    // TODO: Add an eviction policy to release unused cached resources.
    auto renderTarget = new WgRenderTarget;
    renderTarget->initialize(context, targetWidth, targetHeight);
    list.push(renderTarget);

    return renderTarget;
}

void WgRenderTargetPool::initialize(uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
}

void WgRenderTargetPool::release(WgContext& context)
{
    ARRAY_FOREACH(p, list) {
       (*p)->release(context);
       delete(*p);
    }
    list.clear();
    pool.clear();
    height = 0;
    width = 0;
}
