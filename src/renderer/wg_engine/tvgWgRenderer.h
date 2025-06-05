/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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

#include "tvgWgRenderTask.h"

class WgRenderer : public RenderMethod
{
public:
    bool preUpdate() override;
    RenderData prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper) override;
    RenderData prepare(RenderSurface* surface, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) override;
    bool postUpdate() override;
    bool preRender() override;
    bool renderShape(RenderData data) override;
    bool renderImage(RenderData data) override;
    bool postRender() override;
    void dispose(RenderData data) override;
    RenderRegion region(RenderData data) override;
    void damage(const RenderRegion& region) override;
    RenderRegion viewport() override;
    bool viewport(const RenderRegion& vp) override;
    bool blend(BlendMethod method) override;
    ColorSpace colorSpace() override;
    const RenderSurface* mainSurface() override;

    bool clear() override;
    bool sync() override;

    bool target(WGPUDevice device, WGPUInstance instance, void* target, uint32_t width, uint32_t height, int type = 0);

    RenderCompositor* target(const RenderRegion& region, ColorSpace cs, CompositionFlag flags) override;
    bool beginComposite(RenderCompositor* cmp, MaskMethod method, uint8_t opacity) override;
    bool endComposite(RenderCompositor* cmp) override;

    void prepare(RenderEffect* effect, const Matrix& transform) override;
    bool region(RenderEffect* effect) override;
    bool render(RenderCompositor* cmp, const RenderEffect* effect, bool direct) override;
    void dispose(RenderEffect* effect) override;

    static WgRenderer* gen(uint32_t threads);
    static bool term();

private:
    WgRenderer();
    ~WgRenderer();
    void release();
    void disposeObjects();
    void releaseSurfaceTexture();

    void clearTargets();
    bool surfaceConfigure(WGPUSurface surface, WgContext& context, uint32_t width, uint32_t height);

    // render tree stacks
    WgRenderTarget mRenderTargetRoot;
    Array<WgCompose*> mCompositorList;
    Array<WgRenderTarget*> mRenderTargetStack;
    Array<WgRenderDataViewport*> mRenderDataViewportList;
    Array<WgSceneTask*> mSceneTaskStack;
    Array<WgRenderTask*> mRenderTaskList;

    // render target pool
    WgRenderTargetPool mRenderTargetPool;

    // render data paint pools
    WgRenderDataShapePool mRenderDataShapePool;
    WgRenderDataPicturePool mRenderDataPicturePool;
    WgRenderDataViewportPool mRenderDataViewportPool;
    WgRenderDataEffectParamsPool mRenderDataEffectParamsPool;

    // rendering context
    WgContext mContext;
    WgCompositor mCompositor;

    // rendering states
    RenderSurface mTargetSurface;
    BlendMethod mBlendMethod{};
    RenderRegion mViewport{};

    // disposable data list
    Array<RenderData> mDisposeRenderDatas{};
    Key mDisposeKey{};

    // gpu handles
    WGPUTexture targetTexture{}; // external handle
    WGPUSurfaceTexture surfaceTexture{};
    WGPUSurface surface{};  // external handle

    struct {
        WgGeometryBufferPool* pool;   //private buffer pool
        bool individual = false;      //buffer-pool sharing policy
    } mBufferPool;
};

#endif /* _TVG_WG_RENDERER_H_ */
