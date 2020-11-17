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
#include <fstream>
#include <string.h>
#include "tvgLoaderMgr.h"
#include "tvgRawLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

RawLoader::RawLoader()
{

}


RawLoader::~RawLoader()
{
    close();
    if (copy && content) {
        free((void*)content);
        content = nullptr;
    }
}


void RawLoader::run(unsigned tid)
{
}


bool RawLoader::header()
{
    return true;
}


bool RawLoader::open(const string& path)
{
    /* RawLoader isn't supported path loding */
    return false;
}


bool RawLoader::open(const char* data, uint32_t size)
{
    //In Raw image loader, char array load is not supported.
    return false;
}


bool RawLoader::open(const uint32_t* data, uint32_t width, uint32_t height, bool copy)
{
    if (!data || width == 0 || height == 0) return false;
    content = data;

    vx = 0;
    vy = 0;
    vw = width;
    vh = height;

    this->copy = copy;

    return header();
}


bool RawLoader::read()
{
    if (copy && content) {
        const uint32_t *origContent = content;
        content = (uint32_t*)malloc(sizeof(uint32_t) * vw * vh);
        if (!content) return false;
        memcpy((void*)content, origContent, sizeof(uint32_t) * vw * vh);
    }
    return true;
}


bool RawLoader::close()
{
    this->done();

    return true;
}


unique_ptr<Scene> RawLoader::root()
{
    return nullptr;
}


const uint32_t* RawLoader::data()
{
    return this->content;
}
