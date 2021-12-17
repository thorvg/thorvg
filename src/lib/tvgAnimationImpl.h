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
#ifndef _TVG_ANIMATION_IMPL_H_
#define _TVG_ANIMATION_IMPL_H_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Animation::Impl
{
    bool loaded = false;
    int frameNum = 0;
    int totalFrameNum = 0;

    ~Impl()
    {
    }


    bool frame(int frame)
    {
        //if (!loaded) return false;
        frameNum = frame;
        return true;
    }

    bool frame(int* frame)
    {
        if (!loaded || !frame) return false;
        *frame = this->frameNum;
        return true;
    }

    bool totalFrame(int totalFrame)
    {
        if (!loaded) return false;
        this->totalFrameNum = totalFrame;
        return true;
    }

    bool totalFrame(int* totalFrame)
    {
        if (!totalFrame) return false;
        *totalFrame = this->totalFrameNum;
        return true;
    }
};

#endif //_TVG_ANIMATION_IMPL_H_
