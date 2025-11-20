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

#ifndef _TVG_GL_COMMON_H_
#define _TVG_GL_COMMON_H_

#include <cassert>
#include "tvgGl.h"
#include "tvgRender.h"
#include "tvgMath.h"

#define MIN_GL_STROKE_WIDTH 1.0f
#define MIN_GL_STROKE_ALPHA 0.25f

#define MVP_MATRIX(w, h)                \
    float mvp[4*4] = {                  \
        2.f / w,      0.0,  0.0f, 0.0f, \
        0.0,     -2.f / h,  0.0f, 0.0f, \
        0.0f,        0.0f, -1.0f, 0.0f, \
        -1.f,         1.f,  0.0f, 1.0f  \
    };

#define MULTIPLY_MATRIX(A, B, transform)    \
    for(auto i = 0; i < 4; ++i) {           \
        for(auto j = 0; j < 4; ++j) {       \
            float sum = 0.0;                \
            for (auto k = 0; k < 4; ++k)    \
                sum += A[k*4+i] * B[j*4+k]; \
            transform[j*4+i] = sum;         \
        }                                   \
    }

/**
 *  mat3x3               mat4x4
 *
 * [ e11 e12 e13 ]     [ e11 e12 0 e13 ]
 * [ e21 e22 e23 ] =>  [ e21 e22 0 e23 ]
 * [ e31 e32 e33 ]     [ 0   0   1  0  ]
 *                     [ e31 e32 0 e33 ]
 *
 */

// All GPU use 4x4 matrix with column major order
#define GET_MATRIX44(mat3, mat4) \
    do {                         \
        mat4[0] = mat3.e11;      \
        mat4[1] = mat3.e21;      \
        mat4[2] = 0;             \
        mat4[3] = mat3.e31;      \
        mat4[4] = mat3.e12;      \
        mat4[5] = mat3.e22;      \
        mat4[6] = 0;             \
        mat4[7] = mat3.e32;      \
        mat4[8] = 0;             \
        mat4[9] = 0;             \
        mat4[10] = 1;            \
        mat4[11] = 0;            \
        mat4[12] = mat3.e13;     \
        mat4[13] = mat3.e23;     \
        mat4[14] = 0;            \
        mat4[15] = mat3.e33;     \
    } while (false)



enum class GlStencilMode {
    None,
    FillNonZero,
    FillEvenOdd,
    Stroke,
};


enum GlRenderType : uint8_t
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
    // blends (images)
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
    // blends (scenes)
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
    // effects
    RT_Effect_GaussianBlurV,
    RT_Effect_GaussianBlurH,
    RT_Effect_DropShadow,
    RT_Effect_Fill,
    RT_Effect_Tint,
    RT_Effect_Tritone,
    RT_None
};


class GlStageBuffer;
class GlRenderTask;

struct GlGeometryBuffer {
    Array<float> vertex;
    Array<uint32_t> index;

    void clear()
    {
        vertex.clear();
        index.clear();
    }

};

struct GlGeometry
{
    void prepare(const RenderShape& rshape);
    bool tesselateShape(const RenderShape& rshape, float* opacityMultiplier = nullptr);
    bool tesselateStroke(const RenderShape& rshape);
    bool tesselateLine(const RenderPath& path);
    void tesselateImage(const RenderSurface* image);
    bool draw(GlRenderTask* task, GlStageBuffer* gpuBuffer, RenderUpdateFlag flag);
    GlStencilMode getStencilMode(RenderUpdateFlag flag);
    RenderRegion getBounds() const;
    void optimizePath(const RenderPath& path, const Matrix& transform);

    GlGeometryBuffer fill, stroke;
    Matrix matrix = {};
    RenderRegion viewport = {};
    RenderRegion bounds = {};
    FillRule fillRule = FillRule::NonZero;
    RenderPath optimizedPath;
    bool optimized = false;
};


struct GlShape
{
  const RenderShape* rshape = nullptr;
  float viewWd;
  float viewHt;
  uint32_t opacity = 0;
  GLuint texId = 0;
  uint32_t texFlipY = 0;
  ColorSpace texColorSpace = ColorSpace::ABGR8888;
  GlGeometry geometry;
  Array<RenderData> clips;
  bool validFill;
  bool validStroke;
};

struct GlIntersector
{
    bool isPointInTriangle(const Point& p, const Point& a, const Point& b, const Point& c);
    bool isPointInImage(const Point& p, const GlGeometryBuffer& mesh, const Matrix& tr);
    bool isPointInTris(const Point& p, const GlGeometryBuffer& mesh, const Matrix& tr);
    bool isPointInMesh(const Point& p, const GlGeometryBuffer& mesh, const Matrix& tr);
    bool intersectClips(const Point& pt, const tvg::Array<tvg::RenderData>& clips);
    bool intersectShape(const RenderRegion region, const GlShape* shape);
    bool intersectImage(const RenderRegion region, const GlShape* image);
};

#define MAX_GRADIENT_STOPS 16

struct GlLinearGradientBlock
{
    alignas(16) float nStops[4] = {};
    alignas(16) float startPos[2] = {};
    alignas(8) float stopPos[2] = {};
    alignas(8) float stopPoints[MAX_GRADIENT_STOPS] = {};
    alignas(16) float stopColors[4 * MAX_GRADIENT_STOPS] = {};
};

struct GlRadialGradientBlock
{
    alignas(16) float nStops[4] = {};
    alignas(16) float centerPos[4] = {};
    alignas(16) float radius[2] = {};
    alignas(16) float stopPoints[MAX_GRADIENT_STOPS] = {};
    alignas(16) float stopColors[4 * MAX_GRADIENT_STOPS] = {};
};

struct GlCompositor : RenderCompositor
{
    RenderRegion bbox;
    CompositionFlag flags;
    BlendMethod blendMethod = BlendMethod::Normal;

    GlCompositor(const RenderRegion& box, CompositionFlag flags) : bbox(box), flags(flags) {}
};


#endif /* _TVG_GL_COMMON_H_ */