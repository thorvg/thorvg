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

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static void growOutlineContour(SwOutline& outline, size_t n)
{
    if (n == 0) {
        free(outline.cntrs);
        outline.cntrs = nullptr;
        outline.cntrsCnt = 0;
        outline.reservedCntrsCnt = 0;
        return;
    }
    if (outline.reservedCntrsCnt >= outline.cntrsCnt + n) return;

    cout << "Grow Cntrs: " << outline.reservedCntrsCnt << " -> " << outline.cntrsCnt + n << endl;;
    outline.reservedCntrsCnt = n;
    outline.cntrs = static_cast<size_t*>(realloc(outline.cntrs, n * sizeof(size_t)));
    assert(outline.cntrs);
}


static void growOutlinePoint(SwOutline& outline, size_t n)
{
    if (n == 0) {
        free(outline.pts);
        outline.pts = nullptr;
        free(outline.tags);
        outline.tags = nullptr;
        outline.reservedPtsCnt = 0;
        outline.ptsCnt = 0;
        return;
    }

    if (outline.reservedPtsCnt >= outline.ptsCnt + n) return;

    cout << "Grow Pts: " << outline.reservedPtsCnt << " -> " << outline.ptsCnt + n << endl;
    outline.reservedPtsCnt = n;
    outline.pts = static_cast<Point*>(realloc(outline.pts, n * sizeof(Point)));
    assert(outline.pts);
    outline.tags = static_cast<char*>(realloc(outline.tags, n * sizeof(char)));
    assert(outline.tags);
}


static void outlineEnd(SwOutline& outline)
{
    growOutlineContour(outline, 1);
    if (outline.ptsCnt > 0) {
        outline.cntrs[outline.cntrsCnt] = outline.ptsCnt - 1;
        ++outline.cntrs;
    }
}


static void outlineMoveTo(SwOutline& outline, const Point* pt)
{
    assert(pt);

    growOutlinePoint(outline, 1);

    outline.pts[outline.ptsCnt] = *pt;
    outline.tags[outline.ptsCnt] = SW_CURVE_TAG_ON;

    if (outline.ptsCnt > 0) {
        growOutlineContour(outline, 1);
        outline.cntrs[outline.cntrsCnt] = outline.ptsCnt - 1;
        ++outline.cntrsCnt;
    }

    ++outline.ptsCnt;
}


static void outlineLineTo(SwOutline& outline, const Point* pt)
{
    assert(pt);

    growOutlinePoint(outline, 1);

    outline.pts[outline.ptsCnt] = *pt;
    outline.tags[outline.ptsCnt] = SW_CURVE_TAG_ON;

    ++outline.ptsCnt;
}


static void outlineCubicTo(SwOutline& outline, const Point* ctrl1, const Point* ctrl2, const Point* pt)
{
    assert(ctrl1 && ctrl2 && pt);

    growOutlinePoint(outline, 3);

    outline.pts[outline.ptsCnt] = *ctrl1;
    outline.tags[outline.ptsCnt] = SW_CURVE_TAG_CUBIC;
    ++outline.ptsCnt;

    outline.pts[outline.ptsCnt] = *ctrl2;
    outline.tags[outline.ptsCnt] = SW_CURVE_TAG_CUBIC;
    ++outline.ptsCnt;

    outline.pts[outline.ptsCnt] = *ctrl1;
    outline.tags[outline.ptsCnt] = SW_CURVE_TAG_ON;
    ++outline.ptsCnt;
}


static bool outlineClose(SwOutline& outline)
{
    size_t i = 0;

    if (outline.cntrsCnt > 0) {
        i = outline.cntrs[outline.cntrsCnt - 1] + 1;
    } else {
        i = 0;   //First Path
    }

    //Make sure there is at least one point in the current path
    if (outline.ptsCnt == i) return false;

    //Close the path
    growOutlinePoint(outline, 1);

    outline.pts[outline.ptsCnt] = outline.pts[i];
    outline.tags[outline.ptsCnt] = SW_CURVE_TAG_ON;
    ++outline.ptsCnt;

    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool shapeGenRle(const ShapeNode& shape, SwShape& sdata)
{
    //TODO: rle

    return true;
}


bool shapeUpdateBBox(const ShapeNode& shape, SwShape& sdata)
{
    //TODO:
    return true;
}


void shapeDelOutline(const ShapeNode& shape, SwShape& sdata)
{
    if (!sdata.outline) return;

    SwOutline* outline = sdata.outline;
    if (outline->cntrs) free(outline->cntrs);
    if (outline->pts) free(outline->pts);
    if (outline->tags) free(outline->tags);
    free(outline);

    sdata.outline = nullptr;
}


bool shapeGenOutline(const ShapeNode& shape, SwShape& sdata)
{
    const PathCommand* cmds = nullptr;
    auto cmdCnt = shape.pathCommands(&cmds);

    const Point* pts = nullptr;
    auto ptsCnt = shape.pathCoords(&pts);

    //No actual shape data
    if (cmdCnt == 0 || ptsCnt == 0) return false;

    //smart reservation
    auto outlinePtsCnt = 0;
    auto outlineCntrsCnt = 0;
//    auto closed = false;

    for (auto i = 0; i < cmdCnt; ++i) {
        switch(*(cmds + i)) {
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

    ++outlinePtsCnt;    //for close
    ++outlineCntrsCnt;  //for end

    SwOutline* outline = sdata.outline;

    if (!outline) {
        outline = static_cast<SwOutline*>(calloc(1, sizeof(SwOutline)));
        assert(outline);
    } else {
        cout << "Outline was already allocated? How?" << endl;
    }

    growOutlinePoint(*outline, outlinePtsCnt);
    growOutlineContour(*outline, outlineCntrsCnt);

    //Generate Outlines
    while (cmdCnt-- > 0) {
        switch(*cmds) {
            case PathCommand::Close: {
                outlineClose(*outline);
                break;
            }
            case PathCommand::MoveTo: {
                outlineMoveTo(*outline, pts);
                ++pts;
                break;
            }
            case PathCommand::LineTo: {
                outlineLineTo(*outline, pts);
                ++pts;
                break;
            }
            case PathCommand::CubicTo: {
                outlineCubicTo(*outline, pts, pts + 1, pts + 2);
                pts += 3;
                break;
            }
        }
        ++cmds;
    }

    outlineEnd(*outline);

    sdata.outline = outline;

    return true;
}


#endif /* _TVG_SW_SHAPE_H_ */
