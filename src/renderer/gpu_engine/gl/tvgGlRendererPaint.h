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

#ifndef _TVG_GL_RENDERER_PAINT_H_
#define _TVG_GL_RENDERER_PAINT_H_

#include "tvgFill.h"
#include "tvgGlRenderer.h"

// Included by the build-selected renderer so geometry setup can be inlined.
static constexpr float NOISE_LEVEL = 0.5f;

static bool _packGradientBlock(float* stopInfo, float* stopPoints, float* stopColors, const Fill::ColorStop* stops, uint32_t stopCnt, FillSpread spread, float alpha)
{
    stopInfo[1] = NOISE_LEVEL;
    stopInfo[2] = static_cast<int32_t>(spread);

    auto opaque = alpha == 1.0f;
    uint32_t nStops = 0;
    for (uint32_t i = 0; i < stopCnt; ++i) {
        if (nStops > 0 && stopPoints[nStops - 1] > stops[i].offset) {
            opaque = false;
            continue;
        }

        auto index = nStops++;
        stopPoints[index] = stops[i].offset;
        stopColors[index * 4 + 0] = stops[i].r / 255.f;
        stopColors[index * 4 + 1] = stops[i].g / 255.f;
        stopColors[index * 4 + 2] = stops[i].b / 255.f;
        stopColors[index * 4 + 3] = stops[i].a / 255.f * alpha;
        opaque &= stops[i].a == 255;
    }
    stopInfo[0] = nStops;
    return opaque;
}

void GlRenderer::drawPrimitive(GlShape& shape, const Fill* fill, RenderUpdateFlag flag, int32_t depth)
{
    auto vp = currentPass()->getViewport();
    // geometry.viewport carries the fast-tracked clip bounds; the pass
    // viewport can still be the full framebuffer.
    auto viewBounds = shape.geometry.viewport;
    viewBounds.intersect(vp);
    if (viewBounds.invalid()) return;

    RenderRegion bbox;
    if (!primitiveBounds(shape, flag, bbox)) return;
    bbox.intersect(viewBounds);
    if (bbox.invalid()) return;

    const Fill::ColorStop* stops = nullptr;
    auto colorStopCnt = fill->colorStops(&stops);
    auto stopCnt = min(colorStopCnt, static_cast<uint32_t>(MAX_GRADIENT_STOPS));
    if (stopCnt < 1) return;

    GlRenderTarget* dstCopyFbo = nullptr;
    auto type = fill->type();
    auto radial = type == Type::RadialGradient;
    auto conic = type == Type::ConicGradient;
    auto viewRegion = viewportRegion(vp, bbox);

    RenderTypes taskType = RT_None;
    auto blendSource = BlendSource::LinearGradient;

    float x, y, r, fx, fy, fr, angle;

    if (type == Type::LinearGradient) {
        taskType = RT_LinGradient;
    } else if (radial) {
        auto radialFill = static_cast<const RadialGradient*>(fill);
        radialFill->radial(&x, &y, &r, &fx, &fy, &fr);
        // Uncorrectable radial gradients use the last stop as a solid color.
        if (!CONST_RADIAL(radialFill)->correct(fx, fy, fr)) {
            auto& stop = stops[colorStopCnt - 1];
            RenderColor color = {stop.r, stop.g, stop.b, stop.a};
            auto solidFlag = (flag & RenderUpdateFlag::GradientStroke) ? RenderUpdateFlag::Stroke : RenderUpdateFlag::Color;
            drawPrimitive(shape, color, solidFlag, depth);
            return;
        }

        taskType = RT_RadGradient;
        blendSource = BlendSource::RadialGradient;
    } else if (conic) {
        static_cast<const ConicGradient*>(fill)->conic(&x, &y, &angle);
        taskType = RT_ConGradient;
        blendSource = BlendSource::ConicGradient;
    } else return;

    auto task = createPrimitiveTask(taskType, blendSource, viewRegion, dstCopyFbo);
    task->setDrawDepth(depth);

    task->setViewport(viewRegion);

    // transform buffer (inverse fill-space transform)
    float invMat3[GL_MAT3_STD140_SIZE];
    Matrix inv;
    inverse(&fill->transform(), &inv);
    auto alpha = bindGradientGeometry(task, shape, flag, inv);
    if (conic) inv = tvg::gpuConicTransform({x, y}, angle) * inv;
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

    // gradient block
    GlBindingResource gradientBinding{};
    bool opaque;

    if (type == Type::LinearGradient) {
        auto linearFill = static_cast<const LinearGradient*>(fill);

        GlLinearGradientBlock gradientBlock;
        opaque = _packGradientBlock(gradientBlock.nStops, gradientBlock.stopPoints, gradientBlock.stopColors, stops, stopCnt, fill->spread(), alpha);

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
    } else if (radial) {
        GlRadialGradientBlock gradientBlock;
        opaque = _packGradientBlock(gradientBlock.nStops, gradientBlock.stopPoints, gradientBlock.stopColors, stops, stopCnt, fill->spread(), alpha);

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
    } else {
        GlConicGradientBlock gradientBlock;
        opaque = _packGradientBlock(gradientBlock.nStops, gradientBlock.stopPoints, gradientBlock.stopColors, stops, stopCnt, FillSpread::Pad, alpha);

        gradientBinding = GlBindingResource{
            2,
            GlShaderUniformBlock::GradientInfo,
            mGpuBuffer.getBufferId(),
            mGpuBuffer.push(&gradientBlock, sizeof(GlConicGradientBlock), true),
            sizeof(GlConicGradientBlock),
        };
    }

    task->addBindResource(gradientBinding);

    // TransformInfo uses slot 0 and GradientInfo uses slot 2, so BlendRegion moves to 3.
    bindBlendTarget(task, dstCopyFbo, viewRegion, 3);

    submitGradient(task, shape, flag, opaque, viewBounds);
}

inline void GlRenderer::drawShapePaint(GlShape* shape, int32_t drawDepth1, int32_t drawDepth2)
{
    auto processFill = [&]() {
        if (shape->valid.fill) {
            if (const auto& gradient = shape->rshape->fill) {
                drawPrimitive(*shape, gradient, RenderUpdateFlag::Gradient, drawDepth1);
            } else if (shape->rshape->color.a > 0) {
                drawPrimitive(*shape, shape->rshape->color, RenderUpdateFlag::Color, drawDepth1);
            }
        }
    };

    auto processStroke = [&]() {
        if (shape->valid.stroke) {
            if (const auto& gradient = shape->rshape->strokeFill()) {
                drawPrimitive(*shape, gradient, RenderUpdateFlag::GradientStroke, drawDepth2);
            } else if (shape->rshape->stroke->color.a > 0) {
                drawPrimitive(*shape, shape->rshape->stroke->color, RenderUpdateFlag::Stroke, drawDepth2);
            }
        }
    };

    if (shape->rshape->strokeFirst()) {
        processStroke();
        processFill();
    } else {
        processFill();
        processStroke();
    }
}

#endif //_TVG_GL_RENDERER_PAINT_H_
