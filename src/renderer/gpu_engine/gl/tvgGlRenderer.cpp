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

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "tvgFill.h"
#include "tvgGlCommon.h"
#include "tvgGlRenderer.h"
#include "tvgGlGpuBuffer.h"
#include "tvgGlRenderTask.h"
#include "tvgGlProgram.h"
#include "tvgGlShaderSrc.h"
#include "tvgRender.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#define NOISE_LEVEL 0.5f

static int32_t _rendererCnt = -1;
static StrictKey _rendererMtx;

static constexpr float IDENTITY_VERTEX[] = {-1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f, -1.f};
static constexpr uint32_t RECT_INDEX[] = {0, 1, 2, 2, 1, 3};
static constexpr uint32_t RECT_INDEX_COUNT = sizeof(RECT_INDEX) / sizeof(RECT_INDEX[0]);

static bool _skipRender(const Array<RenderData>& clips)
{
    if (clips.empty()) return false;

    // Clip geometries are prepared during update(), before this paint renders.
    // Clip masks are intersected, so one empty clip makes the paint invisible.
    ARRAY_FOREACH(p, clips) {
        auto clip = static_cast<GlShape*>(*p);
        auto flag = (clip->geometry.stroke.vertex.count > 0) ? RenderUpdateFlag::Stroke : RenderUpdateFlag::Path;
        if (!clip->geometry.drawable(flag)) return true;
    }

    return false;
}

void GlRenderer::disposeTexture(GLuint texId)
{
    if (!texId) return;
    ScopedLock lock(mDisposed.key);
    mDisposed.textures.push(texId);
}

void GlRenderer::clearDisposes()
{
    if (mDisposed.textures.count > 0) {
        GL_CHECK(glDeleteTextures(mDisposed.textures.count, mDisposed.textures.data));
        mDisposed.textures.clear();
    }

    ARRAY_FOREACH(p, mRenderPassStack)
    delete (*p);
    mRenderPassStack.clear();
    mSolidBatch.clear();
    mStencilCoverBatch.clear();
}


void GlRenderer::flush()
{
    clearDisposes();

#if defined(THORVG_GL_FLAT_MASK_SUPPORT)
    mFlatMaskTarget.reset();
#endif
    mRootTarget.reset();

    ARRAY_FOREACH(p, mComposePool) delete(*p);
    mComposePool.clear();

    ARRAY_FOREACH(p, mBlendPool) delete(*p);
    mBlendPool.clear();

    ARRAY_FOREACH(p, mComposeStack) delete(*p);
    mComposeStack.clear();
}


bool GlRenderer::currentContext()
{
#if defined(__EMSCRIPTEN__)
    const auto targetContext = reinterpret_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(mContext);
    if (emscripten_webgl_get_current_context() == targetContext) return true;
    return emscripten_webgl_make_context_current(targetContext) == 0;
#elif defined(_WIN32) && !defined(__CYGWIN__) && defined(THORVG_GL_TARGET_GL)
    if (tvgWglGetCurrentContext() == static_cast<HGLRC>(mContext)) return true;
    return (bool) tvgWglMakeCurrent((HDC)mSurface, static_cast<HGLRC>(mContext));
#elif defined(THORVG_GL_TARGET_GLES)
    if (tvgEglGetCurrentContext() == static_cast<EGLContext>(mContext)) return true;
    if (mDisplay && mSurface) return (bool) tvgEglMakeCurrent((EGLDisplay)mDisplay, (EGLSurface)mSurface, (EGLSurface)mSurface, (EGLContext)mContext);
#endif
    TVGLOG("GL_ENGINE", "Maybe missing currentContext()?");
    return true;
}


GlRenderer::GlRenderer() : mEffect(GlEffect(&mGpuBuffer))
{
}


GlRenderer::~GlRenderer()
{
    if (mContext) currentContext();
    flush();
    mTextures.clear();

    ARRAY_FOREACH(p, mPrograms) delete(*p);

    _rendererMtx.lock();
    --_rendererCnt;
    _rendererMtx.unlock();
}


void GlRenderer::initShaders()
{
    mPrograms.reserve((int)RT_None);

#if 1  //for optimization
    #define LINEAR_TOTAL_LENGTH 2831
    #define RADIAL_TOTAL_LENGTH 5315
    #define BLEND_TOTAL_LENGTH 5096
#else
    #define COMMON_TOTAL_LENGTH strlen(STR_GRADIENT_FRAG_COMMON_VARIABLES) + strlen(STR_GRADIENT_FRAG_COMMON_FUNCTIONS) + 1
    #define LINEAR_TOTAL_LENGTH strlen(STR_LINEAR_GRADIENT_VARIABLES) + strlen(STR_LINEAR_GRADIENT_FUNCTIONS) + strlen(STR_LINEAR_GRADIENT_MAIN) + COMMON_TOTAL_LENGTH
    #define RADIAL_TOTAL_LENGTH strlen(STR_RADIAL_GRADIENT_VARIABLES) + strlen(STR_RADIAL_GRADIENT_FUNCTIONS) + strlen(STR_RADIAL_GRADIENT_MAIN) + COMMON_TOTAL_LENGTH
    #define BLEND_TOTAL_LENGTH strlen(BLEND_SCENE_FRAG_HEADER) + strlen(BLEND_FRAG_LUM_HELPER) + strlen(BLEND_FRAG_SAT_HELPER) + strlen(COLOR_BURN_BLEND_FRAG) + 1
#endif

    char linearGradientFragShader[LINEAR_TOTAL_LENGTH];
    snprintf(linearGradientFragShader, LINEAR_TOTAL_LENGTH, "%s%s%s%s%s",
        STR_GRADIENT_FRAG_COMMON_VARIABLES,
        STR_LINEAR_GRADIENT_VARIABLES,
        STR_GRADIENT_FRAG_COMMON_FUNCTIONS,
        STR_LINEAR_GRADIENT_FUNCTIONS,
        STR_LINEAR_GRADIENT_MAIN
    );

    char radialGradientFragShader[RADIAL_TOTAL_LENGTH];
    snprintf(radialGradientFragShader, RADIAL_TOTAL_LENGTH, "%s%s%s%s%s",
        STR_GRADIENT_FRAG_COMMON_VARIABLES,
        STR_RADIAL_GRADIENT_VARIABLES,
        STR_GRADIENT_FRAG_COMMON_FUNCTIONS,
        STR_RADIAL_GRADIENT_FUNCTIONS,
        STR_RADIAL_GRADIENT_MAIN
    );

    mPrograms.push(new GlProgram(COLOR_VERT_SHADER, COLOR_FRAG_SHADER));
    mPrograms.push(new GlProgram(GRADIENT_VERT_SHADER, linearGradientFragShader));
    mPrograms.push(new GlProgram(GRADIENT_VERT_SHADER, radialGradientFragShader));
    mPrograms.push(new GlProgram(IMAGE_VERT_SHADER, IMAGE_FRAG_SHADER));

    // compose Renderer
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_ALPHA_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_INV_ALPHA_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_LUMA_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_INV_LUMA_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_ADD_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_SUB_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_INTERSECT_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_DIFF_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_LIGHTEN_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_DARKEN_FRAG_SHADER));

    // stencil Renderer
    mPrograms.push(new GlProgram(STENCIL_VERT_SHADER, STENCIL_FRAG_SHADER));

#if defined(THORVG_GL_FLAT_MASK_SUPPORT)
    mPrograms.push(new GlProgram(FLAT_DIRECT_BOUNDARY_VERT_SHADER, FLAT_DIRECT_BOUNDARY_FRAG_SHADER));
    mPrograms.push(new GlProgram(CURVE_DIRECT_BOUNDARY_VERT_SHADER, CURVE_DIRECT_BOUNDARY_FRAG_SHADER));
    mPrograms.push(new GlProgram(STENCIL_VERT_SHADER, FLAT_MASK_INTERIOR_FRAG_SHADER));
    mPrograms.push(new GlProgram(FLAT_MASK_EDGE_VERT_SHADER, FLAT_MASK_EDGE_INSIDE_POSITIVE_FRAG_SHADER));
    mPrograms.push(new GlProgram(FLAT_MASK_EDGE_VERT_SHADER, FLAT_MASK_EDGE_INSIDE_NEGATIVE_FRAG_SHADER));
    mPrograms.push(new GlProgram(FLAT_MASK_EDGE_VERT_SHADER, FLAT_MASK_EDGE_OUTSIDE_FRAG_SHADER));
    mPrograms.push(new GlProgram(COLOR_VERT_SHADER, FLAT_MASK_COMPOSITE_FRAG_SHADER));
    mPrograms.push(new GlProgram(STENCIL_VERT_SHADER, CURVE_MASK_INTERIOR_FRAG_SHADER));
    mPrograms.push(new GlProgram(CURVE_MASK_BOUNDARY_VERT_SHADER, CURVE_MASK_BOUNDARY_FRAG_SHADER));
    mPrograms.push(new GlProgram(COLOR_VERT_SHADER, CURVE_MASK_COMPOSITE_FRAG_SHADER));
#endif

    // blit Renderer
    mPrograms.push(new GlProgram(BLIT_VERT_SHADER, BLIT_FRAG_SHADER));

    // blend programs: image (17) + scene (17) + shape solid (17) + shape linear (17) + shape radial (17)
    for (uint32_t i = 0; i < 85; ++i) mPrograms.push(nullptr);
}

RenderRegion GlRenderer::viewportRegion(const RenderRegion& vp, const RenderRegion& bbox)
{
    auto x = bbox.sx() - vp.sx();
    auto y = bbox.sy() - vp.sy();
    auto w = bbox.sw();
    auto h = bbox.sh();
    auto yGl = vp.sh() - y - h;

    return {{x, yGl}, {x + w, yGl + h}};
}

static GlRenderTask* drawPrimitiveGeometry(GlProgram* stencilProgram, GlRenderTask* task, const GlGeometry& geometry,
                                           GlStencilCoverBatch& batch, GlRenderPass* pass,
                                           GlStageBuffer* gpuBuffer, RenderUpdateFlag flag, GlStencilMode stencilMode,
                                           bool clipped, int32_t depth, const Matrix& viewMatrix, const RenderRegion& passViewport, const RenderColor* color,
                                           const RenderRegion& viewBounds, RenderRegion& stencilBounds, const GlGeometryBuffer*& stencilBuffer, uint32_t*& stencilIndices, bool& merge)
{
    if (stencilMode == GlStencilMode::None) {
        stencilBuffer = nullptr;
        stencilIndices = nullptr;
        merge = false;
        geometry.draw(task, gpuBuffer, flag);
        return nullptr;
    }

    return batch.prepare(stencilProgram, pass, task, geometry, gpuBuffer, flag, stencilMode, clipped, depth, viewMatrix, passViewport, color, viewBounds, stencilBounds, stencilBuffer, stencilIndices, merge);
}

static Matrix _viewMatrix(const GlGeometry& geometry, const Matrix& viewMatrix, RenderUpdateFlag flag)
{
    // Most GL meshes are already in world space; local strokes fold model into
    // the shader's view matrix so the draw path can stay shared.
    if ((flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke)) return viewMatrix * geometry.matrix;
    return viewMatrix;
}

#if defined(THORVG_GL_FLAT_MASK_SUPPORT)
static uint32_t pathContourCount(const RenderPath& path)
{
    uint32_t count = 0;
    ARRAY_FOREACH(cmd, path.cmds)
    {
        if (*cmd == PathCommand::MoveTo) ++count;
    }
    return count;
}

static bool directPathClosed(const RenderPath& path)
{
    bool contourOpen = false;
    bool contourHasSegment = false;
    bool hasContour = false;

    ARRAY_FOREACH(cmd, path.cmds)
    {
        switch (*cmd) {
            case PathCommand::MoveTo:
                if (contourOpen) return false;
                contourOpen = true;
                contourHasSegment = false;
                hasContour = true;
                break;
            case PathCommand::LineTo:
            case PathCommand::CubicTo:
                if (!contourOpen) return false;
                contourHasSegment = true;
                break;
            case PathCommand::Close:
                if (!contourOpen || !contourHasSegment) return false;
                contourOpen = false;
                break;
        }
    }
    return hasContour && !contourOpen;
}

struct GlFlatMaskVertex
{
    float x, y;
    float startX, startY;
    float endX, endY;
};

struct GlFlatDirectVertex
{
    float x, y;
    float startX, startY;
    float endX, endY;
    float insideSign;
};

using GlDirectContour = std::vector<Point>;

static float directContourTwiceArea(const GlDirectContour& contour)
{
    float twiceArea = 0.0f;
    for (size_t i = 0; i < contour.size(); ++i) {
        const auto& a = contour[i];
        const auto& b = contour[(i + 1) % contour.size()];
        twiceArea += a.x * b.y - b.x * a.y;
    }
    return twiceArea;
}

static bool directContourContains(const GlDirectContour& contour, const Point& point)
{
    bool inside = false;
    for (size_t i = 0, previous = contour.size() - 1; i < contour.size(); previous = i++) {
        const auto& a = contour[previous];
        const auto& b = contour[i];
        if ((a.y > point.y) == (b.y > point.y)) continue;
        auto intersectionX = (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x;
        if (point.x < intersectionX) inside = !inside;
    }
    return inside;
}

static float directContourInsideSign(const std::vector<GlDirectContour>& contours,
                                     size_t contourIndex, FillRule fillRule)
{
    const auto& contour = contours[contourIndex];
    auto contourWinding = directContourTwiceArea(contour) >= 0.0f ? 1 : -1;
    uint32_t nestingDepth = 0;
    int32_t surroundingWinding = 0;
    for (size_t i = 0; i < contours.size(); ++i) {
        if (i == contourIndex || !directContourContains(contours[i], contour.front())) continue;
        ++nestingDepth;
        surroundingWinding += directContourTwiceArea(contours[i]) >= 0.0f ? 1 : -1;
    }

    bool outsideFilled;
    bool insideFilled;
    if (fillRule == FillRule::EvenOdd) {
        outsideFilled = (nestingDepth & 1u) != 0;
        insideFilled = !outsideFilled;
    } else {
        outsideFilled = surroundingWinding != 0;
        insideFilled = surroundingWinding + contourWinding != 0;
    }
    if (insideFilled == outsideFilled) return 0.0f;
    auto orientationSign = static_cast<float>(contourWinding);
    return insideFilled ? orientationSign : -orientationSign;
}

static void addFlatMaskQuad(GlRenderTask* task, GlStageBuffer* gpuBuffer, const RenderRegion& bounds)
{
    float vertices[] = {
        float(bounds.min.x), float(bounds.min.y),
        float(bounds.min.x), float(bounds.max.y),
        float(bounds.max.x), float(bounds.min.y),
        float(bounds.max.x), float(bounds.max.y),
    };
    auto vertexOffset = gpuBuffer->push(vertices, sizeof(vertices));
    auto indexOffset = gpuBuffer->pushIndex((void*)RECT_INDEX, sizeof(RECT_INDEX));
    task->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), vertexOffset, GL_FLOAT, GL_FALSE, gpuBuffer->getBufferId()});
    task->setDrawRange(indexOffset, RECT_INDEX_COUNT);
}

static bool addFlatMaskBoundary(GlRenderTask* insidePositiveTask, GlRenderTask* insideNegativeTask,
                                GlRenderTask* outsideTask,
                                GlStageBuffer* gpuBuffer, const GlGeometry& geometry,
                                const RenderRegion& passViewport)
{
    Array<GlFlatMaskVertex> vertices;
    vertices.reserve(geometry.fillBoundary.edges.count * 6);

    ARRAY_FOREACH(edge, geometry.fillBoundary.edges) {
        auto from = edge->from * 2;
        auto to = edge->to * 2;
        Point a = {geometry.fill.vertex[from], geometry.fill.vertex[from + 1]};
        Point b = {geometry.fill.vertex[to], geometry.fill.vertex[to + 1]};
        auto delta = b - a;
        auto lengthSquared = dot(delta, delta);
        if (lengthSquared == 0.0f) continue;

        auto invLength = 1.0f / sqrtf(lengthSquared);
        auto tangent = delta * invLength;
        auto normal = Point{-tangent.y, tangent.x};
        auto start = a - tangent * GL_FLAT_MASK_AA_RADIUS;
        auto end = b + tangent * GL_FLAT_MASK_AA_RADIUS;
        auto startLocal = Point{a.x - passViewport.min.x,
                                passViewport.sh() - (a.y - passViewport.min.y)};
        auto endLocal = Point{b.x - passViewport.min.x,
                              passViewport.sh() - (b.y - passViewport.min.y)};

        GlFlatMaskVertex v0 = {start.x + normal.x * GL_FLAT_MASK_AA_RADIUS, start.y + normal.y * GL_FLAT_MASK_AA_RADIUS,
                               startLocal.x, startLocal.y, endLocal.x, endLocal.y};
        GlFlatMaskVertex v1 = {start.x - normal.x * GL_FLAT_MASK_AA_RADIUS, start.y - normal.y * GL_FLAT_MASK_AA_RADIUS,
                               startLocal.x, startLocal.y, endLocal.x, endLocal.y};
        GlFlatMaskVertex v2 = {end.x + normal.x * GL_FLAT_MASK_AA_RADIUS, end.y + normal.y * GL_FLAT_MASK_AA_RADIUS,
                               startLocal.x, startLocal.y, endLocal.x, endLocal.y};
        GlFlatMaskVertex v3 = {end.x - normal.x * GL_FLAT_MASK_AA_RADIUS, end.y - normal.y * GL_FLAT_MASK_AA_RADIUS,
                               startLocal.x, startLocal.y, endLocal.x, endLocal.y};
        vertices.push(v0);
        vertices.push(v1);
        vertices.push(v2);
        vertices.push(v1);
        vertices.push(v3);
        vertices.push(v2);
    }
    if (vertices.empty()) return false;

    auto vertexOffset = gpuBuffer->push(vertices.data, vertices.count * sizeof(GlFlatMaskVertex));
    uint32_t* indices = nullptr;
    auto indexOffset = gpuBuffer->reserveIndex(vertices.count * sizeof(uint32_t), reinterpret_cast<void**>(&indices));
    for (uint32_t i = 0; i < vertices.count; ++i) indices[i] = i;

    auto addLayouts = [&](GlRenderTask* task) {
        auto buffer = gpuBuffer->getBufferId();
        task->addVertexLayout(GlVertexLayout{0, 2, sizeof(GlFlatMaskVertex), vertexOffset, GL_FLOAT, GL_FALSE, buffer});
        task->addVertexLayout(GlVertexLayout{1, 2, sizeof(GlFlatMaskVertex), vertexOffset + 2 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
        task->addVertexLayout(GlVertexLayout{2, 2, sizeof(GlFlatMaskVertex), vertexOffset + 4 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
        task->setDrawRange(indexOffset, vertices.count);
    };
    addLayouts(insidePositiveTask);
    addLayouts(insideNegativeTask);
    addLayouts(outsideTask);
    return true;
}

static bool addFlatDirectBoundary(GlRenderTask* task, GlStageBuffer* gpuBuffer,
                                  const GlGeometry& geometry,
                                  const RenderRegion& passViewport)
{
    Array<GlFlatDirectVertex> vertices;
    vertices.reserve(geometry.fillBoundary.edges.count * 6);

    std::vector<GlDirectContour> contours;
    contours.reserve(geometry.fillBoundary.contourEnds.count);
    uint32_t contourStart = 0;
    ARRAY_FOREACH(contourEnd, geometry.fillBoundary.contourEnds)
    {
        if (*contourEnd <= contourStart || *contourEnd > geometry.fillBoundary.edges.count) return false;
        GlDirectContour contour;
        contour.reserve(*contourEnd - contourStart);
        for (uint32_t i = contourStart; i < *contourEnd; ++i) {
            const auto& edge = geometry.fillBoundary.edges[i];
            auto from = edge.from * 2;
            contour.push_back({geometry.fill.vertex[from] - passViewport.min.x,
                               passViewport.sh() - (geometry.fill.vertex[from + 1] - passViewport.min.y)});
        }
        contours.push_back(std::move(contour));
        contourStart = *contourEnd;
    }
    if (contourStart != geometry.fillBoundary.edges.count) return false;

    contourStart = 0;
    size_t contourIndex = 0;
    ARRAY_FOREACH(contourEnd, geometry.fillBoundary.contourEnds)
    {
        auto insideSign = directContourInsideSign(contours, contourIndex++,
                                                  geometry.fillRule);
        if (insideSign == 0.0f) {
            contourStart = *contourEnd;
            continue;
        }
        for (uint32_t i = contourStart; i < *contourEnd; ++i) {
            const auto& edge = geometry.fillBoundary.edges[i];
            auto from = edge.from * 2;
            auto to = edge.to * 2;
            Point a = {geometry.fill.vertex[from], geometry.fill.vertex[from + 1]};
            Point b = {geometry.fill.vertex[to], geometry.fill.vertex[to + 1]};
            auto delta = b - a;
            auto lengthSquared = dot(delta, delta);
            if (lengthSquared == 0.0f) continue;

            auto tangent = delta / sqrtf(lengthSquared);
            auto normal = Point{-tangent.y, tangent.x};
            auto start = a - tangent * GL_FLAT_MASK_AA_RADIUS;
            auto end = b + tangent * GL_FLAT_MASK_AA_RADIUS;
            auto startLocal = Point{a.x - passViewport.min.x,
                                    passViewport.sh() - (a.y - passViewport.min.y)};
            auto endLocal = Point{b.x - passViewport.min.x,
                                  passViewport.sh() - (b.y - passViewport.min.y)};

            GlFlatDirectVertex v0 = {start.x + normal.x * GL_FLAT_MASK_AA_RADIUS,
                                     start.y + normal.y * GL_FLAT_MASK_AA_RADIUS,
                                     startLocal.x, startLocal.y, endLocal.x, endLocal.y,
                                     insideSign};
            GlFlatDirectVertex v1 = {start.x - normal.x * GL_FLAT_MASK_AA_RADIUS,
                                     start.y - normal.y * GL_FLAT_MASK_AA_RADIUS,
                                     startLocal.x, startLocal.y, endLocal.x, endLocal.y,
                                     insideSign};
            GlFlatDirectVertex v2 = {end.x + normal.x * GL_FLAT_MASK_AA_RADIUS,
                                     end.y + normal.y * GL_FLAT_MASK_AA_RADIUS,
                                     startLocal.x, startLocal.y, endLocal.x, endLocal.y,
                                     insideSign};
            GlFlatDirectVertex v3 = {end.x - normal.x * GL_FLAT_MASK_AA_RADIUS,
                                     end.y - normal.y * GL_FLAT_MASK_AA_RADIUS,
                                     startLocal.x, startLocal.y, endLocal.x, endLocal.y,
                                     insideSign};
            vertices.push(v0);
            vertices.push(v1);
            vertices.push(v2);
            vertices.push(v1);
            vertices.push(v3);
            vertices.push(v2);
        }
        contourStart = *contourEnd;
    }
    if (contourStart != geometry.fillBoundary.edges.count || vertices.empty()) return false;

    auto vertexOffset = gpuBuffer->push(vertices.data, vertices.count * sizeof(GlFlatDirectVertex));
    uint32_t* indices = nullptr;
    auto indexOffset = gpuBuffer->reserveIndex(vertices.count * sizeof(uint32_t),
                                               reinterpret_cast<void**>(&indices));
    for (uint32_t i = 0; i < vertices.count; ++i)
        indices[i] = i;

    auto buffer = gpuBuffer->getBufferId();
    task->addVertexLayout(GlVertexLayout{0, 2, sizeof(GlFlatDirectVertex), vertexOffset, GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{1, 2, sizeof(GlFlatDirectVertex), vertexOffset + 2 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{2, 2, sizeof(GlFlatDirectVertex), vertexOffset + 4 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{3, 1, sizeof(GlFlatDirectVertex), vertexOffset + 6 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->setDrawRange(indexOffset, vertices.count);
    return true;
}

enum class GlCurveMaskPatchKind : uint8_t
{
    Line,
    Cubic,
};

struct GlCurveMaskVertex
{
    float x, y;
    float p0x, p0y;
    float p1x, p1y;
    float p2x, p2y;
    float p3x, p3y;
    float c0, c1, c2, c3;
    float c4, c5, c6, c7;
    float c8, c9;
    float centerX, centerY, inverseScale;
    float kind;
};

struct GlCurveDirectVertex
{
    float x, y;
    float p0x, p0y;
    float p1x, p1y;
    float p2x, p2y;
    float p3x, p3y;
    float c0, c1, c2, c3;
    float c4, c5, c6, c7;
    float c8, c9;
    float centerX, centerY, inverseScale;
    float kind;
    float insideSign;
};

static_assert(sizeof(GlCurveMaskVertex) == 24 * sizeof(float),
              "GlCurveMaskVertex must stay tightly packed");
static_assert(sizeof(GlCurveDirectVertex) == 25 * sizeof(float),
              "GlCurveDirectVertex must stay tightly packed");

struct GlCubicImplicit
{
    std::array<float, 10> coefficients = {};
    Point center = {};
    float inverseScale = 1.0f;
};

enum class GlImplicitResult : uint8_t
{
    Success,
    Empty,
    Failure,
};

static Point curveMaskCubicAt(const Point& p0, const Point& p1,
                              const Point& p2, const Point& p3, double t)
{
    auto s = 1.0 - t;
    auto b0 = static_cast<float>(s * s * s);
    auto b1 = static_cast<float>(3.0 * s * s * t);
    auto b2 = static_cast<float>(3.0 * s * t * t);
    auto b3 = static_cast<float>(t * t * t);
    return p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3;
}

static std::array<double, 10> curveMaskMonomials(const Point& point)
{
    auto x = static_cast<double>(point.x);
    auto y = static_cast<double>(point.y);
    return {x * x * x, x * x * y, x * y * y, y * y * y,
            x * x, x * y, y * y, x, y, 1.0};
}

static double curveMaskEvaluateImplicit(const std::array<double, 10>& coefficients,
                                        const Point& point)
{
    auto monomials = curveMaskMonomials(point);
    double value = 0.0;
    for (size_t i = 0; i < coefficients.size(); ++i) {
        value += coefficients[i] * monomials[i];
    }
    return value;
}

template<size_t Rows, size_t Columns>
static bool curveMaskNullVector(std::array<std::array<double, Columns>, Rows> matrix,
                                std::array<double, Columns>& coefficients)
{
    static_assert(Columns == Rows + 1, "Null-space solve expects one free column");
    std::array<int, Rows> pivotColumns = {};
    size_t rank = 0;
    for (size_t column = 0; column < Columns && rank < Rows; ++column) {
        auto pivot = rank;
        for (size_t row = rank + 1; row < Rows; ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
        }
        if (std::abs(matrix[pivot][column]) < 1e-12) continue;
        if (pivot != rank) std::swap(matrix[pivot], matrix[rank]);

        auto divisor = matrix[rank][column];
        for (size_t i = column; i < Columns; ++i) matrix[rank][i] /= divisor;
        for (size_t row = 0; row < Rows; ++row) {
            if (row == rank) continue;
            auto factor = matrix[row][column];
            for (size_t i = column; i < Columns; ++i) {
                matrix[row][i] -= factor * matrix[rank][i];
            }
        }
        pivotColumns[rank] = static_cast<int>(column);
        ++rank;
    }
    if (rank != Rows) return false;

    std::array<bool, Columns> pivoted = {};
    for (size_t row = 0; row < rank; ++row) pivoted[pivotColumns[row]] = true;
    size_t freeColumn = 0;
    while (freeColumn < Columns && pivoted[freeColumn]) ++freeColumn;
    if (freeColumn == Columns) return false;

    coefficients = {};
    coefficients[freeColumn] = 1.0;
    for (size_t row = 0; row < rank; ++row) {
        coefficients[pivotColumns[row]] = -matrix[row][freeColumn];
    }
    double scale = 0.0;
    for (auto coefficient : coefficients) scale = std::max(scale, std::abs(coefficient));
    if (scale == 0.0) return false;
    for (auto& coefficient : coefficients) coefficient /= scale;
    return true;
}

static GlImplicitResult curveMaskImplicitizeCubic(const Point& p0, const Point& p1,
                                                   const Point& p2, const Point& p3,
                                                   GlCubicImplicit& implicit)
{
    auto minX = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
    auto minY = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
    auto maxX = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
    auto maxY = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));
    implicit.center = {(minX + maxX) * 0.5f, (minY + maxY) * 0.5f};
    auto scale = std::max(maxX - minX, maxY - minY);
    if (scale == 0.0f) return GlImplicitResult::Empty;
    implicit.inverseScale = 1.0f / scale;

    auto normalize = [&](const Point& point) {
        return (point - implicit.center) * implicit.inverseScale;
    };
    auto n0 = normalize(p0);
    auto n1 = normalize(p1);
    auto n2 = normalize(p2);
    auto n3 = normalize(p3);

    const Point controls[] = {n0, n1, n2, n3};
    size_t lineStart = 0;
    size_t lineEnd = 0;
    float maximumDistance2 = 0.0f;
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = i + 1; j < 4; ++j) {
            auto delta = controls[j] - controls[i];
            auto distance2 = dot(delta, delta);
            if (distance2 > maximumDistance2) {
                maximumDistance2 = distance2;
                lineStart = i;
                lineEnd = j;
            }
        }
    }
    if (maximumDistance2 <= FLOAT_EPSILON * FLOAT_EPSILON) return GlImplicitResult::Empty;

    auto line = controls[lineEnd] - controls[lineStart];
    auto lineLength = std::sqrt(maximumDistance2);
    bool collinear = true;
    for (const auto& control : controls) {
        if (std::abs(cross(line, control - controls[lineStart])) >
            FLOAT_EPSILON * lineLength) {
            collinear = false;
            break;
        }
    }

    std::array<double, 10> coefficients = {};
    if (collinear) {
        // Degree-reduced cubics use their normalized supporting line.
        auto lineX = static_cast<double>(line.x);
        auto lineY = static_cast<double>(line.y);
        auto startX = static_cast<double>(controls[lineStart].x);
        auto startY = static_cast<double>(controls[lineStart].y);
        auto divisor = static_cast<double>(lineLength);
        coefficients[7] = -lineY / divisor;
        coefficients[8] = lineX / divisor;
        coefficients[9] = (lineY * startX - lineX * startY) / divisor;
    } else {
        auto cubicPower = n3 - n2 * 3.0f + n1 * 3.0f - n0;
        if (dot(cubicPower, cubicPower) <= FLOAT_EPSILON * FLOAT_EPSILON) {
            // Quadratics reach RenderPath as degree-elevated cubics.
            std::array<std::array<double, 6>, 5> matrix = {};
            for (size_t row = 0; row < matrix.size(); ++row) {
                auto t = static_cast<double>(row) / static_cast<double>(matrix.size() - 1);
                auto monomials = curveMaskMonomials(curveMaskCubicAt(n0, n1, n2, n3, t));
                std::copy(monomials.begin() + 4, monomials.end(), matrix[row].begin());
            }
            std::array<double, 6> quadratic = {};
            if (!curveMaskNullVector(matrix, quadratic)) return GlImplicitResult::Failure;
            std::copy(quadratic.begin(), quadratic.end(), coefficients.begin() + 4);
        } else {
            std::array<std::array<double, 10>, 9> matrix = {};
            for (size_t row = 0; row < matrix.size(); ++row) {
                auto t = static_cast<double>(row) / static_cast<double>(matrix.size() - 1);
                matrix[row] = curveMaskMonomials(curveMaskCubicAt(n0, n1, n2, n3, t));
            }
            if (!curveMaskNullVector(matrix, coefficients)) return GlImplicitResult::Failure;
        }
    }

    // Check between solve samples. Numerical failures remain visible as a POC
    // error instead of silently replacing the original curve with flat edges.
    for (uint32_t i = 0; i <= 32; ++i) {
        auto t = static_cast<double>(i) / 32.0;
        auto point = curveMaskCubicAt(n0, n1, n2, n3, t);
        if (std::abs(curveMaskEvaluateImplicit(coefficients, point)) > 1e-5) {
            return GlImplicitResult::Failure;
        }
    }
    for (size_t i = 0; i < coefficients.size(); ++i) {
        implicit.coefficients[i] = static_cast<float>(coefficients[i]);
    }
    return GlImplicitResult::Success;
}

static Point curveMaskWindowPoint(const Point& point, const RenderRegion& passViewport)
{
    return {point.x - passViewport.min.x,
            passViewport.sh() - (point.y - passViewport.min.y)};
}

static std::vector<GlDirectContour> curveDirectContours(
    const RenderPath& path, const RenderRegion& passViewport)
{
    std::vector<GlDirectContour> contours;
    contours.reserve(pathContourCount(path));
    auto pts = path.pts.data;
    Point previous = {};
    GlDirectContour* contour = nullptr;

    ARRAY_FOREACH(cmd, path.cmds)
    {
        switch (*cmd) {
            case PathCommand::MoveTo:
                previous = *pts++;
                contours.emplace_back();
                contour = &contours.back();
                contour->push_back(curveMaskWindowPoint(previous, passViewport));
                break;
            case PathCommand::LineTo:
                previous = *pts++;
                contour->push_back(curveMaskWindowPoint(previous, passViewport));
                break;
            case PathCommand::CubicTo: {
                Bezier curve{previous, pts[0], pts[1], pts[2]};
                auto count = std::max(curve.segments(), 2u);
                auto step = 1.0f / count;
                for (uint32_t i = 1; i <= count; ++i) {
                    contour->push_back(curveMaskWindowPoint(curve.at(step * i),
                                                            passViewport));
                }
                previous = pts[2];
                pts += 3;
                break;
            }
            case PathCommand::Close:
                contour = nullptr;
                break;
        }
    }
    return contours;
}

struct GlDirectSegment
{
    Point start;
    Point end;
};

using GlDirectPrimitive = std::vector<GlDirectSegment>;
using GlDirectPrimitiveContour = std::vector<GlDirectPrimitive>;

static std::vector<GlDirectPrimitiveContour> directPathPrimitives(
    const RenderPath& path)
{
    std::vector<GlDirectPrimitiveContour> contours;
    contours.reserve(pathContourCount(path));
    auto pts = path.pts.data;
    Point previous = {};
    Point contourFirst = {};
    GlDirectPrimitiveContour* contour = nullptr;

    auto appendLine = [&](const Point& start, const Point& end) {
        auto delta = end - start;
        if (!contour || dot(delta, delta) <= FLOAT_EPSILON * FLOAT_EPSILON) return;
        contour->push_back({GlDirectSegment{start, end}});
    };

    ARRAY_FOREACH(cmd, path.cmds)
    {
        switch (*cmd) {
            case PathCommand::MoveTo:
                contourFirst = previous = *pts++;
                contours.emplace_back();
                contour = &contours.back();
                break;
            case PathCommand::LineTo: {
                auto end = *pts++;
                appendLine(previous, end);
                previous = end;
                break;
            }
            case PathCommand::CubicTo: {
                Bezier curve{previous, pts[0], pts[1], pts[2]};
                auto count = std::max(curve.segments(), 2u);
                auto step = 1.0f / count;
                GlDirectPrimitive primitive;
                primitive.reserve(count);
                auto start = previous;
                for (uint32_t i = 1; i <= count; ++i) {
                    auto end = curve.at(step * i);
                    primitive.push_back({start, end});
                    start = end;
                }
                contour->push_back(std::move(primitive));
                previous = pts[2];
                pts += 3;
                break;
            }
            case PathCommand::Close:
                appendLine(previous, contourFirst);
                contour = nullptr;
                break;
        }
    }
    return contours;
}

static float directCross(const Point& a, const Point& b, const Point& c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static bool directPointOnSegment(const Point& a, const Point& b, const Point& point)
{
    if (std::abs(directCross(a, b, point)) > FLOAT_EPSILON) return false;
    return point.x >= std::min(a.x, b.x) - FLOAT_EPSILON &&
           point.x <= std::max(a.x, b.x) + FLOAT_EPSILON &&
           point.y >= std::min(a.y, b.y) - FLOAT_EPSILON &&
           point.y <= std::max(a.y, b.y) + FLOAT_EPSILON;
}

static bool directSegmentsIntersect(const Point& a, const Point& b,
                                    const Point& c, const Point& d)
{
    auto abC = directCross(a, b, c);
    auto abD = directCross(a, b, d);
    auto cdA = directCross(c, d, a);
    auto cdB = directCross(c, d, b);
    if (((abC > FLOAT_EPSILON && abD < -FLOAT_EPSILON) ||
         (abC < -FLOAT_EPSILON && abD > FLOAT_EPSILON)) &&
        ((cdA > FLOAT_EPSILON && cdB < -FLOAT_EPSILON) ||
         (cdA < -FLOAT_EPSILON && cdB > FLOAT_EPSILON))) {
        return true;
    }
    return directPointOnSegment(a, b, c) || directPointOnSegment(a, b, d) ||
           directPointOnSegment(c, d, a) || directPointOnSegment(c, d, b);
}

static float directPointSegmentDistanceSquared(const Point& point,
                                               const Point& a, const Point& b)
{
    auto segment = b - a;
    auto lengthSquared = dot(segment, segment);
    if (lengthSquared <= FLOAT_EPSILON * FLOAT_EPSILON) {
        auto delta = point - a;
        return dot(delta, delta);
    }
    auto projection = clamp(dot(point - a, segment) / lengthSquared,
                            0.0f, 1.0f);
    auto delta = point - (a + segment * projection);
    return dot(delta, delta);
}

static bool directSegmentsWithinAaBand(const Point& a, const Point& b,
                                       const Point& c, const Point& d)
{
    if (directSegmentsIntersect(a, b, c, d)) return true;
    auto minimum = std::min(
        std::min(directPointSegmentDistanceSquared(a, c, d),
                 directPointSegmentDistanceSquared(b, c, d)),
        std::min(directPointSegmentDistanceSquared(c, a, b),
                 directPointSegmentDistanceSquared(d, a, b)));
    auto diameter = GL_FLAT_MASK_AA_RADIUS * 2.0f;
    return minimum < diameter * diameter;
}

static bool directPointsEqual(const Point& a, const Point& b)
{
    auto delta = b - a;
    return dot(delta, delta) <= FLOAT_EPSILON * FLOAT_EPSILON;
}

static bool directSegmentsRetraceAtSharedEndpoint(const GlDirectSegment& first,
                                                  const GlDirectSegment& second)
{
    Point shared;
    Point firstOther;
    Point secondOther;
    if (directPointsEqual(first.end, second.start)) {
        shared = first.end;
        firstOther = first.start;
        secondOther = second.end;
    } else if (directPointsEqual(first.start, second.end)) {
        shared = first.start;
        firstOther = first.end;
        secondOther = second.start;
    } else if (directPointsEqual(first.start, second.start)) {
        shared = first.start;
        firstOther = first.end;
        secondOther = second.end;
    } else if (directPointsEqual(first.end, second.end)) {
        shared = first.end;
        firstOther = first.start;
        secondOther = second.start;
    } else {
        return false;
    }

    auto firstRay = firstOther - shared;
    auto secondRay = secondOther - shared;
    auto firstLengthSquared = dot(firstRay, firstRay);
    auto secondLengthSquared = dot(secondRay, secondRay);
    if (firstLengthSquared <= FLOAT_EPSILON * FLOAT_EPSILON ||
        secondLengthSquared <= FLOAT_EPSILON * FLOAT_EPSILON) {
        return true;
    }
    auto scale = std::sqrt(firstLengthSquared * secondLengthSquared);
    return std::abs(cross(firstRay, secondRay)) <= FLOAT_EPSILON * scale &&
           dot(firstRay, secondRay) > FLOAT_EPSILON * scale;
}

static bool directPathOverlapSensitive(const RenderPath& path)
{
    auto contours = directPathPrimitives(path);
    for (size_t contourIndex = 0; contourIndex < contours.size(); ++contourIndex) {
        const auto& contour = contours[contourIndex];
        for (size_t i = 0; i < contour.size(); ++i) {
            const auto& primitive = contour[i];
            for (size_t firstIndex = 0; firstIndex < primitive.size(); ++firstIndex) {
                for (size_t secondIndex = firstIndex + 1;
                     secondIndex < primitive.size(); ++secondIndex) {
                    const auto& first = primitive[firstIndex];
                    const auto& second = primitive[secondIndex];
                    auto linearGap = secondIndex - firstIndex - 1;
                    auto localGap = linearGap;
                    auto cyclic = contour.size() == 1 &&
                                  directPointsEqual(primitive.front().start,
                                                    primitive.back().end);
                    if (cyclic) {
                        auto cyclicGap = firstIndex + primitive.size() -
                                         secondIndex - 1;
                        localGap = std::min(localGap, cyclicGap);
                    }
                    if (localGap == 0) {
                        if (directSegmentsRetraceAtSharedEndpoint(first, second)) {
                            return true;
                        }
                    } else if (localGap == 1) {
                        if (directSegmentsIntersect(first.start, first.end,
                                                    second.start, second.end)) {
                            return true;
                        }
                    } else if (directSegmentsWithinAaBand(first.start, first.end,
                                                          second.start, second.end)) {
                        return true;
                    }
                }
            }

            for (size_t j = i + 1; j < contour.size(); ++j) {
                const auto& firstPrimitive = contour[i];
                const auto& secondPrimitive = contour[j];
                auto consecutive = j == i + 1;
                auto cyclic = i == 0 && j + 1 == contour.size();
                for (size_t firstIndex = 0; firstIndex < firstPrimitive.size();
                     ++firstIndex) {
                    for (size_t secondIndex = 0;
                         secondIndex < secondPrimitive.size(); ++secondIndex) {
                        const auto& first = firstPrimitive[firstIndex];
                        const auto& second = secondPrimitive[secondIndex];
                        auto localGap = firstPrimitive.size() +
                                        secondPrimitive.size();
                        if (consecutive) {
                            localGap = firstPrimitive.size() - firstIndex - 1 +
                                       secondIndex;
                        }
                        if (cyclic) {
                            auto cyclicGap = secondPrimitive.size() -
                                             secondIndex - 1 + firstIndex;
                            localGap = std::min(localGap, cyclicGap);
                        }
                        if (localGap == 0) {
                            if (directSegmentsRetraceAtSharedEndpoint(first, second)) {
                                return true;
                            }
                        } else if (localGap == 1) {
                            if (directSegmentsIntersect(first.start, first.end,
                                                        second.start, second.end)) {
                                return true;
                            }
                        } else if (directSegmentsWithinAaBand(first.start, first.end,
                                                              second.start, second.end)) {
                            return true;
                        }
                    }
                }
            }
        }

        for (size_t otherIndex = contourIndex + 1;
             otherIndex < contours.size(); ++otherIndex) {
            const auto& other = contours[otherIndex];
            for (const auto& firstPrimitive : contour) {
                for (const auto& secondPrimitive : other) {
                    for (const auto& first : firstPrimitive) {
                        for (const auto& second : secondPrimitive) {
                            if (directSegmentsWithinAaBand(first.start, first.end,
                                                           second.start, second.end)) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

static bool appendCurveMaskPatch(Array<GlCurveMaskVertex>& vertices,
                                 GlCurveMaskPatchKind kind,
                                 const Point& p0, const Point& p1,
                                 const Point& p2, const Point& p3,
                                 const RenderRegion& passViewport)
{
    auto delta = p3 - p0;
    if (dot(delta, delta) == 0.0f && kind == GlCurveMaskPatchKind::Line) return true;

    auto w0 = curveMaskWindowPoint(p0, passViewport);
    auto w1 = curveMaskWindowPoint(p1, passViewport);
    auto w2 = curveMaskWindowPoint(p2, passViewport);
    auto w3 = curveMaskWindowPoint(p3, passViewport);
    GlCubicImplicit implicit;
    if (kind == GlCurveMaskPatchKind::Cubic) {
        auto result = curveMaskImplicitizeCubic(w0, w1, w2, w3, implicit);
        if (result == GlImplicitResult::Empty) return true;
        if (result == GlImplicitResult::Failure) {
            TVGERR("GL_ENGINE", "Curve AA failed to implicitize a cubic boundary patch");
            return false;
        }
    }

    auto patchKind = static_cast<float>(kind == GlCurveMaskPatchKind::Cubic);
    auto vertex = [&](float x, float y) {
        return GlCurveMaskVertex{
            x, y, w0.x, w0.y, w1.x, w1.y, w2.x, w2.y, w3.x, w3.y,
            implicit.coefficients[0], implicit.coefficients[1],
            implicit.coefficients[2], implicit.coefficients[3],
            implicit.coefficients[4], implicit.coefficients[5],
            implicit.coefficients[6], implicit.coefficients[7],
            implicit.coefficients[8], implicit.coefficients[9],
            implicit.center.x, implicit.center.y, implicit.inverseScale, patchKind
        };
    };

    GlCurveMaskVertex v0;
    GlCurveMaskVertex v1;
    GlCurveMaskVertex v2;
    GlCurveMaskVertex v3;
    if (kind == GlCurveMaskPatchKind::Line) {
        auto length = std::sqrt(dot(delta, delta));
        auto tangent = delta / length;
        Point normal{-tangent.y, tangent.x};
        auto start = p0 - tangent * GL_FLAT_MASK_AA_RADIUS;
        auto end = p3 + tangent * GL_FLAT_MASK_AA_RADIUS;
        v0 = vertex(start.x + normal.x * GL_FLAT_MASK_AA_RADIUS,
                    start.y + normal.y * GL_FLAT_MASK_AA_RADIUS);
        v1 = vertex(start.x - normal.x * GL_FLAT_MASK_AA_RADIUS,
                    start.y - normal.y * GL_FLAT_MASK_AA_RADIUS);
        v2 = vertex(end.x + normal.x * GL_FLAT_MASK_AA_RADIUS,
                    end.y + normal.y * GL_FLAT_MASK_AA_RADIUS);
        v3 = vertex(end.x - normal.x * GL_FLAT_MASK_AA_RADIUS,
                    end.y - normal.y * GL_FLAT_MASK_AA_RADIUS);
    } else {
        // A Bezier lies in its control hull; the expanded control AABB is a
        // deliberately broad conservative patch for this experiment.
        auto minX = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x)) - GL_FLAT_MASK_AA_RADIUS;
        auto minY = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y)) - GL_FLAT_MASK_AA_RADIUS;
        auto maxX = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x)) + GL_FLAT_MASK_AA_RADIUS;
        auto maxY = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y)) + GL_FLAT_MASK_AA_RADIUS;
        v0 = vertex(minX, minY);
        v1 = vertex(maxX, minY);
        v2 = vertex(minX, maxY);
        v3 = vertex(maxX, maxY);
    }
    vertices.push(v0);
    vertices.push(v1);
    vertices.push(v2);
    vertices.push(v2);
    vertices.push(v1);
    vertices.push(v3);
    return true;
}

static bool addCurveMaskBoundary(GlRenderTask* task, GlStageBuffer* gpuBuffer,
                                 const RenderPath& path,
                                 const RenderRegion& passViewport)
{
    Array<GlCurveMaskVertex> vertices;
    vertices.reserve(path.cmds.count * 6);
    auto pts = path.pts.data;
    Point previous = {};
    Point contourFirst = {};
    bool contourOpen = false;
    bool success = true;

    auto appendLine = [&](const Point& from, const Point& to) {
        if (!appendCurveMaskPatch(vertices, GlCurveMaskPatchKind::Line,
                                  from, from, to, to, passViewport)) success = false;
    };
    auto finishContour = [&]() {
        if (!contourOpen) return;
        appendLine(previous, contourFirst);
        contourOpen = false;
    };

    ARRAY_FOREACH(cmd, path.cmds) {
        switch (*cmd) {
            case PathCommand::MoveTo:
                finishContour();
                contourFirst = previous = *pts++;
                contourOpen = true;
                break;
            case PathCommand::LineTo: {
                auto end = *pts++;
                if (!contourOpen) {
                    TVGLOG("GL_ENGINE", "Curve-mask POC excludes drawing commands after Close");
                    return false;
                }
                appendLine(previous, end);
                previous = end;
                break;
            }
            case PathCommand::CubicTo: {
                auto end = pts[2];
                if (!contourOpen) {
                    TVGLOG("GL_ENGINE", "Curve-mask POC excludes drawing commands after Close");
                    return false;
                }
                if (!appendCurveMaskPatch(vertices, GlCurveMaskPatchKind::Cubic,
                                          previous, pts[0], pts[1], end,
                                          passViewport)) {
                    success = false;
                }
                previous = end;
                pts += 3;
                break;
            }
            case PathCommand::Close:
                finishContour();
                break;
        }
    }
    finishContour();
    if (!success || vertices.empty()) return false;

    auto vertexOffset = gpuBuffer->push(vertices.data,
                                        vertices.count * sizeof(GlCurveMaskVertex));
    uint32_t* indices = nullptr;
    auto indexOffset = gpuBuffer->reserveIndex(vertices.count * sizeof(uint32_t),
                                               reinterpret_cast<void**>(&indices));
    for (uint32_t i = 0; i < vertices.count; ++i) indices[i] = i;

    auto buffer = gpuBuffer->getBufferId();
    task->addVertexLayout(GlVertexLayout{0, 2, sizeof(GlCurveMaskVertex), vertexOffset, GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{1, 2, sizeof(GlCurveMaskVertex), vertexOffset + 2 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{2, 2, sizeof(GlCurveMaskVertex), vertexOffset + 4 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{3, 2, sizeof(GlCurveMaskVertex), vertexOffset + 6 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{4, 2, sizeof(GlCurveMaskVertex), vertexOffset + 8 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{5, 4, sizeof(GlCurveMaskVertex), vertexOffset + 10 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{6, 4, sizeof(GlCurveMaskVertex), vertexOffset + 14 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{7, 2, sizeof(GlCurveMaskVertex), vertexOffset + 18 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{8, 3, sizeof(GlCurveMaskVertex), vertexOffset + 20 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{9, 1, sizeof(GlCurveMaskVertex), vertexOffset + 23 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->setDrawRange(indexOffset, vertices.count);
    return true;
}

static bool addCurveDirectBoundary(GlRenderTask* task, GlStageBuffer* gpuBuffer,
                                   const RenderPath& path,
                                   const RenderRegion& passViewport,
                                   FillRule fillRule)
{
    Array<GlCurveDirectVertex> vertices;
    vertices.reserve(path.cmds.count * 6);
    Array<GlCurveMaskVertex> contourVertices;
    auto contours = curveDirectContours(path, passViewport);
    size_t contourIndex = 0;
    auto pts = path.pts.data;
    Point previous = {};
    Point contourFirst = {};
    float insideSign = 0.0f;
    bool contourOpen = false;
    bool success = true;

    auto appendLine = [&](const Point& from, const Point& to) {
        if (insideSign == 0.0f) return;
        if (!appendCurveMaskPatch(contourVertices, GlCurveMaskPatchKind::Line,
                                  from, from, to, to, passViewport)) success = false;
    };
    auto appendCubic = [&](const Point& from, const Point& c1,
                           const Point& c2, const Point& to) {
        if (insideSign == 0.0f) return;
        if (!appendCurveMaskPatch(contourVertices, GlCurveMaskPatchKind::Cubic,
                                  from, c1, c2, to, passViewport)) success = false;
    };
    auto finishContour = [&]() {
        if (!contourOpen) return;
        appendLine(previous, contourFirst);
        ARRAY_FOREACH(vertex, contourVertices)
        {
            vertices.push(GlCurveDirectVertex{
                vertex->x, vertex->y,
                vertex->p0x, vertex->p0y, vertex->p1x, vertex->p1y,
                vertex->p2x, vertex->p2y, vertex->p3x, vertex->p3y,
                vertex->c0, vertex->c1, vertex->c2, vertex->c3,
                vertex->c4, vertex->c5, vertex->c6, vertex->c7,
                vertex->c8, vertex->c9,
                vertex->centerX, vertex->centerY, vertex->inverseScale,
                vertex->kind, insideSign});
        }
        contourVertices.clear();
        contourOpen = false;
    };

    ARRAY_FOREACH(cmd, path.cmds)
    {
        switch (*cmd) {
            case PathCommand::MoveTo:
                finishContour();
                contourFirst = previous = *pts++;
                insideSign = directContourInsideSign(contours, contourIndex++,
                                                     fillRule);
                contourOpen = true;
                break;
            case PathCommand::LineTo: {
                auto end = *pts++;
                if (!contourOpen) return false;
                appendLine(previous, end);
                previous = end;
                break;
            }
            case PathCommand::CubicTo: {
                auto end = pts[2];
                if (!contourOpen) return false;
                appendCubic(previous, pts[0], pts[1], end);
                previous = end;
                pts += 3;
                break;
            }
            case PathCommand::Close:
                finishContour();
                break;
        }
    }
    finishContour();
    if (!success || contourIndex != contours.size() || vertices.empty()) return false;

    auto vertexOffset = gpuBuffer->push(vertices.data,
                                        vertices.count * sizeof(GlCurveDirectVertex));
    uint32_t* indices = nullptr;
    auto indexOffset = gpuBuffer->reserveIndex(vertices.count * sizeof(uint32_t),
                                               reinterpret_cast<void**>(&indices));
    for (uint32_t i = 0; i < vertices.count; ++i)
        indices[i] = i;

    auto buffer = gpuBuffer->getBufferId();
    task->addVertexLayout(GlVertexLayout{0, 2, sizeof(GlCurveDirectVertex), vertexOffset, GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{1, 2, sizeof(GlCurveDirectVertex), vertexOffset + 2 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{2, 2, sizeof(GlCurveDirectVertex), vertexOffset + 4 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{3, 2, sizeof(GlCurveDirectVertex), vertexOffset + 6 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{4, 2, sizeof(GlCurveDirectVertex), vertexOffset + 8 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{5, 4, sizeof(GlCurveDirectVertex), vertexOffset + 10 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{6, 4, sizeof(GlCurveDirectVertex), vertexOffset + 14 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{7, 2, sizeof(GlCurveDirectVertex), vertexOffset + 18 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{8, 3, sizeof(GlCurveDirectVertex), vertexOffset + 20 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{9, 1, sizeof(GlCurveDirectVertex), vertexOffset + 23 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->addVertexLayout(GlVertexLayout{10, 1, sizeof(GlCurveDirectVertex), vertexOffset + 24 * sizeof(float), GL_FLOAT, GL_FALSE, buffer});
    task->setDrawRange(indexOffset, vertices.count);
    return true;
}
#endif

GlRenderTask* GlRenderer::createPrimitiveTask(RenderTypes type, BlendSource source, const RenderRegion& viewRegion, GlRenderTarget*& dstCopyFbo)
{
    dstCopyFbo = nullptr;

    if (mBlendMethod == BlendMethod::Normal) return new GlRenderTask(mPrograms[type]);

    if (mBlendPool.empty()) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h, mRootTarget.samples, mStateCache));
#if defined(THORVG_GL_TARGET_GL)
    dstCopyFbo = mBlendPool[0]->getRenderTarget(viewRegion);
#else  // TODO: create partial buffer when MSAA is disabled
    dstCopyFbo = mBlendPool[0]->getRenderTarget(currentPass()->getViewport());
#endif

    auto program = getBlendProgram(mBlendMethod, source);
    return new GlDirectBlendTask(program, currentPass()->fbo, dstCopyFbo, viewRegion);
}

void GlRenderer::bindBlendTarget(GlRenderTask* task, const GlRenderTarget* dstCopyFbo, const RenderRegion& viewRegion, uint32_t binding)
{
    if (!dstCopyFbo) return;

#if defined(THORVG_GL_TARGET_GL)
    float region[] = {float(viewRegion.sx()), float(viewRegion.sy()), float(dstCopyFbo->width), float(dstCopyFbo->height)};
#else  // TODO: create partial buffer when MSAA is disabled
    float region[] = {0.0f, 0.0f, float(dstCopyFbo->width), float(dstCopyFbo->height)};
#endif
    task->addBindResource(GlBindingResource{
        binding,
        GlShaderUniformBlock::BlendRegion,
        mGpuBuffer.getBufferId(),
        mGpuBuffer.push(region, 4 * sizeof(float), true),
        4 * sizeof(float),
    });
    task->addBindResource(GlBindingResource{0, dstCopyFbo->colorTex, GlShaderUniform::DestinationTexture});
}

#if defined(THORVG_GL_FLAT_MASK_SUPPORT)
bool GlRenderer::drawFlatDirect(GlShape& sdata, const RenderColor& color,
                                int32_t depth, const RenderRegion& viewBounds,
                                const RenderRegion& passViewport)
{
    auto& geometry = sdata.geometry;
    if (geometry.optPathSkipFill || geometry.optPathThin || geometry.fillBoundary.edges.empty() || !geometry.fillBoundary.supported || !directPathClosed(geometry.curveMaskPath) || !sdata.clips.empty()) return false;

    auto shapeBounds = geometry.fillBounds;
    auto expansion = static_cast<int32_t>(ceilf(GL_FLAT_MASK_AA_RADIUS));
    shapeBounds.min.x -= expansion;
    shapeBounds.min.y -= expansion;
    shapeBounds.max.x += expansion;
    shapeBounds.max.y += expansion;
    shapeBounds.intersect(viewBounds);
    shapeBounds.intersect(passViewport);
    if (shapeBounds.invalid()) return false;

    auto shapeRegion = viewportRegion(passViewport, shapeBounds);
    auto viewMatrix = currentPass()->getViewMatrix();
    auto stencilMode = geometry.fillRule == FillRule::EvenOdd ? GlStencilMode::FillEvenOdd : GlStencilMode::FillNonZero;
    auto alpha = MULTIPLY(color.a, sdata.opacity);

    mSolidBatch.clear();
    mStencilCoverBatch.clear();

    auto stencilTask = new GlRenderTask(mPrograms[RT_Stencil]);
    stencilTask->setViewMatrix(viewMatrix);
    stencilTask->setDrawDepth(depth);
    stencilTask->setViewport(shapeRegion);
    geometry.draw(stencilTask, &mGpuBuffer, RenderUpdateFlag::Path);

    auto boundaryTask = new GlRenderTask(mPrograms[RT_FlatDirectBoundary]);
    boundaryTask->setViewMatrix(viewMatrix);
    boundaryTask->setDrawDepth(depth);
    boundaryTask->setViewport(shapeRegion);
    boundaryTask->setVertexColor(color.r / 255.0f, color.g / 255.0f,
                                 color.b / 255.0f, alpha / 255.0f);
    if (!addFlatDirectBoundary(boundaryTask, &mGpuBuffer, geometry,
                               passViewport)) {
        delete stencilTask;
        delete boundaryTask;
        return false;
    }

    auto coverTask = new GlRenderTask(mPrograms[RT_Color]);
    coverTask->setViewMatrix(viewMatrix);
    coverTask->setDrawDepth(depth);
    coverTask->setViewport(shapeRegion);
    coverTask->setVertexColor(color.r / 255.0f, color.g / 255.0f,
                              color.b / 255.0f, alpha / 255.0f);
    addFlatMaskQuad(coverTask, &mGpuBuffer, shapeBounds);

    currentPass()->addRenderTask(new GlDirectAaTask(
        currentPass()->fbo, stencilTask, boundaryTask, coverTask, stencilMode,
        shapeRegion, passViewport.w(), passViewport.h()));
    return true;
}

bool GlRenderer::drawCurveDirect(GlShape& sdata, const RenderColor& color,
                                 int32_t depth, const RenderRegion& viewBounds,
                                 const RenderRegion& passViewport)
{
    auto& geometry = sdata.geometry;
    if (geometry.optPathSkipFill || geometry.optPathThin || geometry.curveMaskPath.empty() || !directPathClosed(geometry.curveMaskPath) || !sdata.clips.empty()) return false;

    auto shapeBounds = geometry.fillBounds;
    auto expansion = static_cast<int32_t>(ceilf(GL_FLAT_MASK_AA_RADIUS));
    shapeBounds.min.x -= expansion;
    shapeBounds.min.y -= expansion;
    shapeBounds.max.x += expansion;
    shapeBounds.max.y += expansion;
    shapeBounds.intersect(viewBounds);
    shapeBounds.intersect(passViewport);
    if (shapeBounds.invalid()) return false;

    auto shapeRegion = viewportRegion(passViewport, shapeBounds);
    auto viewMatrix = currentPass()->getViewMatrix();
    auto stencilMode = geometry.fillRule == FillRule::EvenOdd ? GlStencilMode::FillEvenOdd : GlStencilMode::FillNonZero;
    auto alpha = MULTIPLY(color.a, sdata.opacity);

    mSolidBatch.clear();
    mStencilCoverBatch.clear();

    auto stencilTask = new GlRenderTask(mPrograms[RT_Stencil]);
    stencilTask->setViewMatrix(viewMatrix);
    stencilTask->setDrawDepth(depth);
    stencilTask->setViewport(shapeRegion);
    geometry.draw(stencilTask, &mGpuBuffer, RenderUpdateFlag::Path);

    auto boundaryTask = new GlRenderTask(mPrograms[RT_CurveDirectBoundary]);
    boundaryTask->setViewMatrix(viewMatrix);
    boundaryTask->setDrawDepth(depth);
    boundaryTask->setViewport(shapeRegion);
    boundaryTask->setVertexColor(color.r / 255.0f, color.g / 255.0f,
                                 color.b / 255.0f, alpha / 255.0f);
    if (!addCurveDirectBoundary(boundaryTask, &mGpuBuffer,
                                geometry.curveMaskPath, passViewport,
                                geometry.fillRule)) {
        delete stencilTask;
        delete boundaryTask;
        return false;
    }

    auto coverTask = new GlRenderTask(mPrograms[RT_Color]);
    coverTask->setViewMatrix(viewMatrix);
    coverTask->setDrawDepth(depth);
    coverTask->setViewport(shapeRegion);
    coverTask->setVertexColor(color.r / 255.0f, color.g / 255.0f,
                              color.b / 255.0f, alpha / 255.0f);
    addFlatMaskQuad(coverTask, &mGpuBuffer, shapeBounds);

    currentPass()->addRenderTask(new GlDirectAaTask(
        currentPass()->fbo, stencilTask, boundaryTask, coverTask, stencilMode,
        shapeRegion, passViewport.w(), passViewport.h()));
    return true;
}

bool GlRenderer::drawFlatMask(GlShape& sdata, const RenderColor& color, int32_t depth,
                              const RenderRegion& viewBounds, const RenderRegion& passViewport)
{
    auto& geometry = sdata.geometry;
    if (geometry.optPathSkipFill || !sdata.clips.empty()) return false;
    if (geometry.optPathThin) {
        TVGLOG("GL_ENGINE", "Flat-mask POC excludes thin fills; using the current GL fill path");
        return false;
    }
    if (!geometry.fillBoundary.supported) {
        TVGLOG("GL_ENGINE", "Flat-mask POC excludes overlapping, touching, nearby, or noncanonical boundary segments; using the current GL fill path");
        return false;
    }
    if (geometry.fillBoundary.edges.empty()) return false;

    auto maskBounds = geometry.fillBounds;
    auto expansion = static_cast<int32_t>(ceilf(GL_FLAT_MASK_AA_RADIUS));
    maskBounds.min.x -= expansion;
    maskBounds.min.y -= expansion;
    maskBounds.max.x += expansion;
    maskBounds.max.y += expansion;
    maskBounds.intersect(viewBounds);
    maskBounds.intersect(passViewport);
    if (maskBounds.invalid()) return false;

    auto maskRegion = viewportRegion(passViewport, maskBounds);
    auto viewMatrix = currentPass()->getViewMatrix();
    auto stencilMode = (geometry.fillRule == FillRule::EvenOdd) ? GlStencilMode::FillEvenOdd : GlStencilMode::FillNonZero;

    // Flat-mask writes standalone streams. Close open batches before reserving
    // them so a failed boundary build can safely fall back to the legacy path.
    mSolidBatch.clear();
    mStencilCoverBatch.clear();

    auto stencilTask = new GlRenderTask(mPrograms[RT_Stencil]);
    stencilTask->setViewMatrix(viewMatrix);
    stencilTask->setDrawDepth(depth);
    stencilTask->setViewport(maskRegion);
    geometry.draw(stencilTask, &mGpuBuffer, RenderUpdateFlag::Path);

    auto interiorTask = new GlRenderTask(mPrograms[RT_FlatMaskInterior]);
    interiorTask->setViewMatrix(viewMatrix);
    interiorTask->setDrawDepth(depth);
    interiorTask->setViewport(maskRegion);
    addFlatMaskQuad(interiorTask, &mGpuBuffer, maskBounds);

    auto insidePositiveTask = new GlRenderTask(mPrograms[RT_FlatMaskEdgeInsidePositive]);
    auto insideNegativeTask = new GlRenderTask(mPrograms[RT_FlatMaskEdgeInsideNegative]);
    auto outsideTask = new GlRenderTask(mPrograms[RT_FlatMaskEdgeOutside]);
    insidePositiveTask->setViewMatrix(viewMatrix);
    insidePositiveTask->setDrawDepth(depth);
    insidePositiveTask->setViewport(maskRegion);
    insideNegativeTask->setViewMatrix(viewMatrix);
    insideNegativeTask->setDrawDepth(depth);
    insideNegativeTask->setViewport(maskRegion);
    outsideTask->setViewMatrix(viewMatrix);
    outsideTask->setDrawDepth(depth);
    outsideTask->setViewport(maskRegion);
    if (!addFlatMaskBoundary(insidePositiveTask, insideNegativeTask, outsideTask,
                             &mGpuBuffer, geometry, passViewport)) {
        delete stencilTask;
        delete interiorTask;
        delete insidePositiveTask;
        delete insideNegativeTask;
        delete outsideTask;
        return false;
    }

    auto compositeTask = new GlRenderTask(mPrograms[RT_FlatMaskComposite]);
    compositeTask->setViewMatrix(viewMatrix);
    compositeTask->setDrawDepth(depth);
    compositeTask->setViewport(maskRegion);
    auto alpha = MULTIPLY(color.a, sdata.opacity);
    compositeTask->setVertexColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, alpha / 255.0f);
    compositeTask->addBindResource(GlBindingResource{0, mFlatMaskTarget.colorTex, GlShaderUniform::MaskTexture});
    addFlatMaskQuad(compositeTask, &mGpuBuffer, maskBounds);

    currentPass()->addRenderTask(new GlFlatMaskTask(&mFlatMaskTarget, currentPass()->fbo,
        stencilTask, interiorTask, insidePositiveTask, insideNegativeTask, outsideTask,
        compositeTask, stencilMode, maskRegion,
        passViewport.w(), passViewport.h()));
    return true;
}

bool GlRenderer::drawCurveMask(GlShape& sdata, const RenderColor& color, int32_t depth,
                               const RenderRegion& viewBounds,
                               const RenderRegion& passViewport)
{
    auto& geometry = sdata.geometry;
    if (geometry.optPathSkipFill || geometry.curveMaskPath.empty()) return false;
    if (geometry.optPathThin) {
        TVGLOG("GL_ENGINE", "Curve-mask POC excludes thin fills; using the current GL fill path");
        return false;
    }
    if (!sdata.clips.empty()) {
        TVGLOG("GL_ENGINE", "Curve-mask POC excludes clip paths; using the current GL fill path");
        return false;
    }

    auto maskBounds = geometry.fillBounds;
    auto expansion = static_cast<int32_t>(ceilf(GL_FLAT_MASK_AA_RADIUS));
    maskBounds.min.x -= expansion;
    maskBounds.min.y -= expansion;
    maskBounds.max.x += expansion;
    maskBounds.max.y += expansion;
    maskBounds.intersect(viewBounds);
    maskBounds.intersect(passViewport);
    if (maskBounds.invalid()) return false;

    auto maskRegion = viewportRegion(passViewport, maskBounds);
    auto viewMatrix = currentPass()->getViewMatrix();
    auto stencilMode = (geometry.fillRule == FillRule::EvenOdd) ?
                       GlStencilMode::FillEvenOdd : GlStencilMode::FillNonZero;

    // Curve-mask writes standalone streams. Invalidate any open batches before
    // reserving them so a later POC fallback cannot bridge across these ranges.
    mSolidBatch.clear();
    mStencilCoverBatch.clear();

    auto stencilTask = new GlRenderTask(mPrograms[RT_Stencil]);
    stencilTask->setViewMatrix(viewMatrix);
    stencilTask->setDrawDepth(depth);
    stencilTask->setViewport(maskRegion);
    geometry.draw(stencilTask, &mGpuBuffer, RenderUpdateFlag::Path);

    auto interiorTask = new GlRenderTask(mPrograms[RT_CurveMaskInterior]);
    interiorTask->setViewMatrix(viewMatrix);
    interiorTask->setDrawDepth(depth);
    interiorTask->setViewport(maskRegion);
    addFlatMaskQuad(interiorTask, &mGpuBuffer, maskBounds);

    auto boundaryTask = new GlRenderTask(mPrograms[RT_CurveMaskBoundary]);
    boundaryTask->setViewMatrix(viewMatrix);
    boundaryTask->setDrawDepth(depth);
    boundaryTask->setViewport(maskRegion);
    if (!addCurveMaskBoundary(boundaryTask, &mGpuBuffer, geometry.curveMaskPath,
                              passViewport)) {
        delete stencilTask;
        delete interiorTask;
        delete boundaryTask;
        return false;
    }

    auto compositeTask = new GlRenderTask(mPrograms[RT_CurveMaskComposite]);
    compositeTask->setViewMatrix(viewMatrix);
    compositeTask->setDrawDepth(depth);
    compositeTask->setViewport(maskRegion);
    auto alpha = MULTIPLY(color.a, sdata.opacity);
    compositeTask->setVertexColor(color.r / 255.0f, color.g / 255.0f,
                                  color.b / 255.0f, alpha / 255.0f);
    compositeTask->addBindResource(GlBindingResource{0, mFlatMaskTarget.colorTex, GlShaderUniform::MaskTexture});
    addFlatMaskQuad(compositeTask, &mGpuBuffer, maskBounds);

    currentPass()->addRenderTask(new GlCurveMaskTask(
        &mFlatMaskTarget, currentPass()->fbo, stencilTask, interiorTask,
        boundaryTask, compositeTask, stencilMode, maskRegion,
        passViewport.w(), passViewport.h()));
    return true;
}
#endif

#if defined(THORVG_GL_FLAT_MASK_SUPPORT)
static GlAaMode solidAaRoute(GlAaMode mode, uint8_t effectiveAlpha,
                             bool overlapSensitive)
{
    if (mode != GlAaMode::Hybrid) return mode;
    return effectiveAlpha < 255 || overlapSensitive ? GlAaMode::CurveMask : GlAaMode::CurveDirect;
}
#endif

void GlRenderer::drawPrimitive(GlShape& sdata, const RenderColor& c,
                               RenderUpdateFlag flag, int32_t depth,
                               bool routeAa)
{
    if (!sdata.geometry.drawable(flag)) return;

    auto blendShape = (mBlendMethod != BlendMethod::Normal);
    auto vp = currentPass()->getViewport();
    // geometry.viewport carries the fast-tracked clip bounds; the pass
    // viewport can still be the full framebuffer.
    auto viewBounds = sdata.geometry.viewport;
    viewBounds.intersect(vp);
    if (viewBounds.invalid()) return;

    auto stroke = (flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke);
    auto bbox = stroke ? gpuTransformBounds(sdata.geometry.strokeBounds, sdata.geometry.matrix) : sdata.geometry.fillBounds;
    bbox.intersect(viewBounds);
    if (bbox.invalid()) return;

    auto viewRegion = viewportRegion(vp, bbox);
    auto stencilMode = sdata.geometry.getStencilMode(flag);

    if (routeAa) {
        if (mAaMode == GlAaMode::NoAa) {
            ++mAaStats.noAa;
        } else if (mAaMode == GlAaMode::Msaa4) {
            ++mAaStats.msaa4;
        } else {
            auto rendered = false;
#if defined(THORVG_GL_FLAT_MASK_SUPPORT)
            if (flag == RenderUpdateFlag::Color && mBlendMethod == BlendMethod::Normal && sdata.clips.empty()) {
                auto effectiveAlpha = MULTIPLY(c.a, sdata.opacity);
                auto overlapSensitive = mAaMode == GlAaMode::Hybrid &&
                                        directPathClosed(sdata.geometry.curveMaskPath) &&
                                        directPathOverlapSensitive(sdata.geometry.curveMaskPath);
                auto route = solidAaRoute(mAaMode, effectiveAlpha,
                                          overlapSensitive);
                switch (route) {
                    case GlAaMode::FlatDirect:
                        rendered = drawFlatDirect(sdata, c, depth, viewBounds, vp);
                        if (rendered) ++mAaStats.flatDirect;
                        break;
                    case GlAaMode::CurveDirect:
                        rendered = drawCurveDirect(sdata, c, depth, viewBounds, vp);
                        if (rendered) ++mAaStats.curveDirect;
                        break;
                    case GlAaMode::FlatMask:
                        rendered = drawFlatMask(sdata, c, depth, viewBounds, vp);
                        if (rendered) ++mAaStats.flatMask;
                        break;
                    case GlAaMode::CurveMask:
                        rendered = drawCurveMask(sdata, c, depth, viewBounds, vp);
                        if (rendered) ++mAaStats.curveMask;
                        break;
                    default:
                        break;
                }
            }
#endif
            if (rendered) return;
            ++mAaStats.fallback;
        }
    }

    if (!blendShape && stencilMode == GlStencilMode::None && sdata.clips.empty()) {
        mSolidBatch.draw(*this, sdata, c, depth, viewRegion, viewportRegion(vp, viewBounds));
        return;
    }

    if (!sdata.clips.empty()) mSolidBatch.clear();

    GlRenderTarget* dstCopyFbo = nullptr;
    auto task = createPrimitiveTask(RT_Color, BlendSource::Solid, viewRegion, dstCopyFbo);
    auto viewMatrix = _viewMatrix(sdata.geometry, currentPass()->getViewMatrix(), flag);

    task->setViewMatrix(viewMatrix);
    task->setDrawDepth(depth);

    auto a = MULTIPLY(c.a, sdata.opacity);
    if (flag & RenderUpdateFlag::Stroke) {
        auto strokeWidth = sdata.geometry.strokeRenderWidth;
        if (strokeWidth < MIN_GL_STROKE_WIDTH) {
            auto alpha = strokeWidth / MIN_GL_STROKE_WIDTH;
            a = MULTIPLY(a, static_cast<uint8_t>(alpha * 255));
        }
    }
    RenderColor color = {c.r, c.g, c.b, a};
    if (stencilMode == GlStencilMode::None) task->setVertexColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);
    task->setViewport(viewRegion);

    RenderRegion stencilBounds{};
    const GlGeometryBuffer* stencilBuffer = nullptr;
    uint32_t* stencilIndices = nullptr;
    bool merge = false;
    auto clipped = !sdata.clips.empty();
    auto pass = currentPass();
    auto stencilTask = drawPrimitiveGeometry(mPrograms[RT_Stencil], task, sdata.geometry, mStencilCoverBatch, pass, &mGpuBuffer, flag, stencilMode, clipped, depth, viewMatrix, vp, &color, viewBounds, stencilBounds, stencilBuffer, stencilIndices, merge);
    // Keep BlendRegion on the existing solid-shape blend UBO slot.
    bindBlendTarget(task, dstCopyFbo, viewRegion, 2);

    if (stencilTask) mStencilCoverBatch.draw(pass, stencilTask, task, merge, stencilMode, clipped, stencilBounds, viewBounds, stencilBuffer, stencilIndices);
    else pass->addRenderTask(task);
}

void GlRenderer::drawPrimitive(GlShape& sdata, const Fill* fill,
                               RenderUpdateFlag flag, int32_t depth,
                               bool routeAa)
{
    if (!sdata.geometry.drawable(flag)) return;

    auto vp = currentPass()->getViewport();
    // geometry.viewport carries the fast-tracked clip bounds; the pass
    // viewport can still be the full framebuffer.
    auto viewBounds = sdata.geometry.viewport;
    viewBounds.intersect(vp);
    if (viewBounds.invalid()) return;

    auto stroke = (flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke);
    auto bbox = stroke ? gpuTransformBounds(sdata.geometry.strokeBounds, sdata.geometry.matrix) : sdata.geometry.fillBounds;
    bbox.intersect(viewBounds);
    if (bbox.invalid()) return;

    const Fill::ColorStop* stops = nullptr;
    auto colorStopCnt = fill->colorStops(&stops);
    auto stopCnt = min(colorStopCnt, static_cast<uint32_t>(MAX_GRADIENT_STOPS));
    if (stopCnt < 1) return;

    if (routeAa) {
        if (mAaMode == GlAaMode::NoAa) ++mAaStats.noAa;
        else if (mAaMode == GlAaMode::Msaa4) ++mAaStats.msaa4;
        else ++mAaStats.fallback;
    }

    GlRenderTarget* dstCopyFbo = nullptr;
    auto radial = fill->type() == Type::RadialGradient;
    auto viewRegion = viewportRegion(vp, bbox);

    RenderTypes taskType = RT_None;
    auto blendSource = BlendSource::LinearGradient;

    float x, y, r, fx, fy, fr;

    if (fill->type() == Type::LinearGradient) {
        taskType = RT_LinGradient;
    } else if (radial) {
        auto radialFill = static_cast<const RadialGradient*>(fill);
        radialFill->radial(&x, &y, &r, &fx, &fy, &fr);
        // Uncorrectable radial gradients use the last stop as a solid color.
        if (!CONST_RADIAL(radialFill)->correct(fx, fy, fr)) {
            auto& stop = stops[colorStopCnt - 1];
            RenderColor color = {stop.r, stop.g, stop.b, stop.a};
            auto solidFlag = (flag & RenderUpdateFlag::GradientStroke) ? RenderUpdateFlag::Stroke : RenderUpdateFlag::Color;
            drawPrimitive(sdata, color, solidFlag, depth, false);
            return;
        }

        taskType = RT_RadGradient;
        blendSource = BlendSource::RadialGradient;
    } else return;

    auto task = createPrimitiveTask(taskType, blendSource, viewRegion, dstCopyFbo);
    auto viewMatrix = _viewMatrix(sdata.geometry, currentPass()->getViewMatrix(), flag);

    task->setViewMatrix(viewMatrix);
    task->setDrawDepth(depth);

    task->setViewport(viewRegion);

    GlStencilMode stencilMode = sdata.geometry.getStencilMode(flag);
    RenderRegion stencilBounds{};
    const GlGeometryBuffer* stencilBuffer = nullptr;
    uint32_t* stencilIndices = nullptr;
    bool merge = false;
    auto pass = currentPass();
    auto clipped = !sdata.clips.empty();
    auto stencilTask = drawPrimitiveGeometry(mPrograms[RT_Stencil], task, sdata.geometry, mStencilCoverBatch, pass, &mGpuBuffer, flag, stencilMode, clipped, depth, viewMatrix, vp, nullptr, viewBounds, stencilBounds, stencilBuffer, stencilIndices, merge);

    // transform buffer (inverse fill-space transform)
    float invMat3[GL_MAT3_STD140_SIZE];
    Matrix inv;
    inverse(&fill->transform(), &inv);
    if (!(flag & RenderUpdateFlag::GradientStroke)) {
        // World-space meshes need inverse model before gradient lookup. Local
        // gradient strokes already pass local positions to TransformInfo.
        Matrix invShape;
        inverse(&sdata.geometry.matrix, &invShape);
        inv = inv * invShape;
    }
    getMatrix3Std140(inv, invMat3);

    float transformInfo[GL_MAT3_STD140_SIZE];
    memcpy(transformInfo, invMat3, GL_MAT3_STD140_BYTES);
    auto transformOffset = mGpuBuffer.push(transformInfo, sizeof(transformInfo), true);

    task->addBindResource(GlBindingResource{
        0,
        GlShaderUniformBlock::TransformInfo,
        mGpuBuffer.getBufferId(),
        transformOffset,
        sizeof(transformInfo),
    });

    auto alpha = sdata.opacity / 255.f;

    if (flag & RenderUpdateFlag::GradientStroke) {
        auto strokeWidth = sdata.geometry.strokeRenderWidth;
        if (strokeWidth < MIN_GL_STROKE_WIDTH) {
            alpha = strokeWidth / MIN_GL_STROKE_WIDTH;
        }
    }

    // gradient block
    GlBindingResource gradientBinding{};

    if (fill->type() == Type::LinearGradient) {
        auto linearFill = static_cast<const LinearGradient*>(fill);

        GlLinearGradientBlock gradientBlock;

        gradientBlock.nStops[1] = NOISE_LEVEL;
        gradientBlock.nStops[2] = static_cast<int32_t>(fill->spread()) * 1.f;
        uint32_t nStops = 0;
        for (uint32_t i = 0; i < stopCnt; ++i) {
            if (i > 0 && gradientBlock.stopPoints[nStops - 1] > stops[i].offset) continue;

            gradientBlock.stopPoints[i] = stops[i].offset;
            gradientBlock.stopColors[i * 4 + 0] = stops[i].r / 255.f;
            gradientBlock.stopColors[i * 4 + 1] = stops[i].g / 255.f;
            gradientBlock.stopColors[i * 4 + 2] = stops[i].b / 255.f;
            gradientBlock.stopColors[i * 4 + 3] = stops[i].a / 255.f * alpha;
            nStops++;
        }
        gradientBlock.nStops[0] = nStops * 1.f;

        float x1, x2, y1, y2;
        linearFill->linear(&x1, &y1, &x2, &y2);

        gradientBlock.startPos[0] = x1;
        gradientBlock.startPos[1] = y1;
        gradientBlock.stopPos[0] = x2;
        gradientBlock.stopPos[1] = y2;

        gradientBinding = GlBindingResource{
            2,
            GlShaderUniformBlock::GradientInfo,
            mGpuBuffer.getBufferId(),
            mGpuBuffer.push(&gradientBlock, sizeof(GlLinearGradientBlock), true),
            sizeof(GlLinearGradientBlock),
        };
    } else {
        GlRadialGradientBlock gradientBlock;

        gradientBlock.nStops[1] = NOISE_LEVEL;
        gradientBlock.nStops[2] = static_cast<int32_t>(fill->spread()) * 1.f;

        uint32_t nStops = 0;
        for (uint32_t i = 0; i < stopCnt; ++i) {
            if (i > 0 && gradientBlock.stopPoints[nStops - 1] > stops[i].offset) continue;

            gradientBlock.stopPoints[i] = stops[i].offset;
            gradientBlock.stopColors[i * 4 + 0] = stops[i].r / 255.f;
            gradientBlock.stopColors[i * 4 + 1] = stops[i].g / 255.f;
            gradientBlock.stopColors[i * 4 + 2] = stops[i].b / 255.f;
            gradientBlock.stopColors[i * 4 + 3] = stops[i].a / 255.f * alpha;
            nStops++;
        }
        gradientBlock.nStops[0] = nStops * 1.f;

        gradientBlock.centerPos[0] = fx;
        gradientBlock.centerPos[1] = fy;
        gradientBlock.centerPos[2] = x;
        gradientBlock.centerPos[3] = y;
        gradientBlock.radius[0] = fr;
        gradientBlock.radius[1] = r;

        gradientBinding = GlBindingResource{
            2,
            GlShaderUniformBlock::GradientInfo,
            mGpuBuffer.getBufferId(),
            mGpuBuffer.push(&gradientBlock, sizeof(GlRadialGradientBlock), true),
            sizeof(GlRadialGradientBlock),
        };
    }

    task->addBindResource(gradientBinding);

    // TransformInfo uses slot 0 and GradientInfo uses slot 2, so BlendRegion moves to 3.
    bindBlendTarget(task, dstCopyFbo, viewRegion, 3);

    if (stencilTask) mStencilCoverBatch.draw(pass, stencilTask, task, merge, stencilMode, clipped, stencilBounds, viewBounds, stencilBuffer, stencilIndices);
    else pass->addRenderTask(task);
}


void GlRenderer::drawClip(Array<RenderData>& clips, const RenderRegion& viewBounds)
{
    if (viewBounds.invalid()) return;

    // Clip is a render boundary.
    mStencilCoverBatch.clear();

    // Clip masks must stay inside the target paint view bounds. Fast-tracked
    // Lottie clips narrow geometry.viewport while the pass viewport remains
    // full-size; using the pass viewport here can affect neighboring animations.
    auto identityVertexOffset = mGpuBuffer.push((void*)IDENTITY_VERTEX, sizeof(IDENTITY_VERTEX));
    auto identityIndexOffset = mGpuBuffer.pushIndex((void*)RECT_INDEX, sizeof(RECT_INDEX));

    Array<int32_t> clipDepths(clips.count);
    clipDepths.count = clips.count;

    for (int32_t i = clips.count - 1; i >= 0; i--) {
        clipDepths[i] = currentPass()->nextDrawDepth();
    }

    const auto& passViewport = currentPass()->getViewport();
    const auto& viewMatrix = currentPass()->getViewMatrix();
    const auto viewRegion = viewportRegion(passViewport, viewBounds);

    for (uint32_t i = 0; i < clips.count; ++i) {
        auto sdata = static_cast<GlShape*>(clips[i]);
        auto flag = (sdata->geometry.stroke.vertex.count > 0) ? RenderUpdateFlag::Stroke : RenderUpdateFlag::Path;

        auto clipTask = new GlRenderTask(mPrograms[RT_Stencil]);
        clipTask->setDrawDepth(clipDepths[i]);
        clipTask->setViewMatrix(_viewMatrix(sdata->geometry, viewMatrix, flag));
        sdata->geometry.draw(clipTask, &mGpuBuffer, flag);

        auto clipBounds = sdata->geometry.getBounds();
        clipBounds.intersect(viewBounds);
        clipTask->setViewport(viewportRegion(passViewport, clipBounds));

        auto maskTask = new GlRenderTask(mPrograms[RT_Stencil]);

        maskTask->setDrawDepth(clipDepths[i]);
        maskTask->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), identityVertexOffset, GL_FLOAT, GL_FALSE, mGpuBuffer.getBufferId()});
        maskTask->setDrawRange(identityIndexOffset, RECT_INDEX_COUNT);
        maskTask->setViewport(viewRegion);

        currentPass()->addRenderTask(new GlClipTask(clipTask, maskTask));
    }
}


GlRenderPass* GlRenderer::currentPass()
{
    if (mRenderPassStack.empty()) return nullptr;
    return mRenderPassStack.last();
}


bool GlRenderer::beginComplexBlending(const RenderRegion& vp, RenderRegion bounds)
{
    if (vp.invalid()) return false;

    bounds.intersect(vp);
    if (bounds.invalid()) return false;

    if (mBlendMethod == BlendMethod::Normal) return false;

    if (mBlendPool.empty()) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h, mRootTarget.samples, mStateCache));

    auto blendFbo = mBlendPool[0]->getRenderTarget(bounds);

    mRenderPassStack.push(new GlRenderPass(blendFbo));

    return true;
}

void GlRenderer::endBlendingCompose(GlRenderTask* stencilTask)
{
    auto blendPass = mRenderPassStack.pick();
    blendPass->setDrawDepth(currentPass()->nextDrawDepth());

    auto composeTask = blendPass->endRenderPass<GlComposeTask>(nullptr, currentPass()->getFboId());

    const auto& vp = blendPass->getViewport();
    if (mBlendPool.count < 2) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h, mRootTarget.samples, mStateCache));
#if defined(THORVG_GL_TARGET_GL)
    auto dstCopyFbo = mBlendPool[1]->getRenderTarget(vp);
#else // TODO: create partial buffer when MSAA is disabled        
    auto dstCopyFbo = mBlendPool[1]->getRenderTarget(currentPass()->getViewport());
#endif

    auto x = vp.sx();
    auto y = currentPass()->getViewport().sh() - vp.sy() - vp.sh();
    stencilTask->setViewport({{x, y}, {x + vp.sw(), y + vp.sh()}});

    stencilTask->setDrawDepth(currentPass()->nextDrawDepth());
    stencilTask->setViewMatrix(currentPass()->getViewMatrix());
    
    auto program = getBlendProgram(mBlendMethod, BlendSource::Image);
    auto task = new GlComplexBlendTask(program, currentPass()->fbo, dstCopyFbo, stencilTask, composeTask);
    prepareCmpTask(task, vp, blendPass->getFboWidth(), blendPass->getFboHeight());
    task->setDrawDepth(currentPass()->nextDrawDepth());

#if defined(THORVG_GL_TARGET_GLES)
    float region[] = {0.0f, 0.0f, float(dstCopyFbo->width), float(dstCopyFbo->height)};
    task->addBindResource(GlBindingResource{
        0,
        GlShaderUniformBlock::BlendRegion,
        mGpuBuffer.getBufferId(),
        mGpuBuffer.push(region, 4 * sizeof(float), true),
        4 * sizeof(float),
    });
#endif

    // src and dst texture
    task->addBindResource(GlBindingResource{1, blendPass->fbo->colorTex, GlShaderUniform::SourceTexture});
    task->addBindResource(GlBindingResource{2, dstCopyFbo->colorTex, GlShaderUniform::DestinationTexture});

    currentPass()->addRenderTask(task);

    delete(blendPass);
}


GlProgram* GlRenderer::getBlendProgram(BlendMethod method, BlendSource source)
{
    // custom blend shaders
    static const char* shaderFunc[17] {
        NORMAL_BLEND_FRAG,
        MULTIPLY_BLEND_FRAG,
        SCREEN_BLEND_FRAG,
        OVERLAY_BLEND_FRAG,
        DARKEN_BLEND_FRAG,
        LIGHTEN_BLEND_FRAG,
        COLOR_DODGE_BLEND_FRAG,
        COLOR_BURN_BLEND_FRAG,
        HARD_LIGHT_BLEND_FRAG,
        SOFT_LIGHT_BLEND_FRAG,
        DIFFERENCE_BLEND_FRAG,
        EXCLUSION_BLEND_FRAG,
        HUE_BLEND_FRAG,
        SATURATION_BLEND_FRAG,
        COLOR_BLEND_FRAG,
        LUMINOSITY_BLEND_FRAG,
        ADD_BLEND_FRAG
    };

    uint32_t methodInd = (uint32_t)method;
    uint32_t shaderInd = methodInd;

    switch (source) {
        case BlendSource::Scene: shaderInd += (uint32_t)RT_Blend_Scene_Normal; break;
        case BlendSource::Image: shaderInd += (uint32_t)RT_Blend_Image_Normal; break;
        case BlendSource::Solid: shaderInd += (uint32_t)RT_ShapeBlend_Solid_Normal; break;
        case BlendSource::LinearGradient: shaderInd += (uint32_t)RT_ShapeBlend_Linear_Normal; break;
        case BlendSource::RadialGradient: shaderInd += (uint32_t)RT_ShapeBlend_Radial_Normal; break;
    }

    if (mPrograms[shaderInd]) return mPrograms[shaderInd];

    const char* lumHelper = "";
    const char* satHelper = "";
    if (method == BlendMethod::Hue) {
        lumHelper = BLEND_FRAG_LUM_HELPER;
        satHelper = BLEND_FRAG_SAT_HELPER;
    } else if ((method == BlendMethod::Saturation) || (method == BlendMethod::Color) || (method == BlendMethod::Luminosity)) {
        lumHelper = BLEND_FRAG_LUM_HELPER;
    }

    const char* vertShader;
    char fragShader[BLEND_TOTAL_LENGTH];

    if (source == BlendSource::Scene || source == BlendSource::Image) {
        vertShader = BLIT_VERT_SHADER;
        const char* header = (source == BlendSource::Scene) ? BLEND_SCENE_FRAG_HEADER : BLEND_IMAGE_FRAG_HEADER;
        snprintf(fragShader, BLEND_TOTAL_LENGTH, "%s%s%s%s", header, lumHelper, satHelper, shaderFunc[methodInd]);
        mPrograms[shaderInd] = new GlProgram(vertShader, fragShader);
        return mPrograms[shaderInd];
    }

    vertShader = (source == BlendSource::Solid) ? COLOR_VERT_SHADER : GRADIENT_VERT_SHADER;
    switch (source) {
        case BlendSource::Solid:
            snprintf(fragShader, BLEND_TOTAL_LENGTH, "%s%s%s%s",
                     BLEND_SHAPE_SOLID_FRAG_HEADER,
                     lumHelper,
                     satHelper,
                     shaderFunc[methodInd]);
            break;
        case BlendSource::LinearGradient:
            snprintf(fragShader, BLEND_TOTAL_LENGTH, "%s%s%s%s%s%s%s%s",
                     STR_GRADIENT_FRAG_COMMON_VARIABLES,
                     STR_LINEAR_GRADIENT_VARIABLES,
                     STR_GRADIENT_FRAG_COMMON_FUNCTIONS,
                     STR_LINEAR_GRADIENT_FUNCTIONS,
                     BLEND_SHAPE_LINEAR_FRAG_HEADER,
                     lumHelper,
                     satHelper,
                     shaderFunc[methodInd]);
            break;
        case BlendSource::RadialGradient:
            snprintf(fragShader, BLEND_TOTAL_LENGTH, "%s%s%s%s%s%s%s%s",
                     STR_GRADIENT_FRAG_COMMON_VARIABLES,
                     STR_RADIAL_GRADIENT_VARIABLES,
                     STR_GRADIENT_FRAG_COMMON_FUNCTIONS,
                     STR_RADIAL_GRADIENT_FUNCTIONS,
                     BLEND_SHAPE_RADIAL_FRAG_HEADER,
                     lumHelper,
                     satHelper,
                     shaderFunc[methodInd]);
            break;
        default:
            TVGERR("RENDERER", "Unsupported blend source! = %d", (int)source);
            break;
    }

    mPrograms[shaderInd] = new GlProgram(vertShader, fragShader);
    return mPrograms[shaderInd];
}


void GlRenderer::prepareBlitTask(GlBlitTask* task)
{
    prepareCmpTask(task, {{0, 0}, {int32_t(surface.w), int32_t(surface.h)}}, surface.w, surface.h);
    task->addBindResource(GlBindingResource{0, task->colorTex, GlShaderUniform::SourceTexture});
}


void GlRenderer::prepareCmpTask(GlRenderTask* task, const RenderRegion& vp, uint32_t cmpWidth, uint32_t cmpHeight)
{
    const auto& passVp = currentPass()->getViewport();
    
    auto taskVp = vp;
    taskVp.intersect(passVp);

    auto x = taskVp.sx() - passVp.sx();
    auto y = taskVp.sy() - passVp.sy();
    auto w = taskVp.sw();
    auto h = taskVp.sh();

    float rw = static_cast<float>(passVp.w());
    float rh = static_cast<float>(passVp.h());

    float l = static_cast<float>(x);
    float t = static_cast<float>(rh - y);
    float r = static_cast<float>(x + w);
    float b = static_cast<float>(rh - y - h);

    // map vp ltrp to -1:1
    float left = (l / rw) * 2.f - 1.f;
    float top = (t / rh) * 2.f - 1.f;
    float right = (r / rw) * 2.f - 1.f;
    float bottom = (b / rh) * 2.f - 1.f;

    float uw = static_cast<float>(w) / static_cast<float>(cmpWidth);
    float uh = static_cast<float>(h) / static_cast<float>(cmpHeight);

    float vertices[4*4] {
        left, top,     0.f, uh,  // left top point
        left, bottom,  0.f, 0.f, // left bottom point
        right, top,    uw, uh,   // right top point
        right, bottom, uw, 0.f   // right bottom point
    };
    uint32_t vertexOffset = mGpuBuffer.push(vertices, sizeof(vertices));
    uint32_t indexOffset = mGpuBuffer.pushIndex((void*)RECT_INDEX, sizeof(RECT_INDEX));

    task->addVertexLayout(GlVertexLayout{0, 2, 4 * sizeof(float), vertexOffset, GL_FLOAT, GL_FALSE, mGpuBuffer.getBufferId()});
    task->addVertexLayout(GlVertexLayout{1, 2, 4 * sizeof(float), vertexOffset + 2 * sizeof(float), GL_FLOAT, GL_FALSE, mGpuBuffer.getBufferId()});
    task->setDrawRange(indexOffset, RECT_INDEX_COUNT);
    y = (passVp.sh() - y - h);
    task->setViewport({{x, y}, {x + w, y + h}});
}


void GlRenderer::endRenderPass(RenderCompositor* cmp)
{
    auto glCmp = static_cast<GlCompositor*>(cmp);

    // setup masking and blending render pass configurations
    if ((glCmp->flags & (tvg::Blending | tvg::Masking)) == (tvg::Blending | tvg::Masking)) {
        // rearrange render tree
        auto selfPass = mRenderPassStack.pick();
        auto prevPass = mRenderPassStack.pick();
        auto maskPass = mRenderPassStack.pick();
        mRenderPassStack.push(prevPass);
        mRenderPassStack.push(maskPass);
        mRenderPassStack.push(selfPass);
        // setup composition properties
        auto prevCompose = mComposeStack.last();
        auto opacity = glCmp->opacity;
        auto blendMethod = glCmp->blendMethod;
        // self scene task must be masked but not blended
        glCmp->method = prevCompose->method;
        glCmp->opacity = 255;
        glCmp->blendMethod = BlendMethod::Normal;
        // prev scene task must be blended but not masked
        prevCompose->method = MaskMethod::None;
        prevCompose->opacity = opacity;
        prevCompose->blendMethod = blendMethod;
    };

    if (cmp->method != MaskMethod::None) {
        auto selfPass = mRenderPassStack.pick();
        // mask is pushed first
        auto maskPass = mRenderPassStack.pick();

        GlProgram* program = nullptr;
        switch(cmp->method) {
            case MaskMethod::Alpha: program = mPrograms[RT_MaskAlpha]; break;
            case MaskMethod::InvAlpha: program = mPrograms[RT_MaskAlphaInv]; break;
            case MaskMethod::Luma: program = mPrograms[RT_MaskLuma]; break;
            case MaskMethod::InvLuma: program = mPrograms[RT_MaskLumaInv]; break;
            case MaskMethod::Add: program = mPrograms[RT_MaskAdd]; break;
            case MaskMethod::Subtract: program = mPrograms[RT_MaskSub]; break;
            case MaskMethod::Intersect: program = mPrograms[RT_MaskIntersect]; break;
            case MaskMethod::Difference: program = mPrograms[RT_MaskDifference]; break;
            case MaskMethod::Lighten: program = mPrograms[RT_MaskLighten]; break;
            case MaskMethod::Darken: program = mPrograms[RT_MaskDarken]; break;
            default: break;
        }
        if (program && !selfPass->isEmpty() && !maskPass->isEmpty()) {
            auto prev_task = maskPass->endRenderPass<GlComposeTask>(nullptr, currentPass()->getFboId());
            prev_task->setDrawDepth(currentPass()->nextDrawDepth());
            prev_task->setRenderSize(glCmp->bbox.w(), glCmp->bbox.h());
            prev_task->setViewport(glCmp->bbox);

            auto composeTask = selfPass->endRenderPass<GlDrawBlitTask>(program, currentPass()->getFboId());
            composeTask->setRenderSize(glCmp->bbox.w(), glCmp->bbox.h());
            composeTask->prevTask = prev_task;

            prepareCmpTask(composeTask, glCmp->bbox, selfPass->getFboWidth(), selfPass->getFboHeight());

            composeTask->addBindResource(GlBindingResource{0, selfPass->getTextureId(), GlShaderUniform::SourceTexture});
            composeTask->addBindResource(GlBindingResource{1, maskPass->getTextureId(), GlShaderUniform::MaskTexture});

            composeTask->setDrawDepth(currentPass()->nextDrawDepth());
            composeTask->setParentSize(currentPass()->getViewport().w(), currentPass()->getViewport().h());
            currentPass()->addRenderTask(composeTask);
        }
        delete(selfPass);
        delete(maskPass);
    } else if (glCmp->blendMethod != BlendMethod::Normal) {
        auto renderPass = mRenderPassStack.pick();
        if (!renderPass->isEmpty()) {
            if (mBlendPool.count < 1) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h, mRootTarget.samples, mStateCache));
            if (mBlendPool.count < 2) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h, mRootTarget.samples, mStateCache));
#if defined(THORVG_GL_TARGET_GL)
            auto dstCopyFbo = mBlendPool[1]->getRenderTarget(renderPass->getViewport());
#else // TODO: create partial buffer when MSAA is disabled
            auto dstCopyFbo = mBlendPool[1]->getRenderTarget(currentPass()->getViewport());
#endif
            // image info
            uint32_t info[4] = {(uint32_t)ColorSpace::ABGR8888, 0, cmp->opacity, 0};

            auto program = getBlendProgram(glCmp->blendMethod, BlendSource::Scene);
            auto task = renderPass->endRenderPass<GlSceneBlendTask>(program, currentPass()->getFboId());
            task->srcFbo = currentPass()->fbo;
            task->dstCopyFbo = dstCopyFbo;
            task->setRenderSize(glCmp->bbox.w(), glCmp->bbox.h());
            prepareCmpTask(task, glCmp->bbox, renderPass->getFboWidth(), renderPass->getFboHeight());
            task->setDrawDepth(currentPass()->nextDrawDepth());
#if defined(THORVG_GL_TARGET_GLES)
            float region[] = {0.0f, 0.0f, float(dstCopyFbo->width), float(dstCopyFbo->height)};
            task->addBindResource(GlBindingResource{
                1,
                GlShaderUniformBlock::BlendRegion,
                mGpuBuffer.getBufferId(),
                mGpuBuffer.push(region, 4 * sizeof(float), true),
                4 * sizeof(float),
            });
#endif
            // info
            task->addBindResource(GlBindingResource{0, GlShaderUniformBlock::ColorInfo, mGpuBuffer.getBufferId(), mGpuBuffer.push(info, sizeof(info), true), sizeof(info)});
            // textures
            task->addBindResource(GlBindingResource{0, renderPass->getTextureId(), GlShaderUniform::SourceTexture});
            task->addBindResource(GlBindingResource{1, dstCopyFbo->colorTex, GlShaderUniform::DestinationTexture});
            task->setParentSize(currentPass()->getViewport().w(), currentPass()->getViewport().h());
            currentPass()->addRenderTask(std::move(task));
        }
        delete(renderPass);
    } else {
        auto renderPass = mRenderPassStack.pick();
        if (!renderPass->isEmpty()) {
            auto task = renderPass->endRenderPass<GlDrawBlitTask>(mPrograms[RT_Image], currentPass()->getFboId());
            task->setRenderSize(glCmp->bbox.w(), glCmp->bbox.h());
            prepareCmpTask(task, glCmp->bbox, renderPass->getFboWidth(), renderPass->getFboHeight());
            task->setDrawDepth(currentPass()->nextDrawDepth());
            task->setViewMatrix(tvg::identity());

            // image info
            uint32_t info[4] = {(uint32_t)ColorSpace::ABGR8888, 0, cmp->opacity, 0};

            task->addBindResource(GlBindingResource{
                1,
                GlShaderUniformBlock::ColorInfo,
                mGpuBuffer.getBufferId(),
                mGpuBuffer.push(info, 4 * sizeof(uint32_t), true),
                4 * sizeof(uint32_t),
            });

            // texture id
            task->addBindResource(GlBindingResource{0, renderPass->getTextureId(), GlShaderUniform::Texture});
            task->setParentSize(currentPass()->getViewport().w(), currentPass()->getViewport().h());
            currentPass()->addRenderTask(std::move(task));
        }
        delete(renderPass);
    }
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool GlRenderer::clear()
{
    if (mRootTarget.invalid()) return false;

    mClearBuffer = true;
    return true;
}

Result GlRenderer::target(void* display, void* surface, void* context, int32_t id, uint32_t w, uint32_t h, ColorSpace cs)
{
    if (cs != ColorSpace::ABGR8888S) return Result::NonSupport;

    //assume the context zero is invalid
    if (!context || w == 0 || h == 0) return Result::InvalidArguments;

    mStateCache.invalidate();

    if (mContext) {
        currentContext();
        if (mContext != context) mTextures.clear();
    }

    flush();

    this->surface.stride = w;
    this->surface.w = w;
    this->surface.h = h;
    this->surface.cs = cs;

    mDisplay = display;
    mSurface = surface;
    mContext = context;
    mTargetFboId = static_cast<GLint>(id);

    auto ret = currentContext();
    // Cached values belong to the previous context. Numeric object ids can be
    // reused by the new context, so do not carry any binding assumptions over.
    mStateCache.invalidate();

    mRootTarget.viewport = {{0, 0}, {int32_t(this->surface.w), int32_t(this->surface.h)}};
    auto samples = mAaMode == GlAaMode::Msaa4 ? 4u : 1u;
    mRootTarget.init(mStateCache, this->surface.w, this->surface.h, samples, mTargetFboId);
    mAaStats.mode = mAaMode;
    mAaStats.rootSamples = mRootTarget.samples;
    mStateCache.invalidate();

#if defined(THORVG_GL_FLAT_MASK_SUPPORT)
    auto needsMaskTarget = mAaMode == GlAaMode::FlatMask || mAaMode == GlAaMode::CurveMask || mAaMode == GlAaMode::Hybrid;
    if (needsMaskTarget && !mFlatMaskTarget.init(mStateCache, this->surface.w, this->surface.h)) {
        TVGERR("GL_ENGINE", "Coverage-mask target initialization failed");
        mStateCache.invalidate();
        return Result::InsufficientCondition;
    }
    mStateCache.invalidate();
#endif

    return ret ? Result::Success : Result::InsufficientCondition;
}

bool GlRenderer::aaMode(GlAaMode mode)
{
    if (mContext) return false;
    mAaMode = mode;
    mAaStats.mode = mode;
    return true;
}

GlAaMode GlRenderer::aaMode() const
{
    return mAaMode;
}

uint32_t GlRenderer::aaSamples() const
{
    return mRootTarget.samples;
}

const GlAaStats& GlRenderer::aaStats() const
{
    return mAaStats;
}

void GlRenderer::resetAaStats()
{
    mAaStats = {};
    mAaStats.mode = mAaMode;
    mAaStats.rootSamples = mRootTarget.samples;
}

bool GlRenderer::sync()
{
    // State can also be touched while preparing textures and render targets.
    // Invalidate even for an empty sync() so the next render replays every
    // ThorVG-tracked assumption.
    mStateCache.invalidate();

    //nothing to be done.
    if (mRenderPassStack.empty()) return true;

    currentContext();

    // Keep this baseline limited to state ThorVG explicitly establishes.
    // Applications sharing the context must reset any other OpenGL state that
    // can affect rendering before calling sync().

    // Blend function for straight alpha
    mStateCache.blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    mStateCache.enable(GL_BLEND);
    mStateCache.enable(GL_SCISSOR_TEST);
    GL_CHECK(glCullFace(GL_FRONT_AND_BACK));
    GL_CHECK(glFrontFace(GL_CCW));
    mStateCache.enable(GL_DEPTH_TEST);
    mStateCache.depthFunc(GL_GREATER);

    auto task = mRenderPassStack.first()->endRenderPass<GlBlitTask>(mPrograms[RT_Blit], mTargetFboId);

    prepareBlitTask(task);

    task->clearBuffer = mClearBuffer;
    task->targetViewport = {{0, 0}, {int32_t(surface.w), int32_t(surface.h)}};

    if (mGpuBuffer.flushToGPU(mStateCache)) {
        mGpuBuffer.bind(mStateCache);
        task->run(mStateCache);
    }

    mGpuBuffer.unbind(mStateCache);

    mStateCache.disable(GL_SCISSOR_TEST);
    mStateCache.invalidate();

    clearDisposes();

    // Reset clear buffer flag to default (false) after use.    
    mClearBuffer = false; 

    delete task;

    return true;
}


bool GlRenderer::bounds(RenderData data, Point* pt4, const Matrix& m)
{
    if (!data) return false;

    auto sdata = static_cast<GlShape*>(data);
    if (!sdata->validStroke) return false;

    tvg::BBox bbox;
    bbox.init();
    auto& vertexes = sdata->geometry.stroke.vertex;
    for (uint32_t i = 0; i < vertexes.count / 2; i++) {
        Point vert = {vertexes[i * 2 + 0], vertexes[i * 2 + 1]};
        vert *= m;
        bbox = {min(bbox.min, vert), max(bbox.max, vert)};
    }

    pt4[0] = bbox.min;
    pt4[1] = {bbox.max.x, bbox.min.y};
    pt4[2] = bbox.max;
    pt4[3] = {bbox.min.x, bbox.max.y};
    return true;
}


RenderRegion GlRenderer::region(RenderData data)
{
    if (!data) return {};
    auto shape = reinterpret_cast<GlShape*>(data);
    return shape->geometry.getBounds();
}


bool GlRenderer::preRender()
{
    if (mRootTarget.invalid()) return false;

    currentContext();
    if (mPrograms.empty()) initShaders();
    mRenderPassStack.push(new GlRenderPass(&mRootTarget));

    return true;
}


bool GlRenderer::postRender()
{
    return true;
}


RenderCompositor* GlRenderer::target(const RenderRegion& region, TVG_UNUSED ColorSpace cs, TVG_UNUSED CompositionFlag flags)
{
    auto vp = region;
    if (currentPass()->isEmpty()) return nullptr;

    vp.intersect(currentPass()->getViewport());

    mComposeStack.push(new GlCompositor(vp, flags));
    return mComposeStack.last();
}


bool GlRenderer::beginComposite(RenderCompositor* cmp, MaskMethod method, uint8_t opacity)
{
    if (!cmp) return false;

    auto glCmp = static_cast<GlCompositor*>(cmp);
    glCmp->method = method;
    glCmp->opacity = opacity;
    glCmp->blendMethod = mBlendMethod;

    uint32_t index = mRenderPassStack.count - 1;
    if (index >= mComposePool.count) mComposePool.push(new GlRenderTargetPool(surface.w, surface.h, mRootTarget.samples, mStateCache));

    if (glCmp->bbox.valid()) mRenderPassStack.push(new GlRenderPass(mComposePool[index]->getRenderTarget(glCmp->bbox)));
    else mRenderPassStack.push(new GlRenderPass(nullptr));

    return true;
}


bool GlRenderer::endComposite(RenderCompositor* cmp)
{
    if (mComposeStack.empty()) return false;
    if (mComposeStack.last() != cmp) return false;

    // end current render pass;
    auto curCmp = mComposeStack.pick();
    endRenderPass(curCmp);
    delete(curCmp);

    return true;
}


void GlRenderer::prepare(RenderEffect* effect, const Matrix& transform)
{
    // we must be sure, that we have intermediate FBOs
    if (mBlendPool.count < 1) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h, mRootTarget.samples, mStateCache));
    if (mBlendPool.count < 2) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h, mRootTarget.samples, mStateCache));

    mEffect.update(effect, transform);
}


bool GlRenderer::region(RenderEffect* effect)
{
    return mEffect.region(effect);
}


bool GlRenderer::render(TVG_UNUSED RenderCompositor* cmp, const RenderEffect* effect, TVG_UNUSED bool direct)
{
    return mEffect.render(const_cast<RenderEffect*>(effect), currentPass(), mBlendPool);
}


void GlRenderer::dispose(RenderEffect* effect)
{
    tvg::free(effect->rd);
    effect->rd = nullptr;
}


ColorSpace GlRenderer::colorSpace()
{
    return surface.cs;
}


const RenderSurface* GlRenderer::mainSurface()
{
    return &surface;
}


bool GlRenderer::blend(BlendMethod method)
{
    if (method == mBlendMethod) return true;

    mBlendMethod = (method == BlendMethod::Composition ? BlendMethod::Normal : method);

    return true;
}


bool GlRenderer::renderImage(void* data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    if (currentPass()->isEmpty() || !sdata->validFill) return true;

    auto vp = currentPass()->getViewport();
    auto bbox = sdata->geometry.viewport;
    bbox.intersect(vp);
    if (bbox.invalid()) return true;
    if (!sdata->geometry.drawable(RenderUpdateFlag::Image)) return true;
    if (_skipRender(sdata->clips)) return true;  // TODO: move this in prepare() stage?

    auto drawDepth = currentPass()->nextDrawDepth();

    if (!sdata->clips.empty()) drawClip(sdata->clips, bbox);

    auto task = new GlRenderTask(mPrograms[RT_Image]);
    task->setDrawDepth(drawDepth);
    sdata->geometry.draw(task, &mGpuBuffer, RenderUpdateFlag::Image);

    bool complexBlend = beginComplexBlending(bbox, sdata->geometry.getBounds());
    if (complexBlend) vp = currentPass()->getViewport();
    task->setViewMatrix(currentPass()->getViewMatrix());

    // image info
    uint32_t info[4] = {(uint32_t)sdata->texColorSpace, sdata->texFlipY, sdata->opacity, 0};

    task->addBindResource(GlBindingResource{
        1,
        GlShaderUniformBlock::ColorInfo,
        mGpuBuffer.getBufferId(),
        mGpuBuffer.push(info, 4 * sizeof(uint32_t), true),
        4 * sizeof(uint32_t),
    });

    // texture id
    task->addBindResource(GlBindingResource{0, sdata->texId, GlShaderUniform::Texture});

    auto taskBounds = bbox;
    taskBounds.intersect(vp);
    task->setViewport(viewportRegion(vp, taskBounds));

    currentPass()->addRenderTask(task);

    if (complexBlend) {
        auto task = new GlRenderTask(mPrograms[RT_Stencil]);
        sdata->geometry.draw(task, &mGpuBuffer, RenderUpdateFlag::Image);
        endBlendingCompose(task);
    }

    return true;
}


bool GlRenderer::renderShape(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (currentPass()->isEmpty() || (!sdata->validFill && !sdata->validStroke)) return true;

    auto bbox = sdata->geometry.viewport;
    bbox.intersect(currentPass()->getViewport());
    if (bbox.invalid()) return true;
    if (_skipRender(sdata->clips)) return true;  // TODO: move this in prepare() stage?

    int32_t drawDepth1 = 0, drawDepth2 = 0;
    if (sdata->validFill) drawDepth1 = currentPass()->nextDrawDepth();
    if (sdata->validStroke) drawDepth2 = currentPass()->nextDrawDepth();

    if (!sdata->clips.empty()) drawClip(sdata->clips, bbox);

    auto processFill = [&]() {
        if (sdata->validFill) {
            if (const auto& gradient = sdata->rshape->fill) {
                drawPrimitive(*sdata, gradient, RenderUpdateFlag::Gradient, drawDepth1);
            } else if (sdata->rshape->color.a > 0) {
                drawPrimitive(*sdata, sdata->rshape->color, RenderUpdateFlag::Color, drawDepth1);
            }
        }
    };

    auto processStroke = [&]() {
        if (sdata->validStroke) {
            if (const auto& gradient = sdata->rshape->strokeFill()) {
                drawPrimitive(*sdata, gradient, RenderUpdateFlag::GradientStroke, drawDepth2);
            } else if (sdata->rshape->stroke->color.a > 0) {
                drawPrimitive(*sdata, sdata->rshape->stroke->color, RenderUpdateFlag::Stroke, drawDepth2);
            }
        }
    };

    if (sdata->rshape->strokeFirst()) {
        processStroke();
        processFill();
    } else {
        processFill();
        processStroke();
    }

    return true;
}


void GlRenderer::dispose(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) return;
    auto ownsTexture = sdata->texId && (sdata->texStamp == mTextures.stamp);
    if (ownsTexture) disposeTexture(mTextures.release(sdata->texSource, sdata->texFilter, sdata->texId));
    delete sdata;
}

RenderData GlRenderer::prepare(RenderSurface* image, RenderData data, const Matrix& transform, const Array<RenderData>& clips, uint8_t opacity, FilterMethod filter, RenderUpdateFlag flags)
{
    //TODO: redefine GlImage?
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) sdata = new GlShape;

    if (opacity == 0) {
        sdata->opacity = 0;
        sdata->deferredFlags |= flags;
        return sdata;
    }

    flags |= sdata->deferredFlags;
    sdata->deferredFlags = RenderUpdateFlag::None;

    auto cacheStale = sdata->texId && (sdata->texStamp != mTextures.stamp);
    if (flags == RenderUpdateFlag::None && !cacheStale) return data;

    sdata->validFill = false;
    sdata->viewWd = static_cast<float>(surface.w);
    sdata->viewHt = static_cast<float>(surface.h);

    if (cacheStale || sdata->texId == 0 || sdata->texSource != image || sdata->texFilter != filter) {
        auto ownsTexture = sdata->texId && (sdata->texStamp == mTextures.stamp);
        if (ownsTexture) disposeTexture(mTextures.release(sdata->texSource, sdata->texFilter, sdata->texId));
        sdata->texId = mTextures.retain(mStateCache, image, filter);
        sdata->texSource = image;
        sdata->texFilter = filter;
        sdata->texStamp = mTextures.stamp;
        sdata->geometry = GlGeometry();
    } else if (flags & RenderUpdateFlag::Image) {
        TextureMgr::upload(mStateCache, sdata->texId, image, filter);
    }

    sdata->texColorSpace = image->cs;
    sdata->texFlipY = 1;
    sdata->opacity = opacity;
    sdata->geometry.setMatrix(transform);
    sdata->geometry.viewport = vport;
    sdata->geometry.tesselateImage(image);
    sdata->validFill = true;

    if (flags & RenderUpdateFlag::Clip) {
        sdata->clips.clear();
        sdata->clips.push(clips);
    }

    return sdata;
}

RenderData GlRenderer::prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, const Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) {
        sdata = new GlShape;
        sdata->rshape = &rshape;
        flags = RenderUpdateFlag::All;
    }

    // Defer updates while transparent.
    if (opacity == 0 && !clipper) {
        sdata->opacity = 0;
        sdata->deferredFlags |= flags;
        return sdata;
    }

    flags |= sdata->deferredFlags;
    sdata->deferredFlags = RenderUpdateFlag::None;
    if (flags == RenderUpdateFlag::None) return sdata;

    sdata->viewWd = static_cast<float>(surface.w);
    sdata->viewHt = static_cast<float>(surface.h);
    sdata->opacity = opacity;

    if (flags & RenderUpdateFlag::Path) sdata->geometry = GlGeometry();

    sdata->geometry.setMatrix(transform);
    sdata->geometry.viewport = vport;
    auto strokePathMissing = (flags & RenderUpdateFlag::Stroke) && rshape.stroke && std::isfinite(rshape.strokeWidth()) && !tvg::zero(rshape.strokeWidth()) && sdata->geometry.optStrokePath.empty();
    if ((flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Transform)) || strokePathMissing) sdata->geometry.prepare(rshape);

    //TODO: Please precisely update tessellation not to update only if the color is changed.
    if (flags & (RenderUpdateFlag::Color | RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform | RenderUpdateFlag::Path)) {
        sdata->validFill = false;
        float opacityMultiplier = 1.0f;
#if defined(THORVG_GL_FLAT_MASK_SUPPORT)
        auto captureBoundary = mAaMode == GlAaMode::FlatDirect || mAaMode == GlAaMode::FlatMask;
        if (sdata->geometry.tesselateShape(*(sdata->rshape), &opacityMultiplier,
                                           captureBoundary)) {
#else
        if (sdata->geometry.tesselateShape(*(sdata->rshape), &opacityMultiplier)) {
#endif
            sdata->opacity *= opacityMultiplier;
            sdata->validFill = true;
        }
    }

    //TODO: Please precisely update tessellation not to update only if the color is changed.
    if (flags & (RenderUpdateFlag::Color | RenderUpdateFlag::Stroke | RenderUpdateFlag::GradientStroke | RenderUpdateFlag::Transform | RenderUpdateFlag::Path)) {
        sdata->validStroke = false;
        if (sdata->geometry.tesselateStroke(*(sdata->rshape))) sdata->validStroke = true;
    }

    if (flags & RenderUpdateFlag::Clip) {
        sdata->clips.clear();
        sdata->clips.push(clips);
    }

    return sdata;
}


bool GlRenderer::preUpdate()
{
    if (mRootTarget.invalid()) return false;

    currentContext();
    return true;
}


bool GlRenderer::postUpdate()
{
    return true;
}


void GlRenderer::damage(TVG_UNUSED RenderData rd, TVG_UNUSED const RenderRegion& region)
{
    //TODO
}


bool GlRenderer::partial(bool disable)
{
    //TODO
    return false;
}


bool GlRenderer::intersectsShape(RenderData data, TVG_UNUSED const RenderRegion& region)
{
    if (!data) return false;
    auto shape = (GlShape*)data;
    if (shape->opacity == 0) return false;
    const auto& bbox = shape->geometry.getBounds();
    if (region.intersected(bbox)) {
        if (region.contained(bbox)) return true;
        GlIntersector intersector;
        return intersector.intersectShape(RenderRegion::intersect(region, bbox), shape);
    }
    return false;
}


bool GlRenderer::intersectsImage(RenderData data, TVG_UNUSED const RenderRegion& region)
{
    if (!data) return false;
    auto shape = (GlShape*)data;
    if (shape->opacity == 0) return false;
    const auto& bbox = shape->geometry.getBounds();
    if (region.intersected(bbox)) {
        if (region.contained(bbox)) return true;
        GlIntersector intersector;
        if (intersector.intersectImage(RenderRegion::intersect(region, bbox), shape)) return true;
    }
    return false;
}


bool GlRenderer::term()
{
    _rendererMtx.lock();

    if (_rendererCnt > 0) {
        _rendererMtx.unlock();
        return false;
    }

    glTerm();

    _rendererCnt = -1;
    _rendererMtx.unlock();

    return true;
}


GlRenderer* GlRenderer::gen(TVG_UNUSED uint32_t threads, TVG_UNUSED EngineOption op)
{
    //initialize engine
    _rendererMtx.lock();
    if (_rendererCnt == -1) {
        if (!glInit()) {
            TVGERR("GL_ENGINE", "Failed GL initialization!");
            _rendererMtx.unlock();
            return nullptr;
        }    
        _rendererCnt = 0;
    }
    ++_rendererCnt;
    _rendererMtx.unlock();

    return new GlRenderer;
}
