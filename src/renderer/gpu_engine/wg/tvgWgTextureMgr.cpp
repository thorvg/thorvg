/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

#include "tvgWgTextureMgr.h"
#include "tvgWgShaderSrc.h"

static tvg::Inlist<WgTextureEntry>& _entries(WgTextureMgr::SurfaceEntry& surfaceEntry, FilterMethod filter)
{
    return (filter == FilterMethod::Bilinear) ? surfaceEntry.bilinear : surfaceEntry.nearest;
}

static WgTextureEntry* _findEntry(tvg::Inlist<WgTextureEntry>& entries, WGPUTexture texture)
{
    INLIST_FOREACH(entries, entry)
    {
        if (entry->texture == texture) return entry;
    }
    return nullptr;
}

static bool _matches(const WgTextureEntry& entry, const RenderSurface* surface, WGPUTextureFormat format)
{
    return entry.texture && (wgpuTextureGetWidth(entry.texture) == surface->w) && (wgpuTextureGetHeight(entry.texture) == surface->h) && (wgpuTextureGetFormat(entry.texture) == format);
}

WgTextureMgr::SurfaceEntry* WgTextureMgr::find(const RenderSurface* surface)
{
    INLIST_FOREACH(surfaces, entry)
    {
        if (entry->surface == surface) return entry;
    }
    return nullptr;
}

WGPUTextureFormat WgTextureMgr::textureFormat(const RenderSurface* surface)
{
    if (surface->cs == ColorSpace::ABGR8888 || surface->cs == ColorSpace::ABGR8888S) return WGPUTextureFormat_RGBA8Unorm;
    if (surface->cs == ColorSpace::ARGB8888 || surface->cs == ColorSpace::ARGB8888S) {
        return surface->premultiplied ? WGPUTextureFormat_BGRA8Unorm : WGPUTextureFormat_RGBA8Unorm;
    }
    return WGPUTextureFormat_R8Unorm;  // must be
}

void WgTextureMgr::upload(WgContext& context, WgTextureEntry& entry, const RenderSurface* surface, FilterMethod filter)
{
    uint8_t preprocess = entry.preprocess & Queued;
    if (surface->channelSize == sizeof(uint32_t)) {
        if (!surface->premultiplied) preprocess |= Premultiply;
        if (!surface->premultiplied && (surface->cs == ColorSpace::ARGB8888 || surface->cs == ColorSpace::ARGB8888S)) preprocess |= Bgr;
    }
    entry.preprocess = static_cast<WgTexPrep>(preprocess);

    auto bytesPerRow = surface->stride * CHANNEL_SIZE(surface->cs);
    auto dataSize = static_cast<uint64_t>(bytesPerRow) * surface->h;
    if (!context.allocateTexture(entry.texture, surface->w, surface->h, textureFormat(surface), surface->data, bytesPerRow, dataSize)) return;

    context.releaseTextureView(entry.textureView);
    entry.textureView = context.createTextureView(entry.texture);

    context.layouts.releaseBindGroup(entry.bindGroup);
    auto sampler = (filter == FilterMethod::Bilinear) ? context.samplerLinearClamp : context.samplerNearestClamp;
    entry.bindGroup = context.layouts.createBindGroupTexSampled(sampler, entry.textureView);
}

bool WgTextureMgr::flushPreprocess(WgContext& context)
{
    if (prepRequests.empty()) return true;

    if (!prepShader) {
        WGPUShaderSourceWGSL source{
            .chain = {.sType = WGPUSType_ShaderSourceWGSL},
            .code = {.data = cShaderSrc_Texture_Preprocess, .length = WGPU_STRLEN}};
        const WGPUShaderModuleDescriptor descriptor{.nextInChain = &source.chain};
        prepShader = wgpuDeviceCreateShaderModule(context.device, &descriptor);

        WGPUBindGroupLayout bindGroupLayouts[]{context.layouts.layoutTexSampled, context.layouts.layoutTexStrorage1WO};
        const WGPUPipelineLayoutDescriptor layoutDescriptor{.bindGroupLayoutCount = 2, .bindGroupLayouts = bindGroupLayouts};
        prepLayout = wgpuDeviceCreatePipelineLayout(context.device, &layoutDescriptor);
    }

    WGPUCommandEncoder encoder{};
    ARRAY_FOREACH(p, prepRequests)
    {
        auto* entry = *p;
        auto preprocess = entry->preprocess & (Premultiply | Bgr);
        auto width = wgpuTextureGetWidth(entry->texture);
        auto height = wgpuTextureGetHeight(entry->texture);
        if (!prepStaging || width > prepStagingWidth || height > prepStagingHeight) {
            if (encoder) {
                context.submitCommandEncoder(encoder);
                context.releaseCommandEncoder(encoder);
            }
            context.layouts.releaseBindGroup(prepStagingBindGroup);
            context.releaseTextureView(prepStagingView);
            context.releaseTexture(prepStaging);
            prepStaging = context.createTexStorage(width, height, WGPUTextureFormat_RGBA8Unorm);
            prepStagingView = context.createTextureView(prepStaging);
            prepStagingBindGroup = context.layouts.createBindGroupStrorage1WO(prepStagingView);
            prepStagingWidth = width;
            prepStagingHeight = height;
        }

        auto& pipeline = (preprocess & Bgr) ? prepPremultBgr : prepPremult;
        if (!pipeline) {
            auto entryPoint = (preprocess & Bgr) ? "cs_main_premult_bgr" : "cs_main_premult";
            const WGPUComputePipelineDescriptor descriptor{
                .layout = prepLayout,
                .compute = {.module = prepShader, .entryPoint = {.data = entryPoint, .length = WGPU_STRLEN}}};
            pipeline = wgpuDeviceCreateComputePipeline(context.device, &descriptor);
        }
        if (!encoder) encoder = context.createCommandEncoder();
        const WGPUComputePassDescriptor descriptor{};
        auto pass = wgpuCommandEncoderBeginComputePass(encoder, &descriptor);
        wgpuComputePassEncoderSetPipeline(pass, pipeline);
        wgpuComputePassEncoderSetBindGroup(pass, 0, entry->bindGroup, 0, nullptr);
        wgpuComputePassEncoderSetBindGroup(pass, 1, prepStagingBindGroup, 0, nullptr);
        wgpuComputePassEncoderDispatchWorkgroups(pass, (width + 15) / 16, (height + 15) / 16, 1);
        wgpuComputePassEncoderEnd(pass);
        wgpuComputePassEncoderRelease(pass);

        const WGPUTexelCopyTextureInfo source{.texture = prepStaging};
        const WGPUTexelCopyTextureInfo destination{.texture = entry->texture};
        const WGPUExtent3D size{.width = width, .height = height, .depthOrArrayLayers = 1};
        wgpuCommandEncoderCopyTextureToTexture(encoder, &source, &destination, &size);
        entry->preprocess = PrepNone;
    }
    prepRequests.clear();
    if (encoder) {
        context.submitCommandEncoder(encoder);
        context.releaseCommandEncoder(encoder);
    }
    return true;
}

void WgTextureMgr::releaseEntry(WgContext& context, WgTextureEntry& entry)
{
    context.layouts.releaseBindGroup(entry.bindGroup);
    context.releaseTextureView(entry.textureView);
    context.releaseTexture(entry.texture);
    entry.refCnt = 0;
}

const WgTextureEntry* WgTextureMgr::retain(WgContext& context, const RenderSurface* surface, FilterMethod filter, bool refreshTexture)
{
    auto* surfaceEntry = find(surface);
    if (!surfaceEntry) {
        surfaceEntry = new SurfaceEntry;
        surfaceEntry->surface = surface;
        surfaces.back(surfaceEntry);
    }

    auto& entries = _entries(*surfaceEntry, filter);
    auto* entry = entries.tail;
    auto format = textureFormat(surface);
    if (entry && refreshTexture && !_matches(*entry, surface, format) && entry->refCnt > 0) {
        entry = new WgTextureEntry;
        entries.back(entry);
    } else if (!entry) {
        entry = new WgTextureEntry;
        entries.back(entry);
    }
    if (!entry->texture || refreshTexture) upload(context, *entry, surface, filter);

    if ((entry->preprocess & Queued) && !(entry->preprocess & (Premultiply | Bgr))) {
        for (uint32_t i = 0; i < prepRequests.count; ++i) {
            if (prepRequests[i] != entry) continue;
            prepRequests[i] = prepRequests.last();
            prepRequests.pop();
            break;
        }
        entry->preprocess = PrepNone;
    } else if (entry->texture && (entry->preprocess & (Premultiply | Bgr)) && !(entry->preprocess & Queued)) {
        entry->preprocess = static_cast<WgTexPrep>(entry->preprocess | Queued);
        prepRequests.push(entry);
    }

    ++entry->refCnt;
    return entry;
}

void WgTextureMgr::release(WgContext& context, const RenderSurface* surface, FilterMethod filter, WGPUTexture texture)
{
    auto* surfaceEntry = find(surface);
    if (!surfaceEntry) return;

    auto& entries = _entries(*surfaceEntry, filter);
    auto* entry = _findEntry(entries, texture);
    if (!entry) return;

    if (entry->refCnt > 0) --entry->refCnt;
    if (entry->refCnt > 0) return;

    if (entry->preprocess & Queued) {
        for (uint32_t i = 0; i < prepRequests.count; ++i) {
            if (prepRequests[i] != entry) continue;
            prepRequests[i] = prepRequests.last();
            prepRequests.pop();
            break;
        }
        entry->preprocess = PrepNone;
    }

    releaseEntry(context, *entry);
    entries.remove(entry);
    delete (entry);
    if (surfaceEntry->bilinear.empty() && surfaceEntry->nearest.empty()) {
        surfaces.remove(surfaceEntry);
        delete (surfaceEntry);
    }
}

void WgTextureMgr::clear(WgContext& context)
{
    prepRequests.clear();
    context.layouts.releaseBindGroup(prepStagingBindGroup);
    context.releaseTextureView(prepStagingView);
    context.releaseTexture(prepStaging);
    prepStagingWidth = prepStagingHeight = 0;
    if (prepPremult) wgpuComputePipelineRelease(prepPremult);
    if (prepPremultBgr) wgpuComputePipelineRelease(prepPremultBgr);
    prepPremult = prepPremultBgr = nullptr;
    if (prepLayout) wgpuPipelineLayoutRelease(prepLayout);
    prepLayout = nullptr;
    if (prepShader) wgpuShaderModuleRelease(prepShader);
    prepShader = nullptr;

    while (auto* surfaceEntry = surfaces.front()) {
        while (auto* entry = surfaceEntry->bilinear.front()) {
            releaseEntry(context, *entry);
            delete (entry);
        }
        while (auto* entry = surfaceEntry->nearest.front()) {
            releaseEntry(context, *entry);
            delete (entry);
        }
        delete (surfaceEntry);
    }
    if (++stamp == 0) stamp = 1;
}
