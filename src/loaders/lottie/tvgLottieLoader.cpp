/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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

#define _USE_MATH_DEFINES       //Math Constants are not defined in Standard C/C++.

#include <cstring>
#include <fstream>
#include "tvgLoader.h"
#include "tvgLottieLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void LottieLoader::clear()
{
    //TODO: Clear all used resources
    if (copy) free((char*)content);
    size = 0;
    content = nullptr;
    copy = false;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

LottieLoader::LottieLoader()
{
}


LottieLoader::~LottieLoader()
{
    close();
}


void LottieLoader::run(unsigned tid)
{
    /* TODO: Compose current frame of Lottie Scene tree
       The result should be assigned to "this->root" */
}


bool LottieLoader::header()
{
    /* TODO: In this step, validate the given Lottie file (in open())
       Also expected to get the default view size. */
    return false;
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


bool LottieLoader::open(const string& path)
{
    clear();

    ifstream f;
    f.open(path);

    if (!f.is_open()) return false;

    getline(f, filePath, '\0');
    f.close();

    if (filePath.empty()) return false;

    content = filePath.c_str();
    size = filePath.size();

    return header();
}


bool LottieLoader::resize(Paint* paint, float w, float h)
{
    if (!paint) return false;

    auto sx = w / this->w;
    auto sy = h / this->h;
    Matrix m = {sx, 0, 0, 0, sy, 0, 0, 0, 1};
    paint->transform(m);

    return true;
}


bool LottieLoader::read()
{
    if (!content || size == 0) return false;

    //the loading has been already completed in header()
    if (root) return true;

    TaskScheduler::request(this);

    return true;
}


bool LottieLoader::close()
{
    this->done();

    clear();

    return true;
}


unique_ptr<Paint> LottieLoader::paint()
{
    this->done();

    if (root) return move(root);
    else return nullptr;
}


bool LottieLoader::frame(uint32_t val)
{
    //TODO: set the current frame number with @p val
    return false;
}


uint32_t LottieLoader::totalFrame()
{
    //TODO: return the total frame count
    return 0;
}


uint32_t LottieLoader::curFrame()
{
    //TODO: return the current frame number
    return 0;
}


double LottieLoader::duration()
{
    //TODO: return the animation total duration in seconds
    return 0;
}