
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
    assert(vertexBuffer.vcount > 2);
    vertexCount = vertexBuffer.vcount;
    indexCount = (vertexBuffer.vcount - 2) * 3;
    // buffer position data create and write
    context.allocateBufferVertex(bufferPosition, (float *)&vertexBuffer.vbuff, vertexCount * sizeof(float) * 2);
    // buffer index data create and write
    context.allocateBufferIndexFan(vertexCount);
}


void WgMeshData::update(WgContext& context, const WgVertexBufferInd& vertexBufferInd)
{
    assert(vertexBufferInd.vcount > 2);
    vertexCount = vertexBufferInd.vcount;
    indexCount = vertexBufferInd.icount;
    // buffer position data create and write
    if (vertexCount > 0)
        context.allocateBufferVertex(bufferPosition, (float *)&vertexBufferInd.vbuff, vertexCount * sizeof(float) * 2);
    // buffer tex coords data create and write
    if (vertexCount > 0)
        context.allocateBufferVertex(bufferTexCoord, (float *)&vertexBufferInd.tbuff, vertexCount * sizeof(float) * 2);
    // buffer index data create and write
    if (indexCount > 0)
        context.allocateBufferIndex(bufferIndex, vertexBufferInd.ibuff, indexCount * sizeof(uint32_t));
};


void WgMeshData::update(WgContext& context, const Point pmin, const Point pmax)
{
    vertexCount = 4;
    indexCount = 6;
    const float data[] = {
        pmin.x, pmin.y, pmax.x, pmin.y,
        pmax.x, pmax.y, pmin.x, pmax.y
    };
    context.allocateBufferVertex(bufferPosition, data, sizeof(data));
    context.allocateBufferIndexFan(vertexCount);
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
    for (uint32_t i = 0; i < mList.count; i++) {
        mList[i]->release(context);
        delete mList[i];
    }
    mPool.clear();
    mList.clear();
}

WgMeshDataPool* WgMeshDataGroup::gMeshDataPool = nullptr;

//***********************************************************************
// WgMeshDataGroup
//***********************************************************************

void WgMeshDataGroup::append(WgContext& context, const WgVertexBuffer& vertexBuffer)
{
    assert(vertexBuffer.vcount >= 3);
    meshes.push(gMeshDataPool->allocate(context));
    meshes.last()->update(context, vertexBuffer);
}


void WgMeshDataGroup::append(WgContext& context, const WgVertexBufferInd& vertexBufferInd)
{
    assert(vertexBufferInd.vcount >= 3);
    meshes.push(gMeshDataPool->allocate(context));
    meshes.last()->update(context, vertexBufferInd);
}


void WgMeshDataGroup::append(WgContext& context, const Point pmin, const Point pmax)
{
    meshes.push(gMeshDataPool->allocate(context));
    meshes.last()->update(context, pmin, pmax);
}


void WgMeshDataGroup::release(WgContext& context)
{
    for (uint32_t i = 0; i < meshes.count; i++)
        gMeshDataPool->free(context, meshes[i]);
    meshes.clear();
};


//***********************************************************************
// WgImageData
//***********************************************************************

void WgImageData::update(WgContext& context, RenderSurface* surface)
{
    release(context);
    assert(surface);
    texture = context.createTexture(surface->w, surface->h, WGPUTextureFormat_RGBA8Unorm);
    assert(texture);
    textureView = context.createTextureView(texture);
    assert(textureView);
    // update texture data
    WGPUImageCopyTexture imageCopyTexture{ .texture = texture };
    WGPUTextureDataLayout textureDataLayout{ .bytesPerRow = 4 * surface->w, .rowsPerImage = surface->h };
    WGPUExtent3D writeSize{ .width = surface->w, .height = surface->h, .depthOrArrayLayers = 1 };
    wgpuQueueWriteTexture(context.queue, &imageCopyTexture, surface->data, 4 * surface->w * surface->h, &textureDataLayout, &writeSize);
    wgpuQueueSubmit(context.queue, 0, nullptr);
};


void WgImageData::release(WgContext& context)
{
    context.releaseTextureView(textureView);
    context.releaseTexture(texture);
};

//***********************************************************************
// WgRenderSettings
//***********************************************************************

void WgRenderSettings::update(WgContext& context, const Fill* fill, const uint8_t* color, const RenderUpdateFlag flags)
{
    // setup fill properties
    if ((flags & (RenderUpdateFlag::Gradient)) && fill) {
        rasterType = WgRenderRasterType::Gradient;
        // get gradient transfrom matrix
        Matrix fillTransform = fill->transform();
        Matrix invFillTransform;
        WgShaderTypeMat4x4f gradientTrans; // identity by default
        if (inverse(&fillTransform, &invFillTransform))
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
            context.pipelines->layouts.releaseBindGroup(bindGroupGradient);
            bindGroupGradient = context.pipelines->layouts.createBindGroupTexSampledBuff2Un(
                sampler, texViewGradient, bufferGroupGradient, bufferGroupTransfromGrad);
        }
        skip = false;
    } else if ((flags & (RenderUpdateFlag::Color)) && !fill) {
        rasterType = WgRenderRasterType::Solid;
        WgShaderTypeSolidColor solidColor(color);
        if (context.allocateBufferUniform(bufferGroupSolid, &solidColor, sizeof(solidColor))) {
            context.pipelines->layouts.releaseBindGroup(bindGroupSolid);
            bindGroupSolid = context.pipelines->layouts.createBindGroupBuffer1Un(bufferGroupSolid);
        }
        fillType = WgRenderSettingsType::Solid;
        skip = (color[3] == 0);
    }
};


void WgRenderSettings::release(WgContext& context)
{
    context.pipelines->layouts.releaseBindGroup(bindGroupSolid);
    context.pipelines->layouts.releaseBindGroup(bindGroupGradient);
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
    context.pipelines->layouts.releaseBindGroup(bindGroupPaint);
    context.releaseBuffer(bufferModelMat);
    context.releaseBuffer(bufferBlendSettings);
    clips.clear();
};


void WgRenderDataPaint::update(WgContext& context, const tvg::Matrix& transform, tvg::ColorSpace cs, uint8_t opacity)
{
    WgShaderTypeMat4x4f modelMat(transform);
    WgShaderTypeBlendSettings blendSettings(cs, opacity);
    bool bufferModelMatChanged = context.allocateBufferUniform(bufferModelMat, &modelMat, sizeof(modelMat));
    bool bufferBlendSettingsChanged = context.allocateBufferUniform(bufferBlendSettings, &blendSettings, sizeof(blendSettings));
    if (bufferModelMatChanged || bufferBlendSettingsChanged) {
        context.pipelines->layouts.releaseBindGroup(bindGroupPaint);
        bindGroupPaint = context.pipelines->layouts.createBindGroupBuffer2Un(bufferModelMat, bufferBlendSettings);
    }
}


void WgRenderDataPaint::updateClips(tvg::Array<tvg::RenderData> &clips) {
    this->clips.clear();
    for (uint32_t i = 0; i < clips.count; i++)
        if (clips[i])
            this->clips.push((WgRenderDataPaint*)clips[i]);
}

//***********************************************************************
// WgRenderDataShape
//***********************************************************************

void WgRenderDataShape::appendShape(WgContext context, const WgVertexBuffer& vertexBuffer)
{
    if (vertexBuffer.vcount < 3) return;
    Point pmin{}, pmax{};
    vertexBuffer.getMinMax(pmin, pmax);
    meshGroupShapes.append(context, vertexBuffer);
    meshGroupShapesBBox.append(context, pmin, pmax);
    updateBBox(pmin, pmax);
}


void WgRenderDataShape::appendStroke(WgContext context, const WgVertexBufferInd& vertexBufferInd)
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
    Point p0 = Point{pMin.x, pMin.y} * tr;
    Point p1 = Point{pMax.x, pMin.y} * tr;
    Point p2 = Point{pMin.x, pMax.y} * tr;
    Point p3 = Point{pMax.x, pMax.y} * tr;
    aabb.x = std::min({p0.x, p1.x, p2.x, p3.x});
    aabb.y = std::min({p0.y, p1.y, p2.y, p3.y});
    aabb.w = std::max({p0.x, p1.x, p2.x, p3.x}) - aabb.x;
    aabb.h = std::max({p0.y, p1.y, p2.y, p3.y}) - aabb.y;
}


void WgRenderDataShape::updateMeshes(WgContext &context, const RenderShape &rshape, const Matrix& tr)
{
    releaseMeshes(context);
    strokeFirst = rshape.stroke ? rshape.stroke->strokeFirst : false;

    // path decoded vertex buffer
    WgVertexBuffer pbuff;
    // append shape without strokes
    if (!rshape.stroke) {
        pbuff.decodePath(rshape, false, [&](const WgVertexBuffer& path_buff) {
            appendShape(context, path_buff);
        });
    // append shape with strokes
    } else {
        float tbeg{}, tend{};
        if (!rshape.stroke->strokeTrim(tbeg, tend)) { tbeg = 0.0f; tend = 1.0f; }
        bool loop = tbeg > tend;
        if (tbeg == tend) {
            pbuff.decodePath(rshape, false, [&](const WgVertexBuffer& path_buff) {
                appendShape(context, path_buff);
            });
        } else if (rshape.stroke->trim.simultaneous) {
            pbuff.decodePath(rshape, true, [&](const WgVertexBuffer& path_buff) {
                appendShape(context, path_buff);
                if (loop) {
                    proceedStrokes(context, rshape.stroke, tbeg, 1.0f, path_buff);
                    proceedStrokes(context, rshape.stroke, 0.0f, tend, path_buff);
                } else {
                    proceedStrokes(context, rshape.stroke, tbeg, tend, path_buff);
                }
            });
        } else {
            float totalLen = 0.0f;
            pbuff.decodePath(rshape, true, [&](const WgVertexBuffer& path_buff) {
                appendShape(context, path_buff);
                totalLen += path_buff.total();
            });
            float len_beg = totalLen * tbeg; // trim length begin
            float len_end = totalLen * tend; // trim length end
            float len_acc = 0.0; // accumulated length
            // append strokes
            pbuff.decodePath(rshape, true, [&](const WgVertexBuffer& path_buff) {
                float len_path = path_buff.total(); // current path length
                if (loop) {
                    if (len_acc + len_path >= len_beg) {
                        auto tbeg = len_acc <= len_beg ? (len_beg - len_acc) / len_path : 0.0f;
                        proceedStrokes(context, rshape.stroke, tbeg, 1.0f, path_buff);
                    }
                    if (len_acc < len_end) {
                        auto tend = len_acc + len_path >= len_end ? (len_end - len_acc) / len_path : 1.0f;
                        proceedStrokes(context, rshape.stroke, 0.0f, tend, path_buff);
                    }
                } else {
                    if (len_acc + len_path >= len_beg && len_acc <= len_end) {
                        auto tbeg = len_acc <= len_beg ? (len_beg - len_acc) / len_path : 0.0f;
                        auto tend = len_acc + len_path >= len_end ? (len_end - len_acc) / len_path : 1.0f;
                        proceedStrokes(context, rshape.stroke, tbeg, tend, path_buff);
                    }
                }
                len_acc += len_path;
            });
        }
    } 
    // update shapes bbox
    updateAABB(tr);
    meshDataBBox.update(context, pMin, pMax);
}


void WgRenderDataShape::proceedStrokes(WgContext context, const RenderStroke* rstroke, float tbeg, float tend, const WgVertexBuffer& buff)
{
    assert(rstroke);
    WgVertexBufferInd stroke_buff;
    // trim -> dash -> stroke
    if ((tbeg != 0.0f) || (tend != 1.0f)) {
        if (tbeg == tend) return;
        WgVertexBuffer trimed_buff;
        trimed_buff.trim(buff, tbeg, tend);
        trimed_buff.updateDistances();
        // trim ->dash -> stroke
        if (rstroke->dashPattern) stroke_buff.appendStrokesDashed(trimed_buff, rstroke);
        // trim -> stroke
        else stroke_buff.appendStrokes(trimed_buff, rstroke);
    } else
    // dash -> stroke
    if (rstroke->dashPattern) {
        stroke_buff.appendStrokesDashed(buff, rstroke);
    // stroke
    } else
        stroke_buff.appendStrokes(buff, rstroke);
    appendStroke(context, stroke_buff);
}


void WgRenderDataShape::releaseMeshes(WgContext &context)
{
    meshGroupStrokesBBox.release(context);
    meshGroupStrokes.release(context);
    meshGroupShapesBBox.release(context);
    meshGroupShapes.release(context);
    pMin = {FLT_MAX, FLT_MAX};
    pMax = {0.0f, 0.0f};
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
    WgRenderDataShape* dataShape{};
    if (mPool.count > 0) {
        dataShape = mPool.last();
        mPool.pop();
    } else {
        dataShape = new WgRenderDataShape();
        mList.push(dataShape);
    }
    return dataShape;
}


void WgRenderDataShapePool::free(WgContext& context, WgRenderDataShape* dataShape)
{
    dataShape->meshGroupShapes.release(context);
    dataShape->meshGroupShapesBBox.release(context);
    dataShape->meshGroupStrokes.release(context);
    dataShape->meshGroupStrokesBBox.release(context);
    dataShape->clips.clear();
    mPool.push(dataShape);
}


void WgRenderDataShapePool::release(WgContext& context)
{
    for (uint32_t i = 0; i < mList.count; i++) {
        mList[i]->release(context);
        delete mList[i];
    }
    mPool.clear();
    mList.clear();
}

//***********************************************************************
// WgRenderDataPicture
//***********************************************************************

void WgRenderDataPicture::release(WgContext& context)
{
    meshData.release(context);
    imageData.release(context);
    context.pipelines->layouts.releaseBindGroup(bindGroupPicture);
    WgRenderDataPaint::release(context);
}
