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

static inline SwPoint TO_SWPOINT(const Point* pt)
{
    return {SwCoord(pt->x * 64), SwCoord(pt->y * 64)};
}


static void _growOutlineContour(SwOutline& outline, size_t n)
{
    if (n == 0) {
        free(outline.cntrs);
        outline.cntrs = nullptr;
        outline.cntrsCnt = 0;
        outline.reservedCntrsCnt = 0;
        return;
    }
    if (outline.reservedCntrsCnt >= outline.cntrsCnt + n) return;

    //cout << "Grow Cntrs: " << outline.reservedCntrsCnt << " -> " << outline.cntrsCnt + n << endl;;
    outline.reservedCntrsCnt = n;
    outline.cntrs = static_cast<size_t*>(realloc(outline.cntrs, n * sizeof(size_t)));
    assert(outline.cntrs);
}


static void _growOutlinePoint(SwOutline& outline, size_t n)
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

    //cout << "Grow Pts: " << outline.reservedPtsCnt << " -> " << outline.ptsCnt + n << endl;
    outline.reservedPtsCnt = n;
    outline.pts = static_cast<SwPoint*>(realloc(outline.pts, n * sizeof(SwPoint)));
    assert(outline.pts);
    outline.tags = static_cast<char*>(realloc(outline.tags, n * sizeof(char)));
    assert(outline.tags);
}


static void _outlineEnd(SwOutline& outline)
{
    _growOutlineContour(outline, 1);
    if (outline.ptsCnt > 0) {
        outline.cntrs[outline.cntrsCnt] = outline.ptsCnt - 1;
        ++outline.cntrsCnt;
    }
}


static void _outlineMoveTo(SwOutline& outline, const Point* to)
{
    assert(to);

    _growOutlinePoint(outline, 1);

    outline.pts[outline.ptsCnt] = TO_SWPOINT(to);
    outline.tags[outline.ptsCnt] = SW_CURVE_TAG_ON;

    if (outline.ptsCnt > 0) {
        _growOutlineContour(outline, 1);
        outline.cntrs[outline.cntrsCnt] = outline.ptsCnt - 1;
        ++outline.cntrsCnt;
    }

    ++outline.ptsCnt;
}


static void _outlineLineTo(SwOutline& outline, const Point* to)
{
    assert(to);

    _growOutlinePoint(outline, 1);

    outline.pts[outline.ptsCnt] = TO_SWPOINT(to);
    outline.tags[outline.ptsCnt] = SW_CURVE_TAG_ON;

    ++outline.ptsCnt;
}


static void _outlineCubicTo(SwOutline& outline, const Point* ctrl1, const Point* ctrl2, const Point* to)
{
    assert(ctrl1 && ctrl2 && to);

    _growOutlinePoint(outline, 3);

    outline.pts[outline.ptsCnt] = TO_SWPOINT(ctrl1);
    outline.tags[outline.ptsCnt] = SW_CURVE_TAG_CUBIC;
    ++outline.ptsCnt;

    outline.pts[outline.ptsCnt] = TO_SWPOINT(ctrl2);
    outline.tags[outline.ptsCnt] = SW_CURVE_TAG_CUBIC;
    ++outline.ptsCnt;

    outline.pts[outline.ptsCnt] = TO_SWPOINT(to);
    outline.tags[outline.ptsCnt] = SW_CURVE_TAG_ON;
    ++outline.ptsCnt;
}


static bool _outlineClose(SwOutline& outline)
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
    _growOutlinePoint(outline, 1);

    outline.pts[outline.ptsCnt] = outline.pts[i];
    outline.tags[outline.ptsCnt] = SW_CURVE_TAG_ON;
    ++outline.ptsCnt;

    return true;
}


static void _initBBox(SwShape& sdata)
{
    sdata.bbox.min.x = sdata.bbox.min.y = 0;
    sdata.bbox.max.x = sdata.bbox.max.y = 0;
}


static bool _updateBBox(SwShape& sdata)
{
    auto outline = sdata.outline;
    assert(outline);

    auto pt = outline->pts;
    assert(pt);

    if (outline->ptsCnt <= 0) {
        _initBBox(sdata);
        return false;
    }

    auto xMin = pt->x;
    auto xMax = pt->x;
    auto yMin = pt->y;
    auto yMax = pt->y;

    ++pt;

    for(size_t i = 1; i < outline->ptsCnt; ++i, ++pt) {
        assert(pt);
        if (xMin > pt->x) xMin = pt->x;
        if (xMax < pt->x) xMax = pt->x;
        if (yMin > pt->y) yMin = pt->y;
        if (yMax < pt->y) yMax = pt->y;
    }
    sdata.bbox.min.x = xMin >> 6;
    sdata.bbox.max.x = (xMax + 63) >> 6;
    sdata.bbox.min.y = yMin >> 6;
    sdata.bbox.max.y = (yMax + 63) >> 6;

    if (xMax - xMin < 1 || yMax - yMin < 1) return false;

    return true;
}


void _deleteRle(SwShape& sdata)
{
    if (sdata.rle) {
        if (sdata.rle->spans) free(sdata.rle->spans);
        free(sdata.rle);
    }
    sdata.rle = nullptr;
}


void _deleteOutline(SwShape& sdata)
{
    if (!sdata.outline) return;

    SwOutline* outline = sdata.outline;
    if (outline->cntrs) free(outline->cntrs);
    if (outline->pts) free(outline->pts);
    if (outline->tags) free(outline->tags);
    free(outline);

    sdata.outline = nullptr;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool shapeTransformOutline(const ShapeNode& shape, SwShape& sdata)
{
    //TODO:
    return true;
}


bool shapeGenRle(const ShapeNode& shape, SwShape& sdata)
{
    if (!_updateBBox(sdata)) goto end;
    sdata.rle = rleRender(sdata);
    _deleteOutline(sdata);
end:
    if (sdata.rle) return true;
    return false;
}


void shapeReset(SwShape& sdata)
{
    _deleteOutline(sdata);
    _deleteRle(sdata);
    _initBBox(sdata);
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

    //TODO: Probabry we can copy pts from shape directly.
    _growOutlinePoint(*outline, outlinePtsCnt);
    _growOutlineContour(*outline, outlineCntrsCnt);

    //Generate Outlines
    while (cmdCnt-- > 0) {
        switch(*cmds) {
            case PathCommand::Close: {
                _outlineClose(*outline);
                break;
            }
            case PathCommand::MoveTo: {
                _outlineMoveTo(*outline, pts);
                ++pts;
                break;
            }
            case PathCommand::LineTo: {
                _outlineLineTo(*outline, pts);
                ++pts;
                break;
            }
            case PathCommand::CubicTo: {
                _outlineCubicTo(*outline, pts, pts + 1, pts + 2);
                pts += 3;
                break;
            }
        }
        ++cmds;
    }

    _outlineEnd(*outline);

    //FIXME:
    //outline->flags = SwOutline::FillRule::Winding;

    sdata.outline = outline;

    return true;
}


#endif /* _TVG_SW_SHAPE_H_ */
