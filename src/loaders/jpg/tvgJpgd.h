/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.

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

// jpgd.h - C++ class for JPEG decompression.
// Public domain, Rich Geldreich <richgel99@gmail.com>
#ifndef _TVG_JPGD_H_
#define _TVG_JPGD_H_

// Loads a JPEG image from a memory buffer or a file.
// req_comps can be 1 (grayscale), 3 (RGB), or 4 (RGBA).
// On return, width/height will be set to the image's dimensions, and actual_comps will be set to the either 1 (grayscale) or 3 (RGB).
// Notes: For more control over where and how the source data is read, see the decompress_jpeg_image_from_stream() function below, or call the jpeg_decoder class directly.
// Requesting a 8 or 32bpp image is currently a little faster than 24bpp because the jpeg_decoder class itself currently always unpacks to either 8 or 32bpp.
unsigned char *decompress_jpeg_image_from_memory(const unsigned char *pSrc_data, int src_data_size, int *width, int *height, int *actual_comps, int req_comps);
unsigned char *decompress_jpeg_image_from_file(const char *pSrc_filename, int *width, int *height, int *actual_comps, int req_comps);

#endif //_TVG_JPGD_H_
