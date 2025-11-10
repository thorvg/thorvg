/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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
#ifndef TVG_GIF_ENCODER_H
#define TVG_GIF_ENCODER_H

#include <gif_lib.h>
#include <stdint.h>
#include <vector>

class GifEncoder
{
public:
    bool begin(const char* path, uint32_t w, uint32_t h);
    void writeGlobalPalette(uint8_t* buffer, size_t size);
    void buildGlobalPalette();
    bool writeFrame(uint8_t* buffer, int delayTime);
    bool end();

private:
    bool useLocalPalette();
    bool writeLoop();
    bool writeGCE(SavedImage* image, int delayTime, int disposalMode, int transparentIndex);
    SavedImage* makeSavedImage(uint8_t* buffer);
    void pushSample(uint8_t r, uint8_t g, uint8_t b);
    void clearSample();

    template <typename Function>
    void forEach(uint8_t* buffer, size_t size, Function&& f)
    {
        for (size_t i = 0; i < size; i++) {
            const auto a = buffer[i * 4 + 3];
            const auto r = buffer[i * 4];
            const auto g = buffer[i * 4 + 1];
            const auto b = buffer[i * 4 + 2];
            f(i, a, r, g, b);
        }
    }

    GifFileType* gif = nullptr;
    std::vector<GifByteType> sampleR, sampleG, sampleB;
    std::vector<GifByteType> beforeR, beforeG, beforeB;
    size_t numPixels = 0;
    bool hasTransparent = false;
};

#endif // TVG_GIF_ENCODER_H