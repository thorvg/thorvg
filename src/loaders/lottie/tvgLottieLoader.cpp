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

#include <math.h>
#include "tvgLoader.h"
#include "tvgLottieLoader.h"
#include "tvgLottieModel.h"
#include "tvgLottieParser.h"
#include "tvgLottieBuilder.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static bool _checkDotLottie(const char *str)
{
    //check the .Lottie signature.
    if (str[0] == 0x50 && str[1] == 0x4B && str[2] == 0x03 && str[3] == 0x04) return true;
    else return false;
}


static int _str2int(const char* str, int len)
{
    int ret = 0;
    for(int i = 0; i < len; ++i) {
        ret = ret * 10 + (str[i] - '0');
    }
    return ret;
}


static int _str2float(const char* str, int len)
{
    //strndup()
    auto tmp = (char*)malloc(len + 1);
    if (!tmp) return 0;
    tmp[len] = '\0';
    memcpy(tmp, str, len);

    auto ret = strtof(tmp, nullptr);
    free(tmp);
    return lround(ret);
}


void LottieLoader::clear()
{
    if (copy) free((char*)content);

    size = 0;
    content = nullptr;
    copy = false;
}


void LottieLoader::run(unsigned tid)
{
    //update frame
    if (comp && comp->scene) {
        builder->update(comp, frameNo);
    //initial loading
    } else {
        LottieParser parser(content);
        parser.parse();
        comp = parser.comp;
        builder->build(comp);
    }
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

LottieLoader::LottieLoader() : builder(new LottieBuilder)
{
}


LottieLoader::~LottieLoader()
{
    close();

    //TODO: correct position?
    if (comp) {
        delete(comp);
        comp = nullptr;
    }

    delete(builder);
}


bool LottieLoader::header()
{
    //Quickly validate the given Lottie file without parsing in order to get the animation info.
    auto p = content;

    uint32_t depth = 0;

    while (*p != '\0') {
        if (*p == '{') {
            ++depth;
            ++p;
            continue;
        }
        if (*p == '}') {
            --depth;
            ++p;
            continue;
        }
        if (depth != 1) {
            ++p;
            continue;
        }
        //version.
        if (!strncmp(p, "\"v\":", 4)) {
            p += 4;
            continue;
        }
        //framerate
        if (!strncmp(p, "\"fr\":", 5)) {
            p += 5;
            auto e = strstr(p, ",");
            if (!e) e = strstr(p, "}");
            frameRate = _str2float(p, e - p);
            p = e;
            continue;
        }
        //TODO: need a duration time in advance.

        //width
        if (!strncmp(p, "\"w\":", 4)) {
            p += 4;
            auto e = strstr(p, ",");
            if (!e) e = strstr(p, "}");
            w = static_cast<float>(_str2int(p, e - p));
            p = e;
            continue;
        }
        //height
        if (!strncmp(p, "\"h\":", 4)) {
            p += 4;
            auto e = strstr(p, ",");
            if (!e) e = strstr(p, "}");
            h = static_cast<float>(_str2int(p, e - p));
            p = e;
            continue;
        }
        ++p;
    }
    TVGLOG("LOTTIE", "info: fr = %d, size = %d x %d", frameRate, (int)w, (int)h);

    return true;
}


bool LottieLoader::open(const char* data, uint32_t size, bool copy)
{
    clear();

    //If the format is dotLottie
    auto dotLottie = _checkDotLottie(data);
    if (dotLottie) {
        TVGLOG("LOTTIE", "Requested .Lottie Format, Not Supported yet.");
        return false;
    }

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

    auto f = fopen(path.c_str(), "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);

    size = ftell(f);
    if (size == 0) {
        fclose(f);
        return false;
    }

    auto content = (char*)(malloc(sizeof(char) * size + 1));
    fseek(f, 0, SEEK_SET);
    auto ret = fread(content, sizeof(char), size, f);
    if (ret < size) {
        fclose(f);
        return false;
    }
    content[size] = '\0';

    fclose(f);

    //If the format is dotLottie
    auto dotLottie = _checkDotLottie(content);
    if (dotLottie) {
        TVGLOG("LOTTIE", "Requested .Lottie Format, Not Supported yet.");
        return false;
    }

    this->content = content;
    this->copy = true;

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
    if (comp) return true;

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

    return cast<Paint>(comp->scene);
}


bool LottieLoader::frame(uint32_t frameNo)
{
    if (this->frameNo == frameNo) return true;

    this->done();

    if (!comp || frameNo >= comp->frameCnt()) return false;

    this->frameNo = frameNo;

    TaskScheduler::request(this);

    return true;
}


uint32_t LottieLoader::totalFrame()
{
    this->done();

    if (!comp) return 0;
    return comp->frameCnt();
}


uint32_t LottieLoader::curFrame()
{
    return frameNo;
}


float LottieLoader::duration()
{
    this->done();

    if (!comp) return 0;
    return comp->duration();
}


void LottieLoader::sync()
{
    this->done();
}