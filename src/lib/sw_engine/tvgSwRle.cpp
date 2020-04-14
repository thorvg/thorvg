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
#include <limits.h>
#include "tvgSwCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

constexpr auto MAX_SPANS = 256;
constexpr auto PIXEL_BITS = 8;   //must be at least 6 bits!
constexpr auto ONE_PIXEL = (1L << PIXEL_BITS);


using Area = long;

struct Band
{
    SwCoord min, max;
};

struct Cell
{
    SwCoord x;
    SwCoord cover;
    Area area;
    Cell *next;
};

struct RleWorker
{
    SwPoint cellPos;
    SwPoint cellMin;
    SwPoint cellMax;
    SwCoord cellXCnt;
    SwCoord cellYCnt;

    Area area;
    SwCoord cover;

    Cell* cells;
    ptrdiff_t maxCells;
    ptrdiff_t cellsCnt;

    SwPoint pos;

    SwPoint bezStack[32 * 3 + 1];
    int levStack[32];

    SwOutline* outline;

    SwSpan spans[MAX_SPANS];
    int spansCnt;
    int ySpan;

    int bandSize;
    int bandShoot;

    jmp_buf jmpBuf;

    void* buffer;
    long bufferSize;

    Cell** yCells;
    SwCoord yCnt;

    bool invalid;
};


static inline SwPoint UPSCALE(const SwPoint& pt)
{
    return {pt.x << (PIXEL_BITS - 6), pt.y << (PIXEL_BITS - 6)};
}


static inline SwPoint DOWNSCALE(const SwPoint& pt)
{
    return {pt.x >> (PIXEL_BITS - 6), pt.y >> (PIXEL_BITS - 6)};
}


static inline SwPoint TRUNC(const SwPoint& pt)
{
    return  { pt.x >> PIXEL_BITS, pt.y >> PIXEL_BITS };
}


static inline SwPoint SUBPIXELS(const SwPoint& pt)
{
    return {pt.x << PIXEL_BITS, pt.y << PIXEL_BITS };
}


static inline SwCoord SUBPIXELS(const SwCoord x)
{
    return (x << PIXEL_BITS);
}


static void horizLine(RleWorker& rw, SwCoord x, SwCoord y, SwCoord area, SwCoord acount)
{
    //TODO:
}


static void genSpan(RleWorker& rw)
{
    //TODO:
}


static void sweep(RleWorker& rw)
{
    if (rw.cellsCnt == 0) return;

    rw.spansCnt = 0;

    for (int y = 0; y < rw.yCnt; ++y) {

        auto cover = 0;
        auto x = 0;
        auto cell = rw.yCells[y];

        while (cell) {

            horizLine(rw, x, y, cover * (ONE_PIXEL * 2), cell->x - x);
            cover += cell->cover;
            auto area = cover * (ONE_PIXEL * 2) - cell->area;

            if (area != 0 && cell->x >= 0)
                horizLine(rw, cell->x, y, area, 1);

            x = cell->x + 1;
            cell = cell->next;
        }

        if (cover != 0)
            horizLine(rw, x, y, cover * (ONE_PIXEL * 2), rw.cellXCnt - x);
    }

    if (rw.spansCnt > 0) genSpan(rw);
}


static Cell* findCell(RleWorker& rw)
{
    auto x = rw.cellPos.x;
    if (x > rw.cellXCnt) x = rw.cellXCnt;

    auto pcell = &rw.yCells[rw.cellPos.y];
    assert(pcell);

    while(true) {
        Cell* cell = *pcell;
        if (!cell || cell->x > x) break;
        if (cell->x == x) return cell;
        pcell = &cell->next;
    }

    if (rw.cellsCnt >= rw.maxCells) longjmp(rw.jmpBuf, 1);

    auto cell = rw.cells + rw.cellsCnt++;
    assert(cell);
    cell->x = x;
    cell->area = 0;
    cell->cover = 0;
    cell->next = *pcell;
    *pcell = cell;

    return cell;
}


static void recordCell(RleWorker& rw)
{
    if (rw.area | rw.cover) {
        auto cell = findCell(rw);
        assert(cell);
        cell->area += rw.area;
        cell->cover += rw.cover;
    }
}

static void setCell(RleWorker& rw, SwPoint pos)
{
    /* Move the cell pointer to a new position.  We set the `invalid'      */
    /* flag to indicate that the cell isn't part of those we're interested */
    /* in during the render phase.  This means that:                       */
    /*                                                                     */
    /* . the new vertical position must be within min_ey..max_ey-1.        */
    /* . the new horizontal position must be strictly less than max_ex     */
    /*                                                                     */
    /* Note that if a cell is to the left of the clipping region, it is    */
    /* actually set to the (min_ex-1) horizontal position.                 */

    /* All cells that are on the left of the clipping region go to the
       min_ex - 1 horizontal position. */
    pos.y -= rw.cellMin.y;

    if (pos.x > rw.cellMax.x) pos.x = rw.cellMax.x;
    pos.x -= rw.cellMin.x;
    if (pos.x < 0) pos.x = -1;

    //Are we moving to a different cell?
    if (pos != rw.cellPos) {
        if (!rw.invalid) recordCell(rw);
    }

    rw.area = 0;
    rw.cover = 0;
    rw.cellPos = pos;
    rw.invalid = ((unsigned)pos.y >= (unsigned)rw.cellYCnt || pos.x >= rw.cellXCnt);
}


static void startCell(RleWorker& rw, SwPoint pos)
{
    if (pos.x > rw.cellMax.x) pos.x = rw.cellMax.x;
    if (pos.x < rw.cellMin.x) pos.x = rw.cellMin.x;

    rw.area = 0;
    rw.cover = 0;
    rw.cellPos = pos - rw.cellMin;
    rw.invalid = false;

    setCell(rw, pos);
}


static void moveTo(RleWorker& rw, const SwPoint& to)
{
    //record current cell, if any */
    if (!rw.invalid) recordCell(rw);

    //start to a new position
    startCell(rw, TRUNC(to));

    rw.pos = to;
}


static void lineTo(RleWorker& rw, const SwPoint& to)
{
#define SW_UDIV(a, b) \
    static_cast<SwCoord>(((unsigned long)(a) * (unsigned long)(b)) >> \
    (sizeof(long) * CHAR_BIT - PIXEL_BITS))

    auto e1 = TRUNC(rw.pos);
    auto e2 = TRUNC(to);

    //vertical clipping
    if ((e1.y >= rw.cellMax.y && e2.y >= rw.cellMax.y) ||
        (e1.y < rw.cellMin.y && e2.y >= rw.cellMin.y)) {
            rw.pos = to;
            return;
    }

    auto diff = to - rw.pos;
    auto f1 = rw.pos - SUBPIXELS(e1);
    SwPoint f2;

    //inside one cell
    if (e1 == e2) {
        ;
    //any horizontal line
    } else if (diff.y == 0) {
        e1.x = e2.x;
        setCell(rw, e1);
    } else if (diff.x == 0) {
        //vertical line up
        if (diff.y > 0) {
            do {
                f2.y = ONE_PIXEL;
                rw.cover += (f2.y - f1.y);
                rw.area += (f2.y - f1.y) * f1.x * 2;
                f1.y = 0;
                ++e1.y;
                setCell(rw, e1);
            } while(e1.y != e2.y);
        //vertical line down
        } else {
            do {
                f2.y = 0;
                rw.cover += (f2.y - f1.y);
                rw.area += (f2.y - f1.y) * f1.x * 2;
                f1.y = ONE_PIXEL;
                --e1.y;
                setCell(rw, e1);
            } while(e1.y != e2.y);
        }
    //any other line
    } else {
        Area prod = diff.x * f1.y - diff.y * f1.x;

        /* These macros speed up repetitive divisions by replacing them
           with multiplications and right shifts. */
        auto dx_r = (ULONG_MAX >> PIXEL_BITS) / (diff.x);
        auto dy_r = (ULONG_MAX >> PIXEL_BITS) / (diff.y);

        /* The fundamental value `prod' determines which side and the  */
        /* exact coordinate where the line exits current cell.  It is  */
        /* also easily updated when moving from one cell to the next.  */
        do {
            auto px = diff.x * ONE_PIXEL;
            auto py = diff.y * ONE_PIXEL;

            //left
            if (prod <= 0 && prod - px) {
                f2 = {0, SW_UDIV(-prod, -dx_r)};
                prod -= py;
                rw.cover += (f2.y - f1.y);
                rw.area += (f2.y - f1.y) * (f1.x + f2.x);
                f1 = {ONE_PIXEL, f2.y};
                --e1.x;
            //up
            } else if (prod - px <= 0 && prod - px + py > 0) {
                prod -= px;
                f2 = {SW_UDIV(-prod, dy_r), ONE_PIXEL};
                rw.cover += (f2.y - f1.y);
                rw.area += (f2.y - f1.y) * (f1.x + f2.x);
                f1 = {f2.x, 0};
                ++e1.y;
            //right
            } else if (prod - px + py <= 0 && prod + py >= 0) {
                prod += py;
                f2 = {ONE_PIXEL, SW_UDIV(prod, dx_r)};
                rw.cover += (f2.y - f1.y);
                rw.area += (f2.y - f1.y) * (f1.x + f2.x);
                f1 = {0, f2.y};
                ++e1.x;
            //down
            } else {
                f2 = {SW_UDIV(prod, -dy_r), 0};
                prod += px;
                rw.cover += (f2.y - f1.y);
                rw.area += (f2.y - f1.y) * (f1.x + f2.x);
                f1 = {f2.x, ONE_PIXEL};
                --e1.y;
            }

            setCell(rw, e1);

        } while(e1 != e2);
    }

    f2 = {to.x - SUBPIXELS(e2.x), to.y - SUBPIXELS(e2.y)};
    rw.cover += (f2.y - f1.y);
    rw.area += (f2.y - f1.y) * (f1.x + f2.x);
    rw.pos = to;
}


static bool renderCubic(RleWorker& rw, SwPoint& ctrl1, SwPoint& ctrl2, SwPoint& to)
{
    return true;
}


static bool cubicTo(RleWorker& rw, SwPoint& ctrl1, SwPoint& ctrl2, SwPoint& to)
{
    return renderCubic(rw, ctrl1, ctrl2, to);
}


static bool decomposeOutline(RleWorker& rw)
{
 //   printf("decomposOutline\n");
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

        moveTo(rw, UPSCALE(outline->pts[first]));

        while (pt < limit) {
            assert(++pt);
            assert(++tags);

            //emit a single line_to
            if (tags[0] == SW_CURVE_TAG_ON) {
                lineTo(rw, UPSCALE(*pt));
            //tag cubic
            } else {
                if (pt + 1 > limit || tags[1] != SW_CURVE_TAG_CUBIC)
                    goto invalid_outline;

                pt += 2;
                tags += 2;

                if (pt <= limit) {
                    if (!cubicTo(rw, pt[-2], pt[-1], pt[0])) return false;
                    continue;
                }
                if (!cubicTo(rw, pt[-2], pt[-1], outline->pts[first])) return false;
                goto close;
            }
        }

        //Close the contour with a line segment?
        //if (!lineTo(rw, outline->pts[first]));
    close:
       first = last + 1;
    }

    return true;

invalid_outline:
    cout << "Invalid Outline!" << endl;
    return false;
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
    constexpr auto BAND_SIZE = 39;

    auto outline = sdata.outline;
    assert(outline);

    if (outline->ptsCnt == 0 || outline->cntrsCnt <= 0) return false;

    assert(outline->cntrs && outline->pts);
    assert(outline->ptsCnt == outline->cntrs[outline->cntrsCnt - 1] + 1);

    //TODO: We can preserve several static workers in advance
    RleWorker rw;
    Cell buffer[RENDER_POOL_SIZE / sizeof(Cell)];

    //Init Cells
    rw.buffer = buffer;
    rw.bufferSize = sizeof(buffer);
    rw.yCells = reinterpret_cast<Cell**>(buffer);
    rw.cells = nullptr;
    rw.maxCells = 0;
    rw.cellsCnt = 0;
    rw.area = 0;
    rw.cover = 0;
    rw.invalid = true;
    rw.cellMin = sdata.bbox.min;
    rw.cellMax = sdata.bbox.max;
    rw.cellXCnt = rw.cellMax.x - rw.cellMin.x;
    rw.cellYCnt = rw.cellMax.y - rw.cellMin.y;
    rw.outline = outline;
    rw.bandSize = rw.bufferSize / (sizeof(Cell) * 8);  //bandSize: 64
    rw.bandShoot = 0;
    //printf("bufferSize = %d, bbox(%f %f %f %f), exCnt(%f), eyCnt(%f), bandSize(%d)\n", rw.bufferSize, rw.exMin, rw.eyMin, rw.exMax, rw.eyMax, rw.exCnt, rw.eyCnt, rw.bandSize);

    //Generate RLE
    Band bands[BAND_SIZE];
    Band* band;

    /* set up vertical bands */
    auto bandCnt = static_cast<int>((rw.cellMax.y - rw.cellMin.y) / rw.bandSize);
    if (bandCnt == 0) bandCnt = 1;
    else if (bandCnt >= BAND_SIZE) bandCnt = BAND_SIZE;

    auto min = rw.cellMin.y;
    auto yMax = rw.cellMax.y;
    SwCoord max;

    for (int n = 0; n < bandCnt; ++n, min = max) {
        max = min + rw.bandSize;
        if (n == bandCnt -1 || max > yMax) max = yMax;

        bands[0].min = min;
        bands[0].max = max;
        band = bands;

        while (band >= bands) {
            rw.yCells = static_cast<Cell**>(rw.buffer);
            rw.yCnt = band->max - band->min;

            auto cellStart = sizeof(Cell*) * (int)rw.yCnt;
            auto cellMod = cellStart % sizeof(Cell);

            if (cellMod > 0) cellStart += sizeof(Cell) - cellMod;

            auto cellEnd = rw.bufferSize;
            cellEnd -= cellEnd % sizeof(Cell);
//printf("n:%d, cellStart(%d), cellEnd(%d) cellMod(%d)\n", n, cellStart, cellEnd, cellMod);

            auto cellsMax = reinterpret_cast<Cell*>((char*)rw.buffer + cellEnd);
            rw.cells = reinterpret_cast<Cell*>((char*)rw.buffer + cellStart);

            if (rw.cells >= cellsMax) goto reduce_bands;

            rw.maxCells = cellsMax - rw.cells;
            if (rw.maxCells < 2) goto reduce_bands;

            for (auto y = 0; y < rw.yCnt; ++y)
                rw.yCells[y] = nullptr;

            rw.cellsCnt = 0;
            rw.invalid = true;
            rw.cellMin.y = band->min;
            rw.cellMax.y = band->max;
            rw.cellYCnt = band->max - band->min;

            if (!genRle(rw)) return -1;

            sweep(rw);
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
        rw.bandSize = (rw.bandSize >> 1);

    return true;
}

#endif /* _TVG_SW_RLE_H_ */
