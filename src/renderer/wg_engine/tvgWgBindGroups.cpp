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

// canvas information group
WGPUBindGroupLayout WgBindGroupCanvas::layout = nullptr;
// paint object information group
WGPUBindGroupLayout WgBindGroupPaint::layout = nullptr;
// fill properties information groups
WGPUBindGroupLayout WgBindGroupSolidColor::layout = nullptr;
WGPUBindGroupLayout WgBindGroupLinearGradient::layout = nullptr;
WGPUBindGroupLayout WgBindGroupRadialGradient::layout = nullptr;
WGPUBindGroupLayout WgBindGroupPicture::layout = nullptr;
// composition and blending properties gropus
WGPUBindGroupLayout WgBindGroupTexture::layout = nullptr;
WGPUBindGroupLayout WgBindGroupTextureStorage::layout = nullptr;
WGPUBindGroupLayout WgBindGroupTextureSampled::layout = nullptr;
WGPUBindGroupLayout WgBindGroupOpacity::layout = nullptr;
WGPUBindGroupLayout WgBindGroupBlendMethod::layout = nullptr;
WGPUBindGroupLayout WgBindGroupCompositeMethod::layout = nullptr;


WGPUBindGroupLayout WgBindGroupCanvas::getLayout(WGPUDevice device)
{
    if (layout) return layout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryBuffer(0)
    };
    layout = createBindGroupLayout(device, bindGroupLayoutEntries, 1);
    assert(layout);
    return layout;
}


void WgBindGroupCanvas::releaseLayout()
{
    releaseBindGroupLayout(layout);
}


void WgBindGroupCanvas::initialize(WGPUDevice device, WGPUQueue queue, WgShaderTypeMat4x4f& uViewMat)
{
    release();
    uBufferViewMat = createBuffer(device, queue, &uViewMat, sizeof(uViewMat));
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryBuffer(0, uBufferViewMat)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 1);
    assert(mBindGroup);
}


void WgBindGroupCanvas::release()
{
    releaseBindGroup(mBindGroup);
    releaseBuffer(uBufferViewMat);
}


WGPUBindGroupLayout WgBindGroupPaint::getLayout(WGPUDevice device)
{
    if (layout) return layout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryBuffer(0),
        makeBindGroupLayoutEntryBuffer(1)
    };
    layout = createBindGroupLayout(device, bindGroupLayoutEntries, 2);
    assert(layout);
    return layout;
}


void WgBindGroupPaint::releaseLayout()
{
    releaseBindGroupLayout(layout);
}


void WgBindGroupPaint::initialize(WGPUDevice device, WGPUQueue queue, WgShaderTypeMat4x4f& uModelMat, WgShaderTypeBlendSettings& uBlendSettings)
{
    release();
    uBufferModelMat = createBuffer(device, queue, &uModelMat, sizeof(uModelMat));
    uBufferBlendSettings = createBuffer(device, queue, &uBlendSettings, sizeof(uBlendSettings));
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryBuffer(0, uBufferModelMat),
        makeBindGroupEntryBuffer(1, uBufferBlendSettings)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 2);
    assert(mBindGroup);
}


void WgBindGroupPaint::release()
{
    releaseBindGroup(mBindGroup);
    releaseBuffer(uBufferBlendSettings);
    releaseBuffer(uBufferModelMat);
}


WGPUBindGroupLayout WgBindGroupSolidColor::getLayout(WGPUDevice device)
{
    if (layout) return layout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryBuffer(0)
    };
    layout = createBindGroupLayout(device, bindGroupLayoutEntries, 1);
    assert(layout);
    return layout;
}


void WgBindGroupSolidColor::releaseLayout()
{
    releaseBindGroupLayout(layout);
}


void WgBindGroupSolidColor::initialize(WGPUDevice device, WGPUQueue queue, WgShaderTypeSolidColor &uSolidColor)
{
    release();
    uBufferSolidColor = createBuffer(device, queue, &uSolidColor, sizeof(uSolidColor));
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryBuffer(0, uBufferSolidColor)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 1);
    assert(mBindGroup);
}


void WgBindGroupSolidColor::release()
{
    releaseBindGroup(mBindGroup);
    releaseBuffer(uBufferSolidColor);
}


WGPUBindGroupLayout WgBindGroupLinearGradient::getLayout(WGPUDevice device)
{
    if (layout) return layout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryBuffer(0)
    };
    layout = createBindGroupLayout(device, bindGroupLayoutEntries, 1);
    assert(layout);
    return layout;
}


void WgBindGroupLinearGradient::releaseLayout()
{
    releaseBindGroupLayout(layout);
}


void WgBindGroupLinearGradient::initialize(WGPUDevice device, WGPUQueue queue, WgShaderTypeLinearGradient &uLinearGradient)
{
    release();
    uBufferLinearGradient = createBuffer(device, queue, &uLinearGradient, sizeof(uLinearGradient));
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryBuffer(0, uBufferLinearGradient)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 1);
    assert(mBindGroup);
}


void WgBindGroupLinearGradient::release()
{
    releaseBindGroup(mBindGroup);
    releaseBuffer(uBufferLinearGradient);
}


WGPUBindGroupLayout WgBindGroupRadialGradient::getLayout(WGPUDevice device)
{
    if (layout) return layout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryBuffer(0)
    };
    layout = createBindGroupLayout(device, bindGroupLayoutEntries, 1);
    assert(layout);
    return layout;
}


void WgBindGroupRadialGradient::releaseLayout()
{
    releaseBindGroupLayout(layout);
}


void WgBindGroupRadialGradient::initialize(WGPUDevice device, WGPUQueue queue, WgShaderTypeRadialGradient &uRadialGradient)
{
    release();
    uBufferRadialGradient = createBuffer(device, queue, &uRadialGradient, sizeof(uRadialGradient));
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryBuffer(0, uBufferRadialGradient)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 1);
    assert(mBindGroup);
}


void WgBindGroupRadialGradient::release()
{
    releaseBuffer(uBufferRadialGradient);
    releaseBindGroup(mBindGroup);
}


WGPUBindGroupLayout WgBindGroupPicture::getLayout(WGPUDevice device)
{
    if (layout) return layout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntrySampler(0),
        makeBindGroupLayoutEntryTexture(1)
    };
    layout = createBindGroupLayout(device, bindGroupLayoutEntries, 2);
    assert(layout);
    return layout;
}


void WgBindGroupPicture::releaseLayout()
{
    releaseBindGroupLayout(layout);
}


void WgBindGroupPicture::initialize(WGPUDevice device, WGPUQueue queue, WGPUSampler uSampler, WGPUTextureView uTextureView)
{
    release();
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntrySampler(0, uSampler),
        makeBindGroupEntryTextureView(1, uTextureView)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 2);
    assert(mBindGroup);
}


void WgBindGroupPicture::release()
{
    releaseBindGroup(mBindGroup);
}


WGPUBindGroupLayout WgBindGroupTexture::getLayout(WGPUDevice device)
{
    if (layout) return layout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryTexture(0)
    };
    layout = createBindGroupLayout(device, bindGroupLayoutEntries, 1);
    assert(layout);
    return layout;
}


void WgBindGroupTexture::releaseLayout()
{
    releaseBindGroupLayout(layout);
}


void WgBindGroupTexture::initialize(WGPUDevice device, WGPUQueue queue, WGPUTextureView uTexture)
{
    release();
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryTextureView(0, uTexture)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 1);
    assert(mBindGroup);
}


void WgBindGroupTexture::release()
{
    releaseBindGroup(mBindGroup);
}


WGPUBindGroupLayout WgBindGroupTextureStorage::getLayout(WGPUDevice device)
{
    if (layout) return layout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryStorageTexture(0, WGPUStorageTextureAccess_ReadWrite)
    };
    layout = createBindGroupLayout(device, bindGroupLayoutEntries, 1);
    assert(layout);
    return layout;
}


void WgBindGroupTextureStorage::releaseLayout()
{
    releaseBindGroupLayout(layout);
}


void WgBindGroupTextureStorage::initialize(WGPUDevice device, WGPUQueue queue, WGPUTextureView uTexture)
{
    release();
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryTextureView(0, uTexture)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 1);
    assert(mBindGroup);
}


void WgBindGroupTextureStorage::release()
{
    releaseBindGroup(mBindGroup);
}


WGPUBindGroupLayout WgBindGroupTextureSampled::getLayout(WGPUDevice device)
{
    if (layout) return layout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntrySampler(0),
        makeBindGroupLayoutEntryTexture(1)
    };
    layout = createBindGroupLayout(device, bindGroupLayoutEntries, 2);
    assert(layout);
    return layout;
}


void WgBindGroupTextureSampled::releaseLayout()
{
    releaseBindGroupLayout(layout);
}


void WgBindGroupTextureSampled::initialize(WGPUDevice device, WGPUQueue queue, WGPUSampler uSampler, WGPUTextureView uTexture)
{
    release();
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntrySampler(0, uSampler),
        makeBindGroupEntryTextureView(1, uTexture)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 2);
    assert(mBindGroup);
}


void WgBindGroupTextureSampled::release()
{
    releaseBindGroup(mBindGroup);
}


WGPUBindGroupLayout WgBindGroupOpacity::getLayout(WGPUDevice device)
{
    if (layout) return layout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryBuffer(0)
    };
    layout = createBindGroupLayout(device, bindGroupLayoutEntries, 1);
    assert(layout);
    return layout;
}


void WgBindGroupOpacity::releaseLayout()
{
    releaseBindGroupLayout(layout);
}


void WgBindGroupOpacity::initialize(WGPUDevice device, WGPUQueue queue, uint32_t uOpacity)
{
    release();
    float opacity = uOpacity / 255.0f;
    uBufferOpacity = createBuffer(device, queue, &opacity, sizeof(float));
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryBuffer(0, uBufferOpacity)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 1);
    assert(mBindGroup);
}


void WgBindGroupOpacity::release()
{
    releaseBuffer(uBufferOpacity);
    releaseBindGroup(mBindGroup);
}


WGPUBindGroupLayout WgBindGroupBlendMethod::getLayout(WGPUDevice device)
{
    if (layout) return layout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryBuffer(0)
    };
    layout = createBindGroupLayout(device, bindGroupLayoutEntries, 1);
    assert(layout);
    return layout;
}


void WgBindGroupBlendMethod::releaseLayout()
{
    releaseBindGroupLayout(layout);
}


void WgBindGroupBlendMethod::initialize(WGPUDevice device, WGPUQueue queue, BlendMethod uBlendMethod)
{
    release();
    uint32_t blendMethod = (uint32_t)uBlendMethod;
    uBufferBlendMethod = createBuffer(device, queue, &blendMethod, sizeof(blendMethod));
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryBuffer(0, uBufferBlendMethod)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 1);
    assert(mBindGroup);
}


void WgBindGroupBlendMethod::release()
{
    releaseBuffer(uBufferBlendMethod);
    releaseBindGroup(mBindGroup);
}


WGPUBindGroupLayout WgBindGroupCompositeMethod::getLayout(WGPUDevice device)
{
    if (layout) return layout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryBuffer(0)
    };
    layout = createBindGroupLayout(device, bindGroupLayoutEntries, 1);
    assert(layout);
    return layout;
}


void WgBindGroupCompositeMethod::releaseLayout()
{
    releaseBindGroupLayout(layout);
}


void WgBindGroupCompositeMethod::initialize(WGPUDevice device, WGPUQueue queue, CompositeMethod uCompositeMethod)
{
    release();
    uint32_t compositeMethod = (uint32_t)uCompositeMethod;
    uBufferCompositeMethod = createBuffer(device, queue, &compositeMethod, sizeof(compositeMethod));
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryBuffer(0, uBufferCompositeMethod)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 1);
    assert(mBindGroup);
}


void WgBindGroupCompositeMethod::release()
{
    releaseBuffer(uBufferCompositeMethod);
    releaseBindGroup(mBindGroup);
}

//************************************************************************
// bind group pools
//************************************************************************

void WgBindGroupOpacityPool::initialize(WgContext& context)
{
    release(context);
    for (uint32_t opacity = 0; opacity < 256; opacity++) {
        mPool[opacity] = new WgBindGroupOpacity;
        mPool[opacity]->initialize(context.device, context.queue, opacity);
    }
}


void WgBindGroupOpacityPool::release(WgContext& context)
{
    for (uint32_t i = 0; i < 256; i++) {
        if (mPool[i]) {
            mPool[i]->release();
            delete mPool[i];
            mPool[i] = nullptr;
        }
    }
}


WgBindGroupOpacity* WgBindGroupOpacityPool::allocate(WgContext& context, uint8_t opacity)
{
    return mPool[opacity];
}


void WgBindGroupBlendMethodPool::initialize(WgContext& context)
{
    release(context);
    for (uint8_t blendMethod = (uint8_t)BlendMethod::Normal;
         blendMethod <= (uint8_t)BlendMethod::SoftLight;
         blendMethod++) {
        mPool[blendMethod] = new WgBindGroupBlendMethod;
        mPool[blendMethod]->initialize(context.device, context.queue, (BlendMethod)blendMethod);
    }
}


void WgBindGroupBlendMethodPool::release(WgContext& context)
{
    for (uint8_t blendMethod = (uint8_t)BlendMethod::Normal;
         blendMethod <= (uint8_t)BlendMethod::SoftLight;
         blendMethod++) {
        if (mPool[blendMethod]) {
            mPool[blendMethod]->release();
            delete mPool[blendMethod];
            mPool[blendMethod] = nullptr;
        }
    }
}


WgBindGroupBlendMethod* WgBindGroupBlendMethodPool::allocate(WgContext& context, BlendMethod blendMethod)
{
    return mPool[(uint8_t)blendMethod];
}


void WgBindGroupCompositeMethodPool::initialize(WgContext& context)
{
    release(context);
    for (uint8_t composeMethos = (uint8_t)CompositeMethod::None;
         composeMethos <= (uint8_t)CompositeMethod::DifferenceMask;
         composeMethos++) {
        mPool[composeMethos] = new WgBindGroupCompositeMethod;
        mPool[composeMethos]->initialize(context.device, context.queue, (CompositeMethod)composeMethos);
    }
}


void WgBindGroupCompositeMethodPool::release(WgContext& context)
{
    for (uint8_t blendMethod = (uint8_t)CompositeMethod::None;
         blendMethod <= (uint8_t)CompositeMethod::DifferenceMask;
         blendMethod++) {
        if (mPool[blendMethod]) {
            mPool[blendMethod]->release();
            delete mPool[blendMethod];
            mPool[blendMethod] = nullptr;
        }
    }
}


WgBindGroupCompositeMethod* WgBindGroupCompositeMethodPool::allocate(WgContext& context, CompositeMethod composeMethod)
{
    return mPool[(uint8_t)composeMethod];
}
