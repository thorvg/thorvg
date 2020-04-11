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
#ifndef _TVG_SW_RLE_H_
#define _TVG_SW_RLE_H_

#include <setjmp.h>
#include "tvgSwCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
constexpr auto MAX_SPANS = 256;

using SwPtrDist = ptrdiff_t;
using TPos = long;
using TCoord = long;
using TArea = int;

struct TBand
{
    TPos min, max;
};

struct TCell
{
    TPos x;
    TCoord cover;
    TArea area;
    TCell *next;
};

struct RleWorker
{
    TCoord ex, ey;
    TPos exMin, exMax;
    TPos eyMin, eyMax;
    TPos exCnt, eyCnt;

    TArea area;
    TCoord cover;

    TCell* cells;
    SwPtrDist maxCells;
    SwPtrDist numCells;

    TPos x, y;

    Point bezStack[32 * 3 + 1];
    int   levStack[32];

    SwOutline* outline;
    //SwBBox clipBox;

    SwSpan spans[MAX_SPANS];
    int spansCnt;

    //render_span
    //render_span_data;
    int ySpan;

    int bandSize;
    int bandShoot;

    jmp_buf jmpBuf;

    void* buffer;
    long bufferSize;

    TCell** yCells;
    TPos   yCnt;

    bool invalid;
};

static bool rleSweep(RleWorker& rw)
{
    //TODO:
    return true;
}

static bool moveTo(SwPoint& pt)
{
    printf("moveTo = %f %f\n", pt.x, pt.y);
    return true;
}


static bool lineTo(SwPoint& pt)
{
    printf("lineTo = %f %f\n", pt.x, pt.y);
    return true;
}


static bool cubicTo(SwPoint& ctrl1, SwPoint& ctrl2, SwPoint& pt)
{
    printf("cubicTo = ctrl1(%f %f) ctrl2(%f %f) pt(%f %f)\n", ctrl1.x, ctrl1.y, ctrl2.x, ctrl2.y, pt.x, pt.y);
    return true;
}


static bool decomposeOutline(RleWorker& rw)
{
    printf("decomposOutline\n");
    auto outline = rw.outline;
    assert(outline);

    auto first = 0;  //index of first point in contour

    for (size_t n = 0; n < outline->cntrsCnt; ++n) {
        auto last = outline->cntrs[n];
        if (last < 0) goto invalid_outline;

        auto limit = outline->pts + last;
        assert(limit);

        auto pt = outline->pts + first;
        auto tags = outline->tags + first;

        /* A contour cannot start with a cubic control point! */
        if (tags[0] == SW_CURVE_TAG_CUBIC) goto invalid_outline;

        if (!moveTo(outline->pts[first])) return false;

        while (pt < limit) {
            assert(++pt);
            assert(++tags);

            //emit a single line_to
            if (tags[0] == SW_CURVE_TAG_ON) {
                if (!lineTo(*pt)) return false;
                continue;
            //tag cubic
            } else {
                if (pt + 1 > limit || tags[1] != SW_CURVE_TAG_CUBIC)
                    goto invalid_outline;

                pt += 2;
                tags += 2;

                if (pt <= limit) {
                    if (!cubicTo(pt[-2], pt[-1], pt[0])) return false;
                    continue;
                }
                if (!cubicTo(pt[-2], pt[-1], outline->pts[first])) return false;
                goto close;
            }
        }

        //Close the contour with a line segment?
        //if (!lineTo(outline->pts[first]));
    close:
       first = last + 1;
    }

    return true;

invalid_outline:
    cout << "Invalid Outline!" << endl;
    return false;
}


static TCell* findCell(RleWorker& rw)
{
    //TODO:
    return nullptr;
}


static void recordCell(RleWorker& rw)
{
    if (rw.area | rw.cover) {
        TCell* cell = findCell(rw);
        assert(cell);
        cell->area += rw.area;
        cell->cover += rw.cover;
    }
}


static bool genRle(RleWorker& rw)
{
    bool ret = false;

    if (setjmp(rw.jmpBuf) == 0) {
        ret = decomposeOutline(rw);
        if (!rw.invalid)  recordCell(rw);
    } else {
        cout <<  "Memory Overflow" << endl;
    }
    return ret;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool rleRender(SwShape& sdata)
{
    constexpr auto RENDER_POOL_SIZE = 16384L;
    constexpr auto BAND_SIZE = 40;

    auto outline = sdata.outline;
    assert(outline);

    if (outline->ptsCnt == 0 || outline->cntrsCnt <= 0) return false;

    assert(outline->cntrs && outline->pts);
    assert(outline->ptsCnt == outline->cntrs[outline->cntrsCnt - 1] + 1);

    //TODO: We can preserve several static workers in advance
    RleWorker rw;
    TCell buffer[RENDER_POOL_SIZE / sizeof(TCell)];

    //Init Cells
    rw.buffer = buffer;
    rw.bufferSize = sizeof(buffer);
    rw.yCells = reinterpret_cast<TCell**>(buffer);
    rw.cells = nullptr;
    rw.maxCells = 0;
    rw.numCells = 0;
    rw.area = 0;
    rw.cover = 0;
    rw.invalid = true;
    rw.exMin = sdata.bbox.xMin;
    rw.exMax = sdata.bbox.xMax;
    rw.eyMin = sdata.bbox.yMin;
    rw.eyMax = sdata.bbox.yMax;
    rw.exCnt = rw.exMax - rw.exMin;
    rw.eyCnt = rw.eyMax - rw.eyMin;
    rw.outline = outline;
    rw.bandSize = rw.bufferSize / (sizeof(TCell) * 8);  //bandSize: 64
    rw.bandShoot = 0;
    //printf("bufferSize = %d, bbox(%d %d %d %d), exCnt(%d), eyCnt(%d), bandSize(%d)\n", rw.bufferSize, rw.exMin, rw.eyMin, rw.exMax, rw.eyMax, rw.exCnt, rw.eyCnt, rw.bandSize);

    //Generate RLE
    TBand bands[BAND_SIZE];
    TBand* band;

    /* set up vertical bands */
    auto bandCnt = (rw.eyMax - rw.eyMin) / rw.bandSize;
    if (bandCnt == 0) bandCnt = 1;
    else if (bandCnt >= BAND_SIZE) bandCnt = BAND_SIZE - 1;

    auto min = rw.eyMin;
    auto yMax = rw.eyMax;
    TPos max;
//printf("bandCnt(%d)\n", bandCnt);

    for (int n = 0; n < bandCnt; ++n, min = max) {
        max = min + rw.bandSize;
        if (n == bandCnt -1 || max > yMax) max = yMax;

        bands[0].min = min;
        bands[0].max = max;
        band = bands;

        while (band >= bands) {
            rw.yCells = static_cast<TCell**>(rw.buffer);
            rw.yCnt = band->max - band->min;

            auto cellStart = sizeof(TCell*) * rw.yCnt;
            auto cellMod = cellStart % sizeof(TCell);

            if (cellMod > 0) cellStart += sizeof(TCell) - cellMod;

            auto cellEnd = rw.bufferSize;
            cellEnd -= cellEnd % sizeof(TCell);
//printf("n:%d, cellStart(%d), cellEnd(%d) cellMod(%d)\n", n, cellStart, cellEnd, cellMod);

            auto cellsMax = reinterpret_cast<TCell*>((char*)rw.buffer + cellEnd);
            rw.cells = reinterpret_cast<TCell*>((char*)rw.buffer + cellStart);

            if (rw.cells >= cellsMax) goto reduce_bands;

            rw.maxCells = cellsMax - rw.cells;
            if (rw.maxCells < 2) goto reduce_bands;

            for (auto y = 0; y < rw.yCnt; ++y)
                rw.yCells[y] = nullptr;

            rw.numCells = 0;
            rw.invalid = true;
            rw.eyMin = band->min;
            rw.eyMax = band->max;
            rw.eyCnt = band->max - band->min;

            if (!genRle(rw)) return -1;

            rleSweep(rw);
            --band;
            continue;

        reduce_bands:
            /* render pool overflow: we will reduce the render band by half */
            auto bottom = band->min;
            auto top = band->max;
            auto middle = bottom + ((top - bottom) >> 1);

            /* This is too complex for a single scanline; there must
               be some problems */
            if (middle == bottom) return -1;

            if (bottom - top >= rw.bandSize) ++rw.bandShoot;

            band[1].min = bottom;
            band[1].max = middle;
            band[0].min = middle;
            band[0].max = top;
            ++band;
        }
    }

    if (rw.bandShoot > 8 && rw.bandSize > 16)
        rw.bandSize = rw.bandSize / 2;

    return true;
}

#endif /* _TVG_SW_RLE_H_ */
