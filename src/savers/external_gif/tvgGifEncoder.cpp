/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include "tvgGifEncoder.h"
#include "tvgCommon.h"
#include <cstring>

#define TRANSPARENT_THRESHOLD 127
#define BIT_DEPTH 8
#define MAX_PALETTESIZE 256

static ColorMapObject* _quantize(std::vector<GifByteType>& r, std::vector<GifByteType>& g, std::vector<GifByteType>& b, GifByteType* outputBuffer)
{
    int paletteSize = MAX_PALETTESIZE - 1;
    std::vector<GifColorType> palette(paletteSize, {0, 0, 0});
    std::vector<GifByteType> tempIndex;
    if (outputBuffer == nullptr) {
        tempIndex.resize(r.size(), 0);
        outputBuffer = tempIndex.data();
    }
    if (r.size() > 0 && GifQuantizeBuffer((int)r.size(), 1, &paletteSize, r.data(), g.data(), b.data(), outputBuffer, palette.data()) == GIF_ERROR) {
        return nullptr;
    }

    int colorMapSize = 1 << GifBitSize(paletteSize + 1); // +1: transparent index
    auto* colorMap = GifMakeMapObject(colorMapSize, nullptr);
    if (colorMap) memcpy(colorMap->Colors, palette.data(), paletteSize * sizeof(GifColorType));

    return colorMap;
}

static int _getClosestPaletteColor(uint8_t r, uint8_t g, uint8_t b, const GifColorType* cols, int colorCount)
{
    int bestInd = 0;
    int bestDiff = INT32_MAX;
    for (int i = 0; i < colorCount; ++i) {
        int rd = (int)cols[i].Red - (int)r;
        int gd = (int)cols[i].Green - (int)g;
        int bd = (int)cols[i].Blue - (int)b;
        int d2 = rd * rd + gd * gd + bd * bd;

        if (d2 < bestDiff) {
            bestDiff = d2;
            bestInd = i;
            if (d2 == 0)
                break;
        }
    }
    return bestInd;
}

bool GifEncoder::useLocalPalette()
{
    return gif->SColorMap == nullptr;
}

bool GifEncoder::writeLoop()
{
    unsigned char netscapeStr[] = "NETSCAPE2.0";
    GifByteType loopData[] = {/*control loop*/ 0x01, /*low loop count*/ 0x00, /*high loop count*/ 0x00}; // low == high == 0: infinite loop
    if (GifAddExtensionBlock(&gif->ExtensionBlockCount, &gif->ExtensionBlocks, APPLICATION_EXT_FUNC_CODE, sizeof(netscapeStr) - 1, netscapeStr) == GIF_ERROR)
        return false;
    if (GifAddExtensionBlock(&gif->ExtensionBlockCount, &gif->ExtensionBlocks, CONTINUE_EXT_FUNC_CODE, sizeof(loopData), loopData) == GIF_ERROR)
        return false;
    return true;
}

bool GifEncoder::writeGCE(SavedImage* image, int delayTime, int disposalMode, int transparentIndex)
{
    GraphicsControlBlock gcb{.DisposalMode = disposalMode, .UserInputFlag = false, .DelayTime = delayTime, .TransparentColor = transparentIndex};
    GifByteType ext[4];
    const auto extSize = EGifGCBToExtension(&gcb, ext);
    if (0 < extSize)
        return GifAddExtensionBlock(&image->ExtensionBlockCount, &image->ExtensionBlocks, GRAPHICS_EXT_FUNC_CODE, extSize, ext) != GIF_ERROR;
    return false;
}

SavedImage* GifEncoder::makeSavedImage(uint8_t* buffer)
{
    SavedImage* img = GifMakeSavedImage(gif, nullptr);
    if (img) {
        img->RasterBits = (GifByteType*)calloc(numPixels, sizeof(GifByteType));
        img->ImageDesc.Left = 0;
        img->ImageDesc.Top = 0;
        img->ImageDesc.Width = gif->SWidth;
        img->ImageDesc.Height = gif->SHeight;
        img->ImageDesc.Interlace = false;
        img->ImageDesc.ColorMap = nullptr;

        if (useLocalPalette()) {
            hasTransparent = false;
            clearSample();
            forEach(buffer, numPixels, [&](int index, uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
                pushSample(r, g, b);
                hasTransparent |= (a < TRANSPARENT_THRESHOLD);
            });

            img->ImageDesc.ColorMap = _quantize(sampleR, sampleG, sampleB, img->RasterBits);
            if (img->ImageDesc.ColorMap == nullptr)
                return nullptr;
        }
        if (!img->RasterBits) return nullptr;

        return img;
    }
    return nullptr;
}

void GifEncoder::pushSample(uint8_t r, uint8_t g, uint8_t b)
{
    sampleR.push_back(r);
    sampleG.push_back(g);
    sampleB.push_back(b);
}

void GifEncoder::clearSample()
{
    sampleR.clear();
    sampleG.clear();
    sampleB.clear();
    sampleR.reserve(numPixels);
    sampleG.reserve(numPixels);
    sampleB.reserve(numPixels);
}

bool GifEncoder::begin(const char* path, uint32_t w, uint32_t h)
{
    int err = 0;
    gif = EGifOpenFileName(path, false, &err);
    if (!gif) {
        TVGERR("GIF_SAVER", "Failed gif begin: %s", GifErrorString(err));
        return false;
    }
    gif->SWidth = static_cast<GifWord>(w);
    gif->SHeight = static_cast<GifWord>(h);
    gif->SColorResolution = BIT_DEPTH;
    gif->AspectByte = 0;
    gif->SColorMap = nullptr;

    if (!writeLoop()) {
        EGifCloseFile(gif, &err);
        return false;
    }
    numPixels = w * h;
    beforeR.resize(numPixels, 0);
    beforeG.resize(numPixels, 0);
    beforeB.resize(numPixels, 0);
    clearSample();
    return true;
}

void GifEncoder::writeGlobalPalette(uint8_t* buffer, size_t size)
{
    forEach(buffer, size, [&](int index, uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
        if (a < TRANSPARENT_THRESHOLD || (beforeR[index] == r && beforeG[index] == g && beforeB[index] == b))
            return;

        pushSample(r, g, b);

        beforeR[index] = r;
        beforeG[index] = g;
        beforeB[index] = b;
    });
}

void GifEncoder::buildGlobalPalette()
{
    gif->SColorMap = _quantize(sampleR, sampleG, sampleB, nullptr);
    gif->SBackGroundColor = gif->SColorMap->ColorCount - 1;
    clearSample();
}

bool GifEncoder::writeFrame(uint8_t* buffer, int delayTime)
{
    auto* img = makeSavedImage(buffer);

    if (img == nullptr || (img->ImageDesc.ColorMap == nullptr && gif->SColorMap == nullptr))
        return false;

    const bool isGlobal = !useLocalPalette();
    const auto* const colorMap = isGlobal ? gif->SColorMap : img->ImageDesc.ColorMap;
    const auto transparentIdx = colorMap->ColorCount - 1;

    if (hasTransparent || isGlobal) {
        auto* paletteIndex = img->RasterBits;
        hasTransparent = false;

        forEach(buffer, numPixels, [&](int index, uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
            if (a >= TRANSPARENT_THRESHOLD) {
                if (paletteIndex[index] == 0)
                    paletteIndex[index] = _getClosestPaletteColor(r, g, b, colorMap->Colors, colorMap->ColorCount - 1);
            } else {
                paletteIndex[index] = transparentIdx;
                hasTransparent = true;
            }
        });
    }

    if (img != nullptr && !writeGCE(img, delayTime, hasTransparent ? DISPOSE_BACKGROUND : DISPOSE_DO_NOT, transparentIdx))
        return false;

    return true;
}

bool GifEncoder::end()
{
    bool success = (EGifSpew(gif) != GIF_ERROR);
    GifFreeSavedImages(gif);
    return success;
}