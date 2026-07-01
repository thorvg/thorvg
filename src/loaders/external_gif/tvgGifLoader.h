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

#include <gif_lib.h>
#include "tvgLoader.h"

struct GifLoader : AnimLoader
{
    GifLoader();
    ~GifLoader();

    bool open(const char* path, const LoaderOps* ops) override;
    bool open(const char* data, uint32_t size, const LoaderOps* ops, bool copy) override;
    bool read() override;

    // Frame controls
    bool frame(float no) override;
    float totalFrame() override;
    float curFrame() override;
    float duration() override;
    Result segment(float begin, float end) override;

private:
    void clear();
    void compositeFrame(uint32_t frameIndex, bool draw = true);
    void disposeBackground(const GifImageDesc* prevDesc);
    void blitRow(const uint8_t* raster, size_t frameIdx, size_t canvasIdx, int count, const ColorMapObject* colorMap, int transparent);
    void blitFrame(const SavedImage* frame, const GifImageDesc* imageDesc, const GraphicsControlBlock& gcb);
    bool needsReset(uint32_t frameIndex);
    void compositeFromStart(uint32_t frameIndex);
    void calculateFrameRate();

    GifFileType* gifFile = nullptr;
    uint8_t* data = nullptr;
    uint32_t size = 0;
    bool freeData = false;

    uint8_t* canvas = nullptr;  // Composited frame buffer
    uint32_t currentFrameIndex = 0;
    uint32_t lastCompositedFrame = 0xFFFFFFFF;
    float frameRate = 10.0f;
    bool abgr = false;  // pack pixels in RGBA(ABGR8888) byte order instead of BGRA(ARGB8888)
};

#endif  //_TVG_GIF_LOADER_H_
