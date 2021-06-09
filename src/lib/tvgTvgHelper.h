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
#ifndef _TVG_TVG_HELPER_H_
#define _TVG_TVG_HELPER_H_

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
    const char* blockEnd;
};

#define TVG_HEADER_TVG_SIGN_CODE "TVG"
#define TVG_HEADER_TVG_VERSION_CODE "000"
#define TVG_HEADER_TVG_SIGN_CODE_LENGTH 3
#define TVG_HEADER_TVG_VERSION_CODE_LENGTH 3
#define TVG_HEADER_TVG_DATA_LENGTH 2

#define TVG_SCENE_BEGIN_INDICATOR     (TvgIndicator)0xfe // Scene indicator
#define TVG_SHAPE_BEGIN_INDICATOR     (TvgIndicator)0xfd // Shape indicator
#define TVG_PICTURE_BEGIN_INDICATOR   (TvgIndicator)0xfc // Picture indicator

// Paint
#define TVG_PAINT_OPACITY_INDICATOR          (TvgIndicator)0x10 // Paint opacity
#define TVG_PAINT_TRANSFORM_MATRIX_INDICATOR (TvgIndicator)0x11 // Paint transformation matrix
#define TVG_PAINT_CMP_TARGET_INDICATOR       (TvgIndicator)0x12 // Paint composite

#define TVG_PAINT_CMP_METHOD_INDICATOR     (TvgIndicator)0x20 // Paint composite method
#define TVG_PAINT_CMP_METHOD_CLIPPATH_FLAG      (TvgFlag)0x01 // CompositeMethod::ClipPath
#define TVG_PAINT_CMP_METHOD_ALPHAMASK_FLAG     (TvgFlag)0x02 // CompositeMethod::AlphaMask
#define TVG_PAINT_CMP_METHOD_INV_ALPHAMASK_FLAG (TvgFlag)0x03 // CompositeMethod::InvAlphaMask

// Scene
#define TVG_SCENE_FLAG_RESERVEDCNT           (TvgIndicator)0x30 // Scene reserved count

// Shape
#define TVG_SHAPE_PATH_INDICATOR    (TvgIndicator)0x40 // Shape has path section
#define TVG_SHAPE_STROKE_INDICATOR  (TvgIndicator)0x41 // Shape has stroke section
#define TVG_SHAPE_FILL_INDICATOR    (TvgIndicator)0x42 // Shape has fill section
#define TVG_SHAPE_COLOR_INDICATOR   (TvgIndicator)0x43 // Shape has color

#define TVG_SHAPE_FILLRULE_INDICATOR    (TvgIndicator)0x44 // Shape FillRule
#define TVG_SHAPE_FILLRULE_WINDING_FLAG      (TvgFlag)0x00 // FillRule::Winding
#define TVG_SHAPE_FILLRULE_EVENODD_FLAG      (TvgFlag)0x01 // FillRule::EvenOdd

#define TVG_SHAPE_STROKE_CAP_INDICATOR  (TvgIndicator)0x50 // Stroke StrokeCap
#define TVG_SHAPE_STROKE_CAP_SQUARE_FLAG     (TvgFlag)0x00 // StrokeCap::Square
#define TVG_SHAPE_STROKE_CAP_ROUND_FLAG      (TvgFlag)0x01 // StrokeCap::Round
#define TVG_SHAPE_STROKE_CAP_BUTT_FLAG       (TvgFlag)0x02 // StrokeCap::Butt

#define TVG_SHAPE_STROKE_JOIN_INDICATOR (TvgIndicator)0x51 // Stroke StrokeJoin
#define TVG_SHAPE_STROKE_JOIN_BEVEL_FLAG     (TvgFlag)0x00 // StrokeJoin::Bevel
#define TVG_SHAPE_STROKE_JOIN_ROUND_FLAG     (TvgFlag)0x01 // StrokeJoin::Round
#define TVG_SHAPE_STROKE_JOIN_MITER_FLAG     (TvgFlag)0x02 // StrokeJoin::Miter

#define TVG_SHAPE_STROKE_WIDTH_INDICATOR    (TvgIndicator)0x52 // Stroke width
#define TVG_SHAPE_STROKE_COLOR_INDICATOR    (TvgIndicator)0x53 // Stroke color
#define TVG_SHAPE_STROKE_FILL_INDICATOR     (TvgIndicator)0x54 // Stroke fill
#define TVG_SHAPE_STROKE_DASHPTRN_INDICATOR (TvgIndicator)0x55 // Stroke dashed stroke

#define TVG_FILL_LINEAR_GRADIENT_INDICATOR  (TvgIndicator)0x60 // Linear gradient
#define TVG_FILL_RADIAL_GRADIENT_INDICATOR  (TvgIndicator)0x61 // Radial gradient
#define TVG_FILL_COLORSTOPS_INDICATOR       (TvgIndicator)0x62 // Gradient color stops
#define TVG_FILL_FILLSPREAD_INDICATOR       (TvgIndicator)0x63 // Gradient fill spread
#define TVG_FILL_FILLSPREAD_PAD_FLAG             (TvgFlag)0x00 // FillSpread::Pad
#define TVG_FILL_FILLSPREAD_REFLECT_FLAG         (TvgFlag)0x01 // FillSpread::Reflect
#define TVG_FILL_FILLSPREAD_REPEAT_FLAG          (TvgFlag)0x02 // FillSpread::Repeat

// Picture
#define TVG_RAW_IMAGE_BEGIN_INDICATOR (TvgIndicator)0x70 // Picture raw data

#endif //_TVG_TVG_HELPER_H_
