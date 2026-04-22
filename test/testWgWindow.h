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

#ifndef _TVG_TEST_WG_WINDOW_H_
#define _TVG_TEST_WG_WINDOW_H_

#include <memory>
#include <thorvg.h>
#include "config.h"

#ifdef THORVG_WG_TEST_SUPPORT
    #include <webgpu/webgpu.h>
#endif

#if defined(THORVG_WG_ENGINE_SUPPORT) && defined(THORVG_WG_TEST_SUPPORT)

struct WgTestContext
{
    WGPUInstance instance = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
    WGPUTextureFormat format = WGPUTextureFormat_BGRA8Unorm;
};

struct WgWindow
{
    WgTestContext context;
    WGPUTexture texture = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;

    ~WgWindow();

    bool init(uint32_t w, uint32_t h);
    tvg::Result target(tvg::WgCanvas* canvas);
    bool hasRenderedContent();

private:
    bool recreateTarget();
    void clear();
    void releaseTexture();
};

std::unique_ptr<WgWindow> genWgWindow(uint32_t w, uint32_t h);

#endif

#endif
