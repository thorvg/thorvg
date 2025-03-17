
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

#include <algorithm>
#include "tvgMath.h"
#include "tvgWgRenderData.h"
#include "tvgWgShaderTypes.h"

//***********************************************************************
// WgMeshData
//***********************************************************************

void WgMeshData::draw(WgContext& context, WGPURenderPassEncoder renderPassEncoder)
{
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, bufferPosition, 0, vertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, bufferIndex, WGPUIndexFormat_Uint32, 0, indexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, indexCount, 1, 0, 0, 0);
}


void WgMeshData::drawFan(WgContext& context, WGPURenderPassEncoder renderPassEncoder)
{
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, bufferPosition, 0, vertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, context.bufferIndexFan, WGPUIndexFormat_Uint32, 0, indexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, indexCount, 1, 0, 0, 0);
}


void WgMeshData::drawImage(WgContext& context, WGPURenderPassEncoder renderPassEncoder)
{
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, bufferPosition, 0, vertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, bufferTexCoord, 0, vertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, bufferIndex, WGPUIndexFormat_Uint32, 0, indexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, indexCount, 1, 0, 0, 0);
};


void WgMeshData::update(WgContext& context, const WgVertexBuffer& vertexBuffer)
{
    assert(vertexBuffer.count > 2);
    vertexCount = vertexBuffer.count;
    indexCount = (vertexBuffer.count - 2) * 3;
    context.allocateBufferVertex(bufferPosition, (float*)vertexBuffer.data, vertexCount * sizeof(float) * 2);
    context.allocateBufferIndexFan(vertexCount);
}


void WgMeshData::update(WgContext& context, const WgIndexedVertexBuffer& vertexBufferInd)
{
    assert(vertexBufferInd.vcount > 2);
    vertexCount = vertexBufferInd.vcount;
    indexCount = vertexBufferInd.icount;
    if (vertexCount > 0) context.allocateBufferVertex(bufferPosition, (float*)vertexBufferInd.vbuff, vertexCount * sizeof(float) * 2);
    if (indexCount > 0) context.allocateBufferIndex(bufferIndex, vertexBufferInd.ibuff, indexCount * sizeof(uint32_t));
};


void WgMeshData::bbox(WgContext& context, const Point pmin, const Point pmax)
{
    vertexCount = 4;
    indexCount = 6;
    const float data[] = {pmin.x, pmin.y, pmax.x, pmin.y, pmax.x, pmax.y, pmin.x, pmax.y};
    context.allocateBufferVertex(bufferPosition, data, sizeof(data));
    context.allocateBufferIndexFan(vertexCount);
}


void WgMeshData::imageBox(WgContext& context, float w, float h)
{
    vertexCount = 4;
    indexCount = 6;
    const float vdata[] = {0.0f, 0.0f, w, 0.0f, w, h, 0.0f, h};
    const float tdata[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    const uint32_t idata[] = {0, 1, 2, 0, 2, 3};
    context.allocateBufferVertex(bufferPosition, vdata, sizeof(vdata));
    context.allocateBufferVertex(bufferTexCoord, tdata, sizeof(tdata));
    context.allocateBufferIndex(bufferIndex, idata, sizeof(idata));
}


void WgMeshData::blitBox(WgContext& context)
{
    vertexCount = 4;
    indexCount = 6;
    const float vdata[] = {-1.0f, +1.0f, +1.0f, +1.0f, +1.0f, -1.0f, -1.0f, -1.0f};
    const float tdata[] = {+0.0f, +0.0f, +1.0f, +0.0f, +1.0f, +1.0f, +0.0f, +1.0f};
    const uint32_t idata[] = { 0, 1, 2, 0, 2, 3 };
    context.allocateBufferVertex(bufferPosition, vdata, sizeof(vdata));
    context.allocateBufferVertex(bufferTexCoord, tdata, sizeof(tdata));
    context.allocateBufferIndex(bufferIndex, idata, sizeof(idata));
}


void WgMeshData::release(WgContext& context)
{
    context.releaseBuffer(bufferIndex);
    context.releaseBuffer(bufferTexCoord);
    context.releaseBuffer(bufferPosition);
};


//***********************************************************************
// WgMeshDataPool
//***********************************************************************

WgMeshData* WgMeshDataPool::allocate(WgContext& context)
{
    WgMeshData* meshData{};
    if (mPool.count > 0) {
        meshData = mPool.last();
        mPool.pop();
    } else {
        meshData = new WgMeshData();
        mList.push(meshData);
    }
    return meshData;
}


void WgMeshDataPool::free(WgContext& context, WgMeshData* meshData)
{
    mPool.push(meshData);
}


void WgMeshDataPool::release(WgContext& context)
{
    ARRAY_FOREACH(p, mList) {
        (*p)->release(context);
        delete(*p);
    }
    mPool.clear();
    mList.clear();
}

WgMeshDataPool gMeshDataPoolInstance;
WgMeshDataPool* WgMeshDataPool::gMeshDataPool = &gMeshDataPoolInstance;

//***********************************************************************
// WgMeshDataGroup
//***********************************************************************

void WgMeshDataGroup::append(WgContext& context, const WgVertexBuffer& vertexBuffer)
{
    assert(vertexBuffer.count >= 3);
    meshes.push(WgMeshDataPool::gMeshDataPool->allocate(context));
    meshes.last()->update(context, vertexBuffer);
}


void WgMeshDataGroup::append(WgContext& context, const WgIndexedVertexBuffer& vertexBufferInd)
{
    assert(vertexBufferInd.vcount >= 3);
    meshes.push(WgMeshDataPool::gMeshDataPool->allocate(context));
    meshes.last()->update(context, vertexBufferInd);
}


void WgMeshDataGroup::append(WgContext& context, const Point pmin, const Point pmax)
{
    meshes.push(WgMeshDataPool::gMeshDataPool->allocate(context));
    meshes.last()->bbox(context, pmin, pmax);
}


void WgMeshDataGroup::release(WgContext& context)
{
    ARRAY_FOREACH(p, meshes)
        WgMeshDataPool::gMeshDataPool->free(context, *p);
    meshes.clear();
};


//***********************************************************************
// WgImageData
//***********************************************************************

void WgImageData::update(WgContext& context, const RenderSurface* surface)
{
    // get appropriate texture format from color space
    WGPUTextureFormat texFormat = WGPUTextureFormat_BGRA8Unorm;
    if (surface->cs == ColorSpace::ABGR8888S)
        texFormat = WGPUTextureFormat_RGBA8Unorm;
    if (surface->cs == ColorSpace::Grayscale8)
        texFormat = WGPUTextureFormat_R8Unorm;
    // allocate new texture handle
    bool texHandleChanged = context.allocateTexture(texture, surface->w, surface->h, texFormat, surface->data);
    // update texture view of texture handle was changed
    if (texHandleChanged) {
        context.releaseTextureView(textureView);
        textureView = context.createTextureView(texture);
    }
};


void WgImageData::release(WgContext& context)
{
    context.releaseTextureView(textureView);
    context.releaseTexture(texture);
};

//***********************************************************************
// WgRenderSettings
//***********************************************************************

void WgRenderSettings::updateFill(WgContext& context, const Fill* fill)
{
    rasterType = WgRenderRasterType::Gradient;
    // get gradient transfrom matrix
    Matrix invFillTransform;
    WgShaderTypeMat4x4f gradientTrans; // identity by default
    if (inverse(&fill->transform(), &invFillTransform))
        gradientTrans.update(invFillTransform);
    // get gradient rasterisation settings
    WgShaderTypeGradient gradient;
    if (fill->type() == Type::LinearGradient) {
        gradient.update((LinearGradient*)fill);
        fillType = WgRenderSettingsType::Linear;
    } else if (fill->type() == Type::RadialGradient) {
        gradient.update((RadialGradient*)fill);
        fillType = WgRenderSettingsType::Radial;
    }
    // update gpu assets
    bool bufferGradientSettingsChanged = context.allocateBufferUniform(bufferGroupGradient, &gradient.settings, sizeof(gradient.settings));
    bool bufferGradientTransformChanged = context.allocateBufferUniform(bufferGroupTransfromGrad, &gradientTrans.mat, sizeof(gradientTrans.mat));
    bool textureGradientChanged = context.allocateTexture(texGradient, WG_TEXTURE_GRADIENT_SIZE, 1, WGPUTextureFormat_RGBA8Unorm, gradient.texData);
    if (bufferGradientSettingsChanged || textureGradientChanged || bufferGradientTransformChanged) {
        // update texture view
        context.releaseTextureView(texViewGradient);
        texViewGradient = context.createTextureView(texGradient);
        // get sampler by spread type
        WGPUSampler sampler = context.samplerLinearClamp;
        if (fill->spread() == FillSpread::Reflect) sampler = context.samplerLinearMirror;
        if (fill->spread() == FillSpread::Repeat) sampler = context.samplerLinearRepeat;
        // update bind group
        context.layouts.releaseBindGroup(bindGroupGradient);
        bindGroupGradient = context.layouts.createBindGroupTexSampledBuff2Un(
            sampler, texViewGradient, bufferGroupGradient, bufferGroupTransfromGrad);
    }
    skip = false;
};


void WgRenderSettings::updateColor(WgContext& context, const RenderColor& c)
{
    rasterType = WgRenderRasterType::Solid;
    WgShaderTypeVec4f solidColor(c);
    if (context.allocateBufferUniform(bufferGroupSolid, &solidColor, sizeof(solidColor))) {
        context.layouts.releaseBindGroup(bindGroupSolid);
        bindGroupSolid = context.layouts.createBindGroupBuffer1Un(bufferGroupSolid);
    }
    fillType = WgRenderSettingsType::Solid;
    skip = (c.a == 0);
};


void WgRenderSettings::release(WgContext& context)
{
    context.layouts.releaseBindGroup(bindGroupSolid);
    context.layouts.releaseBindGroup(bindGroupGradient);
    context.releaseBuffer(bufferGroupSolid);
    context.releaseBuffer(bufferGroupGradient);
    context.releaseBuffer(bufferGroupTransfromGrad);
    context.releaseTexture(texGradient);
    context.releaseTextureView(texViewGradient);
};

//***********************************************************************
// WgRenderDataPaint
//***********************************************************************

void WgRenderDataPaint::release(WgContext& context)
{
    context.layouts.releaseBindGroup(bindGroupPaint);
    context.releaseBuffer(bufferModelMat);
    context.releaseBuffer(bufferBlendSettings);
    clips.clear();
};


void WgRenderDataPaint::update(WgContext& context, const tvg::Matrix& transform, tvg::ColorSpace cs, uint8_t opacity)
{
    WgShaderTypeMat4x4f modelMat(transform);
    WgShaderTypeVec4f blendSettings(cs, opacity);
    bool bufferModelMatChanged = context.allocateBufferUniform(bufferModelMat, &modelMat, sizeof(modelMat));
    bool bufferBlendSettingsChanged = context.allocateBufferUniform(bufferBlendSettings, &blendSettings, sizeof(blendSettings));
    if (bufferModelMatChanged || bufferBlendSettingsChanged) {
        context.layouts.releaseBindGroup(bindGroupPaint);
        bindGroupPaint = context.layouts.createBindGroupBuffer2Un(bufferModelMat, bufferBlendSettings);
    }
}


void WgRenderDataPaint::updateClips(tvg::Array<tvg::RenderData> &clips) {
    this->clips.clear();
    ARRAY_FOREACH(p, clips) {
        this->clips.push((WgRenderDataPaint*)(*p));
    }
}

//***********************************************************************
// WgRenderDataShape
//***********************************************************************

void WgRenderDataShape::appendShape(WgContext& context, const WgVertexBuffer& vertexBuffer)
{
    if (vertexBuffer.count < 3) return;
    Point pmin{}, pmax{};
    vertexBuffer.getMinMax(pmin, pmax);
    meshGroupShapes.append(context, vertexBuffer);
    meshGroupShapesBBox.append(context, pmin, pmax);
    updateBBox(pmin, pmax);
}


void WgRenderDataShape::appendStroke(WgContext& context, const WgIndexedVertexBuffer& vertexBufferInd)
{
    if (vertexBufferInd.vcount < 3) return;
    Point pmin{}, pmax{};
    vertexBufferInd.getMinMax(pmin, pmax);
    meshGroupStrokes.append(context, vertexBufferInd);
    meshGroupStrokesBBox.append(context, pmin, pmax);
    updateBBox(pmin, pmax);
}


void WgRenderDataShape::updateBBox(Point pmin, Point pmax)
{
    pMin.x = std::min(pMin.x, pmin.x);
    pMin.y = std::min(pMin.y, pmin.y);
    pMax.x = std::max(pMax.x, pmax.x);
    pMax.y = std::max(pMax.y, pmax.y);
}


void WgRenderDataShape::updateAABB(const Matrix& tr) {
    auto p0 = Point{pMin.x, pMin.y} * tr;
    auto p1 = Point{pMax.x, pMin.y} * tr;
    auto p2 = Point{pMin.x, pMax.y} * tr;
    auto p3 = Point{pMax.x, pMax.y} * tr;
    aabb.min = {std::min({p0.x, p1.x, p2.x, p3.x}), std::min({p0.y, p1.y, p2.y, p3.y})};
    aabb.max = {std::max({p0.x, p1.x, p2.x, p3.x}), std::max({p0.y, p1.y, p2.y, p3.y})};
}


void WgRenderDataShape::updateMeshes(WgContext& context, const RenderShape &rshape, const Matrix& tr, WgGeometryBufferPool* pool)
{
    releaseMeshes(context);
    strokeFirst = rshape.stroke ? rshape.stroke->strokeFirst : false;

    // get object scale
    float scale = std::max(std::min(length(Point{tr.e11 + tr.e12,tr.e21 + tr.e22}), 8.0f), 1.0f);

    // path decoded vertex buffer
    auto pbuff = pool->reqVertexBuffer(scale);

    pbuff->decodePath(rshape, true, [&](const WgVertexBuffer& path_buff) {
        appendShape(context, path_buff);
        if ((rshape.stroke) && (rshape.stroke->width > 0)) proceedStrokes(context, rshape.stroke, path_buff, pool);
    }, rshape.trimpath());

    // update shapes bbox (with empty path handling)
    if ((this->meshGroupShapesBBox.meshes.count > 0 ) ||
        (this->meshGroupStrokesBBox.meshes.count > 0)) {
        updateAABB(tr);
        meshDataBBox.bbox(context, pMin, pMax);
    } else aabb = {{0, 0}, {0, 0}};

    pool->retVertexBuffer(pbuff);
}


void WgRenderDataShape::proceedStrokes(WgContext& context, const RenderStroke* rstroke, const WgVertexBuffer& buff, WgGeometryBufferPool* pool)
{
    assert(rstroke);
    auto strokesGenerator = pool->reqIndexedVertexBuffer(buff.scale);
    if (rstroke->dash.length <= FLT_EPSILON) strokesGenerator->appendStrokes(buff, rstroke);
    else strokesGenerator->appendStrokesDashed(buff, rstroke);

    appendStroke(context, *strokesGenerator);

    pool->retIndexedVertexBuffer(strokesGenerator);
}


void WgRenderDataShape::releaseMeshes(WgContext& context)
{
    meshGroupStrokesBBox.release(context);
    meshGroupStrokes.release(context);
    meshGroupShapesBBox.release(context);
    meshGroupShapes.release(context);
    pMin = {FLT_MAX, FLT_MAX};
    pMax = {0.0f, 0.0f};
    aabb = {{0, 0}, {0, 0}};
    clips.clear();
}


void WgRenderDataShape::release(WgContext& context)
{
    releaseMeshes(context);
    meshDataBBox.release(context);
    renderSettingsStroke.release(context);
    renderSettingsShape.release(context);
    WgRenderDataPaint::release(context);
};

//***********************************************************************
// WgRenderDataShapePool
//***********************************************************************

WgRenderDataShape* WgRenderDataShapePool::allocate(WgContext& context)
{
    WgRenderDataShape* renderData{};
    if (mPool.count > 0) {
        renderData = mPool.last();
        mPool.pop();
    } else {
        renderData = new WgRenderDataShape();
        mList.push(renderData);
    }
    return renderData;
}


void WgRenderDataShapePool::free(WgContext& context, WgRenderDataShape* renderData)
{
    renderData->meshGroupShapes.release(context);
    renderData->meshGroupShapesBBox.release(context);
    renderData->meshGroupStrokes.release(context);
    renderData->meshGroupStrokesBBox.release(context);
    renderData->clips.clear();
    mPool.push(renderData);
}


void WgRenderDataShapePool::release(WgContext& context)
{
    ARRAY_FOREACH(p, mList) {
        (*p)->release(context);
        delete(*p);
    }
    mPool.clear();
    mList.clear();
}

//***********************************************************************
// WgRenderDataPicture
//***********************************************************************

void WgRenderDataPicture::updateSurface(WgContext& context, const RenderSurface* surface)
{
    // upoate mesh data
    meshData.imageBox(context, surface->w, surface->h);
    // update texture data
    imageData.update(context, surface);
    // update texture bind group
    context.layouts.releaseBindGroup(bindGroupPicture);
    bindGroupPicture = context.layouts.createBindGroupTexSampled(
        context.samplerLinearRepeat, imageData.textureView
    );
}


void WgRenderDataPicture::release(WgContext& context)
{
    context.layouts.releaseBindGroup(bindGroupPicture);
    imageData.release(context);
    meshData.release(context);
    WgRenderDataPaint::release(context);
}

//***********************************************************************
// WgRenderDataPicturePool
//***********************************************************************

WgRenderDataPicture* WgRenderDataPicturePool::allocate(WgContext& context)
{
    WgRenderDataPicture* renderData{};
    if (mPool.count > 0) {
        renderData = mPool.last();
        mPool.pop();
    } else {
        renderData = new WgRenderDataPicture();
        mList.push(renderData);
    }
    return renderData;
}


void WgRenderDataPicturePool::free(WgContext& context, WgRenderDataPicture* renderData)
{
    renderData->clips.clear();
    mPool.push(renderData);
}


void WgRenderDataPicturePool::release(WgContext& context)
{
    ARRAY_FOREACH(p, mList) {
        (*p)->release(context);
        delete(*p);
    }
    mPool.clear();
    mList.clear();
}

//***********************************************************************
// WgRenderDataGaussian
//***********************************************************************

void WgRenderDataViewport::update(WgContext& context, const RenderRegion& region) {
    WgShaderTypeVec4f viewport;
    viewport.update(region);
    bool bufferViewportChanged = context.allocateBufferUniform(bufferViewport, &viewport, sizeof(viewport));
    if (bufferViewportChanged) {
        context.layouts.releaseBindGroup(bindGroupViewport);
        bindGroupViewport = context.layouts.createBindGroupBuffer1Un(bufferViewport);
    }
}


void WgRenderDataViewport::release(WgContext& context) {
    context.releaseBuffer(bufferViewport);
    context.layouts.releaseBindGroup(bindGroupViewport);
}

//***********************************************************************
// WgRenderDataViewportPool
//***********************************************************************

WgRenderDataViewport* WgRenderDataViewportPool::allocate(WgContext& context)
{
    WgRenderDataViewport* renderData{};
    if (mPool.count > 0) {
        renderData = mPool.last();
        mPool.pop();
    } else {
        renderData = new WgRenderDataViewport();
        mList.push(renderData);
    }
    return renderData;
}


void WgRenderDataViewportPool::free(WgContext& context, WgRenderDataViewport* renderData)
{
    if (renderData) mPool.push(renderData);
}


void WgRenderDataViewportPool::release(WgContext& context)
{
    ARRAY_FOREACH(p, mList) {
        (*p)->release(context);
        delete(*p);
    }
    mPool.clear();
    mList.clear();
}

//***********************************************************************
// WgRenderDataEffectParams
//***********************************************************************

void WgRenderDataEffectParams::update(WgContext& context, const WgShaderTypeEffectParams& effectParams)
{
    if (context.allocateBufferUniform(bufferParams, &effectParams.params, sizeof(effectParams.params))) {
        context.layouts.releaseBindGroup(bindGroupParams);
        bindGroupParams = context.layouts.createBindGroupBuffer1Un(bufferParams);
    }
}


void WgRenderDataEffectParams::update(WgContext& context, const RenderEffectGaussianBlur* gaussian, const Matrix& transform)
{
    assert(gaussian);
    WgShaderTypeEffectParams effectParams;
    effectParams.update(gaussian, transform);
    update(context, effectParams);
    level = int(WG_GAUSSIAN_MAX_LEVEL * ((gaussian->quality - 1) * 0.01f)) + 1;
    extend = effectParams.extend;
}


void WgRenderDataEffectParams::update(WgContext& context, const RenderEffectDropShadow* dropShadow, const Matrix& transform)
{
    assert(dropShadow);
    WgShaderTypeEffectParams effectParams;
    effectParams.update(dropShadow, transform);
    update(context, effectParams);
    level = int(WG_GAUSSIAN_MAX_LEVEL * ((dropShadow->quality - 1) * 0.01f)) + 1;
    extend = effectParams.extend;
    offset = effectParams.offset;
}


void WgRenderDataEffectParams::update(WgContext& context, const RenderEffectFill* fill)
{
    assert(fill);
    WgShaderTypeEffectParams effectParams;
    effectParams.update(fill);
    update(context, effectParams);
}


void WgRenderDataEffectParams::update(WgContext& context, const RenderEffectTint* tint)
{
    assert(tint);
    WgShaderTypeEffectParams effectParams;
    effectParams.update(tint);
    update(context, effectParams);
}


void WgRenderDataEffectParams::update(WgContext& context, const RenderEffectTritone* tritone)
{
    assert(tritone);
    WgShaderTypeEffectParams effectParams;
    effectParams.update(tritone);
    update(context, effectParams);
}


void WgRenderDataEffectParams::release(WgContext& context)
{
    context.releaseBuffer(bufferParams);
    context.layouts.releaseBindGroup(bindGroupParams);
}

//***********************************************************************
// WgRenderDataColorsPool
//***********************************************************************

WgRenderDataEffectParams* WgRenderDataEffectParamsPool::allocate(WgContext& context)
{
    WgRenderDataEffectParams* renderData{};
    if (mPool.count > 0) {
        renderData = mPool.last();
        mPool.pop();
    } else {
        renderData = new WgRenderDataEffectParams();
        mList.push(renderData);
    }
    return renderData;
}


void WgRenderDataEffectParamsPool::free(WgContext& context, WgRenderDataEffectParams* renderData)
{
    if (renderData) mPool.push(renderData);
}


void WgRenderDataEffectParamsPool::release(WgContext& context)
{
    ARRAY_FOREACH(p, mList) {
        (*p)->release(context);
        delete(*p);
    }
    mPool.clear();
    mList.clear();
}
