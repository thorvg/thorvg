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
#include <vector>
#include "tvgSwCommon.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static unsigned threadsCnt = 1;
static vector<SwOutline> sharedOutline;


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SwOutline* resMgrRequestOutline(unsigned idx)
{
    return &sharedOutline[idx];
}


void resMgrRetrieveOutline(unsigned idx)
{
    sharedOutline[idx].cntrsCnt = 0;
    sharedOutline[idx].ptsCnt = 0;
}


bool resMgrInit(unsigned threads)
{
    sharedOutline.reserve(threads);
    sharedOutline.resize(threads);
    threadsCnt = threads;

    for (auto& outline : sharedOutline) {
        outline.cntrs = nullptr;
        outline.pts = nullptr;
        outline.types = nullptr;
        outline.cntrsCnt = outline.reservedCntrsCnt = 0;
        outline.ptsCnt = outline.reservedPtsCnt = 0;
    }

    return true;
}


bool resMgrClear()
{
    for (auto& outline : sharedOutline) {
        if (outline.cntrs) {
            free(outline.cntrs);
            outline.cntrs = nullptr;
        }
        if (outline.pts) {
            free(outline.pts);
            outline.pts = nullptr;
        }
        if (outline.types) {
            free(outline.types);
            outline.types = nullptr;
        }
        outline.cntrsCnt = outline.reservedCntrsCnt = 0;
        outline.ptsCnt = outline.reservedPtsCnt = 0;
    }
    return true;
}


bool resMgrTerm()
{
    return resMgrClear();
}