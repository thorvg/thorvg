/*
 * Copyright (c) 2025 the ThorVG project. All rights reserved.
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

#include <cstdint>
#include <cstring>

struct GifFrame
{
    uint8_t* pixels = nullptr;
    uint32_t delay = 0;  // delay in centiseconds
    uint32_t disposal = 0;  // disposal method
    bool transparent = false;
    uint8_t transparentIndex = 0;
    uint16_t left = 0;
    uint16_t top = 0;
    uint16_t width = 0;
    uint16_t height = 0;
};

struct GifDecoder
{
    const uint8_t* data = nullptr;
    uint32_t size = 0;
    uint32_t pos = 0;
    
    uint16_t width = 0;
    uint16_t height = 0;
    uint32_t frameCount = 0;
    float frameRate = 0.0f;
    
    GifFrame* frames = nullptr;
    uint8_t* globalPalette = nullptr;
    uint32_t globalPaletteSize = 0;
    bool globalColorTable = false;
    uint8_t bgIndex = 0;
    
    uint8_t* canvas = nullptr;  // composited canvas
    
    GifDecoder() {}
    ~GifDecoder()
    {
        clear();
    }
    
    bool load(const uint8_t* data, uint32_t size);
    bool decodeFrame(uint32_t frameIndex);
    void compositeFrame(uint32_t frameIndex, bool draw = true);
    void clear();
    
private:
    uint8_t readByte();
    uint16_t readWord();
    bool readHeader();
    bool readLogicalScreenDescriptor();
    bool readColorTable(uint8_t*& palette, uint32_t& size, uint8_t packed);
    uint32_t lzwDecode(const uint8_t* data, uint32_t dataSize, uint8_t* output, uint32_t outputSize, uint8_t minCodeSize);
};

#endif //_TVG_GIF_DECODER_H_
