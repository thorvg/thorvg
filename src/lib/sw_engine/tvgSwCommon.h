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
using SwPos = signed long;

struct SwVector
{
  SwPos  x;
  SwPos  y;
};

struct SwOutline
{
  short*      cntrs;           /* the contour end points             */
  short       cntrsCnt;        /* number of contours in glyph        */
  SwVector*   pts;             /* the outline's points               */
  short       ptsCnt;          /* number of points in the glyph      */
  char*       tags;            /* the points flags                   */
  int         flags;           /* outline masks                      */
};

struct SwShape
{
    SwOutline outline;
};

bool shapeGenOutline(ShapeNode *shape, SwShape* sdata);

#endif /* _TVG_SW_COMMON_H_ */
