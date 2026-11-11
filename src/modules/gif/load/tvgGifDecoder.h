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

#ifndef _TVG_GIF_DECODER_H_
#define _TVG_GIF_DECODER_H_

#include "tvgCommon.h"
#include "tvgArray.h"

struct GifFrame
{
    uint32_t* pixels;
    uint16_t delay;
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t height;
    uint8_t disposal;
    uint8_t transparentIndex;
    bool transparent;
};

struct GifDecoder
{
    const uint8_t* data;
    uint32_t size;
    uint32_t pos = 0;

    uint32_t width, height;

    Array<GifFrame> frames;
    uint8_t* gPalette = nullptr;  // global palette
    uint32_t gPaletteSize = 0;

    uint32_t* output = nullptr;
    ColorSpace cs;

    ~GifDecoder();

    bool load(const char* data, uint32_t size, ColorSpace cs);
    void compositeFrame(uint32_t frameIndex, bool draw = true);
    float duration(float begin, float end) const;

private:
    uint8_t readByte()
    {
        return (pos < size) ? data[pos++] : 0;
    }

    uint16_t readWord()
    {
        uint16_t low = readByte();
        uint16_t high = readByte();
        return low | (high << 8);
    }

    void readLogicalScreenDescriptor();
    void readColorTable(uint8_t*& palette, uint32_t& size, uint8_t packed);
    uint32_t lzwDecode(const uint8_t* data, uint32_t size, uint8_t* output, uint32_t outputSize, uint8_t minCodeSize);
    void blitFrame(const GifFrame& frame);
    bool readGCE(GifFrame& pendingGCE);
    void skipExtension();
    uint8_t* readImageData(uint32_t& size);
    uint8_t* deinterlace(uint8_t* pixels, uint16_t width, uint16_t height);
    void indexToPixels(GifFrame& frame, const uint8_t* pixels, const uint8_t* palette, uint32_t paletteSize, size_t pixelCnt);
    bool readImageFrame(GifFrame& frame);
    bool parseBlocks();
};

#endif  //_TVG_GIF_DECODER_H_
