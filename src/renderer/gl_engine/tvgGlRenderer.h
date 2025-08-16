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
#include "tvgGlEffect.h"

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
        // blends
        RT_Blend_Normal,
        RT_Blend_Multiply,
        RT_Blend_Screen,
        RT_Blend_Overlay,
        RT_Blend_Darken,
        RT_Blend_Lighten,
        RT_Blend_ColorDodge,
        RT_Blend_ColorBurn,
        RT_Blend_HardLight,
        RT_Blend_SoftLight,
        RT_Blend_Difference,
        RT_Blend_Exclusion,
        RT_Blend_Hue,
        RT_Blend_Saturation,
        RT_Blend_Color,
        RT_Blend_Luminosity,
        RT_Blend_Add,
        // blends (gradients)
        RT_Blend_Gradient_Normal,
        RT_Blend_Gradient_Multiply,
        RT_Blend_Gradient_Screen,
        RT_Blend_Gradient_Overlay,
        RT_Blend_Gradient_Darken,
        RT_Blend_Gradient_Lighten,
        RT_Blend_Gradient_ColorDodge,
        RT_Blend_Gradient_ColorBurn,
        RT_Blend_Gradient_HardLight,
        RT_Blend_Gradient_SoftLight,
        RT_Blend_Gradient_Difference,
        RT_Blend_Gradient_Exclusion,
        RT_Blend_Gradient_Hue,
        RT_Blend_Gradient_Saturation,
        RT_Blend_Gradient_Color,
        RT_Blend_Gradient_Luminosity,
        RT_Blend_Gradient_Add,
        // blends (gradients)
        RT_Blend_Image_Normal,
        RT_Blend_Image_Multiply,
        RT_Blend_Image_Screen,
        RT_Blend_Image_Overlay,
        RT_Blend_Image_Darken,
        RT_Blend_Image_Lighten,
        RT_Blend_Image_ColorDodge,
        RT_Blend_Image_ColorBurn,
        RT_Blend_Image_HardLight,
        RT_Blend_Image_SoftLight,
        RT_Blend_Image_Difference,
        RT_Blend_Image_Exclusion,
        RT_Blend_Image_Hue,
        RT_Blend_Image_Saturation,
        RT_Blend_Image_Color,
        RT_Blend_Image_Luminosity,
        RT_Blend_Image_Add,
        // blends (scene)
        RT_Blend_Scene_Normal,
        RT_Blend_Scene_Multiply,
        RT_Blend_Scene_Screen,
        RT_Blend_Scene_Overlay,
        RT_Blend_Scene_Darken,
        RT_Blend_Scene_Lighten,
        RT_Blend_Scene_ColorDodge,
        RT_Blend_Scene_ColorBurn,
        RT_Blend_Scene_HardLight,
        RT_Blend_Scene_SoftLight,
        RT_Blend_Scene_Difference,
        RT_Blend_Scene_Exclusion,
        RT_Blend_Scene_Hue,
        RT_Blend_Scene_Saturation,
        RT_Blend_Scene_Color,
        RT_Blend_Scene_Luminosity,
        RT_Blend_Scene_Add,
        RT_None
    };

    //main features
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
    bool bounds(RenderData data, Point* pt4, const Matrix& m) override;
    bool blend(BlendMethod method) override;
    ColorSpace colorSpace() override;
    const RenderSurface* mainSurface() override;
    bool sync() override;
    bool clear() override;
    bool intersectsShape(RenderData data, const RenderRegion& region) override;
    bool intersectsImage(RenderData data, const RenderRegion& region) override;
    bool target(void* context, int32_t id, uint32_t w, uint32_t h);

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

    static GlRenderer* gen(uint32_t threads);
    static bool term();

private:
    GlRenderer(); 
    ~GlRenderer();

    void initShaders();
    void drawPrimitive(GlShape& sdata, const RenderColor& c, RenderUpdateFlag flag, int32_t depth);
    void drawPrimitive(GlShape& sdata, const Fill* fill, RenderUpdateFlag flag, int32_t depth);
    void drawClip(Array<RenderData>& clips);

    GlRenderPass* currentPass();

    bool beginComplexBlending(const RenderRegion& vp, RenderRegion bounds);
    void endBlendingCompose(GlRenderTask* stencilTask, const Matrix& matrix, bool gradient, bool image);
    GlProgram* getBlendProgram(BlendMethod method, bool gradient, bool image, bool scene);

    void prepareBlitTask(GlBlitTask* task);
    void prepareCmpTask(GlRenderTask* task, const RenderRegion& vp, uint32_t cmpWidth, uint32_t cmpHeight);
    void endRenderPass(RenderCompositor* cmp);

    void flush();
    void clearDisposes();
    void currentContext();

    void* mContext = nullptr;
    RenderSurface surface;
    GLint mTargetFboId = 0;
    GlStageBuffer mGpuBuffer;
    GlRenderTarget mRootTarget;
    GlEffect mEffect;
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
    bool mClearBuffer = false;
};

#endif /* _TVG_GL_RENDERER_H_ */
