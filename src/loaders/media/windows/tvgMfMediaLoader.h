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

#include <thread>
#include <d3d11.h>
#include <mfmediaengine.h>

#include "tvgLock.h"
#include "tvgMediaLoader.h"

struct MfNotify;

struct MfMediaLoader : MediaLoader
{
    ~MfMediaLoader();

    Result open(const char* data, uint32_t size, const LoaderOps& ops) override;
    Result open(const char* path, const LoaderOps& ops) override;
    bool read() override;
    bool sync() override;

    Result play() override;
    Result pause() override;
    Result stop() override;
    Result seek(float seconds) override;
    Result loop(bool on) override;
    Result volume(float volume) override;
    Result mute(bool on) override;

private:
    Result open(const wchar_t* url, IMFByteStream* stream);
    bool readback(uint32_t* frame);
    void post(double position, bool run);  // caller holds key
    void run();

    static constexpr uint32_t BUFFER_COUNT = 3;
    static constexpr double NO_SEEK = -1.0;

    // Keep only the latest control request.
    struct Request
    {
        double position = NO_SEEK;
        bool running = false;
        bool pending = false;
    } request;

    // sync() exchanges the latest frame with the canvas buffer.
    struct Ring
    {
        uint32_t* frames[BUFFER_COUNT] = {};
        uint32_t write = 0;
        uint32_t latest = 0;
        float time = -1.0f;  // negative when position is unavailable
        bool updated = false;
    } ring;

    IMFMediaEngineEx* engine = nullptr;
    MfNotify* notify = nullptr;
    ID3D11Texture2D* texture = nullptr;  // GPU conversion target
    ID3D11Texture2D* staging = nullptr;  // CPU readback

    std::thread worker;
    HANDLE wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);  // controls or shutdown
    std::atomic<bool> quit{false};
    double shown = -1.0;   // last engine PTS
    uint32_t idle = 0;     // empty polls while idle
    bool playing = false;  // worker's playback state
    bool landing = false;  // awaiting a paused seek frame

    ColorSpace cs = ColorSpace::Unknown;
    StrictKey key;         // guards request, ring and ended
    bool started = false;  // played since the last stop
    bool ended = false;    // playback or seek reached the end
};

#endif  //_TVG_MF_LOADER_H_
