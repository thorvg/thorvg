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

#ifndef _TVG_WG_RENDERER_H_
#define _TVG_WG_RENDERER_H_

#include "tvgWgRenderTarget.h"

class WgRenderer : public RenderMethod
{
private:
    WgRenderer();
    ~WgRenderer();
    void initialize();
    void release();
public:
    RenderData prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper);
    RenderData prepare(const Array<RenderData>& scene, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags);
    RenderData prepare(Surface* surface, const RenderMesh* mesh, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags);
    bool preRender();
    bool renderShape(RenderData data);
    bool renderImage(RenderData data);
    bool postRender();
    bool dispose(RenderData data);
    RenderRegion region(RenderData data);
    RenderRegion viewport();
    bool viewport(const RenderRegion& vp);
    bool blend(BlendMethod method);
    ColorSpace colorSpace();

    bool clear();
    bool sync();

    bool target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h);
    bool target(void* window, uint32_t w, uint32_t h); // temporary solution
    Compositor* target(const RenderRegion& region, ColorSpace cs);
    bool beginComposite(Compositor* cmp, CompositeMethod method, uint8_t opacity);
    bool endComposite(Compositor* cmp);

    static WgRenderer* gen();
    static bool init(uint32_t threads);
    static bool term();
private:
    // render handles
    WGPUCommandEncoder mCommandEncoder{};
    Array<Compositor*> mCompositorStack;
    Array<WgRenderTarget*> mRenderTargetStack;

    // render object pools
    WgRenderTargetPool mRenderTargetPool;
    WgBindGroupOpacityPool mBindGroupOpacityPool;
private:
    WgContext mContext;
    WgPipelines mPipelines;
    WgRenderTarget mRenderTargetRoot;
    WgRenderTarget mRenderTargetWnd;
    WGPUSurface mSurface{};
    Surface mTargetSurface;
};

#endif /* _TVG_WG_RENDERER_H_ */
