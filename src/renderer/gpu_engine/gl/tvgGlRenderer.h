/*
 * Copyright (c) 2020 - 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_GL_RENDERER_H_
#define _TVG_GL_RENDERER_H_

#include "tvgArray.h"
#include "tvgGlRenderTarget.h"
#include "tvgGlRenderTask.h"
#include "tvgGlGpuBuffer.h"
#include "tvgGlTextureMgr.h"
#include "tvgGlRenderPass.h"
#include "tvgGlEffect.h"
#include "tvgGlStateCache.h"
#include "tvgGlSolidBatch.h"
#include "tvgGlStencilCoverBatch.h"

struct GlRenderer : RenderMethod
{
    enum class BlendSource { Image, Scene, Solid, LinearGradient, RadialGradient, ConicGradient, Count };

    enum BlendShader
    {
        BS_Multiply,
        BS_Overlay,
        BS_Darken,
        BS_Lighten,
        BS_ColorDodge,
        BS_ColorBurn,
        BS_HardLight,
        BS_SoftLight,
        BS_Difference,
        BS_Hue,
        BS_Saturation,
        BS_Color,
        BS_Luminosity,
        BlendShaderCount
    };

    enum RenderTypes
    {
        RT_Color = 0,
        RT_LinGradient,
        RT_RadGradient,
        RT_ConGradient,
        RT_Image,
        RT_MaskAlpha,
        RT_MaskAlphaInv,
        RT_MaskLuma,
        RT_MaskLumaInv,
        RT_MaskAdd,
        RT_MaskSub,
        RT_MaskIntersect,
        RT_MaskDifference,
        RT_MaskLighten,
        RT_MaskDarken,
        RT_Stencil,
        RT_Blit,
        RT_Blend,
        RT_None = RT_Blend + BlendShaderCount * uint32_t(BlendSource::Count)
    };

    //main features
    bool preUpdate() override;
    RenderData prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, const Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper) override;
    RenderData prepare(RenderSurface* surface, RenderData data, const Matrix& transform, const Array<RenderData>& clips, uint8_t opacity, FilterMethod filter, RenderUpdateFlag flags) override;
    bool postUpdate() override;
    bool preRender() override;
    bool renderShape(RenderData data) override;
    bool renderImage(RenderData data) override;
    bool postRender() override;
    void dispose(RenderData data) override;;
    RenderRegion region(RenderData data) override;
    bool bounds(RenderData data, Point* pt4, const Matrix& m) override;
    bool blend(BlendMethod method) override;
    ColorSpace colorSpace() override;
    const RenderSurface* mainSurface() override;
    bool sync() override;
    bool clear() override;
    bool intersectsShape(RenderData data, const RenderRegion& region) override;
    bool intersectsImage(RenderData data, const RenderRegion& region) override;
    Result target(void* display, void* surface, void* context, int32_t id, uint32_t w, uint32_t h, ColorSpace cs);

    //composition
    RenderCompositor* target(const RenderRegion& region, ColorSpace cs, CompositionFlag flags) override;
    bool beginComposite(RenderCompositor* cmp, MaskMethod method, uint8_t opacity) override;
    bool endComposite(RenderCompositor* cmp) override;

    //post effects
    void prepare(RenderEffect* effect, const Matrix& transform) override;
    bool region(RenderEffect* effect) override;
    bool render(RenderCompositor* cmp, const RenderEffect* effect, bool direct) override;
    void dispose(RenderEffect* effect) override;

    //partial rendering
    void damage(RenderData rd, const RenderRegion& region) override;
    bool partial(bool disable) override;
    uint32_t dirtyRegions() const override;
    Result dirtyRegions(Region* regions, uint32_t cnt) const override;

    static GlRenderer* gen(uint32_t threads, EngineOption op);
    static bool term();

    // gl renderer specific
    void disposeTexture(GLuint texId);

    TextureMgr textures;

    GlRenderer(); 
    ~GlRenderer();

    void initShaders();
    static RenderRegion viewportRegion(const RenderRegion& vp, const RenderRegion& bbox);
    GlRenderTask* createPrimitiveTask(RenderTypes type, BlendSource source, const RenderRegion& viewRegion, GlRenderTarget*& dstCopyFbo);
    void bindBlendTarget(GlRenderTask* task, const GlRenderTarget* dstCopyFbo, const RenderRegion& viewRegion, uint32_t binding);
    void drawPrimitive(GlShape& sdata, const RenderColor& c, RenderUpdateFlag flag, int32_t depth);
    void drawPrimitive(GlShape& sdata, const Fill* fill, RenderUpdateFlag flag, int32_t depth);
    void drawClip(Array<RenderData>& clips, const RenderRegion& viewBounds);

    GlRenderPass* currentPass();

    bool beginComplexBlending(const RenderRegion& vp, RenderRegion bounds);
    void endBlendingCompose(GlRenderTask* stencilTask);
    GlProgram* getBlendProgram(BlendMethod method, BlendSource source);
    void prepareBlitTask(GlBlitTask* task);
    void prepareCmpTask(GlRenderTask* task, const RenderRegion& vp, uint32_t cmpWidth, uint32_t cmpHeight);
    void endRenderPass(RenderCompositor* cmp);

    void flush();
    void clearDisposes();
    bool currentContext();
    void dirty(GlDrawable* drawable, bool visible, bool retained = false);
    bool culled(const RenderRegion& box);

    void* mDisplay = nullptr;   // EGLDisplay for EGL; unused for other app-managed contexts.
    void* mSurface = nullptr;   // EGLSurface for EGL, HDC for WGL; unused for other app-managed contexts.
    void* mContext = nullptr;

    RenderSurface surface;
    GLint mTargetFboId = 0;
    GlStateCache mStateCache;
    GlStageBuffer mGpuBuffer;
    GlRenderTarget mRootTarget;
    GlEffect mEffect;
    Array<GlProgram*> mPrograms;

    Array<GlRenderTargetPool*> mComposePool;
    Array<GlRenderTargetPool*> mBlendPool;
    Array<GlRenderPass*> mRenderPassStack;
    Array<GlCompositor*> mComposeStack;
    GlSolidBatch mSolidBatch;
    GlStencilCoverBatch mStencilCoverBatch;

    //Disposed resources. They should be released on synced call.
    struct {
        Array<GLuint> textures;
        Key key;
    } mDisposed;

    RenderDirtyRegion mDirtyRegion;
    Array<RenderRegion> mRenderRegions;  // the disjoint regions redrawn by the current frame
    RenderRegion mRenderBounds;          // the bounds of the redrawn regions

    BlendMethod mBlendMethod = BlendMethod::Normal;
    bool mClearBuffer = false;
    bool mFullDraw = true;      // the next frame must redraw everything
    bool mPartialDraw = false;  // the current frame redraws the dirty regions only
    bool restoreBounds = false;  // restore precise bounds before partial rendering resumes
    bool mBlended = false;      // a blended scene was composited since the last frame
};

#endif /* _TVG_GL_RENDERER_H_ */
