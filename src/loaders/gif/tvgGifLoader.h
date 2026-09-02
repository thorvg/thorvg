/*
 * Copyright (c) 2026 the ThorVG project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _TVG_GIF_LOADER_H_
#define _TVG_GIF_LOADER_H_

#include "tvgLoader.h"
#include "tvgTaskScheduler.h"
#include "tvgGifDecoder.h"

struct GifLoader : AnimLoader, Task
{
    GifLoader();
    ~GifLoader();

    bool open(const char* path, const LoaderOps* ops) override;
    bool open(const char* data, uint32_t size, const LoaderOps* ops, bool copy) override;
    bool read() override;

    RenderSurface* bitmap() override;

    bool frame(float no) override;
    float totalFrame() override;
    float curFrame() override;
    float duration() override;
    Result segment(float begin, float end) override;

private:
    static constexpr uint32_t NO_FRAME = 0xFFFFFFFF;

    RenderSurface surface;
    unsigned char* data = nullptr;
    bool freeData = false;

    GifDecoder decoder;
    uint32_t currentFrameIndex = 0;
    uint32_t lastCompositedFrame = NO_FRAME;
    float durationCache = -1.0f;

    void run(unsigned tid) override;
};

#endif  //_TVG_GIF_LOADER_H_
