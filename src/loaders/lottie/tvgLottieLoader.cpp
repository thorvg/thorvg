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

#include <fstream>
#include <memory.h>
#include "tvgLoader.h"
#include "tvgLottieLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void LottieLoader::clear()
{
    data = nullptr;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


LottieLoader::~LottieLoader()
{
}


bool LottieLoader::header()
{
printf("asd\n");
    return true;
}


bool LottieLoader::open(const string& path)
{
    clear();

    ifstream f;
    f.open(path);

    if (!f.is_open()) return false;

    svgPath = path;
    getline(f, filePath, '\0');
    f.close();

    if (filePath.empty()) return false;

    content = filePath.c_str();
    size = filePath.size();

    return header();
}


bool LottieLoader::open(const char* data, uint32_t size, bool copy)
{
    clear();

    if (copy) {
        content = (char*)malloc(size);
        if (!content) return false;
        memcpy((char*)content, data, size);
    } else content = data;

    this->size = size;
    this->copy = copy;

    return header();
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



void LottieLoader::run(unsigned tid)
{
    //image = jpgdDecompress(decoder);
}

unique_ptr<Paint> LottieLoader::paint()
{
    this->done();
    if (root) return move(root);
    else return nullptr;
}
