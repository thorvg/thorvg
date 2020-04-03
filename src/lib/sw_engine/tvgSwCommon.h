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

constexpr auto SW_CURVE_TAG_ON = 1;
constexpr auto SW_CURVE_TAG_CUBIC = 2;

struct SwOutline
{
  size_t*     cntrs;            //the contour end points
  size_t      cntrsCnt;         //number of contours in glyph
  size_t      reservedCntrsCnt;
  Point*      pts;              //the outline's points
  size_t      ptsCnt;           //number of points in the glyph
  size_t      reservedPtsCnt;
  char*       tags;             //the points flags
  size_t      flags;            //outline masks
};

struct SwShape
{
//    SwRleRaster raster;
    SwOutline*   outline;
};

bool shapeGenOutline(const ShapeNode& shape, SwShape& sdata);
void shapeDelOutline(const ShapeNode& shape, SwShape& sdata);
bool shapeGenRle(const ShapeNode& shape, SwShape& sdata);
bool shapeUpdateBBox(const ShapeNode& shape, SwShape& sdata);

#endif /* _TVG_SW_COMMON_H_ */
