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
#ifndef _TVG_BINARY_DESC_H_
#define _TVG_BINARY_DESC_H_

// now only little endian
#define _read_tvg_ui16(dst, src) memcpy(dst, (src), sizeof(uint16_t))
#define _read_tvg_ui32(dst, src) memcpy(dst, (src), sizeof(uint32_t))
#define _read_tvg_float(dst, src) memcpy(dst, (src), sizeof(float))

using TvgIndicator = uint8_t;
using ByteCounter = uint32_t;
using TvgFlag = uint8_t;

#define TVG_INDICATOR_SIZE sizeof(TvgIndicator)
#define BYTE_COUNTER_SIZE sizeof(ByteCounter)
#define TVG_FLAG_SIZE sizeof(TvgFlag)

struct tvgBlock
{
    TvgIndicator type;
    ByteCounter length;
    const char* data;
    const char* end;
};

//TODO: replace it when this feature is completed.
#if 0
    #define TVG_BIN_HEADER_SIGNATURE "ThorVG"
    #define TVG_BIN_HEADER_SIGNATURE_LENGTH 6
    #define TVG_BIN_HEADER_VERSION "000200"
    #define TVG_BIN_HEADER_VERSION_LENGTH 6
#else
    #define TVG_BIN_HEADER_SIGNATURE "TVG"
    #define TVG_BIN_HEADER_SIGNATURE_LENGTH 3
    #define TVG_BIN_HEADER_VERSION "000"
    #define TVG_BIN_HEADER_VERSION_LENGTH 3
#endif

#define TVG_PICTURE_BEGIN_INDICATOR   (TvgIndicator)0xfc
#define TVG_SHAPE_BEGIN_INDICATOR     (TvgIndicator)0xfd
#define TVG_SCENE_BEGIN_INDICATOR     (TvgIndicator)0xfe

// Paint
#define TVG_PAINT_OPACITY_INDICATOR          (TvgIndicator)0x10
#define TVG_PAINT_TRANSFORM_MATRIX_INDICATOR (TvgIndicator)0x11
#define TVG_PAINT_CMP_TARGET_INDICATOR       (TvgIndicator)0x1

#define TVG_PAINT_CMP_METHOD_INDICATOR     (TvgIndicator)0x20
#define TVG_PAINT_CMP_METHOD_CLIPPATH_FLAG      (TvgFlag)0x01
#define TVG_PAINT_CMP_METHOD_ALPHAMASK_FLAG     (TvgFlag)0x02
#define TVG_PAINT_CMP_METHOD_INV_ALPHAMASK_FLAG (TvgFlag)0x03

// Scene
#define TVG_SCENE_FLAG_RESERVEDCNT           (TvgIndicator)0x30

// Shape
#define TVG_SHAPE_PATH_INDICATOR    (TvgIndicator)0x40
#define TVG_SHAPE_STROKE_INDICATOR  (TvgIndicator)0x41
#define TVG_SHAPE_FILL_INDICATOR    (TvgIndicator)0x42
#define TVG_SHAPE_COLOR_INDICATOR   (TvgIndicator)0x43

#define TVG_SHAPE_FILLRULE_INDICATOR    (TvgIndicator)0x44
#define TVG_SHAPE_FILLRULE_WINDING_FLAG      (TvgFlag)0x00
#define TVG_SHAPE_FILLRULE_EVENODD_FLAG      (TvgFlag)0x01

#define TVG_SHAPE_STROKE_CAP_INDICATOR  (TvgIndicator)0x50
#define TVG_SHAPE_STROKE_CAP_SQUARE_FLAG     (TvgFlag)0x00
#define TVG_SHAPE_STROKE_CAP_ROUND_FLAG      (TvgFlag)0x01
#define TVG_SHAPE_STROKE_CAP_BUTT_FLAG       (TvgFlag)0x02

#define TVG_SHAPE_STROKE_JOIN_INDICATOR (TvgIndicator)0x51
#define TVG_SHAPE_STROKE_JOIN_BEVEL_FLAG     (TvgFlag)0x00
#define TVG_SHAPE_STROKE_JOIN_ROUND_FLAG     (TvgFlag)0x01
#define TVG_SHAPE_STROKE_JOIN_MITER_FLAG     (TvgFlag)0x02

#define TVG_SHAPE_STROKE_WIDTH_INDICATOR    (TvgIndicator)0x52
#define TVG_SHAPE_STROKE_COLOR_INDICATOR    (TvgIndicator)0x53
#define TVG_SHAPE_STROKE_FILL_INDICATOR     (TvgIndicator)0x54
#define TVG_SHAPE_STROKE_DASHPTRN_INDICATOR (TvgIndicator)0x55

#define TVG_FILL_LINEAR_GRADIENT_INDICATOR  (TvgIndicator)0x60
#define TVG_FILL_RADIAL_GRADIENT_INDICATOR  (TvgIndicator)0x61
#define TVG_FILL_COLORSTOPS_INDICATOR       (TvgIndicator)0x62
#define TVG_FILL_FILLSPREAD_INDICATOR       (TvgIndicator)0x63
#define TVG_FILL_FILLSPREAD_PAD_FLAG             (TvgFlag)0x00
#define TVG_FILL_FILLSPREAD_REFLECT_FLAG         (TvgFlag)0x01
#define TVG_FILL_FILLSPREAD_REPEAT_FLAG          (TvgFlag)0x02

// Picture
#define TVG_RAW_IMAGE_BEGIN_INDICATOR (TvgIndicator)0x70

#endif //_TVG_BINARY_DESC_H_
