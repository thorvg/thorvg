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

static void _growOutlineContour(SwOutline& outline, uint32_t n)
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
    outline.cntrs = static_cast<uint32_t*>(realloc(outline.cntrs, n * sizeof(uint32_t)));
    assert(outline.cntrs);
}


static void _growOutlinePoint(SwOutline& outline, uint32_t n)
{
    if (n == 0) {
        free(outline.pts);
        outline.pts = nullptr;
        free(outline.types);
        outline.types = nullptr;
        outline.reservedPtsCnt = 0;
        outline.ptsCnt = 0;
        return;
    }

    if (outline.reservedPtsCnt >= outline.ptsCnt + n) return;

    //cout << "Grow Pts: " << outline.reservedPtsCnt << " -> " << outline.ptsCnt + n << endl;
    outline.reservedPtsCnt = n;
    outline.pts = static_cast<SwPoint*>(realloc(outline.pts, n * sizeof(SwPoint)));
    assert(outline.pts);
    outline.types = static_cast<uint8_t*>(realloc(outline.types, n * sizeof(uint8_t)));
    assert(outline.types);
}


static void _freeOutline(SwOutline* outline)
{
    if (!outline) return;

    if (outline->cntrs) free(outline->cntrs);
    if (outline->pts) free(outline->pts);
    if (outline->types) free(outline->types);
    free(outline);
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
    outline.types[outline.ptsCnt] = SW_CURVE_TYPE_POINT;

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
    outline.types[outline.ptsCnt] = SW_CURVE_TYPE_POINT;

    ++outline.ptsCnt;
}


static void _outlineCubicTo(SwOutline& outline, const Point* ctrl1, const Point* ctrl2, const Point* to)
{
    assert(ctrl1 && ctrl2 && to);

    _growOutlinePoint(outline, 3);

    outline.pts[outline.ptsCnt] = TO_SWPOINT(ctrl1);
    outline.types[outline.ptsCnt] = SW_CURVE_TYPE_CUBIC;
    ++outline.ptsCnt;

    outline.pts[outline.ptsCnt] = TO_SWPOINT(ctrl2);
    outline.types[outline.ptsCnt] = SW_CURVE_TYPE_CUBIC;
    ++outline.ptsCnt;

    outline.pts[outline.ptsCnt] = TO_SWPOINT(to);
    outline.types[outline.ptsCnt] = SW_CURVE_TYPE_POINT;
    ++outline.ptsCnt;
}


static void _outlineClose(SwOutline& outline)
{
    uint32_t i = 0;

    if (outline.cntrsCnt > 0) {
        i = outline.cntrs[outline.cntrsCnt - 1] + 1;
    } else {
        i = 0;   //First Path
    }

    //Make sure there is at least one point in the current path
    if (outline.ptsCnt == i) {
        outline.opened = true;
        return;
    }

    //Close the path
    _growOutlinePoint(outline, 1);

    outline.pts[outline.ptsCnt] = outline.pts[i];
    outline.types[outline.ptsCnt] = SW_CURVE_TYPE_POINT;
    ++outline.ptsCnt;

    outline.opened = false;
}


static void _initBBox(SwBBox& bbox)
{
    bbox.min.x = bbox.min.y = 0;
    bbox.max.x = bbox.max.y = 0;
}


static bool _updateBBox(SwOutline* outline, SwBBox& bbox)
{
    if (!outline) return false;

    auto pt = outline->pts;
    assert(pt);

    if (outline->ptsCnt <= 0) {
        _initBBox(bbox);
        return false;
    }

    auto xMin = pt->x;
    auto xMax = pt->x;
    auto yMin = pt->y;
    auto yMax = pt->y;

    ++pt;

    for(uint32_t i = 1; i < outline->ptsCnt; ++i, ++pt) {
        assert(pt);
        if (xMin > pt->x) xMin = pt->x;
        if (xMax < pt->x) xMax = pt->x;
        if (yMin > pt->y) yMin = pt->y;
        if (yMax < pt->y) yMax = pt->y;
    }
    bbox.min.x = xMin >> 6;
    bbox.max.x = (xMax + 63) >> 6;
    bbox.min.y = yMin >> 6;
    bbox.max.y = (yMax + 63) >> 6;

    if (xMax - xMin < 1 || yMax - yMin < 1) return false;

    return true;
}


static bool _checkValid(SwShape& sdata, const SwSize& clip)
{
    assert(sdata.outline);

    if (sdata.outline->ptsCnt == 0 || sdata.outline->cntrsCnt <= 0) return false;

    //Check boundary
    if ((sdata.bbox.min.x > clip.w || sdata.bbox.min.y > clip.h) ||
        (sdata.bbox.min.x + sdata.bbox.max.x < 0) ||
        (sdata.bbox.min.y + sdata.bbox.max.y < 0)) return false;

    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void shapeTransformOutline(const Shape& shape, SwShape& sdata, const RenderTransform& transform)
{
    auto outline = sdata.outline;
    assert(outline);

    for(uint32_t i = 0; i < outline->ptsCnt; ++i) {
        auto dx = static_cast<float>(outline->pts[i].x >> 6);
        auto dy = static_cast<float>(outline->pts[i].y >> 6);
        auto tx = dx * transform.e11 + dy * transform.e12 + transform.e13;
        auto ty = dx * transform.e21 + dy * transform.e22 + transform.e23;
        auto pt = Point{tx + transform.e31, ty + transform.e32};
        outline->pts[i] = TO_SWPOINT(&pt);
    }
}


bool shapeGenRle(const Shape& shape, SwShape& sdata, const SwSize& clip)
{
    if (!_updateBBox(sdata.outline, sdata.bbox)) goto end;
    if (!_checkValid(sdata, clip)) goto end;

    sdata.rle = rleRender(sdata.outline, sdata.bbox, clip);

end:
    if (sdata.rle) return true;
    return false;
}


void shapeDelOutline(SwShape& sdata)
{
    auto outline = sdata.outline;
    _freeOutline(outline);
    sdata.outline = nullptr;
}


void shapeReset(SwShape& sdata)
{
    shapeDelOutline(sdata);
    rleFree(sdata.rle);
    sdata.rle = nullptr;
    _initBBox(sdata.bbox);
}


bool shapeGenOutline(const Shape& shape, SwShape& sdata)
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

    for (uint32_t i = 0; i < cmdCnt; ++i) {
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

    auto outline = sdata.outline;
    if (!outline) outline = static_cast<SwOutline*>(calloc(1, sizeof(SwOutline)));
    assert(outline);
    outline->opened = true;

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


void shapeFree(SwShape* sdata)
{
    assert(sdata);

    shapeDelOutline(*sdata);
    rleFree(sdata->rle);

    if (sdata->stroke) {
        rleFree(sdata->strokeRle);
        strokeFree(sdata->stroke);
    }

    free(sdata);
}


void shapeResetStroke(const Shape& shape, SwShape& sdata)
{
    if (!sdata.stroke) sdata.stroke = static_cast<SwStroke*>(calloc(1, sizeof(SwStroke)));
    auto stroke = sdata.stroke;
    assert(stroke);
    strokeReset(*stroke, shape.strokeWidth(), shape.strokeCap(), shape.strokeJoin());
    rleFree(sdata.strokeRle);
    sdata.strokeRle = nullptr;
}


bool shapeGenStrokeRle(const Shape& shape, SwShape& sdata, const SwSize& clip)
{
    if (!sdata.outline) {
        if (!shapeGenOutline(shape, sdata)) return false;
    }

    if (!_checkValid(sdata, clip)) return false;

    if (!strokeParseOutline(*sdata.stroke, *sdata.outline)) return false;

    auto outline = strokeExportOutline(*sdata.stroke);
    if (!outline) return false;

    SwBBox bbox;
    _updateBBox(outline, bbox);

    sdata.strokeRle = rleRender(outline, bbox, clip);

    _freeOutline(outline);

    return true;
}

#endif /* _TVG_SW_SHAPE_H_ */
