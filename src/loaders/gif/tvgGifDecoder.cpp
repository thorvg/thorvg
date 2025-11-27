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

#include "tvgGifDecoder.h"
#include "tvgAllocator.h"
#include "tvgArray.h"
#include "tvgMath.h"

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
    
    if (data[pos] != 'G' || data[pos+1] != 'I' || data[pos+2] != 'F') return false;
    pos += 3;
    
    if ((data[pos] != '8' || data[pos+1] != '7' || data[pos+2] != 'a') &&
        (data[pos] != '8' || data[pos+1] != '9' || data[pos+2] != 'a')) {
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
    
    bgIndex = readByte();
    readByte(); // pixel aspect ratio
    
    if (globalColorTable && globalPaletteSize > 0) {
        globalPalette = tvg::malloc<uint8_t>(globalPaletteSize * 3);
        if (!globalPalette) return false;
        
        for (uint32_t i = 0; i < globalPaletteSize; i++) {
            globalPalette[i * 3 + 0] = readByte(); // R
            globalPalette[i * 3 + 1] = readByte(); // G
            globalPalette[i * 3 + 2] = readByte(); // B
        }
    }
    
    // Initialize canvas
    uint32_t canvasSize = width * height * 4;
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
        palette[i * 3 + 0] = readByte(); // R
        palette[i * 3 + 1] = readByte(); // G
        palette[i * 3 + 2] = readByte(); // B
    }
    
    return true;
}

uint32_t GifDecoder::lzwDecode(const uint8_t* data, uint32_t dataSize, uint8_t* output, uint32_t outputSize, uint8_t minCodeSize)
{
    if (minCodeSize < 2 || minCodeSize > 8) return 0;
    if (dataSize == 0 || outputSize == 0) return 0;
    
    // LZW constants
    uint32_t clearCode = 1 << minCodeSize;
    uint32_t endCode = clearCode + 1;
    uint32_t nextCode = endCode + 1;
    uint32_t codeSize = minCodeSize + 1;
    uint32_t codeMask = (1 << codeSize) - 1;
    
    // Bit reading state (GIF uses LSB-first bit order)
    uint32_t bitPos = 0;
    uint32_t outputPos = 0;
    
    // Dictionary: each entry stores the first byte and a link to the previous code
    // We use a simple approach: store sequences in a stack
    struct DictEntry {
        uint32_t prefix;  // previous code
        uint8_t suffix;   // last byte
    };
    
    DictEntry* dictionary = tvg::malloc<DictEntry>(4096 * sizeof(DictEntry));
    if (!dictionary) return 0;
    
    // Initialize dictionary with single-byte codes
    for (uint32_t i = 0; i < clearCode; i++) {
        dictionary[i].prefix = 0xFFFF; // invalid (marks leaf)
        dictionary[i].suffix = (uint8_t)i;
    }
    
    uint32_t oldCode = 0xFFFF;
    bool first = true;
    
    // Read a code from the bit stream (LSB-first)
    uint32_t bitBuffer = 0;
    uint32_t bitsInBuffer = 0;
    
    auto readCode = [&]() -> uint32_t {
        // Fill buffer if needed
        while (bitsInBuffer < codeSize && (bitPos / 8) < dataSize) {
            bitBuffer |= ((uint32_t)data[bitPos / 8]) << bitsInBuffer;
            bitsInBuffer += 8;
            bitPos += 8;
        }
        
        if (bitsInBuffer < codeSize) return endCode;
        
        uint32_t code = bitBuffer & codeMask;
        bitBuffer >>= codeSize;
        bitsInBuffer -= codeSize;
        
        return code;
    };
    
    // Output a sequence from the dictionary
    auto outputSequence = [&](uint32_t code) -> bool {
        uint8_t stack[4096];
        uint32_t stackPos = 0;
        
        // Walk back through dictionary to build sequence
        uint32_t current = code;
        while (current != 0xFFFF && stackPos < 4096) {
            if (current >= nextCode) {
                // Invalid code
                return false;
            }
            stack[stackPos++] = dictionary[current].suffix;
            current = dictionary[current].prefix;
        }
        
        // Output in reverse (stack order)
        while (stackPos > 0 && outputPos < outputSize) {
            output[outputPos++] = stack[--stackPos];
        }
        
        return true;
    };
    
    while (outputPos < outputSize) {
        uint32_t code = readCode();
        
        if (code == endCode) break;
        if (code == clearCode) {
            // Reset dictionary
            codeSize = minCodeSize + 1;
            codeMask = (1 << codeSize) - 1;
            nextCode = endCode + 1;
            oldCode = 0xFFFF;
            first = true;
            continue;
        }
        
        if (first) {
            // First code is always a literal
            if (code >= clearCode) break; // Invalid
            output[outputPos++] = (uint8_t)code;
            oldCode = code;
            first = false;
        } else {
            uint32_t inCode = code;
            
            if (code >= nextCode) {
                // Special case: code not in dictionary yet
                // Output oldCode + first byte of oldCode
                if (!outputSequence(oldCode)) break;
                if (outputPos >= outputSize) break;
                uint8_t firstByte = output[outputPos - 1];
                output[outputPos++] = firstByte;
            } else {
                // Normal case: output sequence for this code
                if (!outputSequence(code)) break;
            }
            
            // Add new dictionary entry: oldCode + first byte of current sequence
            if (nextCode < 4096 && oldCode != 0xFFFF) {
                dictionary[nextCode].prefix = oldCode;
                // Get first byte of current sequence
                uint32_t tempCode = (code >= nextCode) ? oldCode : code;
                uint8_t firstByte = 0;
                if (tempCode < clearCode) {
                    firstByte = (uint8_t)tempCode;
                } else if (tempCode < nextCode) {
                    // Walk to leaf to get first byte
                    uint32_t walk = tempCode;
                    while (walk != 0xFFFF && walk >= clearCode) {
                        walk = dictionary[walk].prefix;
                    }
                    if (walk < clearCode) {
                        firstByte = (uint8_t)walk;
                    }
                }
                dictionary[nextCode].suffix = firstByte;
                nextCode++;
                
                // Increase code size if needed
                if (nextCode >= (1 << codeSize) && codeSize < 12) {
                    codeSize++;
                    codeMask = (1 << codeSize) - 1;
                }
            }
            
            oldCode = inCode;
        }
    }
    
    tvg::free(dictionary);
    return outputPos;
}

bool GifDecoder::decodeFrame(uint32_t frameIndex)
{
    if (frameIndex >= frameCount) return false;
    compositeFrame(frameIndex);
    return true;
}

void GifDecoder::compositeFrame(uint32_t frameIndex, bool draw)
{
    if (frameIndex >= frameCount || !frames[frameIndex].pixels) return;
    
    GifFrame& frame = frames[frameIndex];
    
    // Handle disposal of previous frame
    if (frameIndex > 0) {
        uint8_t prevDisposal = frames[frameIndex - 1].disposal;
        if (prevDisposal == GIF_DISPOSAL_BACKGROUND) {
            // Clear previous frame area to transparent
            GifFrame& prevFrame = frames[frameIndex - 1];
            uint32_t* canvas32 = (uint32_t*)canvas;
            
            // Calculate valid bounds once
            uint32_t endY = (prevFrame.top + prevFrame.height > height) ? 
                            height - prevFrame.top : prevFrame.height;
            uint32_t endX = (prevFrame.left + prevFrame.width > width) ? 
                            width - prevFrame.left : prevFrame.width;
            
            for (uint32_t y = 0; y < endY; y++) {
                uint32_t canvasY = prevFrame.top + y;
                if (canvasY >= height) break;
                
                uint32_t canvasIdx = canvasY * width + prevFrame.left;
                // Use memset for the row if it's fully within bounds
                if (prevFrame.left + endX <= width) {
                    memset(&canvas32[canvasIdx], 0, endX * sizeof(uint32_t));
                } else {
                    // Handle partial row
                    for (uint32_t x = 0; x < endX && (prevFrame.left + x) < width; x++) {
                        canvas32[canvasIdx + x] = 0;
                    }
                }
            }
        }
    }
    
    if (!draw) return;

    // Composite current frame
    uint32_t* canvas32 = (uint32_t*)canvas;
    uint32_t* framePixels32 = (uint32_t*)frame.pixels;
    
    // Pre-calculate valid bounds once
    uint32_t startY = 0;
    uint32_t endY = (frame.top + frame.height > height) ? 
                     height - frame.top : frame.height;
    uint32_t startX = 0;
    uint32_t endX = (frame.left + frame.width > width) ? 
                     width - frame.left : frame.width;
    
    // Early exit if frame is completely out of bounds
    if (frame.top >= height || frame.left >= width) return;
    
    // Check if we can use memcpy for entire rows (only if no transparency)
    bool canUseMemcpy = !frame.transparent && (frame.left + frame.width <= width);
    
    for (uint32_t y = startY; y < endY; y++) {
        uint32_t canvasY = frame.top + y;
        uint32_t frameIdx = y * frame.width;
        uint32_t canvasIdx = canvasY * width + frame.left;
        
        if (canUseMemcpy) {
            // Fast path: copy entire row at once (no transparency)
            memcpy(&canvas32[canvasIdx], &framePixels32[frameIdx], 
                   frame.width * sizeof(uint32_t));
        } else {
            // Slow path: copy pixel by pixel with bounds and transparency checking
            for (uint32_t x = startX; x < endX; x++) {
                if (frame.left + x < width) {
                    uint32_t pixel = framePixels32[frameIdx + x];
                    // Skip transparent pixels (alpha = 0)
                    if ((pixel & 0xFF000000) != 0) {
                        canvas32[canvasIdx + x] = pixel;
                    }
                }
            }
        }
    }
}

bool GifDecoder::load(const uint8_t* data, uint32_t size)
{
    clear();
    
    if (size < 13) return false; // Minimum GIF size
    
    this->data = data;
    this->size = size;
    this->pos = 0;
    
    if (!readHeader()) return false;
    if (!readLogicalScreenDescriptor()) return false;
    
    // Single-pass parsing: use dynamic array for frames
    tvg::Array<GifFrame> frameArray;
    GifFrame pendingGCE = {};  // Store GCE data until we see the image descriptor
    bool hasGCE = false;
    
    while (pos < size) {
        uint8_t marker = readByte();
        
        if (marker == GIF_EXTENSION_INTRODUCER) {
            uint8_t label = readByte();
            if (label == GIF_EXTENSION_GCE) {
                // Read Graphic Control Extension
                uint8_t blockSize = readByte();
                if (blockSize != 4) {
                    clear();
                    return false;
                }
                
                uint8_t packed = readByte();
                pendingGCE.disposal = (packed >> 2) & 0x07;
                pendingGCE.transparent = (packed & 0x01) != 0;
                
                pendingGCE.delay = readWord();
                pendingGCE.transparentIndex = readByte();
                
                readByte(); // block terminator
                hasGCE = true;
            } else {
                // Skip other extensions (Application, Comment, Plain Text, etc.)
                if (pos < size) {
                    uint8_t extSize = readByte();
                    if (pos + extSize > size) break;
                    pos += extSize;
                    // Skip data sub-blocks
                    while (pos < size) {
                        uint8_t blockSize = data[pos++];
                        if (blockSize == 0) break;
                        if (pos + blockSize > size) break;
                        pos += blockSize;
                    }
                } else {
                    break;
                }
            }
        } else if (marker == GIF_IMAGE_SEPARATOR) {
            // Add new frame to array
            GifFrame& frame = frameArray.next();
            
            // Copy GCE data if available
            if (hasGCE) {
                frame.disposal = pendingGCE.disposal;
                frame.transparent = pendingGCE.transparent;
                frame.transparentIndex = pendingGCE.transparentIndex;
                frame.delay = pendingGCE.delay;
                hasGCE = false;
                pendingGCE = {};
            }
            
            // Read image descriptor and decode
            uint16_t left = readWord();
            uint16_t top = readWord();
            uint16_t width = readWord();
            uint16_t height = readWord();
            uint8_t packed = readByte();
            
            frame.left = left;
            frame.top = top;
            frame.width = width;
            frame.height = height;
            
            uint8_t* localPalette = nullptr;
            uint32_t localPaletteSize = 0;
            
            if (!readColorTable(localPalette, localPaletteSize, packed)) {
                clear();
                return false;
            }
            
            uint8_t minCodeSize = readByte();
            
            // Read and concatenate image data sub-blocks
            uint32_t dataSize = 0;
            uint32_t capacity = 256;
            uint8_t* imageData = tvg::malloc<uint8_t>(capacity);
            if (!imageData) {
                if (localPalette) tvg::free(localPalette);
                clear();
                return false;
            }
            
            while (true) {
                uint8_t blockSize = readByte();
                if (blockSize == 0) break;
                
                if (dataSize + blockSize > capacity) {
                    capacity = (dataSize + blockSize) * 2;
                    uint8_t* newData = tvg::realloc<uint8_t>(imageData, capacity);
                    if (!newData) {
                        tvg::free(imageData);
                        if (localPalette) tvg::free(localPalette);
                        clear();
                        return false;
                    }
                    imageData = newData;
                }
                
                // Batch read entire block at once
                if (pos + blockSize <= size) {
                    memcpy(imageData + dataSize, data + pos, blockSize);
                    pos += blockSize;
                    dataSize += blockSize;
                } else {
                    tvg::free(imageData);
                    if (localPalette) tvg::free(localPalette);
                    clear();
                    return false;
                }
            }
            
            // Decode LZW data
            uint32_t pixelCount = width * height;
            uint8_t* pixels = tvg::malloc<uint8_t>(pixelCount);
            if (!pixels) {
                tvg::free(imageData);
                if (localPalette) tvg::free(localPalette);
                clear();
                return false;
            }
            
            uint32_t decoded = lzwDecode(imageData, dataSize, pixels, pixelCount, minCodeSize);
            tvg::free(imageData);
            
            if (decoded != pixelCount) {
                tvg::free(pixels);
                if (localPalette) tvg::free(localPalette);
                clear();
                return false;
            }
            
            // Convert indexed pixels to RGBA
            uint8_t* palette = localPalette ? localPalette : globalPalette;
            uint32_t paletteSize = localPalette ? localPaletteSize : globalPaletteSize;
            
            if (palette) {
                uint32_t rgbaSize = pixelCount * 4;
                frame.pixels = tvg::malloc<uint8_t>(rgbaSize);
                if (!frame.pixels) {
                    tvg::free(pixels);
                    if (localPalette) tvg::free(localPalette);
                    clear();
                    return false;
                }
                
                auto pixels32 = reinterpret_cast<uint32_t*>(frame.pixels);
                uint8_t transIdx = frame.transparentIndex;
                bool hasTrans = frame.transparent;
                
                for (uint32_t i = 0; i < pixelCount; i++) {
                    uint8_t index = pixels[i];
                    if (hasTrans && index == transIdx) {
                        pixels32[i] = 0; // transparent
                    } else if (index < paletteSize) {
                        uint8_t r = palette[index * 3 + 0];
                        uint8_t g = palette[index * 3 + 1];
                        uint8_t b = palette[index * 3 + 2];
                        pixels32[i] = 0xFF000000 | (b << 0) | (g << 8) | (r << 16);
                    } else {
                        pixels32[i] = 0; // out of range
                    }
                }
            }
            
            tvg::free(pixels);
            if (localPalette) tvg::free(localPalette);
            
        } else if (marker == GIF_TRAILER) {
            break;
        } else {
            // Unknown marker - skip and continue
            if (pos >= size) break;
        }
    }
    
    frameCount = frameArray.count;
    
    if (frameCount == 0) {
        clear();
        return false;
    }
    
    // Convert dynamic array to fixed array
    frames = tvg::malloc<GifFrame>(frameCount * sizeof(GifFrame));
    if (!frames) {
        clear();
        return false;
    }
    memcpy(frames, frameArray.data, frameCount * sizeof(GifFrame));
    
    // Clear frame array without freeing pixel data (it's now owned by frames)
    frameArray.clear();
    frameArray.reset();
    
    // Calculate frame rate (average delay)
    uint32_t totalDelay = 0;
    for (uint32_t i = 0; i < frameCount; i++) {
        totalDelay += frames[i].delay;
    }
    if (frameCount > 0 && totalDelay > FLOAT_EPSILON) {
        frameRate = (frameCount * 100.0f) / totalDelay;
    } else {
        frameRate = 10.0f; // default
    }
    
    return true;
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
