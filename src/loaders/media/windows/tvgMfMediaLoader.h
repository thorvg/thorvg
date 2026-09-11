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

#ifndef _TVG_MF_LOADER_H_
#define _TVG_MF_LOADER_H_

#include <windows.h>
#include <d3d11.h>
#include <mfapi.h>
#include <mfmediaengine.h>

#include "tvgMediaLoader.h"

struct MfNotify;

struct MfMediaLoader : MediaLoader
{
    struct
    {
        IMFMediaEngine* engine = nullptr;
        IMFByteStream* stream = nullptr;
        MfNotify* notify = nullptr;
        CO_MTA_USAGE_COOKIE mta = nullptr;
        bool started = false;
    } mf;

    struct
    {
        IMFDXGIDeviceManager* manager = nullptr;
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        ID3D11Texture2D* texture = nullptr;
        ID3D11Texture2D* staging = nullptr;
    } d3d;

    struct
    {
        DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
        ColorSpace cs = ColorSpace::ARGB8888;
        bool alphaIgnored = true;
    } output;

    ~MfMediaLoader() override;

    bool open(const char* path, const LoaderOps* ops) override;
    bool open(const char* data, uint32_t size, const LoaderOps* ops, bool copy) override;
    bool read() override;
    bool sync() override;

    Result play() override;
    Result pause() override;
    Result stop() override;
    Result seek(float seconds) override;
    Result loop(bool on) override;
    Result volume(float volume) override;
    Result mute(bool on) override;
};

#endif  //_TVG_MF_LOADER_H_
