/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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
#include "tvgGlRenderPass.h"

class GlRenderer : public RenderMethod
{
public:
    enum RenderTypes
    {
        RT_Color = 0,
        RT_LinGradient,
        RT_RadGradient,
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
        RT_MultiplyBlend,
        RT_ScreenBlend,
        RT_OverlayBlend,
        RT_ColorDodgeBlend,
        RT_ColorBurnBlend,
        RT_HardLightBlend,
        RT_SoftLightBlend,
        RT_DifferenceBlend,
        RT_ExclusionBlend,
        RT_LightenBlend,
        RT_DarkenBlend,

        RT_None,
    };

    bool preUpdate() override;
    RenderData prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper) override;
    RenderData prepare(RenderSurface* surface, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) override;
    bool postUpdate() override;
    bool preRender() override;
    bool renderShape(RenderData data) override;
    bool renderImage(RenderData data) override;
    bool postRender() override;
    void dispose(RenderData data) override;;
    RenderRegion region(RenderData data) override;
    RenderRegion viewport() override;
    bool viewport(const RenderRegion& vp) override;
    bool blend(BlendMethod method) override;
    ColorSpace colorSpace() override;
    const RenderSurface* mainSurface() override;

    bool target(void* context, int32_t id, uint32_t w, uint32_t h);
    bool sync() override;
    bool clear() override;

    RenderCompositor* target(const RenderRegion& region, ColorSpace cs, CompositionFlag flags) override;
    bool beginComposite(RenderCompositor* cmp, MaskMethod method, uint8_t opacity) override;
    bool endComposite(RenderCompositor* cmp) override;

    void prepare(RenderEffect* effect, const Matrix& transform) override;
    bool region(RenderEffect* effect) override;
    bool render(RenderCompositor* cmp, const RenderEffect* effect, bool direct) override;
    void dispose(TVG_UNUSED RenderEffect* effect) override;

    static GlRenderer* gen();
    static int init(TVG_UNUSED uint32_t threads);
    static int32_t init();
    static int term();

private:
    GlRenderer(); 
    ~GlRenderer();

    void initShaders();
    void drawPrimitive(GlShape& sdata, const RenderColor& c, RenderUpdateFlag flag, int32_t depth);
    void drawPrimitive(GlShape& sdata, const Fill* fill, RenderUpdateFlag flag, int32_t depth);
    void drawClip(Array<RenderData>& clips);

    GlRenderPass* currentPass();

    bool beginComplexBlending(BlendMethod blend, const RenderRegion& vp, RenderRegion bounds);
    void endBlendingCompose(BlendMethod blend);
    GlProgram* getBlendProgram(BlendMethod blend);

    void prepareBlitTask(GlBlitTask* task);
    void prepareCmpTask(GlRenderTask* task, const RenderRegion& vp, uint32_t cmpWidth, uint32_t cmpHeight);
    void prepareCmpStencilTask(GlRenderTask* task, RenderRegion& vp);
    void endRenderPass(RenderCompositor* cmp);

    void flush();
    void clearDisposes();
    void currentContext();

    void* mContext = nullptr;
    RenderSurface surface;
    GLint mTargetFboId = 0;
    RenderRegion mViewport;
    GlStageBuffer mGpuBuffer;
    GlRenderTarget mRootTarget;
    Array<GlProgram*> mPrograms;
    Array<GlRenderTargetPool*> mComposePool;
    Array<GlRenderTargetPool*> mBlendPool;
    Array<GlRenderPass*> mRenderPassStack;
    Array<GlCompositor*> mComposeStack;

    //Disposed resources. They should be released on synced call.
    struct {
        Array<GLuint> textures;
        Key key;
    } mDisposed;

    BlendMethod mBlendMethod = BlendMethod::Normal;
    bool mClearBuffer = true;  //FIXME: clear buffer should be optional (default is false)
};

#endif /* _TVG_GL_RENDERER_H_ */
