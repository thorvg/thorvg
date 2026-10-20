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

#define GIF_DISPOSAL_BACKGROUND 2

GifDecoder::~GifDecoder()
{
    for (auto& frame : frames) {
        tvg::free(frame.pixels);
    }
    tvg::free(globalPalette);
    tvg::free(canvas);
}

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

bool GifDecoder::readLogicalScreenDescriptor()
{
    width = readWord();
    height = readWord();

    auto packed = readByte();
    readByte();  //background color index
    readByte();  //pixel aspect ratio

    if (!readColorTable(globalPalette, globalPaletteSize, packed)) return false;

    auto canvasSize = static_cast<size_t>(width) * static_cast<size_t>(height) * sizeof(uint32_t);
    canvas = tvg::malloc<uint8_t>(canvasSize);
    if (!canvas) return false;

    return true;
}

bool GifDecoder::readColorTable(uint8_t*& palette, uint32_t& size, uint8_t packed)
{
    auto colorTable = (packed & 0x80) != 0;
    if (!colorTable) {
        palette = nullptr;
        size = 0;
        return true;
    }

    size = 1 << ((packed & 0x07) + 1);
    palette = tvg::malloc<uint8_t>(size * 3);
    if (!palette) return false;

    for (uint32_t i = 0; i < size; i++) {
        palette[i * 3 + 0] = readByte();
        palette[i * 3 + 1] = readByte();
        palette[i * 3 + 2] = readByte();
    }

    return true;
}

namespace
{

struct GifLzwDictEntry
{
    uint32_t prefix;  //index of the entry for this string minus its last byte, or 0xFFFF for a single-byte string
    uint8_t suffix;
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
            auto code = readCode();
            if (code == endCode) break;

            //A clear code resets the code width and dictionary growth state before decoding the next code.
            if (code == clearCode) {
                reset();
                continue;
            }

            //The first code after a reset must be a literal; emit it and retain it as the next dictionary prefix.
            if (first) {
                if (code >= clearCode) break;
                output[outputPos++] = static_cast<uint8_t>(code);
                oldCode = code;
                first = false;
                continue;
            }
            if (!emitSequence(code)) break;
        }
        return outputPos;
    }

    bool emitSequence(uint32_t code)
    {
        auto inCode = code;
        if (code > nextCode) return false;
        if (code == nextCode) {
            if (!outputSequence(oldCode)) return false;
            if (outputPos == 0 || outputPos >= outputSize) return false;
            output[outputPos] = firstByte(oldCode);
            outputPos++;
        } else if (!outputSequence(code)) {
            return false;
        }

        extendDictionary(oldCode, code);
        oldCode = inCode;
        return true;
    }

    //GIF packs codes LSB-first across byte boundaries
    uint32_t readCode()
    {
        while (bitsInBuffer < codeSize && (bitPos / 8) < dataSize) {
            bitBuffer |= (static_cast<uint32_t>(data[bitPos / 8])) << bitsInBuffer;
            bitsInBuffer += 8;
            bitPos += 8;
        }
        if (bitsInBuffer < codeSize) return endCode;

        auto code = bitBuffer & codeMask;
        bitBuffer >>= codeSize;
        bitsInBuffer -= codeSize;
        return code;
    }

    bool outputSequence(uint32_t code)
    {
        uint8_t stack[4096];
        uint32_t stackPos = 0;

        auto current = code;
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

        auto walk = code;
        while (walk != 0xFFFF && walk >= clearCode) {
            walk = dictionary[walk].prefix;
        }
        return (walk < clearCode) ? static_cast<uint8_t>(walk) : 0;
    }

    void extendDictionary(uint32_t oldCode, uint32_t code)
    {
        if (nextCode >= 4096 || oldCode == 0xFFFF) return;

        dictionary[nextCode].prefix = oldCode;
        auto tempCode = (code >= nextCode) ? oldCode : code;
        dictionary[nextCode].suffix = firstByte(tempCode);
        nextCode++;

        if (nextCode >= (1u << codeSize) && codeSize < 12) {
            codeSize++;
            codeMask = (1 << codeSize) - 1;
        }
    }
};

}  //namespace

uint32_t GifDecoder::lzwDecode(const uint8_t* data, uint32_t dataSize, uint8_t* output, uint32_t outputSize, uint8_t minCodeSize)
{
    if (minCodeSize < 2 || minCodeSize > 8) return 0;
    if (dataSize == 0 || outputSize == 0) return 0;

    auto dictionary = tvg::calloc<GifLzwDictEntry>(4096, sizeof(GifLzwDictEntry));
    if (!dictionary) return 0;

    //Initialize the input/output state, derive the control codes, and seed every single-byte dictionary entry.
    GifLzwState lzw{};
    lzw.data = data;
    lzw.dataSize = dataSize;
    lzw.output = output;
    lzw.outputSize = outputSize;
    lzw.dictionary = dictionary;
    lzw.minCodeSize = minCodeSize;
    lzw.clearCode = 1 << minCodeSize;
    lzw.endCode = lzw.clearCode + 1;
    lzw.reset();
    for (auto i = 0u; i < lzw.clearCode; i++) {
        dictionary[i].prefix = 0xFFFF;
        dictionary[i].suffix = static_cast<uint8_t>(i);
    }
    auto outputPos = lzw.decode();

    tvg::free(dictionary);
    return outputPos;
}

void GifDecoder::blitFrame(const GifFrame& frame)
{
    if (frame.top >= height || frame.left >= width) return;

    auto canvas32 = reinterpret_cast<uint32_t*>(canvas);
    auto framePixels32 = reinterpret_cast<uint32_t*>(frame.pixels);

    uint32_t endY = (frame.height > height - frame.top) ? height - frame.top : frame.height;
    uint32_t endX = (frame.width > width - frame.left) ? width - frame.left : frame.width;

    auto useMemcpy = !frame.transparent && (frame.left + frame.width <= width);

    for (uint32_t y = 0; y < endY; y++) {
        auto canvasY = frame.top + y;
        auto frameIdx = static_cast<size_t>(y) * frame.width;
        auto canvasIdx = static_cast<size_t>(canvasY) * width + frame.left;

        if (useMemcpy) {
            memcpy(&canvas32[canvasIdx], &framePixels32[frameIdx], frame.width * sizeof(uint32_t));
        } else {
            for (uint32_t x = 0; x < endX; x++) {
                auto pixel = framePixels32[frameIdx + x];
                if ((pixel & 0xFF000000) != 0) canvas32[canvasIdx + x] = pixel;
            }
        }
    }
}

void GifDecoder::compositeFrame(uint32_t frameIndex, bool draw)
{
    if (frameIndex >= frames.count) return;

    //Clear only the clipped rectangle of the previous frame before drawing the next frame.
    if (frameIndex > 0 && frames[frameIndex - 1].disposal == GIF_DISPOSAL_BACKGROUND) {
        auto& prevFrame = frames[frameIndex - 1];
        if (prevFrame.top < height && prevFrame.left < width) {
            auto canvas32 = reinterpret_cast<uint32_t*>(canvas);
            auto endY = (prevFrame.height > height - prevFrame.top) ? height - prevFrame.top : prevFrame.height;
            auto endX = (prevFrame.width > width - prevFrame.left) ? width - prevFrame.left : prevFrame.width;
            for (auto y = 0u; y < endY; y++) {
                auto canvasY = prevFrame.top + y;
                auto canvasIdx = static_cast<size_t>(canvasY) * width + prevFrame.left;
                memset(&canvas32[canvasIdx], 0, endX * sizeof(uint32_t));
            }
        }
    }

    if (draw) blitFrame(frames[frameIndex]);
}

bool GifDecoder::readGCE(GifFrame& pendingGCE)
{
    if (readByte() != 4) return false;  //the GCE block size is always 4

    auto packed = readByte();
    pendingGCE.disposal = (packed >> 2) & 0x07;
    pendingGCE.transparent = (packed & 0x01) != 0;
    pendingGCE.delay = readWord();
    pendingGCE.transparentIndex = readByte();

    readByte();  //block terminator
    return true;
}

void GifDecoder::skipExtension()
{
    auto extSize = readByte();
    if (extSize == 0 || pos + extSize > size) return;
    pos += extSize;

    while (pos < size) {
        auto blockSize = data[pos++];
        if (blockSize == 0) break;
        if (pos + blockSize > size) break;
        pos += blockSize;
    }
}

//GIF stores LZW-compressed image data in length-prefixed sub-blocks terminated by a zero-sized block.
uint8_t* GifDecoder::readImageData(uint32_t& dataSize)
{
    dataSize = 0;
    uint32_t capacity = 256;
    auto* imageData = tvg::malloc<uint8_t>(capacity);
    if (!imageData) return nullptr;

    while (true) {
        auto blockSize = readByte();
        if (blockSize == 0) break;

        auto required = dataSize + blockSize;
        if (required > capacity) {
            capacity = required * 2;
            auto* newData = tvg::realloc<uint8_t>(imageData, capacity);
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
        dataSize = required;
    }

    return imageData;
}

//GIF stores interlaced frames in 4 passes (image descriptor bit 0x40)
uint8_t* GifDecoder::deinterlace(uint8_t* pixels, uint16_t width, uint16_t height)
{
    auto pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    auto* dst = tvg::malloc<uint8_t>(pixelCount);
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
    auto transIdx = frame.transparentIndex;
    auto transparent = frame.transparent;

    for (size_t i = 0; i < pixelCount; i++) {
        auto index = pixels[i];
        if (transparent && index == transIdx) {
            pixels32[i] = 0;
        } else if (index < paletteSize) {
            auto r = palette[index * 3 + 0];
            auto g = palette[index * 3 + 1];
            auto b = palette[index * 3 + 2];
            pixels32[i] = abgr ? (0xFF000000 | (static_cast<uint32_t>(r) << 0) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16))
                               : (0xFF000000 | (static_cast<uint32_t>(b) << 0) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(r) << 16));
        } else {
            pixels32[i] = 0;
        }
    }
}

bool GifDecoder::readImageFrame(GifFrame& frame)
{
    bool ret = false;
    uint8_t* localPalette = nullptr;
    uint8_t* imageData = nullptr;
    uint8_t* pixels = nullptr;

    {
        frame.left = readWord();
        frame.top = readWord();
        frame.width = readWord();
        frame.height = readWord();
        auto packed = readByte();

        uint32_t localPaletteSize = 0;
        if (!readColorTable(localPalette, localPaletteSize, packed)) goto cleanup;

        auto minCodeSize = readByte();

        uint32_t dataSize = 0;
        imageData = readImageData(dataSize);
        if (!imageData) goto cleanup;

        auto pixelCount = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
        pixels = tvg::malloc<uint8_t>(pixelCount);
        if (!pixels) goto cleanup;

        auto decoded = lzwDecode(imageData, dataSize, pixels, static_cast<uint32_t>(pixelCount), minCodeSize);
        if (decoded != static_cast<uint32_t>(pixelCount)) goto cleanup;

        if (packed & 0x40) {
            auto* deinterlaced = deinterlace(pixels, frame.width, frame.height);
            if (!deinterlaced) goto cleanup;
            pixels = deinterlaced;
        }

        auto* palette = localPalette ? localPalette : globalPalette;
        auto paletteSize = localPalette ? localPaletteSize : globalPaletteSize;
        if (!palette) goto cleanup;

        frame.pixels = tvg::malloc<uint8_t>(pixelCount * sizeof(uint32_t));
        if (!frame.pixels) goto cleanup;

        indexToRGBA(frame, pixels, palette, paletteSize, pixelCount);
        ret = true;
    }

cleanup:
    tvg::free(imageData);
    tvg::free(pixels);
    tvg::free(localPalette);
    return ret;
}

bool GifDecoder::load(const uint8_t* data, uint32_t size)
{
    if (size < 13) return false;  //smaller than a header + logical screen descriptor

    this->data = data;
    this->size = size;

    //A GIF stream starts with the "GIF" signature followed by either the 87a or 89a version.
    if (memcmp(data + pos, "GIF", 3)) return false;
    pos += 3;
    if (memcmp(data + pos, "87a", 3) && memcmp(data + pos, "89a", 3)) return false;
    pos += 3;
    if (!readLogicalScreenDescriptor()) return false;

    if (!(parseBlocks() && frames.count != 0)) return false;

    return true;
}

bool GifDecoder::parseBlocks()
{
    auto pendingGCE = GifFrame{};

    while (pos < size) {
        auto marker = readByte();

        if (marker == GIF_EXTENSION_INTRODUCER) {
            auto label = readByte();
            if (label == GIF_EXTENSION_GCE) {
                if (!readGCE(pendingGCE)) return false;
            } else if (pos < size) {
                skipExtension();
            } else {
                break;
            }
        } else if (marker == GIF_IMAGE_SEPARATOR) {
            auto& frame = frames.next();
            frame = pendingGCE;
            pendingGCE = GifFrame{};

            if (!readImageFrame(frame)) return false;
        } else if (marker == GIF_TRAILER) {
            break;
        } else if (pos >= size) break;
    }

    return true;
}

float GifDecoder::duration(float begin, float end) const
{
    auto seconds = 0.0f;
    for (auto i = 0u; i < frames.count; i++) {
        auto frameBegin = static_cast<float>(i);
        auto frameEnd = frameBegin + 1.0f;
        auto overlapBegin = begin > frameBegin ? begin : frameBegin;
        auto overlapEnd = end < frameEnd ? end : frameEnd;
        if (overlapBegin >= overlapEnd) continue;

        //GIF delay is in hundredths of a second; use 10 (100ms) when unspecified.
        auto delay = frames[i].delay ? frames[i].delay : 10;
        seconds += (overlapEnd - overlapBegin) * static_cast<float>(delay) * 0.01f;
    }
    return seconds;
}
