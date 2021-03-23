/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All rights reserved.

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
#include "tvgSwCommon.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static SwOutline* outline = nullptr;
static SwOutline* strokeOutline = nullptr;
static unsigned allocSize = 0;


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SwOutline* mpoolReqOutline(unsigned idx)
{
    return &outline[idx];
}


void mpoolRetOutline(unsigned idx)
{
    outline[idx].cntrsCnt = 0;
    outline[idx].ptsCnt = 0;
}


SwOutline* mpoolReqStrokeOutline(unsigned idx)
{
    return &strokeOutline[idx];
}


void mpoolRetStrokeOutline(unsigned idx)
{
    strokeOutline[idx].cntrsCnt = 0;
    strokeOutline[idx].ptsCnt = 0;
}


bool mpoolInit(unsigned threads)
{
    if (outline || strokeOutline) return false;
    if (threads == 0) threads = 1;

    outline = static_cast<SwOutline*>(calloc(1, sizeof(SwOutline) * threads));
    if (!outline) goto err;

    strokeOutline = static_cast<SwOutline*>(calloc(1, sizeof(SwOutline) * threads));
    if (!strokeOutline) goto err;

    allocSize = threads;

    return true;

err:
    if (outline) {
        free(outline);
        outline = nullptr;
    }

    if (strokeOutline) {
        free(strokeOutline);
        strokeOutline = nullptr;
    }
    return false;
}


bool mpoolClear()
{
    SwOutline* p;

    for (unsigned i = 0; i < allocSize; ++i) {

        p = &outline[i];

        if (p->cntrs) {
            free(p->cntrs);
            p->cntrs = nullptr;
        }
        if (p->pts) {
            free(p->pts);
            p->pts = nullptr;
        }
        if (p->types) {
            free(p->types);
            p->types = nullptr;
        }
        p->cntrsCnt = p->reservedCntrsCnt = 0;
        p->ptsCnt = p->reservedPtsCnt = 0;

        p = &strokeOutline[i];

        if (p->cntrs) {
            free(p->cntrs);
            p->cntrs = nullptr;
        }
        if (p->pts) {
            free(p->pts);
            p->pts = nullptr;
        }
        if (p->types) {
            free(p->types);
            p->types = nullptr;
        }
        p->cntrsCnt = p->reservedCntrsCnt = 0;
        p->ptsCnt = p->reservedPtsCnt = 0;
    }

    return true;
}


bool mpoolTerm()
{
    mpoolClear();

    if (outline) {
        free(outline);
        outline = nullptr;
    }

    if (strokeOutline) {
        free(strokeOutline);
        strokeOutline = nullptr;
    }

    allocSize = 0;

    return true;
}