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

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

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
    clearDisposes();
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
    mRenderStorageMask.release(mContext);
    mRenderStorageScreen.release(mContext);
    mRenderStorageInterm.release(mContext);
    mPipelines.release();
}


void WgRenderer::clearDisposes()
{
    if (mDisposed.renderDatas.count == 0) return;

    for (auto p = mDisposed.renderDatas.begin(); p < mDisposed.renderDatas.end(); p++) {
        auto renderData = (WgRenderDataPaint*)(*p);
        if (renderData->type() == Type::Shape) {
            mRenderDataShapePool.free(mContext, (WgRenderDataShape*)renderData);
        } else {
            renderData->release(mContext);
        }
    }
    mDisposed.renderDatas.clear();
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

RenderData WgRenderer::prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
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
    renderDataShape->viewport = mViewport;
    renderDataShape->opacity = opacity;
    renderDataShape->renderSettingsShape.update(mContext, rshape.fill, rshape.color, flags);
    if (rshape.stroke)
        renderDataShape->renderSettingsStroke.update(mContext, rshape.stroke->fill, rshape.stroke->color, flags);

    // store clips data
    renderDataShape->updateClips(clips);

    return renderDataShape;
}


RenderData WgRenderer::prepare(TVG_UNUSED const Array<RenderData>& scene, TVG_UNUSED RenderData data, TVG_UNUSED const RenderTransform* transform, TVG_UNUSED Array<RenderData>& clips, TVG_UNUSED uint8_t opacity, TVG_UNUSED RenderUpdateFlag flags)
{
    return nullptr;
}


RenderData WgRenderer::prepare(Surface* surface, const RenderMesh* mesh, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags)
{
    // get or create render data shape
    auto renderDataPicture = (WgRenderDataPicture*)data;
    if (!renderDataPicture)
        renderDataPicture = new WgRenderDataPicture();

    // update paint settings
    renderDataPicture->viewport = mViewport;
    renderDataPicture->opacity = opacity;
    if (flags & (RenderUpdateFlag::Transform | RenderUpdateFlag::Blend)) {
        WgShaderTypeMat4x4f modelMat(transform);
        WgShaderTypeBlendSettings blendSettings(surface->cs, opacity);
        renderDataPicture->bindGroupPaint.initialize(mContext.device, mContext.queue, modelMat, blendSettings);
    }

    // update image data
    if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Image)) {
        WgGeometryData geometryData;
        if (mesh->triangleCnt == 0) geometryData.appendImageBox(surface->w, surface->h);
        else geometryData.appendMesh(mesh);
        renderDataPicture->meshData.release(mContext);
        renderDataPicture->meshData.update(mContext, &geometryData);
        renderDataPicture->imageData.update(mContext, surface);
        renderDataPicture->bindGroupPicture.initialize(
            mContext.device, mContext.queue,
            mContext.samplerLinear,
            renderDataPicture->imageData.textureView);
    }

    // store clips data
    renderDataPicture->updateClips(clips);

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


void WgRenderer::renderClipPath(Array<WgRenderDataPaint*>& clips)
{
    for (uint32_t i = 0; i < clips.count; i++) {
        renderClipPath(clips[i]->clips);
        // render image to render target
        mRenderStorageInterm.beginRenderPass(mCommandEncoder, true);
        mRenderStorageInterm.renderClipPath(mContext, clips[i]);
        mRenderStorageInterm.endRenderPass();
        mRenderStorageMask.maskCompose(mContext, mCommandEncoder, &mRenderStorageInterm);
    }
}


bool WgRenderer::renderShape(RenderData data)
{
    // get current render storage
    WgRenderDataShape *dataShape = (WgRenderDataShape*)data;
    if (dataShape->opacity == 0) return 0;
    WgPipelineBlendType blendType = WgPipelines::blendMethodToBlendType(mBlendMethod);
    WgRenderStorage* renderStorage = mRenderStorageStack.last();
    assert(renderStorage);
    // use masked blend
    if (dataShape->clips.count > 0) {
        // terminate current render pass
        renderStorage->endRenderPass();
        // render clip path
        mRenderStorageMask.beginRenderPass(mCommandEncoder, true);
        mRenderStorageMask.renderClipPath(mContext, dataShape->clips[0]);
        mRenderStorageMask.endRenderPass();
        renderClipPath(dataShape->clips);
        // render image to render target
        mRenderStorageInterm.beginRenderPass(mCommandEncoder, true);
        mRenderStorageInterm.renderShape(mContext, dataShape, blendType);
        mRenderStorageInterm.endRenderPass();
        // blend shape with current render storage
        WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
        WgBindGroupOpacity* opacity = mOpacityPool.allocate(mContext, 255);
        WgPipelineBlendMask* pipeline = &mContext.pipelines->computeBlendSolidMask;
        if (dataShape->renderSettingsShape.fillType != WgRenderSettingsType::Solid)
            pipeline = &mContext.pipelines->computeBlendGradientMask;
        renderStorage->blendMask(mContext, mCommandEncoder, 
            pipeline, &mRenderStorageMask, &mRenderStorageInterm, blendMethod, opacity);
        // restore current render pass
        renderStorage->beginRenderPass(mCommandEncoder, false);
    // use hardware blend
    } else if (WgPipelines::isBlendMethodSupportsHW(mBlendMethod)) {
        renderStorage->renderShape(mContext, dataShape, blendType);
    // use custom blend
    } else {
        // terminate current render pass
        renderStorage->endRenderPass();
        // render image to render target
        mRenderStorageInterm.beginRenderPass(mCommandEncoder, true);
        mRenderStorageInterm.renderShape(mContext, dataShape, blendType);
        mRenderStorageInterm.endRenderPass();
        // blend shape with current render storage
        WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
        WgBindGroupOpacity* opacity = mOpacityPool.allocate(mContext, 255);
        WgPipelineBlend* pipeline = &mContext.pipelines->computeBlendSolid;
        if (dataShape->renderSettingsShape.fillType != WgRenderSettingsType::Solid)
            pipeline = &mContext.pipelines->computeBlendGradient;
        renderStorage->blend(mContext, mCommandEncoder, 
            pipeline, &mRenderStorageInterm, blendMethod, opacity);
        // restore current render pass
        renderStorage->beginRenderPass(mCommandEncoder, false);
    }
    return true;
}


bool WgRenderer::renderImage(RenderData data)
{
    // get current render storage
    WgRenderDataPicture *dataPicture = (WgRenderDataPicture*)data;
    // get current render storage
    WgPipelineBlendType blendType = WgPipelines::blendMethodToBlendType(mBlendMethod);
    WgRenderStorage* renderStorage = mRenderStorageStack.last();
    assert(renderStorage);
        // use masked blend
    if (dataPicture->clips.count > 0) {
        // terminate current render pass
        renderStorage->endRenderPass();
        // render clip path
        mRenderStorageMask.beginRenderPass(mCommandEncoder, true);
        mRenderStorageMask.renderClipPath(mContext, dataPicture->clips[0]);
        mRenderStorageMask.endRenderPass();
        renderClipPath(dataPicture->clips);
        // render image to render target
        mRenderStorageInterm.beginRenderPass(mCommandEncoder, true);
        mRenderStorageInterm.renderPicture(mContext, dataPicture, blendType);
        mRenderStorageInterm.endRenderPass();
        // blend shape with current render storage
        WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
        WgBindGroupOpacity* opacity = mOpacityPool.allocate(mContext, 255);
        WgPipelineBlendMask* pipeline = &mContext.pipelines->computeBlendImageMask;
        renderStorage->blendMask(mContext, mCommandEncoder, 
            pipeline, &mRenderStorageMask, &mRenderStorageInterm, blendMethod, opacity);
        // restore current render pass
        renderStorage->beginRenderPass(mCommandEncoder, false);
    // use hardware blend
    } else if (WgPipelines::isBlendMethodSupportsHW(mBlendMethod))
        renderStorage->renderPicture(mContext, dataPicture, blendType);
    // use custom blend
    else {
        // terminate current render pass
        renderStorage->endRenderPass();
        // render image to render target
        mRenderStorageInterm.beginRenderPass(mCommandEncoder, true);
        mRenderStorageInterm.renderPicture(mContext, dataPicture, blendType);
        mRenderStorageInterm.endRenderPass();
        // blend shape with current render storage
        WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
        WgBindGroupOpacity* opacity = mOpacityPool.allocate(mContext, 255);
        WgPipelineBlend* pipeline = &mContext.pipelines->computeBlendImage;
        renderStorage->blend(mContext, mCommandEncoder, 
            pipeline, &mRenderStorageInterm, blendMethod, opacity);
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


void WgRenderer::dispose(RenderData data) {
    auto renderData = (WgRenderDataPaint*)data;
    if (renderData) {
        ScopedLock lock(mDisposed.key);
        mDisposed.renderDatas.push(data);
    }
}


RenderRegion WgRenderer::region(TVG_UNUSED RenderData data)
{
    return { 0, 0, (int32_t)mTargetSurface.w, (int32_t)mTargetSurface.h };
}


RenderRegion WgRenderer::viewport() {
    return mViewport;
}


bool WgRenderer::viewport(const RenderRegion& vp)
{
    mViewport = vp;
    return true;
}


bool WgRenderer::blend(BlendMethod method)
{
    mBlendMethod = method;
    return false;
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
    clearDisposes();
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
    mRenderStorageInterm.initialize(mContext, w, h, WG_SSAA_SAMPLES);
    mRenderStorageMask.initialize(mContext, w, h, WG_SSAA_SAMPLES);
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
        WgBindGroupOpacity* opacity = mOpacityPool.allocate(mContext, comp->opacity);
        mRenderStorageStack.last()->blend(mContext, mCommandEncoder,
            &mContext.pipelines->computeBlendImage,
            renderStorageSrc, blendMethod, opacity);

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
        renderStorageDst->compose(mContext, mCommandEncoder,
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
