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

#include "tvgGifDecoder.h"
#include "tvgAllocator.h"
#include "tvgArray.h"

#define GIF_EXTENSION_INTRODUCER 0x21
#define GIF_IMAGE_SEPARATOR 0x2C
#define GIF_TRAILER 0x3B

#define GIF_EXTENSION_GCE 0xF9

#define GIF_DISPOSAL_NONE 0
#define GIF_DISPOSAL_LEAVE 1
#define GIF_DISPOSAL_BACKGROUND 2
#define GIF_DISPOSAL_PREVIOUS 3

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

uint8_t GifDecoder::readByte()
{
    if (pos >= size) return 0;
    return data[pos++];
}

uint16_t GifDecoder::readWord()
{
    uint16_t low = readByte();
    uint16_t high = readByte();
    return low | (high << 8);
}

bool GifDecoder::readHeader()
{
    if (size < 6) return false;

    if (data[pos] != 'G' || data[pos + 1] != 'I' || data[pos + 2] != 'F') return false;
    pos += 3;

    if ((data[pos] != '8' || data[pos + 1] != '7' || data[pos + 2] != 'a') && (data[pos] != '8' || data[pos + 1] != '9' || data[pos + 2] != 'a')) {
        return false;
    }
    pos += 3;

    return true;
}

bool GifDecoder::readLogicalScreenDescriptor()
{
    width = readWord();
    height = readWord();

    uint8_t packed = readByte();
    globalColorTable = (packed & 0x80) != 0;
    globalPaletteSize = globalColorTable ? (1 << ((packed & 0x07) + 1)) : 0;

    readByte();  // background color index
    readByte();  // pixel aspect ratio

    if (globalColorTable && globalPaletteSize > 0) {
        globalPalette = tvg::malloc<uint8_t>(globalPaletteSize * 3);
        if (!globalPalette) return false;

        for (uint32_t i = 0; i < globalPaletteSize; i++) {
            globalPalette[i * 3 + 0] = readByte();  // R
            globalPalette[i * 3 + 1] = readByte();  // G
            globalPalette[i * 3 + 2] = readByte();  // B
        }
    }

    size_t canvasSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    canvas = tvg::malloc<uint8_t>(canvasSize);
    if (!canvas) return false;

    return true;
}

bool GifDecoder::readColorTable(uint8_t*& palette, uint32_t& size, uint8_t packed)
{
    bool hasColorTable = (packed & 0x80) != 0;
    if (!hasColorTable) {
        palette = nullptr;
        size = 0;
        return true;
    }

    size = 1 << ((packed & 0x07) + 1);
    palette = tvg::malloc<uint8_t>(size * 3);
    if (!palette) return false;

    for (uint32_t i = 0; i < size; i++) {
        palette[i * 3 + 0] = readByte();  // R
        palette[i * 3 + 1] = readByte();  // G
        palette[i * 3 + 2] = readByte();  // B
    }

    return true;
}

namespace
{

struct GifLzwDictEntry
{
    uint32_t prefix;  // previous code (0xFFFF marks a single-byte leaf)
    uint8_t suffix;   // last byte
};

struct GifLzwState
{
    const uint8_t* data;
    uint32_t dataSize;
    uint8_t* output;
    uint32_t outputSize;

    GifLzwDictEntry* dictionary;
    uint8_t minCodeSize;
    uint32_t clearCode;
    uint32_t endCode;
    uint32_t nextCode;
    uint32_t codeSize;
    uint32_t codeMask;

    uint32_t bitPos = 0;
    uint32_t bitBuffer = 0;
    uint32_t bitsInBuffer = 0;
    uint32_t outputPos = 0;

    uint32_t oldCode = 0xFFFF;
    bool first = true;

    void init(const uint8_t* d, uint32_t dSize, uint8_t* out, uint32_t outSize, GifLzwDictEntry* dict, uint8_t minCode)
    {
        data = d;
        dataSize = dSize;
        output = out;
        outputSize = outSize;
        dictionary = dict;
        minCodeSize = minCode;
        clearCode = 1 << minCode;
        endCode = clearCode + 1;
        reset();

        for (uint32_t i = 0; i < clearCode; i++) {
            dictionary[i].prefix = 0xFFFF;  // invalid (marks leaf)
            dictionary[i].suffix = static_cast<uint8_t>(i);
        }
    }

    void reset()
    {
        codeSize = minCodeSize + 1;
        codeMask = (1 << codeSize) - 1;
        nextCode = endCode + 1;
        oldCode = 0xFFFF;
        first = true;
    }

    uint32_t decode()
    {
        while (outputPos < outputSize) {
            uint32_t code = readCode();
            if (code == endCode) break;
            if (!processCode(code)) break;
        }
        return outputPos;
    }

    bool processCode(uint32_t code)
    {
        if (code == clearCode) {
            reset();
            return true;
        }
        if (first) return emitLiteral(code);
        return emitSequence(code);
    }

    bool emitLiteral(uint32_t code)
    {
        if (code >= clearCode) return false;
        output[outputPos++] = static_cast<uint8_t>(code);
        oldCode = code;
        first = false;
        return true;
    }

    bool emitSequence(uint32_t code)
    {
        uint32_t inCode = code;
        if (code >= nextCode) {
            if (!outputSequence(oldCode)) return false;
            if (outputPos == 0 || outputPos >= outputSize) return false;
            output[outputPos] = output[outputPos - 1];
            outputPos++;
        } else if (!outputSequence(code)) {
            return false;
        }

        extendDictionary(oldCode, code);
        oldCode = inCode;
        return true;
    }

    // GIF packs codes LSB-first across byte boundaries.
    uint32_t readCode()
    {
        while (bitsInBuffer < codeSize && (bitPos / 8) < dataSize) {
            bitBuffer |= (static_cast<uint32_t>(data[bitPos / 8])) << bitsInBuffer;
            bitsInBuffer += 8;
            bitPos += 8;
        }
        if (bitsInBuffer < codeSize) return endCode;

        uint32_t code = bitBuffer & codeMask;
        bitBuffer >>= codeSize;
        bitsInBuffer -= codeSize;
        return code;
    }

    bool outputSequence(uint32_t code)
    {
        uint8_t stack[4096];
        uint32_t stackPos = 0;

        uint32_t current = code;
        while (current != 0xFFFF && stackPos < 4096) {
            if (current >= nextCode) return false;
            stack[stackPos++] = dictionary[current].suffix;
            current = dictionary[current].prefix;
        }
        if (current != 0xFFFF && stackPos >= 4096) return false;

        while (stackPos > 0 && outputPos < outputSize) {
            output[outputPos++] = stack[--stackPos];
        }
        return true;
    }

    uint8_t firstByte(uint32_t code)
    {
        if (code < clearCode) return static_cast<uint8_t>(code);
        if (code >= nextCode) return 0;

        uint32_t walk = code;
        while (walk != 0xFFFF && walk >= clearCode) {
            walk = dictionary[walk].prefix;
        }
        return (walk < clearCode) ? static_cast<uint8_t>(walk) : 0;
    }

    void extendDictionary(uint32_t oldCode, uint32_t code)
    {
        if (nextCode >= 4096 || oldCode == 0xFFFF) return;

        dictionary[nextCode].prefix = oldCode;
        uint32_t tempCode = (code >= nextCode) ? oldCode : code;
        dictionary[nextCode].suffix = firstByte(tempCode);
        nextCode++;

        if (nextCode >= (1u << codeSize) && codeSize < 12) {
            codeSize++;
            codeMask = (1 << codeSize) - 1;
        }
    }
};

}  // namespace

uint32_t GifDecoder::lzwDecode(const uint8_t* data, uint32_t dataSize, uint8_t* output, uint32_t outputSize, uint8_t minCodeSize)
{
    if (minCodeSize < 2 || minCodeSize > 8) return 0;
    if (dataSize == 0 || outputSize == 0) return 0;

    auto dictionary = tvg::calloc<GifLzwDictEntry>(4096, sizeof(GifLzwDictEntry));
    if (!dictionary) return 0;

    GifLzwState lzw{};
    lzw.init(data, dataSize, output, outputSize, dictionary, minCodeSize);
    uint32_t outputPos = lzw.decode();

    tvg::free(dictionary);
    return outputPos;
}

void GifDecoder::disposeBackground(const GifFrame& prevFrame)
{
    if (prevFrame.top >= height || prevFrame.left >= width) return;

    auto canvas32 = reinterpret_cast<uint32_t*>(canvas);
    uint32_t endY = (prevFrame.height > height - prevFrame.top) ? height - prevFrame.top : prevFrame.height;
    uint32_t endX = (prevFrame.width > width - prevFrame.left) ? width - prevFrame.left : prevFrame.width;

    for (uint32_t y = 0; y < endY; y++) {
        uint32_t canvasY = prevFrame.top + y;
        if (canvasY >= height) break;

        size_t canvasIdx = static_cast<size_t>(canvasY) * width + prevFrame.left;
        if (prevFrame.left + endX <= width) {
            memset(&canvas32[canvasIdx], 0, endX * sizeof(uint32_t));
        } else {
            for (uint32_t x = 0; x < endX && (prevFrame.left + x) < width; x++) {
                canvas32[canvasIdx + x] = 0;
            }
        }
    }
}

void GifDecoder::blitFrame(const GifFrame& frame)
{
    if (frame.top >= height || frame.left >= width) return;

    auto canvas32 = reinterpret_cast<uint32_t*>(canvas);
    auto framePixels32 = reinterpret_cast<uint32_t*>(frame.pixels);

    uint32_t endY = (frame.height > height - frame.top) ? height - frame.top : frame.height;
    uint32_t endX = (frame.width > width - frame.left) ? width - frame.left : frame.width;

    bool canUseMemcpy = !frame.transparent && (frame.left + frame.width <= width);

    for (uint32_t y = 0; y < endY; y++) {
        uint32_t canvasY = frame.top + y;
        size_t frameIdx = static_cast<size_t>(y) * frame.width;
        size_t canvasIdx = static_cast<size_t>(canvasY) * width + frame.left;

        if (canUseMemcpy) {
            memcpy(&canvas32[canvasIdx], &framePixels32[frameIdx], frame.width * sizeof(uint32_t));
        } else {
            for (uint32_t x = 0; x < endX; x++) {
                if (frame.left + x < width) {
                    uint32_t pixel = framePixels32[frameIdx + x];
                    if ((pixel & 0xFF000000) != 0) {
                        canvas32[canvasIdx + x] = pixel;
                    }
                }
            }
        }
    }
}

void GifDecoder::compositeFrame(uint32_t frameIndex, bool draw)
{
    if (frameIndex >= frameCount || !frames[frameIndex].pixels) return;

    if (frameIndex > 0 && frames[frameIndex - 1].disposal == GIF_DISPOSAL_BACKGROUND) {
        disposeBackground(frames[frameIndex - 1]);
    }

    if (draw) blitFrame(frames[frameIndex]);
}

bool GifDecoder::readGCE(GifFrame& pendingGCE)
{
    if (readByte() != 4) return false;  // GCE block size is fixed at 4 bytes

    uint8_t packed = readByte();
    pendingGCE.disposal = (packed >> 2) & 0x07;
    pendingGCE.transparent = (packed & 0x01) != 0;
    pendingGCE.delay = readWord();
    pendingGCE.transparentIndex = readByte();

    readByte();  // block terminator
    return true;
}

void GifDecoder::skipExtension()
{
    uint8_t extSize = readByte();
    if (pos + extSize > size) return;
    pos += extSize;

    while (pos < size) {
        uint8_t blockSize = data[pos++];
        if (blockSize == 0) break;
        if (pos + blockSize > size) break;
        pos += blockSize;
    }
}

uint8_t* GifDecoder::readImageData(uint32_t& dataSize)
{
    dataSize = 0;
    uint32_t capacity = 256;
    uint8_t* imageData = tvg::malloc<uint8_t>(capacity);
    if (!imageData) return nullptr;

    while (true) {
        uint8_t blockSize = readByte();
        if (blockSize == 0) break;

        if (dataSize + blockSize > capacity) {
            capacity = (dataSize + blockSize) * 2;
            uint8_t* newData = tvg::realloc<uint8_t>(imageData, capacity);
            if (!newData) {
                tvg::free(imageData);
                return nullptr;
            }
            imageData = newData;
        }

        if (pos + blockSize > size) {
            tvg::free(imageData);
            return nullptr;
        }
        memcpy(imageData + dataSize, data + pos, blockSize);
        pos += blockSize;
        dataSize += blockSize;
    }

    return imageData;
}

// GIF stores interlaced frames in 4 passes (image descriptor bit 0x40).
uint8_t* GifDecoder::deinterlace(uint8_t* pixels, uint16_t width, uint16_t height)
{
    size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    uint8_t* dst = tvg::malloc<uint8_t>(pixelCount);
    if (!dst) return nullptr;

    static const uint32_t istart[4] = {0, 4, 2, 1};
    static const uint32_t istep[4] = {8, 8, 4, 2};
    uint32_t srcRow = 0;
    for (int p = 0; p < 4; ++p) {
        for (uint32_t y = istart[p]; y < height; y += istep[p]) {
            memcpy(dst + static_cast<size_t>(y) * width, pixels + static_cast<size_t>(srcRow) * width, width);
            ++srcRow;
        }
    }
    tvg::free(pixels);
    return dst;
}

void GifDecoder::indexToRGBA(GifFrame& frame, const uint8_t* pixels, const uint8_t* palette, uint32_t paletteSize, size_t pixelCount)
{
    auto pixels32 = reinterpret_cast<uint32_t*>(frame.pixels);
    uint8_t transIdx = frame.transparentIndex;
    bool hasTrans = frame.transparent;

    for (size_t i = 0; i < pixelCount; i++) {
        uint8_t index = pixels[i];
        if (hasTrans && index == transIdx) {
            pixels32[i] = 0;
        } else if (index < paletteSize) {
            uint8_t r = palette[index * 3 + 0];
            uint8_t g = palette[index * 3 + 1];
            uint8_t b = palette[index * 3 + 2];
            pixels32[i] = abgr ? (0xFF000000 | (static_cast<uint32_t>(r) << 0) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16))
                               : (0xFF000000 | (static_cast<uint32_t>(b) << 0) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(r) << 16));
        } else {
            pixels32[i] = 0;
        }
    }
}

bool GifDecoder::readImageFrame(GifFrame& frame)
{
    frame.left = readWord();
    frame.top = readWord();
    frame.width = readWord();
    frame.height = readWord();
    uint8_t packed = readByte();

    uint8_t* localPalette = nullptr;
    uint32_t localPaletteSize = 0;
    if (!readColorTable(localPalette, localPaletteSize, packed)) return false;

    uint8_t minCodeSize = readByte();

    uint32_t dataSize = 0;
    uint8_t* imageData = readImageData(dataSize);
    if (!imageData) {
        tvg::free(localPalette);
        return false;
    }

    size_t pixelCount = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
    uint8_t* pixels = tvg::malloc<uint8_t>(pixelCount);
    if (!pixels) {
        tvg::free(imageData);
        tvg::free(localPalette);
        return false;
    }

    uint32_t decoded = lzwDecode(imageData, dataSize, pixels, static_cast<uint32_t>(pixelCount), minCodeSize);
    tvg::free(imageData);

    if (decoded != static_cast<uint32_t>(pixelCount)) {
        tvg::free(pixels);
        tvg::free(localPalette);
        return false;
    }

    if (packed & 0x40) {
        uint8_t* deinterlaced = deinterlace(pixels, frame.width, frame.height);
        if (!deinterlaced) {
            tvg::free(pixels);
            tvg::free(localPalette);
            return false;
        }
        pixels = deinterlaced;
    }

    uint8_t* palette = localPalette ? localPalette : globalPalette;
    uint32_t paletteSize = localPalette ? localPaletteSize : globalPaletteSize;

    if (palette) {
        frame.pixels = tvg::malloc<uint8_t>(pixelCount * 4);
        if (!frame.pixels) {
            tvg::free(pixels);
            tvg::free(localPalette);
            return false;
        }
        indexToRGBA(frame, pixels, palette, paletteSize, pixelCount);
    }

    tvg::free(pixels);
    tvg::free(localPalette);
    return true;
}

bool GifDecoder::load(const uint8_t* data, uint32_t size)
{
    clear();

    if (size < 13) return false;  // Minimum GIF size

    this->data = data;
    this->size = size;
    this->pos = 0;

    if (!readHeader()) return false;
    if (!readLogicalScreenDescriptor()) return false;

    tvg::Array<GifFrame> frameArray;
    if (!parseBlocks(frameArray)) {
        clear();
        return false;
    }

    frameCount = frameArray.count;
    if (frameCount == 0) {
        clear();
        return false;
    }

    frames = tvg::calloc<GifFrame>(frameCount, sizeof(GifFrame));
    if (!frames) {
        clear();
        return false;
    }
    memcpy(frames, frameArray.data, frameCount * sizeof(GifFrame));

    // Frames now own the pixel data; clear the array without freeing it.
    frameArray.clear();
    frameArray.reset();

    calculateFrameRate();
    return true;
}

bool GifDecoder::parseBlocks(tvg::Array<GifFrame>& frameArray)
{
    GifFrame pendingGCE = {};
    bool hasGCE = false;

    while (pos < size) {
        uint8_t marker = readByte();

        if (marker == GIF_EXTENSION_INTRODUCER) {
            uint8_t label = readByte();
            if (label == GIF_EXTENSION_GCE) {
                if (!readGCE(pendingGCE)) return false;
                hasGCE = true;
            } else if (pos < size) {
                skipExtension();
            } else {
                break;
            }
        } else if (marker == GIF_IMAGE_SEPARATOR) {
            GifFrame& frame = frameArray.next();
            frame = {};

            if (hasGCE) {
                frame.disposal = pendingGCE.disposal;
                frame.transparent = pendingGCE.transparent;
                frame.transparentIndex = pendingGCE.transparentIndex;
                frame.delay = pendingGCE.delay;
                hasGCE = false;
                pendingGCE = {};
            }

            if (!readImageFrame(frame)) return false;
        } else if (marker == GIF_TRAILER) {
            break;
        } else {
            if (pos >= size) break;
        }
    }

    return true;
}

void GifDecoder::calculateFrameRate()
{
    uint32_t totalDelay = 0;
    for (uint32_t i = 0; i < frameCount; i++) {
        totalDelay += frames[i].delay;
    }
    frameRate = (frameCount > 0 && totalDelay > 0) ? (frameCount * 100.0f) / totalDelay : 10.0f;
}

void GifDecoder::clear()
{
    if (frames) {
        for (uint32_t i = 0; i < frameCount; i++) {
            tvg::free(frames[i].pixels);
            frames[i].pixels = nullptr;
        }
        tvg::free(frames);
        frames = nullptr;
    }

    tvg::free(globalPalette);
    globalPalette = nullptr;

    tvg::free(canvas);
    canvas = nullptr;

    data = nullptr;

    frameCount = 0;
    width = 0;
    height = 0;
    pos = 0;
    size = 0;
}
