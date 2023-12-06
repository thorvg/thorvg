/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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

// WgBindGroupCanvas
WGPUBindGroupLayout WgBindGroupCanvas::mLayout = nullptr;
WGPUBindGroupLayout WgBindGroupCanvas::getLayout(WGPUDevice device) {
    if (mLayout) return mLayout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryBuffer(0)
    };
    mLayout = createBindGroupLayout(device, bindGroupLayoutEntries, 1);
    assert(mLayout);
    return mLayout;
}

void WgBindGroupCanvas::releaseLayout() {
    releaseBindGroupLayout(mLayout);
};

void WgBindGroupCanvas::initialize(WGPUDevice device, WGPUQueue queue, WgShaderTypeMat4x4f& uViewMat) {
    release();
    uBufferViewMat = createBuffer(device, queue, &uViewMat, sizeof(uViewMat));
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryBuffer(0, uBufferViewMat)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 1);
    assert(mBindGroup);
}

void WgBindGroupCanvas::release() {
    releaseBindGroup(mBindGroup);
    releaseBuffer(uBufferViewMat);
}

// WgBindGroupPaint
WGPUBindGroupLayout WgBindGroupPaint::mLayout = nullptr;
WGPUBindGroupLayout WgBindGroupPaint::getLayout(WGPUDevice device) {
    if (mLayout) return mLayout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryBuffer(0),
        makeBindGroupLayoutEntryBuffer(1)
    };
    mLayout = createBindGroupLayout(device, bindGroupLayoutEntries, 2);
    assert(mLayout);
    return mLayout;
}

void WgBindGroupPaint::releaseLayout() {
    releaseBindGroupLayout(mLayout);
};

void WgBindGroupPaint::initialize(WGPUDevice device, WGPUQueue queue, WgShaderTypeMat4x4f& uModelMat, WgShaderTypeBlendSettigs& uBlendSettigs) {
    release();
    uBufferModelMat = createBuffer(device, queue, &uModelMat, sizeof(uModelMat));
    uBufferBlendSettigs = createBuffer(device, queue, &uBlendSettigs, sizeof(uBlendSettigs));
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryBuffer(0, uBufferModelMat),
        makeBindGroupEntryBuffer(1, uBufferBlendSettigs)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 2);
    assert(mBindGroup);
}

void WgBindGroupPaint::release() {
    releaseBindGroup(mBindGroup);
    releaseBuffer(uBufferBlendSettigs);
    releaseBuffer(uBufferModelMat);
}

// WgBindGroupSolidColor
WGPUBindGroupLayout WgBindGroupSolidColor::mLayout = nullptr;
WGPUBindGroupLayout WgBindGroupSolidColor::getLayout(WGPUDevice device) {
    if (mLayout) return mLayout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryBuffer(0)
    };
    mLayout = createBindGroupLayout(device, bindGroupLayoutEntries, 1);
    assert(mLayout);
    return mLayout;
}

void WgBindGroupSolidColor::releaseLayout() {
    releaseBindGroupLayout(mLayout);
};

void WgBindGroupSolidColor::initialize(WGPUDevice device, WGPUQueue queue, WgShaderTypeSolidColor &uSolidColor) {
    release();
    uBufferSolidColor = createBuffer(device, queue, &uSolidColor, sizeof(uSolidColor));
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryBuffer(0, uBufferSolidColor)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 1);
    assert(mBindGroup);
}

void WgBindGroupSolidColor::release() {
    releaseBindGroup(mBindGroup);
    releaseBuffer(uBufferSolidColor);
}

// WgBindGroupLinearGradient
WGPUBindGroupLayout WgBindGroupLinearGradient::mLayout = nullptr;
WGPUBindGroupLayout WgBindGroupLinearGradient::getLayout(WGPUDevice device) {
    if (mLayout) return mLayout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryBuffer(0)
    };
    mLayout = createBindGroupLayout(device, bindGroupLayoutEntries, 1);
    assert(mLayout);
    return mLayout;
}

void WgBindGroupLinearGradient::releaseLayout() {
    releaseBindGroupLayout(mLayout);
};

void WgBindGroupLinearGradient::initialize(WGPUDevice device, WGPUQueue queue, WgShaderTypeLinearGradient &uLinearGradient) {
    release();
    uBufferLinearGradient = createBuffer(device, queue, &uLinearGradient, sizeof(uLinearGradient));
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryBuffer(0, uBufferLinearGradient)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 1);
    assert(mBindGroup);
}

void WgBindGroupLinearGradient::release() {
    releaseBindGroup(mBindGroup);
    releaseBuffer(uBufferLinearGradient);
}

// WgBindGroupRadialGradient
WGPUBindGroupLayout WgBindGroupRadialGradient::mLayout = nullptr;
WGPUBindGroupLayout WgBindGroupRadialGradient::getLayout(WGPUDevice device) {
    if (mLayout) return mLayout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntryBuffer(0)
    };
    mLayout = createBindGroupLayout(device, bindGroupLayoutEntries, 1);
    assert(mLayout);
    return mLayout;
}

void WgBindGroupRadialGradient::releaseLayout() {
    releaseBindGroupLayout(mLayout);
};

void WgBindGroupRadialGradient::initialize(WGPUDevice device, WGPUQueue queue, WgShaderTypeRadialGradient &uRadialGradient) {
    release();
    uBufferRadialGradient = createBuffer(device, queue, &uRadialGradient, sizeof(uRadialGradient));
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntryBuffer(0, uBufferRadialGradient)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 1);
    assert(mBindGroup);
}

void WgBindGroupRadialGradient::release() {
    releaseBuffer(uBufferRadialGradient);
    releaseBindGroup(mBindGroup);
}

// WgBindGroupPicture
WGPUBindGroupLayout WgBindGroupPicture::mLayout = nullptr;
WGPUBindGroupLayout WgBindGroupPicture::getLayout(WGPUDevice device) {
    if (mLayout) return mLayout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntrySampler(0),
        makeBindGroupLayoutEntryTextureView(1)
    };
    mLayout = createBindGroupLayout(device, bindGroupLayoutEntries, 2);
    assert(mLayout);
    return mLayout;
}

void WgBindGroupPicture::releaseLayout() {
    releaseBindGroupLayout(mLayout);
};

void WgBindGroupPicture::initialize(WGPUDevice device, WGPUQueue queue, WGPUSampler uSampler, WGPUTextureView uTextureView) {
    release();
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntrySampler(0, uSampler),
        makeBindGroupEntryTextureView(1, uTextureView)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 2);
    assert(mBindGroup);
}

void WgBindGroupPicture::release() {
    releaseBindGroup(mBindGroup);
}

// WgBindGroupCompose
WGPUBindGroupLayout WgBindGroupCompose::mLayout = nullptr;
WGPUBindGroupLayout WgBindGroupCompose::getLayout(WGPUDevice device) {
    if (mLayout) return mLayout;
    const WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        makeBindGroupLayoutEntrySampler(0),
        makeBindGroupLayoutEntryTextureView(1),
        makeBindGroupLayoutEntryTextureView(2)
    };
    mLayout = createBindGroupLayout(device, bindGroupLayoutEntries, 3);
    assert(mLayout);
    return mLayout;
}

void WgBindGroupCompose::releaseLayout() {
    releaseBindGroupLayout(mLayout);
};

void WgBindGroupCompose::initialize(WGPUDevice device, WGPUQueue queue, WGPUSampler uSampler, WGPUTextureView uTextureSrc, WGPUTextureView uTextureDst) {
    release();
    const WGPUBindGroupEntry bindGroupEntries[] {
        makeBindGroupEntrySampler(0, uSampler),
        makeBindGroupEntryTextureView(1, uTextureSrc),
        makeBindGroupEntryTextureView(2, uTextureDst)
    };
    mBindGroup = createBindGroup(device, getLayout(device), bindGroupEntries, 3);
    assert(mBindGroup);
}

void WgBindGroupCompose::release() {
    releaseBindGroup(mBindGroup);
}
