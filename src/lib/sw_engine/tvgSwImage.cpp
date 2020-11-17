/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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
#include <math.h>
#include "tvgSwCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/


static SwPoint _transform(const Point* to, const Matrix* transform)
{
    if (!transform) return {TO_SWCOORD(to->x), TO_SWCOORD(to->y)};

    auto tx = round(to->x * transform->e11 + to->y * transform->e12 + transform->e13);
    auto ty = round(to->x * transform->e21 + to->y * transform->e22 + transform->e23);

    return {TO_SWCOORD(tx), TO_SWCOORD(ty)};
}


static void _delOutline(SwOutline* outline)
{
    if (!outline) return;

    if (outline->cntrs) free(outline->cntrs);
    if (outline->pts) free(outline->pts);
    if (outline->types) free(outline->types);
    free(outline);
}


static void _initBBox(SwBBox& bbox)
{
    bbox.min.x = bbox.min.y = 0;
    bbox.max.x = bbox.max.y = 0;
}


static bool _updateBBox(SwOutline* outline, SwBBox& bbox, const SwSize& clip)
{
    if (!outline) return false;

    auto pt = outline->pts;

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
        if (xMin > pt->x) xMin = pt->x;
        if (xMax < pt->x) xMax = pt->x;
        if (yMin > pt->y) yMin = pt->y;
        if (yMax < pt->y) yMax = pt->y;
    }
    bbox.min.x = xMin >> 6;
    bbox.max.x = (xMax + 63) >> 6;
    bbox.min.y = yMin >> 6;
    bbox.max.y = (yMax + 63) >> 6;

    bbox.min.x = max(bbox.min.x, TO_SWCOORD(0));
    bbox.min.y = max(bbox.min.y, TO_SWCOORD(0));
    bbox.max.x = min(bbox.max.x, clip.w);
    bbox.max.y = min(bbox.max.y, clip.h);

    if (xMax - xMin < 1 && yMax - yMin < 1) return false;

    return true;
}


static bool _checkValid(const SwOutline* outline, const SwBBox& bbox, const SwSize& clip)
{
    if (outline->ptsCnt == 0 || outline->cntrsCnt <= 0) return false;

    //Check boundary
    if (bbox.min.x >= clip.w || bbox.min.y >= clip.h || bbox.max.x <= 0 || bbox.max.y <= 0) return false;

    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


bool imagePrepare(SwImage* image, const Picture* pdata, const SwSize& clip, const Matrix* transform)
{
    if (!imageGenOutline(image, pdata, transform)) return false;

    if (!_updateBBox(image->outline, image->bbox, clip))  return false;

    if (!_checkValid(image->outline, image->bbox, clip)) return false;

    return true;
}


bool imagePrepared(SwImage* image)
{
    return image->rle ? true : false;
}


bool imageGenRle(SwImage* image, TVG_UNUSED const Picture* pdata, const SwSize& clip, bool antiAlias, bool hasComposite)
{
    if ((image->rle = rleRender(image->rle, image->outline, image->bbox, clip, antiAlias))) return true;

    return false;
}


void imageDelOutline(SwImage* image)
{
    auto outline = image->outline;
    _delOutline(outline);
    image->outline = nullptr;
}


void imageReset(SwImage* image)
{
    imageDelOutline(image);
    rleFree(image->rle);
    image->rle = nullptr;
    _initBBox(image->bbox);
}


bool imageGenOutline(SwImage* image, const Picture* pdata, const Matrix* transform)
{
    auto outline = image->outline;
    if (!outline) outline = static_cast<SwOutline*>(calloc(1, sizeof(SwOutline)));

    float w, h;
    pdata->viewbox(nullptr, nullptr, &w, &h);
    if (w == 0 || h == 0) return false;

    outline->reservedPtsCnt = 5;
    outline->pts = static_cast<SwPoint*>(realloc(outline->pts, outline->reservedPtsCnt * sizeof(SwPoint)));
    outline->types = static_cast<uint8_t*>(realloc(outline->types, outline->reservedPtsCnt * sizeof(uint8_t)));

    outline->reservedCntrsCnt = 1;
    outline->cntrs = static_cast<uint32_t*>(realloc(outline->cntrs, outline->reservedCntrsCnt * sizeof(uint32_t)));

    Point to[4] = {{0 ,0}, {w, 0}, {w, h}, {0, h}};
    for (int i = 0; i < 4; i++) {
        outline->pts[outline->ptsCnt] = _transform(&to[i], transform);
        outline->types[outline->ptsCnt] = SW_CURVE_TYPE_POINT;
        ++outline->ptsCnt;
    }

    outline->pts[outline->ptsCnt] = outline->pts[0];
    outline->types[outline->ptsCnt] = SW_CURVE_TYPE_POINT;
    ++outline->ptsCnt;

    outline->cntrs[outline->cntrsCnt] = outline->ptsCnt - 1;
    ++outline->cntrsCnt;

    outline->opened = false;
    image->outline = outline;

    image->width = w;
    image->height = h;
    return true;
}

void imageFree(SwImage* image)
{
    imageDelOutline(image);
    rleFree(image->rle);
}
