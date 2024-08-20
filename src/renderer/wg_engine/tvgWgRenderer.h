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
    RenderData prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper) override;
    RenderData prepare(Surface* surface, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) override;
    bool preRender() override;
    bool renderShape(RenderData data) override;
    bool renderImage(RenderData data) override;
    bool postRender() override;
    void dispose(RenderData data) override;
    RenderRegion region(RenderData data) override;
    RenderRegion viewport() override;
    bool viewport(const RenderRegion& vp) override;
    bool blend(BlendMethod method, bool direct) override;
    ColorSpace colorSpace() override;
    const Surface* mainSurface() override;

    bool clear() override;
    bool sync() override;

    bool target(WGPUInstance instance, WGPUSurface surface, uint32_t w, uint32_t h);

    Compositor* target(const RenderRegion& region, ColorSpace cs) override;
    bool beginComposite(Compositor* cmp, CompositeMethod method, uint8_t opacity) override;
    bool endComposite(Compositor* cmp) override;

    static WgRenderer* gen();
    static bool init(uint32_t threads);
    static bool term();
private:
    // render handles
    WGPUCommandEncoder mCommandEncoder{};
    WgRenderStorage mRenderTarget;
    WgRenderStorage mRenderStorageRoot;
    WgRenderStorage mRenderStorageScreen;
    WgRenderStoragePool mRenderStoragePool;
    WgBindGroupOpacityPool mOpacityPool;
    WgBindGroupBlendMethodPool mBlendMethodPool;
    WgBindGroupCompositeMethodPool mCompositeMethodPool;
    WgRenderDataShapePool mRenderDataShapePool;

    // render tree stacks
    Array<Compositor*> mCompositorStack;
    Array<WgRenderStorage*> mRenderStorageStack;

    // native window handles
    WgContext mContext;
    WgPipelines mPipelines;
    Surface mTargetSurface;
    BlendMethod mBlendMethod{};
};

#endif /* _TVG_WG_RENDERER_H_ */
