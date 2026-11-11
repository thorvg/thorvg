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
    tvg::free(gPalette);
    tvg::free(output);
}

void GifDecoder::readLogicalScreenDescriptor()
{
    width = readWord();
    height = readWord();

    auto packed = readByte();
    readByte();  // background color index
    readByte();  // pixel aspect ratio
    readColorTable(gPalette, gPaletteSize, packed);

    output = tvg::malloc<uint32_t>(width * height * sizeof(uint32_t));
}

void GifDecoder::readColorTable(uint8_t*& palette, uint32_t& size, uint8_t packed)
{
    if (!(packed & 0x80)) return;

    size = 1 << ((packed & 0x07) + 1);
    palette = tvg::malloc<uint8_t>(size * 3);

    for (uint32_t i = 0; i < size; i++) {
        palette[i * 3 + 0] = readByte();
        palette[i * 3 + 1] = readByte();
        palette[i * 3 + 2] = readByte();
    }
}

struct GifLzwDictEntry
{
    uint32_t prefix;  // index of the entry for this string minus its last byte, or 0xFFFF for a single-byte string
    uint8_t suffix;
};

struct GifLzwState
{
    const uint8_t* input;
    uint32_t inputSize;
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

    void reset()
    {
        codeSize = minCodeSize + 1;
        codeMask = (1 << codeSize) - 1;
        nextCode = endCode + 1;
        oldCode = 0xFFFF;
    }

    uint32_t decode()
    {
        while (outputPos < outputSize) {
            auto code = readCode();
            if (code == endCode) break;

            // A clear code resets the code width and dictionary growth state before decoding the next code.
            if (code == clearCode) {
                reset();
                continue;
            }
            // The first code after a reset must be a literal; emit it and retain it as the next dictionary prefix.
            if (oldCode == 0xFFFF) {
                if (code >= clearCode) break;
                output[outputPos++] = static_cast<uint8_t>(code);
                oldCode = code;
                continue;
            }
            if (!emitSequence(code)) break;
        }
        return outputPos;
    }

    bool emitSequence(uint32_t code)
    {
        if (code > nextCode) return false;
        if (code == nextCode) {
            if (!outputSequence(oldCode)) return false;
            if (outputPos >= outputSize) return false;
            output[outputPos] = firstByte(oldCode);
            outputPos++;
        } else if (!outputSequence(code)) {
            return false;
        }

        extendDictionary(oldCode, code);
        oldCode = code;
        return true;
    }

    // GIF packs codes LSB-first across byte boundaries
    uint32_t readCode()
    {
        while (bitsInBuffer < codeSize && (bitPos / 8) < inputSize) {
            bitBuffer |= (static_cast<uint32_t>(input[bitPos / 8])) << bitsInBuffer;
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
        if (nextCode >= 4096) return;

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

uint32_t GifDecoder::lzwDecode(const uint8_t* data, uint32_t size, uint8_t* output, uint32_t outputSize, uint8_t minCodeSize)
{
    if (minCodeSize < 2 || minCodeSize > 8) return 0;
    if (size == 0 || outputSize == 0) return 0;

    auto dictionary = tvg::calloc<GifLzwDictEntry>(4096, sizeof(GifLzwDictEntry));
    if (!dictionary) return 0;

    // Initialize the input/output state, derive the control codes, and seed every single-byte dictionary entry.
    GifLzwState lzw{};
    lzw.input = data;
    lzw.inputSize = size;
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

    uint32_t endY = (frame.height > height - frame.top) ? height - frame.top : frame.height;
    uint32_t endX = (frame.width > width - frame.left) ? width - frame.left : frame.width;

    auto useMemcpy = !frame.transparent && (frame.left + frame.width <= width);

    for (uint32_t y = 0; y < endY; y++) {
        auto canvasY = frame.top + y;
        auto frameIdx = static_cast<size_t>(y) * frame.width;
        auto canvasIdx = static_cast<size_t>(canvasY) * width + frame.left;

        if (useMemcpy) {
            memcpy(&output[canvasIdx], &frame.pixels[frameIdx], frame.width * sizeof(uint32_t));
        } else {
            for (uint32_t x = 0; x < endX; x++) {
                auto pixel = frame.pixels[frameIdx + x];
                if ((pixel & 0xFF000000) != 0) output[canvasIdx + x] = pixel;
            }
        }
    }
}

void GifDecoder::compositeFrame(uint32_t frameIndex, bool draw)
{
    if (frameIndex >= frames.count) return;

    // Clear only the clipped rectangle of the previous frame before drawing the next frame.
    if (frameIndex > 0 && frames[frameIndex - 1].disposal == GIF_DISPOSAL_BACKGROUND) {
        auto& prevFrame = frames[frameIndex - 1];
        if (prevFrame.top < height && prevFrame.left < width) {
            auto endY = (prevFrame.height > height - prevFrame.top) ? height - prevFrame.top : prevFrame.height;
            auto endX = (prevFrame.width > width - prevFrame.left) ? width - prevFrame.left : prevFrame.width;
            for (auto y = 0u; y < endY; y++) {
                auto canvasY = prevFrame.top + y;
                auto canvasIdx = static_cast<size_t>(canvasY) * width + prevFrame.left;
                memset(&output[canvasIdx], 0, endX * sizeof(uint32_t));
            }
        }
    }

    if (draw) blitFrame(frames[frameIndex]);
}

bool GifDecoder::readGCE(GifFrame& pendingGCE)
{
    if (readByte() != 4) return false;  // the GCE block size is always 4

    auto packed = readByte();
    pendingGCE.disposal = (packed >> 2) & 0x07;
    pendingGCE.transparent = (packed & 0x01) != 0;
    pendingGCE.delay = readWord();
    pendingGCE.transparentIndex = readByte();

    readByte();  // block terminator
    return true;
}

void GifDecoder::skipExtension()
{
    while (pos < size) {
        auto blockSize = data[pos++];
        if (blockSize == 0) break;
        if (blockSize > size - pos) break;
        pos += blockSize;
    }
}

// GIF stores LZW-compressed image data in length-prefixed sub-blocks terminated by a zero-sized block.
uint8_t* GifDecoder::readImageData(uint32_t& size)
{
    size = 0;
    uint32_t capacity = 256;
    auto* data = tvg::malloc<uint8_t>(capacity);

    while (true) {
        auto blockSize = readByte();
        if (blockSize == 0) break;

        auto required = size + blockSize;
        if (required > capacity) {
            capacity = required * 2;
            auto* newData = tvg::realloc<uint8_t>(data, capacity);
            if (!newData) {
                tvg::free(data);
                return nullptr;
            }
            data = newData;
        }

        if (blockSize > this->size - pos) {
            tvg::free(data);
            return nullptr;
        }
        memcpy(data + size, this->data + pos, blockSize);
        pos += blockSize;
        size = required;
    }

    return data;
}

// GIF stores interlaced frames in 4 passes (image descriptor bit 0x40)
uint8_t* GifDecoder::deinterlace(uint8_t* pixels, uint16_t width, uint16_t height)
{
    auto pixelCnt = static_cast<size_t>(width) * height;
    auto* dst = tvg::malloc<uint8_t>(pixelCnt);

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

void GifDecoder::indexToPixels(GifFrame& frame, const uint8_t* pixels, const uint8_t* palette, uint32_t paletteSize, size_t pixelCnt)
{
    uint32_t colors[256] = {};
    auto abgr = (cs == ColorSpace::ABGR8888);

    for (uint32_t i = 0; i < paletteSize; i++) {
        auto r = palette[i * 3 + 0];
        auto g = palette[i * 3 + 1];
        auto b = palette[i * 3 + 2];
        if (abgr) std::swap(r, b);
        colors[i] = 0xFF000000 | static_cast<uint32_t>(b) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(r) << 16);
    }
    if (frame.transparent) colors[frame.transparentIndex] = 0;

    for (size_t i = 0; i < pixelCnt; i++) frame.pixels[i] = colors[pixels[i]];
}

bool GifDecoder::readImageFrame(GifFrame& frame)
{
    frame.left = readWord();
    frame.top = readWord();
    frame.width = readWord();
    frame.height = readWord();
    auto packed = readByte();
    auto pixelCnt = static_cast<size_t>(frame.width) * frame.height;

    bool ret = false;
    uint8_t* localPalette = nullptr;
    uint8_t* image = nullptr;
    uint8_t* pixels = nullptr;

    {
        uint32_t localPaletteSize = 0;
        readColorTable(localPalette, localPaletteSize, packed);

        auto* palette = localPalette ? localPalette : gPalette;
        if (!palette) goto cleanup;

        auto paletteSize = localPalette ? localPaletteSize : gPaletteSize;
        auto minCodeSize = readByte();

        uint32_t size = 0;
        image = readImageData(size);
        if (!image) goto cleanup;

        pixels = tvg::malloc<uint8_t>(pixelCnt);
        if (lzwDecode(image, size, pixels, static_cast<uint32_t>(pixelCnt), minCodeSize) != pixelCnt) goto cleanup;

        if (packed & 0x40) {
            auto* deinterlaced = deinterlace(pixels, frame.width, frame.height);
            if (!deinterlaced) goto cleanup;
            pixels = deinterlaced;
        }

        frame.pixels = tvg::malloc<uint32_t>(pixelCnt * sizeof(uint32_t));
        indexToPixels(frame, pixels, palette, paletteSize, pixelCnt);
        ret = true;
    }

cleanup:
    tvg::free(image);
    tvg::free(pixels);
    tvg::free(localPalette);
    return ret;
}

bool GifDecoder::load(const char* data, uint32_t size, ColorSpace cs)
{
    if (size < 13) return false;  // smaller than a header + logical screen descriptor

    this->data = (const uint8_t*) data;
    this->size = size;
    this->cs = (cs == ColorSpace::ARGB8888 || cs == ColorSpace::ARGB8888S || cs == ColorSpace::XRGB8888) ? ColorSpace::ARGB8888 : ColorSpace::ABGR8888;

    // A GIF stream starts with the "GIF" signature followed by either the 87a or 89a version.
    if (memcmp(data + pos, "GIF", 3)) return false;
    pos += 3;
    if (memcmp(data + pos, "87a", 3) && memcmp(data + pos, "89a", 3)) return false;
    pos += 3;
    readLogicalScreenDescriptor();

    return parseBlocks() && !frames.empty();
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

        seconds += (overlapEnd - overlapBegin) * static_cast<float>(frames[i].delay) * 0.01f;
    }
    return seconds;
}
