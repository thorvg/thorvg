/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#ifndef _TVG_SW_COMMON_H_
#define _TVG_SW_COMMON_H_

#include "tvgCommon.h"

using namespace tvg;

constexpr auto SW_CURVE_TYPE_POINT = 0;
constexpr auto SW_CURVE_TYPE_CUBIC = 1;
constexpr auto SW_OUTLINE_FILL_WINDING = 0;
constexpr auto SW_OUTLINE_FILL_EVEN_ODD = 1;

using SwCoord = signed long;

struct SwPoint
{
    SwCoord x, y;

    SwPoint operator+(const SwPoint& rhs) const {
        return {x + rhs.x, y + rhs.y};
    }

    SwPoint operator-(const SwPoint& rhs) const {
        return {x - rhs.x, y - rhs.y};
    }

    bool operator==(const SwPoint& rhs ) const {
        return (x == rhs.x && y == rhs.y);
    }

    bool operator!=(const SwPoint& rhs) const {
        return (x != rhs.x || y != rhs.y);
    }
};

struct SwSize
{
    SwCoord w, h;
};

struct SwOutline
{
    size_t*     cntrs;            //the contour end points
    size_t      cntrsCnt;         //number of contours in glyph
    size_t      reservedCntrsCnt;
    SwPoint*    pts;              //the outline's points
    size_t      ptsCnt;           //number of points in the glyph
    size_t      reservedPtsCnt;
    uint8_t*    types;            //curve type
    uint8_t     fillMode;         //outline fill mode
};

struct SwSpan
{
    int16_t x, y;
    uint16_t len;
    uint8_t coverage;
};

struct SwRleData
{
    SwSpan *spans;
    size_t alloc;
    size_t size;
};

struct SwBBox
{
    SwPoint min, max;
};

struct SwShape
{
    SwOutline*   outline;
    SwRleData*   rle;
    SwBBox       bbox;
};

void shapeReset(SwShape& sdata);
bool shapeGenOutline(const Shape& shape, SwShape& sdata);
void shapeDelOutline(SwShape& sdata);
bool shapeGenRle(const Shape& shape, SwShape& sdata, const SwSize& clip);
void shapeTransformOutline(const Shape& shape, SwShape& sdata, const RenderTransform& transform);
SwRleData* rleRender(const SwShape& sdata, const SwSize& clip);

bool rasterShape(Surface& surface, SwShape& sdata, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

#endif /* _TVG_SW_COMMON_H_ */
