/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_PNG_ENCODER_H_
#define _TVG_PNG_ENCODER_H_

#include "tvgCommon.h"

/**
 * Encodes an image into a PNG file.
 *
 * @param[in] filename The path of the file to be written.
 * @param[in] pixels   The 32bit pixels in a row-major order, tightly packed (stride == width).
 *                     Each pixel is a non-premultiplied ABGR8888 value (R: the lowest byte, A: the highest byte).
 * @param[in] w        The image width in pixels.
 * @param[in] h        The image height in pixels.
 * @param[in] quality  The compression level in [0, 100]. @c 0 stores the data uncompressed for the fastest encoding.
 *
 * The image is written as an 8bit RGB image if it is fully opaque, otherwise as an 8bit RGBA image.
 */
bool pngEncode(const char* filename, const uint32_t* pixels, uint32_t w, uint32_t h, uint32_t quality);

#endif //_TVG_PNG_ENCODER_H_
