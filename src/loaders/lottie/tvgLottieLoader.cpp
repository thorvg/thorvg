/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.

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

#include <memory.h>
#include "tvgLoader.h"
#include "tvgLottieLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void LottieLoader::clear()
{
    //jpgdDelete(decoder);
    if (freeData) free(data);
    //decoder = nullptr;
    data = nullptr;
    freeData = false;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


LottieLoader::~LottieLoader()
{
    //jpgdDelete(decoder);
    if (freeData) free(data);
}


bool LottieLoader::open(const string& path)
{
    clear();

    int width, height;
    width = height = 0;
    //decoder = jpgdHeader(path.c_str(), &width, &height);
    //if (!decoder) return false;

    w = static_cast<float>(width);
    h = static_cast<float>(height);

    return true;
}


bool LottieLoader::open(const char* data, uint32_t size, bool copy)
{
    clear();

    if (copy) {
        this->data = (char *) malloc(size);
        if (!this->data) return false;
        memcpy((char *)this->data, data, size);
        freeData = true;
    } else {
        this->data = (char *) data;
        freeData = false;
    }

    int width, height;
    width = height = 0;
    //decoder = jpgdHeader(this->data, size, &width, &height);
    //if (!decoder) return false;

    w = static_cast<float>(width);
    h = static_cast<float>(height);

    return true;
}



bool LottieLoader::read()
{
    if (/*!decoder || */w <= 0 || h <= 0) return false;

    TaskScheduler::request(this);

    return true;
}


bool LottieLoader::close()
{
    this->done();
    clear();
    return true;
}


const uint32_t* LottieLoader::pixels()
{
    this->done();

    return (const uint32_t*)image;
}


void LottieLoader::run(unsigned tid)
{
    //image = jpgdDecompress(decoder);
}
