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

#ifndef _TVG_WG_RENDER_TARGET_H_
#define _TVG_WG_RENDER_TARGET_H_

#include "tvgWgPipelines.h"
#include "tvgRender.h"

struct WgRenderTarget
{
    WGPUTexture texture{}, textureMS{};
    WGPUTextureView texView{}, texViewMS{};
    WGPUBindGroup bgRead{}, bgWrite{};
    WGPUBindGroup bgTexture{};        // Nearest + repeat for general compositing and pixel-aligned effects.
    WGPUBindGroup bgTextureLinear{};  // Linear + clamp for subpixel sampling, such as motion blur.
    uint32_t width{}, height{};

    void initialize(WgContext& context, uint32_t width, uint32_t height);
    WGPUBindGroup getBindGroupTextureLinear(WgContext& context);
    void release(WgContext& context);
};

struct WgRenderTargetPool
{
    Array<WgRenderTarget*> list, pool;
    uint32_t width{}, height{};

    WgRenderTarget* allocate(WgContext& context);
    void initialize(uint32_t width, uint32_t height);
    void release(WgContext& context);
};

#endif // _TVG_WG_RENDER_TARGET_H_
