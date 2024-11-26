/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

void WgRenderStorage::initialize(WgContext& context, uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
    texture = context.createTexStorage(width, height, WGPUTextureFormat_RGBA8Unorm);
    textureMS = context.createTexAttachement(width, height, WGPUTextureFormat_RGBA8Unorm, 4);
    texView = context.createTextureView(texture);
    texViewMS = context.createTextureView(textureMS);
    bindGroupRead = context.layouts.createBindGroupStrorage1RO(texView);
    bindGroupWrite = context.layouts.createBindGroupStrorage1WO(texView);
    bindGroupTexure = context.layouts.createBindGroupTexSampled(context.samplerNearestRepeat, texView);
}


void WgRenderStorage::release(WgContext& context)
{
    context.layouts.releaseBindGroup(bindGroupTexure);
    context.layouts.releaseBindGroup(bindGroupWrite);
    context.layouts.releaseBindGroup(bindGroupRead);
    context.releaseTextureView(texViewMS);
    context.releaseTexture(textureMS);
    context.releaseTextureView(texView);
    context.releaseTexture(texture);
    height = 0;
    width = 0;
}

//*****************************************************************************
// render storage pool
//*****************************************************************************

WgRenderStorage* WgRenderStoragePool::allocate(WgContext& context)
{
    WgRenderStorage* renderStorage{};
    if (pool.count > 0) {
        renderStorage = pool.last();
        pool.pop();
    } else {
        renderStorage = new WgRenderStorage;
        renderStorage->initialize(context, width, height);
        list.push(renderStorage);
    }
    return renderStorage;
};


void WgRenderStoragePool::free(WgContext& context, WgRenderStorage* renderStorage)
{
    pool.push(renderStorage);
};


void WgRenderStoragePool::initialize(WgContext& context, uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
}


void WgRenderStoragePool::release(WgContext& context)
{
    for (uint32_t i = 0; i < list.count; i++) {
       list[i]->release(context);
       delete list[i];
    }
    list.clear();
    pool.clear();
    height = 0;
    width = 0;
};
