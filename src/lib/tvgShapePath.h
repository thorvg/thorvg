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
#ifndef _TVG_SHAPE_PATH_CPP_
#define _TVG_SHAPE_PATH_CPP_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct ShapePath
{
    PathCommand* cmds = nullptr;
    size_t cmdCnt = 0;
    size_t reservedCmdCnt = 0;

    Point *pts = nullptr;
    size_t ptsCnt = 0;
    size_t reservedPtsCnt = 0;


    ~ShapePath()
    {
        if (cmds) delete(cmds);
        if (pts) delete(pts);
    }

    void reserveCmd(size_t cmdCnt)
    {
        if (cmdCnt <= reservedCmdCnt) return;
        reservedCmdCnt = cmdCnt;
        cmds = static_cast<PathCommand*>(realloc(cmds, sizeof(PathCommand) * reservedCmdCnt));
        assert(cmds);
    }

    void reservePts(size_t ptsCnt)
    {
        if (ptsCnt <= reservedPtsCnt) return;
        reservedPtsCnt = ptsCnt;
        pts = static_cast<Point*>(realloc(pts, sizeof(Point) * reservedPtsCnt));
        assert(pts);
    }

    void reserve(size_t cmdCnt, size_t ptsCnt)
    {
        reserveCmd(cmdCnt);
        reservePts(ptsCnt);
    }

    void grow(size_t cmdCnt, size_t ptsCnt)
    {
        reserveCmd(this->cmdCnt + cmdCnt);
        reservePts(this->ptsCnt + ptsCnt);
    }

    void clear()
    {
        cmdCnt = 0;
        ptsCnt = 0;
    }

    void append(const PathCommand* cmds, size_t cmdCnt, const Point* pts, size_t ptsCnt)
    {
        grow(cmdCnt, ptsCnt);
        memcpy(this->cmds + this->cmdCnt, cmds, sizeof(PathCommand) * cmdCnt);
        memcpy(this->pts + this->ptsCnt, pts, sizeof(Point) * ptsCnt);
        this->cmdCnt += cmdCnt;
        this->ptsCnt += ptsCnt;
    }

    void moveTo(float x, float y)
    {
        if (cmdCnt + 1 > reservedCmdCnt) reserveCmd((cmdCnt + 1) * 2);
        if (ptsCnt + 2 > reservedPtsCnt) reservePts((ptsCnt + 2) * 2);

        cmds[cmdCnt++] = PathCommand::MoveTo;
        pts[ptsCnt++] = {x, y};
    }

    void lineTo(float x, float y)
    {
        if (cmdCnt + 1 > reservedCmdCnt) reserveCmd((cmdCnt + 1) * 2);
        if (ptsCnt + 2 > reservedPtsCnt) reservePts((ptsCnt + 2) * 2);

        cmds[cmdCnt++] = PathCommand::LineTo;
        pts[ptsCnt++] = {x, y};
    }

    void cubicTo(float cx1, float cy1, float cx2, float cy2, float x, float y)
    {
        if (cmdCnt + 1 > reservedCmdCnt) reserveCmd((cmdCnt + 1) * 2);
        if (ptsCnt + 3 > reservedPtsCnt) reservePts((ptsCnt + 3) * 2);

        cmds[cmdCnt++] = PathCommand::CubicTo;
        pts[ptsCnt++] = {cx1, cy1};
        pts[ptsCnt++] = {cx2, cy2};
        pts[ptsCnt++] = {x, y};
    }

    void close()
    {
        if (cmdCnt + 1 > reservedCmdCnt) reserveCmd((cmdCnt + 1) * 2);
        cmds[cmdCnt++] = PathCommand::Close;
    }
};

#endif //_TVG_SHAPE_PATH_CPP_
