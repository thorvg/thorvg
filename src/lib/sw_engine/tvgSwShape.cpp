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
#ifndef _TVG_SW_SHAPE_H_
#define _TVG_SW_SHAPE_H_

#include "tvgSwCommon.h"

static void growOutlineContour(SwShape *sdata, size_t n)
{
    sdata->outline.cntrsCnt = n;

    if (n == 0) {
        free(sdata->outline.cntrs);
        sdata->outline.cntrs = nullptr;
        return;
    }
    sdata->outline.cntrs = static_cast<short *>(realloc(sdata->outline.cntrs, n * sizeof(short)));
    assert(sdata->outline.cntrs);
}


static void growOutlinePoint(SwShape *sdata, size_t n)
{
    sdata->outline.ptsCnt = n;

    if (n == 0) {
        free(sdata->outline.pts);
        sdata->outline.pts = nullptr;
        free(sdata->outline.tags);
        sdata->outline.tags = nullptr;
        return;
    }

    sdata->outline.pts = static_cast<SwVector *>(realloc(sdata->outline.pts, n * sizeof(SwVector)));
    assert(sdata->outline.pts);
    sdata->outline.tags = static_cast<char*>(realloc(sdata->outline.tags, n * sizeof(char)));
    assert(sdata->outline.tags);
}


static void outlineEnd(SwShape* sdata)
{
    //grow contour 1
}


static void outlineMoveTo(SwShape* sdata, const Point* pt)
{
    //grow pts 1,
    //grow contour 1
}


static void outlineLineTo(SwShape* sdata, const Point* pt)
{
    //grow pts 1
}


static void outlineCubicTo(SwShape* sdata, const Point* ctrl1, const Point* ctrl2, const Point* pt)
{
    //grow pts 3
}


static void outlineClose(SwShape* sdata)
{
    //grow pts 1
}


bool shapeGenOutline(ShapeNode *shape, SwShape* sdata)
{
    bool closed = false;

    const PathCommand* cmds = nullptr;
    auto cmdCnt = shape->pathCommands(&cmds);

    const Point* pts = nullptr;
    shape->pathCoords(&pts);

    //reservation
    auto outlinePtsCnt = 0;
    auto outlineCntrsCnt = 0;

    for (auto i = 0; i < cmdCnt; ++i) {
        switch(*cmds) {
            case PathCommand::Close: {
                ++outlinePtsCnt;
                break;
            }
            case PathCommand::MoveTo: {
                ++outlineCntrsCnt;
                ++outlinePtsCnt;
                break;
            }
            case PathCommand::LineTo: {
                ++outlinePtsCnt;
                break;
            }
            case PathCommand::CubicTo: {
                outlinePtsCnt += 3;
                break;
            }
        }
    }

    ++outlinePtsCnt;  //for close

    growOutlinePoint(sdata, outlinePtsCnt);
    growOutlineContour(sdata, outlineCntrsCnt);

    //Generate Outlines
    while (cmdCnt-- > 0) {
        switch(*cmds) {
            case PathCommand::Close: {
                outlineClose(sdata);
                break;
            }
            case PathCommand::MoveTo: {
                outlineMoveTo(sdata, pts);
                ++pts;
                break;
            }
            case PathCommand::LineTo: {
                outlineLineTo(sdata, pts);
                ++pts;
                break;
            }
            case PathCommand::CubicTo: {
                outlineCubicTo(sdata, pts, pts + 1, pts + 2);
                pts += 3;
                break;
            }
        }
        ++cmds;
    }

    outlineEnd(sdata);

    return closed;
}


#endif /* _TVG_SW_SHAPE_H_ */
