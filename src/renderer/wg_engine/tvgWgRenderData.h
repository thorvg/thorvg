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

#include "tvgWgPipelines.h"
#include "tvgWgGeometry.h"

struct WgMeshData {
    WGPUBuffer bufferPosition{};
    WGPUBuffer bufferTexCoord{};
    WGPUBuffer bufferIndex{};
    size_t vertexCount{};
    size_t indexCount{};

    void draw(WGPURenderPassEncoder renderPassEncoder);
    void drawImage(WGPURenderPassEncoder renderPassEncoder);

    void update(WgContext& context, WgGeometryData* geometryData);
    void release(WgContext& context);
};

struct WgMeshDataGroup {
    Array<WgMeshData*> meshes{};

    void update(WgContext& context, WgGeometryDataGroup* geometryDataGroup);
    void release(WgContext& context);
};

struct WgImageData {
    WGPUTexture texture{};
    WGPUTextureView textureView{};

    void update(WgContext& context, Surface* surface);
    void release(WgContext& context);
};

enum class WgRenderSettingsType { None = 0, Solid = 1, Linear = 2, Radial = 3 };

struct WgRenderSettings
{
    WgBindGroupSolidColor bindGroupSolid{};
    WgBindGroupLinearGradient bindGroupLinear{};
    WgBindGroupRadialGradient bindGroupRadial{};
    WgRenderSettingsType fillType{};

    void update(WgContext& context, const Fill* fill, const uint8_t* color, const RenderUpdateFlag flags);
    void release(WgContext& context);
};

struct WgRenderDataPaint
{
    WgBindGroupPaint bindGroupPaint{};

    virtual void release(WgContext& context);
    virtual uint32_t identifier() { return TVG_CLASS_ID_UNDEFINED; };
};

struct WgRenderDataShape: public WgRenderDataPaint
{
    WgRenderSettings renderSettingsShape{};
    WgRenderSettings renderSettingsStroke{};
    WgMeshDataGroup meshGroupShapes{};
    WgMeshDataGroup meshGroupStrokes{};
    WgMeshData meshBBoxShapes{};
    WgMeshData meshBBoxStrokes{};

    void updateMeshes(WgContext& context, const RenderShape& rshape);
    void releaseMeshes(WgContext& context);
    void release(WgContext& context) override;
    uint32_t identifier() override { return TVG_CLASS_ID_SHAPE; };
};

struct WgRenderDataPicture: public WgRenderDataPaint
{
    WgBindGroupPicture bindGroupPicture{};
    WgImageData imageData{};
    WgMeshData meshData{};

    void update(WgContext& context);
    void release(WgContext& context) override;
    uint32_t identifier() override { return TVG_CLASS_ID_PICTURE; };
};
