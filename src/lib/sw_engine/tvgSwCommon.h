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

constexpr auto SW_STROKE_TAG_ON = 1;
constexpr auto SW_STROKE_TAG_BEGIN = 4;
constexpr auto SW_STROKE_TAG_END = 8;

using SwCoord = signed long;
using SwFixed = signed long long;

struct SwPoint
{
    SwCoord x, y;

    SwPoint& operator+=(const SwPoint& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

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
    uint32_t*     cntrs;            //the contour end points
    uint32_t      cntrsCnt;         //number of contours in glyph
    uint32_t      reservedCntrsCnt;
    SwPoint*      pts;              //the outline's points
    uint32_t      ptsCnt;           //number of points in the glyph
    uint32_t      reservedPtsCnt;
    uint8_t*      types;            //curve type
    uint8_t       fillMode;         //outline fill mode
    bool          opened;           //opened path?
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
    uint32_t alloc;
    uint32_t size;
};

struct SwBBox
{
    SwPoint min, max;
};

struct SwStrokeBorder
{
    uint32_t ptsCnt;
    uint32_t maxPts;
    SwPoint* pts;
    uint8_t* tags;
    int32_t start;     //index of current sub-path start point
    bool movable;      //true: for ends of lineto borders
    bool valid;
};

struct SwStroke
{
    SwRleData* rle;

    SwFixed angleIn;
    SwFixed angleOut;
    SwPoint center;
    SwFixed lineLength;
    SwPoint subPathStart;
    SwFixed subPathLineLength;
    SwFixed width;

    StrokeCap cap;
    StrokeJoin join;
    StrokeJoin joinSaved;

    SwStrokeBorder borders[2];

    bool firstPt;
    bool subPathOpen;
    bool subPathAngle;
    bool handleWideStrokes;
};

struct SwShape
{
    SwOutline*   outline;
    SwRleData*   rle;
    SwStroke*    stroke;
    SwBBox       bbox;
};

static inline SwPoint TO_SWPOINT(const Point* pt)
{
    return {SwCoord(pt->x * 64), SwCoord(pt->y * 64)};
}


static inline SwCoord TO_SWCOORD(float val)
{
    return SwCoord(val * 64);
}


void shapeReset(SwShape& sdata);
bool shapeGenOutline(const Shape& shape, SwShape& sdata);
bool shapeGenRle(const Shape& shape, SwShape& sdata, const SwSize& clip);
void shapeDelOutline(SwShape& sdata);
void shapeResetStroke(const Shape& shape, SwShape& sdata);
bool shapeGenStrokeOutline(const Shape& shape, SwShape& sdata);
bool shapeGenStrokeRle(const Shape& shape, SwShape& sdata, const SwSize& clip);
void shapeTransformOutline(const Shape& shape, SwShape& sdata, const RenderTransform& transform);
void shapeFree(SwShape* sdata);

void strokeReset(SwStroke& stroke, float width, StrokeCap cap, StrokeJoin join);
bool strokeParseOutline(SwStroke& stroke, SwOutline& outline);
void strokeFree(SwStroke* stroke);

SwRleData* rleRender(const SwShape& sdata, const SwSize& clip);
SwRleData* rleStrokeRender(const SwShape& sdata);

bool rasterShape(Surface& surface, SwShape& sdata, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

#endif /* _TVG_SW_COMMON_H_ */
