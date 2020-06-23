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

#if 1
#include <sys/time.h>
static double timeStamp()
{
   struct timeval tv;
   gettimeofday(&tv, NULL);
   return (tv.tv_sec + tv.tv_usec / 1000000.0);
}
#endif

using namespace tvg;

#define SW_CURVE_TYPE_POINT 0
#define SW_CURVE_TYPE_CUBIC 1
#define SW_OUTLINE_FILL_WINDING 0
#define SW_OUTLINE_FILL_EVEN_ODD 1
#define SW_ANGLE_PI (180L << 16)
#define SW_ANGLE_2PI (SW_ANGLE_PI << 1)
#define SW_ANGLE_PI2 (SW_ANGLE_PI >> 1)
#define SW_ANGLE_PI4 (SW_ANGLE_PI >> 2)

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

    bool zero()
    {
        if (x == 0 && y == 0) return true;
        else return false;
    }

    bool small()
    {
        //2 is epsilon...
        if (abs(x) < 2 && abs(y) < 2) return true;
        else return false;
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
    SwFixed angleIn;
    SwFixed angleOut;
    SwPoint center;
    SwFixed lineLength;
    SwFixed subPathAngle;
    SwPoint ptStartSubPath;
    SwFixed subPathLineLength;
    SwFixed width;

    StrokeCap cap;
    StrokeJoin join;
    StrokeJoin joinSaved;

    SwStrokeBorder borders[2];

    bool firstPt;
    bool openSubPath;
    bool handleWideStrokes;
};

struct SwDashStroke
{
    SwOutline* outline;
    int32_t curLen;
    int32_t curIdx;
    Point ptStart;
    Point ptCur;
    float* pattern;
    uint32_t cnt;
    bool curOpGap;
};

struct SwFill
{
    struct SwLinear {
        float dx, dy;
        float len;
        float offset;
    };

    struct SwRadial {
        float cx, cy;
        float a;
        float inv2a;
    };

    union {
        SwLinear linear;
        SwRadial radial;
    };

    uint32_t* ctable;
    FillSpread spread;
    bool translucent;
};

struct SwShape
{
    SwOutline*   outline;
    SwStroke*    stroke;
    SwFill*      fill;
    SwRleData*   rle;
    SwRleData*   strokeRle;
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


static inline uint32_t COLOR_ALPHA(uint32_t rgba)
{
  return (rgba >> 24) & 0xff;
}


static inline uint32_t COLOR_ALPHA_BLEND(uint32_t rgba, uint32_t alpha)
{
  return (((((rgba >> 8) & 0x00ff00ff) * alpha) & 0xff00ff00) +
          ((((rgba & 0x00ff00ff) * alpha) >> 8) & 0x00ff00ff));
}


static inline uint32_t COLOR_INTERPOLATE(uint32_t rgba1, uint32_t a, uint32_t rgba2, uint32_t b)
{
   auto t = (((rgba1 & 0xff00ff) * a + (rgba2 & 0xff00ff) * b) >> 8) & 0xff00ff;
   rgba1 = (((rgba1 >> 8) & 0xff00ff) * a + ((rgba2 >> 8) & 0xff00ff) * b) & 0xff00ff00;
   return (rgba1 |= t);
}


static inline uint32_t COLOR_ARGB_JOIN(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
  return (a << 24 | r << 16 | g << 8 | b);
}


static inline void COLOR_SET(uint32_t *dst, uint32_t val, uint32_t len)
{
    while (len--) *dst++ = val;
}


int64_t mathMultiply(int64_t a, int64_t b);
int64_t mathDivide(int64_t a, int64_t b);
int64_t mathMulDiv(int64_t a, int64_t b, int64_t c);
void mathRotate(SwPoint& pt, SwFixed angle);
SwFixed mathTan(SwFixed angle);
SwFixed mathAtan(const SwPoint& pt);
SwFixed mathCos(SwFixed angle);
SwFixed mathSin(SwFixed angle);
void mathSplitCubic(SwPoint* base);
SwFixed mathDiff(SwFixed angle1, SwFixed angle2);
SwFixed mathLength(SwPoint& pt);
bool mathSmallCubic(SwPoint* base, SwFixed& angleIn, SwFixed& angleMid, SwFixed& angleOut);
SwFixed mathMean(SwFixed angle1, SwFixed angle2);

void shapeReset(SwShape& shape);
bool shapeGenOutline(SwShape& shape, const Shape& sdata);
bool shapeGenRle(SwShape& shape, const Shape& sdata, const SwSize& clip, const RenderTransform* transform);
void shapeDelOutline(SwShape& shape);
void shapeResetStroke(SwShape& shape, const Shape& sdata);
bool shapeGenStrokeRle(SwShape& shape, const Shape& sdata, const SwSize& clip);
void shapeFree(SwShape* shape);
void shapeDelStroke(SwShape& shape);
bool shapeGenFillColors(SwShape& shape, const Fill* fill, const RenderTransform* transform, bool ctable);
void shapeResetFill(SwShape& shape, const Fill* fill);
void shapeDelFill(SwShape& shape);

void strokeReset(SwStroke& stroke, const Shape& shape);
bool strokeParseOutline(SwStroke& stroke, const SwOutline& outline);
SwOutline* strokeExportOutline(SwStroke& stroke);
void strokeFree(SwStroke* stroke);

bool fillGenColorTable(SwFill* fill, const Fill* fdata, const RenderTransform* transform, bool ctable);
void fillReset(SwFill* fill, const Fill* fdata);
void fillFree(SwFill* fill);
void fillFetchLinear(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len);
void fillFetchRadial(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len);

SwRleData* rleRender(const SwOutline* outline, const SwBBox& bbox, const SwSize& clip);
void rleFree(SwRleData* rle);

bool rasterGradientShape(Surface& surface, SwShape& shape, unsigned id);
bool rasterSolidShape(Surface& surface, SwShape& shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
bool rasterStroke(Surface& surface, SwShape& shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
bool rasterClear(Surface& surface);

#endif /* _TVG_SW_COMMON_H_ */
