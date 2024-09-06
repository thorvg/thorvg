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

#include "tvgWgRenderer.h"

#define WG_SSAA_SAMPLES (2)

WgRenderer::WgRenderer()
{
}


WgRenderer::~WgRenderer()
{
    release();
    mContext.release();
}


void WgRenderer::initialize()
{
    mPipelines.initialize(mContext);
    mOpacityPool.initialize(mContext);
    mBlendMethodPool.initialize(mContext);
    mCompositeMethodPool.initialize(mContext);
    WgMeshDataGroup::gMeshDataPool = new WgMeshDataPool();
    WgGeometryData::gMath = new WgMath();
    WgGeometryData::gMath->initialize();
}


void WgRenderer::release()
{
    WgGeometryData::gMath->release();
    delete WgGeometryData::gMath;
    mRenderDataShapePool.release(mContext);
    WgMeshDataGroup::gMeshDataPool->release(mContext);
    delete WgMeshDataGroup::gMeshDataPool;
    mCompositorStack.clear();
    mRenderStorageStack.clear();
    mRenderStoragePool.release(mContext);
    mCompositeMethodPool.release(mContext);
    mBlendMethodPool.release(mContext);
    mOpacityPool.release(mContext);
    mRenderStorageRoot.release(mContext);
    mRenderStorageScreen.release(mContext);
    mRenderTarget.release(mContext);
    mPipelines.release();
}


RenderData WgRenderer::prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
{
    // get or create render data shape
    auto renderDataShape = (WgRenderDataShape*)data;
    if (!renderDataShape)
        renderDataShape = mRenderDataShapePool.allocate(mContext);

    // update geometry
    if ((!data) || (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Stroke))) {
        renderDataShape->updateMeshes(mContext, rshape, transform);
    }

     // update paint settings
    if ((!data) || (flags & (RenderUpdateFlag::Transform | RenderUpdateFlag::Blend))) {
        WgShaderTypeMat4x4f modelMat(transform);
        WgShaderTypeBlendSettings blendSettings(mTargetSurface.cs, opacity);
        renderDataShape->bindGroupPaint.initialize(mContext.device, mContext.queue, modelMat, blendSettings);
        renderDataShape->fillRule = rshape.rule;
    }

    // setup fill settings
    renderDataShape->opacity = opacity;
    renderDataShape->renderSettingsShape.update(mContext, rshape.fill, rshape.color, flags);
    if (rshape.stroke)
        renderDataShape->renderSettingsStroke.update(mContext, rshape.stroke->fill, rshape.stroke->color, flags);

    return renderDataShape;
}


RenderData WgRenderer::prepare(Surface* surface, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags)
{
    // get or create render data shape
    auto renderDataPicture = (WgRenderDataPicture*)data;
    if (!renderDataPicture)
        renderDataPicture = new WgRenderDataPicture();

    // update paint settings
    renderDataPicture->opacity = opacity;
    if (flags & (RenderUpdateFlag::Transform | RenderUpdateFlag::Blend)) {
        WgShaderTypeMat4x4f modelMat(transform);
        WgShaderTypeBlendSettings blendSettings(surface->cs, opacity);
        renderDataPicture->bindGroupPaint.initialize(mContext.device, mContext.queue, modelMat, blendSettings);
    }

    // update image data
    if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Image)) {
        WgGeometryData geometryData;
        geometryData.appendImageBox(surface->w, surface->h);
        renderDataPicture->meshData.release(mContext);
        renderDataPicture->meshData.update(mContext, &geometryData);
        renderDataPicture->imageData.update(mContext, surface);
        renderDataPicture->bindGroupPicture.initialize(
            mContext.device, mContext.queue,
            mContext.samplerLinear,
            renderDataPicture->imageData.textureView);
    }

    return renderDataPicture;
}

bool WgRenderer::preRender()
{
    WGPUCommandEncoderDescriptor commandEncoderDesc{};
    commandEncoderDesc.nextInChain = nullptr;
    commandEncoderDesc.label = "The command encoder";
    mCommandEncoder = wgpuDeviceCreateCommandEncoder(mContext.device, &commandEncoderDesc);
    mRenderStorageStack.push(&mRenderStorageRoot);
    //mRenderStorageRoot.clear(mCommandEncoder);
    mRenderStorageRoot.beginRenderPass(mCommandEncoder, true);
    return true;
}


bool WgRenderer::renderShape(RenderData data)
{
    // get current render storage
    if (((WgRenderDataShape*)data)->opacity == 0) return 0;
    WgPipelineBlendType blendType = WgPipelines::blendMethodToBlendType(mBlendMethod);
    WgRenderStorage* renderStorage = mRenderStorageStack.last();
    assert(renderStorage);
    // use hardware blend
    if (WgPipelines::isBlendMethodSupportsHW(mBlendMethod))
        renderStorage->renderShape(mContext, (WgRenderDataShape *)data, blendType);
    else { // use custom blend
        // terminate current render pass
        renderStorage->endRenderPass();
        // render image to render target
        mRenderTarget.beginRenderPass(mCommandEncoder, true);
        mRenderTarget.renderShape(mContext, (WgRenderDataShape *)data, blendType);
        mRenderTarget.endRenderPass();
        // blend shape with current render storage
        WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
        renderStorage->blend(mCommandEncoder, &mRenderTarget, blendMethod);
        // restore current render pass
        renderStorage->beginRenderPass(mCommandEncoder, false);
    }
    return true;
}


bool WgRenderer::renderImage(RenderData data)
{
    // get current render storage
    WgPipelineBlendType blendType = WgPipelines::blendMethodToBlendType(mBlendMethod);
    WgRenderStorage* renderStorage = mRenderStorageStack.last();
    assert(renderStorage);
    // use hardware blend
    if (WgPipelines::isBlendMethodSupportsHW(mBlendMethod))
        renderStorage->renderPicture(mContext, (WgRenderDataPicture *)data, blendType);
    else { // use custom blend
        // terminate current render pass
        renderStorage->endRenderPass();
        // render image to render target
        mRenderTarget.beginRenderPass(mCommandEncoder, true);
        mRenderTarget.renderPicture(mContext, (WgRenderDataPicture *)data, blendType);
        mRenderTarget.endRenderPass();
        // blend shape with current render storage
        WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
        renderStorage->blend(mCommandEncoder, &mRenderTarget, blendMethod);
        // restore current render pass
        renderStorage->beginRenderPass(mCommandEncoder, false);
    }
    return true;
}


bool WgRenderer::postRender()
{
    mRenderStorageRoot.endRenderPass();
    mRenderStorageStack.pop();
    mContext.executeCommandEncoder(mCommandEncoder);
    wgpuCommandEncoderRelease(mCommandEncoder);
    return true;
}


void WgRenderer::dispose(RenderData data)
{
    auto renderData = (WgRenderDataPaint*)data;
    if (renderData) {
        if (renderData->identifier() == TVG_CLASS_ID_SHAPE)
            mRenderDataShapePool.free(mContext, (WgRenderDataShape*)renderData);
        else
            renderData->release(mContext);
    }
}


RenderRegion WgRenderer::region(TVG_UNUSED RenderData data)
{
    return { 0, 0, INT32_MAX, INT32_MAX };
}


RenderRegion WgRenderer::viewport() {
    return { 0, 0, INT32_MAX, INT32_MAX };
}


bool WgRenderer::viewport(TVG_UNUSED const RenderRegion& vp)
{
    return true;
}


bool WgRenderer::blend(BlendMethod method)
{
    mBlendMethod = method;
    return true;
}


ColorSpace WgRenderer::colorSpace()
{
    return ColorSpace::Unsupported;
}


const Surface* WgRenderer::mainSurface()
{
    return &mTargetSurface;
}


bool WgRenderer::clear()
{
    return true;
}


bool WgRenderer::sync()
{
    WGPUSurfaceTexture backBuffer{};
    wgpuSurfaceGetCurrentTexture(mContext.surface, &backBuffer);
    
    WGPUCommandEncoderDescriptor commandEncoderDesc{};
    commandEncoderDesc.nextInChain = nullptr;
    commandEncoderDesc.label = "The command encoder";
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(mContext.device, &commandEncoderDesc);

    mRenderStorageScreen.antialias(commandEncoder, &mRenderStorageRoot);

    WGPUImageCopyTexture source{};
    source.texture = mRenderStorageScreen.texColor;
    WGPUImageCopyTexture dest{};
    dest.texture = backBuffer.texture;
    WGPUExtent3D copySize{};
    copySize.width = mTargetSurface.w;
    copySize.height = mTargetSurface.h;
    copySize.depthOrArrayLayers = 1;
    wgpuCommandEncoderCopyTextureToTexture(commandEncoder, &source, &dest, &copySize);

    mContext.executeCommandEncoder(commandEncoder);
    wgpuCommandEncoderRelease(commandEncoder);
    
    wgpuSurfacePresent(mContext.surface);
    return true;
}


// target for native window handle
bool WgRenderer::target(WGPUInstance instance, WGPUSurface surface, uint32_t w, uint32_t h)
{
    // store target surface properties
    mTargetSurface.stride = w;
    mTargetSurface.w = w > 0 ? w : 1;
    mTargetSurface.h = h > 0 ? h : 1;
    
    mContext.initialize(instance, surface);

    // configure surface
    WGPUSurfaceConfiguration surfaceConfiguration{};
    surfaceConfiguration.nextInChain = nullptr;
    surfaceConfiguration.device = mContext.device;
    surfaceConfiguration.format = WGPUTextureFormat_BGRA8Unorm;
    surfaceConfiguration.usage = WGPUTextureUsage_CopyDst;
    surfaceConfiguration.viewFormatCount = 0;
    surfaceConfiguration.viewFormats = nullptr;
    surfaceConfiguration.alphaMode = WGPUCompositeAlphaMode_Auto;
    surfaceConfiguration.width = w;
    surfaceConfiguration.height = h;
    surfaceConfiguration.presentMode = WGPUPresentMode_Immediate;
    wgpuSurfaceConfigure(mContext.surface, &surfaceConfiguration);

    initialize();
    mRenderTarget.initialize(mContext, w, h, WG_SSAA_SAMPLES);
    mRenderStorageRoot.initialize(mContext, w, h, WG_SSAA_SAMPLES);
    mRenderStorageScreen.initialize(mContext, w, h, 1, WGPUTextureFormat_BGRA8Unorm);

    return true;
}


Compositor* WgRenderer::target(TVG_UNUSED const RenderRegion& region, TVG_UNUSED ColorSpace cs)
{
    mCompositorStack.push(new WgCompositor);
    return mCompositorStack.last();
}


bool WgRenderer::beginComposite(TVG_UNUSED Compositor* cmp, TVG_UNUSED CompositeMethod method, TVG_UNUSED uint8_t opacity)
{
    // save current composition settings
    WgCompositor *comp = (WgCompositor*)cmp;
    comp->method = method;
    comp->opacity = opacity;
    comp->blendMethod = mBlendMethod;

    // end current render pass
    mRenderStorageStack.last()->endRenderPass();
    // allocate new render storage and push it to top of render tree
    WgRenderStorage* renderStorage = mRenderStoragePool.allocate(mContext, mTargetSurface.w, mTargetSurface.h, WG_SSAA_SAMPLES);
    mRenderStorageStack.push(renderStorage);
    // begin last render pass
    mRenderStorageStack.last()->beginRenderPass(mCommandEncoder, true);
    return true;
}


bool WgRenderer::endComposite(TVG_UNUSED Compositor* cmp)
{
    WgCompositor *comp = (WgCompositor*)cmp;
    if (comp->method == CompositeMethod::None) {
        // end current render pass
        mRenderStorageStack.last()->endRenderPass();
        // get two last render targets
        WgRenderStorage* renderStorageSrc = mRenderStorageStack.last();
        mRenderStorageStack.pop();
    
        // blent scene to current render storage
        WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, comp->blendMethod);
        mRenderStorageStack.last()->blend(mCommandEncoder, renderStorageSrc, blendMethod);

        // back render targets to the pool
        mRenderStoragePool.free(mContext, renderStorageSrc);
        // begin last render pass
        mRenderStorageStack.last()->beginRenderPass(mCommandEncoder, false);
    } else {
        // end current render pass
        mRenderStorageStack.last()->endRenderPass();

        // get source, mask and destination render storages
        WgRenderStorage* renderStorageSrc = mRenderStorageStack.last();
        mRenderStorageStack.pop();
        WgRenderStorage* renderStorageMsk = mRenderStorageStack.last();
        mRenderStorageStack.pop();
        WgRenderStorage* renderStorageDst = mRenderStorageStack.last();

        // get compose, blend and opacity settings
        WgBindGroupCompositeMethod* composeMethod = mCompositeMethodPool.allocate(mContext, comp->method);
        WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
        WgBindGroupOpacity* opacity = mOpacityPool.allocate(mContext, comp->opacity);

        // compose and blend
        // dest = blend(dest, compose(src, msk, composeMethod), blendMethod, opacity)
        renderStorageDst->composeBlend(mContext, mCommandEncoder,
            renderStorageSrc, renderStorageMsk,
            composeMethod, blendMethod, opacity);

        // back render targets to the pool
        mRenderStoragePool.free(mContext, renderStorageSrc);
        mRenderStoragePool.free(mContext, renderStorageMsk);
        // begin last render pass
        mRenderStorageStack.last()->beginRenderPass(mCommandEncoder, false);
    }

    // delete current compositor
    delete mCompositorStack.last();
    mCompositorStack.pop();

    return true;
}


WgRenderer* WgRenderer::gen()
{
    return new WgRenderer();
}


bool WgRenderer::init(TVG_UNUSED uint32_t threads)
{
    return true;
}


bool WgRenderer::term()
{
    return true;
}
