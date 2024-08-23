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

#include "tvgWgBindGroups.h"

WGPUBindGroup WgBindGroupLayouts::createBindGroupTexSampled(WGPUSampler sampler, WGPUTextureView texView)
{
    const WGPUBindGroupEntry bindGroupEntrys[] = {
        { .binding = 0, .sampler = sampler },
        { .binding = 1, .textureView = texView }
    };
    const WGPUBindGroupDescriptor bindGroupDesc { .layout = layoutTexSampled, .entryCount = 2, .entries = bindGroupEntrys };
    return wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}


WGPUBindGroup WgBindGroupLayouts::createBindGroupTexSampledBuff1Un(WGPUSampler sampler, WGPUTextureView texView, WGPUBuffer buff)
{
    const WGPUBindGroupEntry bindGroupEntrys[] = {
        { .binding = 0, .sampler = sampler },
        { .binding = 1, .textureView = texView },
        { .binding = 2, .buffer = buff, .size = wgpuBufferGetSize(buff) }
    };
    const WGPUBindGroupDescriptor bindGroupDesc { .layout = layoutTexSampledBuff1Un, .entryCount = 3, .entries = bindGroupEntrys };
    return wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}


WGPUBindGroup WgBindGroupLayouts::createBindGroupStrorage1WO(WGPUTextureView texView)
{
    const WGPUBindGroupEntry bindGroupEntrys[] = {
        { .binding = 0, .textureView = texView }
    };
    const WGPUBindGroupDescriptor bindGroupDesc { .layout = layoutTexStrorage1WO, .entryCount = 1, .entries = bindGroupEntrys };
    return wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}


WGPUBindGroup WgBindGroupLayouts::createBindGroupStrorage1RO(WGPUTextureView texView)
{
    const WGPUBindGroupEntry bindGroupEntrys[] = {
        { .binding = 0, .textureView = texView }
    };
    const WGPUBindGroupDescriptor bindGroupDesc { .layout = layoutTexStrorage1RO, .entryCount = 1, .entries = bindGroupEntrys };
    return wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}


WGPUBindGroup WgBindGroupLayouts::createBindGroupStrorage2RO(WGPUTextureView texView0, WGPUTextureView texView1)
{
    const WGPUBindGroupEntry bindGroupEntrys[] = {
        { .binding = 0, .textureView = texView0 },
        { .binding = 1, .textureView = texView1 }
    };
    const WGPUBindGroupDescriptor bindGroupDesc { .layout = layoutTexStrorage2RO, .entryCount = 2, .entries = bindGroupEntrys };
    return wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}


WGPUBindGroup WgBindGroupLayouts::createBindGroupStrorage3RO(WGPUTextureView texView0, WGPUTextureView texView1, WGPUTextureView texView2)
{
    const WGPUBindGroupEntry bindGroupEntrys[] = {
        { .binding = 0, .textureView = texView0 },
        { .binding = 1, .textureView = texView1 },
        { .binding = 2, .textureView = texView2 }
    };
    const WGPUBindGroupDescriptor bindGroupDesc { .layout = layoutTexStrorage3RO, .entryCount = 3, .entries = bindGroupEntrys };
    return wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}


WGPUBindGroup WgBindGroupLayouts::createBindGroupBuffer1Un(WGPUBuffer buff)
{
    const WGPUBindGroupEntry bindGroupEntrys[] = {
        { .binding = 0, .buffer = buff, .size = wgpuBufferGetSize(buff) }
    };
    const WGPUBindGroupDescriptor bindGroupDesc { .layout = layoutBuffer1Un, .entryCount = 1, .entries = bindGroupEntrys };
    return wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}


WGPUBindGroup WgBindGroupLayouts::createBindGroupBuffer2Un(WGPUBuffer buff0, WGPUBuffer buff1)
{
    const WGPUBindGroupEntry bindGroupEntrys[] = {
        { .binding = 0, .buffer = buff0, .size = wgpuBufferGetSize(buff0) },
        { .binding = 1, .buffer = buff1, .size = wgpuBufferGetSize(buff1) }
    };
    const WGPUBindGroupDescriptor bindGroupDesc { .layout = layoutBuffer2Un, .entryCount = 2, .entries = bindGroupEntrys };
    return wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}


WGPUBindGroup WgBindGroupLayouts::createBindGroupBuffer3Un(WGPUBuffer buff0, WGPUBuffer buff1, WGPUBuffer buff2)
{
    const WGPUBindGroupEntry bindGroupEntrys[] = {
        { .binding = 0, .buffer = buff0, .size = wgpuBufferGetSize(buff0) },
        { .binding = 1, .buffer = buff1, .size = wgpuBufferGetSize(buff1) },
        { .binding = 2, .buffer = buff2, .size = wgpuBufferGetSize(buff2) }
    };
    const WGPUBindGroupDescriptor bindGroupDesc { .layout = layoutBuffer3Un, .entryCount = 3, .entries = bindGroupEntrys };
    return wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}


void WgBindGroupLayouts::releaseBindGroup(WGPUBindGroup& bindGroup)
{
    if (bindGroup) wgpuBindGroupRelease(bindGroup);
    bindGroup = nullptr;
}


void WgBindGroupLayouts::initialize(WgContext& context)
{
    // store device handle
    device = context.device;
    assert(device);

    // common bind group settings
    const WGPUShaderStageFlags visibility_vert = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
    const WGPUShaderStageFlags visibility_frag = WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
    const WGPUSamplerBindingLayout sampler = { .type = WGPUSamplerBindingType_Filtering };
    const WGPUTextureBindingLayout texture = { .sampleType = WGPUTextureSampleType_Float, .viewDimension = WGPUTextureViewDimension_2D };
    const WGPUStorageTextureBindingLayout storageTextureWO { .access = WGPUStorageTextureAccess_WriteOnly, .format = WGPUTextureFormat_RGBA8Unorm, .viewDimension = WGPUTextureViewDimension_2D };
    const WGPUStorageTextureBindingLayout storageTextureRO { .access = WGPUStorageTextureAccess_ReadOnly,  .format = WGPUTextureFormat_RGBA8Unorm, .viewDimension = WGPUTextureViewDimension_2D };
    const WGPUBufferBindingLayout bufferUniform { .type = WGPUBufferBindingType_Uniform };

    { // bind group layout tex sampled
        const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
            { .binding = 0, .visibility = visibility_frag, .sampler = sampler },
            { .binding = 1, .visibility = visibility_frag, .texture = texture }
        };
        const WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc { .entryCount = 2, .entries = bindGroupLayoutEntries };
        layoutTexSampled = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
        assert(layoutTexSampled);
    }

    { // bind group layout tex sampled with buffer uniform
        const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
            { .binding = 0, .visibility = visibility_frag, .sampler = sampler },
            { .binding = 1, .visibility = visibility_frag, .texture = texture },
            { .binding = 2, .visibility = visibility_vert, .buffer = bufferUniform }
        };
        const WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc { .entryCount = 3, .entries = bindGroupLayoutEntries };
        layoutTexSampledBuff1Un = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
        assert(layoutTexSampledBuff1Un);
    }

    { // bind group layout tex storage 1 WO
        const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
            { .binding = 0, .visibility = visibility_frag, .storageTexture = storageTextureWO }
        };
        const WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc { .entryCount = 1, .entries = bindGroupLayoutEntries };
        layoutTexStrorage1WO = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
        assert(layoutTexStrorage1WO);
    }

    { // bind group layout tex storage 1 RO
        const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
            { .binding = 0, .visibility = visibility_frag, .storageTexture = storageTextureRO }
        };
        const WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc { .entryCount = 1, .entries = bindGroupLayoutEntries };
        layoutTexStrorage1RO = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
        assert(layoutTexStrorage1RO);
    }

    { // bind group layout tex storage 2 RO
        const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
            { .binding = 0, .visibility = visibility_frag, .storageTexture = storageTextureRO },
            { .binding = 1, .visibility = visibility_frag, .storageTexture = storageTextureRO }
        };
        const WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc { .entryCount = 2, .entries = bindGroupLayoutEntries };
        layoutTexStrorage2RO = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
        assert(layoutTexStrorage2RO);
    }

    { // bind group layout tex storage 3 RO
        const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
            { .binding = 0, .visibility = visibility_frag, .storageTexture = storageTextureRO },
            { .binding = 1, .visibility = visibility_frag, .storageTexture = storageTextureRO },
            { .binding = 2, .visibility = visibility_frag, .storageTexture = storageTextureRO }
        };
        const WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc { .entryCount = 3, .entries = bindGroupLayoutEntries };
        layoutTexStrorage3RO = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
        assert(layoutTexStrorage3RO);
    }

    { // bind group layout buffer 1 uniform
        const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
            { .binding = 0, .visibility = visibility_vert, .buffer = bufferUniform }
        };
        const WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc { .entryCount = 1, .entries = bindGroupLayoutEntries };
        layoutBuffer1Un = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
        assert(layoutBuffer1Un);
    }

    { // bind group layout buffer 2 uniform
        const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
            { .binding = 0, .visibility = visibility_vert, .buffer = bufferUniform },
            { .binding = 1, .visibility = visibility_vert, .buffer = bufferUniform }
        };
        const WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc { .entryCount = 2, .entries = bindGroupLayoutEntries };
        layoutBuffer2Un = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
        assert(layoutBuffer2Un);
    }

    { // bind group layout buffer 3 uniform
        const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
            { .binding = 0, .visibility = visibility_vert, .buffer = bufferUniform },
            { .binding = 1, .visibility = visibility_vert, .buffer = bufferUniform },
            { .binding = 2, .visibility = visibility_vert, .buffer = bufferUniform }
        };
        const WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc { .entryCount = 3, .entries = bindGroupLayoutEntries };
        layoutBuffer3Un = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
        assert(layoutBuffer3Un);
    }
}


void WgBindGroupLayouts::release(WgContext& context)
{
    wgpuBindGroupLayoutRelease(layoutBuffer3Un);
    wgpuBindGroupLayoutRelease(layoutBuffer2Un);
    wgpuBindGroupLayoutRelease(layoutBuffer1Un);
    wgpuBindGroupLayoutRelease(layoutTexStrorage3RO);
    wgpuBindGroupLayoutRelease(layoutTexStrorage2RO);
    wgpuBindGroupLayoutRelease(layoutTexStrorage1RO);
    wgpuBindGroupLayoutRelease(layoutTexStrorage1WO);
    wgpuBindGroupLayoutRelease(layoutTexSampledBuff1Un);
    wgpuBindGroupLayoutRelease(layoutTexSampled);
    device = nullptr;
}
