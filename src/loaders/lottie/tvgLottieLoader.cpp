/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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

#include "tvgStr.h"
 #include "tvgLottieLoader.h"
#include "tvgLottieModel.h"
#include "tvgLottieParser.h"
#include "tvgLottieBuilder.h"
#include "tvgCompressor.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void LottieLoader::run(unsigned tid)
{
    //update frame
    if (comp) {
        builder->update(comp, frameNo);
    //initial loading
    } else {
        LottieParser parser(content, dirName, builder->expressions());
        if (!parser.parse()) return;
        {
            ScopedLock lock(key);
            comp = parser.comp;
        }
        if (parser.slots) {
            auto slotId = genSlot(parser.slots);
            applySlot(slotId, true);
            delSlot(slotId, true);
            parser.slots = nullptr;
        }
        builder->build(comp);

        release();
    }
    rebuild = false;
}


void LottieLoader::release()
{
    if (copy) {
        tvg::free((char*)content);
        content = nullptr;
    }
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

LottieLoader::LottieLoader() : FrameModule(FileType::Lot), builder(new LottieBuilder)
{

}


LottieLoader::~LottieLoader()
{
    done();

    release();

    //TODO: correct position?
    delete(comp);
    delete(builder);

    tvg::free(dirName);
}


bool LottieLoader::header()
{
    //A single thread doesn't need to perform intensive tasks.
    if (TaskScheduler::threads() == 0) {
        LoadModule::read();
        run(0);
        if (comp) {
            w = static_cast<float>(comp->w);
            h = static_cast<float>(comp->h);
            segmentEnd = frameCnt = comp->frameCnt();
            frameRate = comp->frameRate;
            return true;
        } else {
            return false;
        }
    }

    //Quickly validate the given Lottie file without parsing in order to get the animation info.
    auto startFrame = 0.0f;
    auto endFrame = 0.0f;
    uint32_t depth = 0;

    auto p = content;

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
        //version
        if (!strncmp(p, "\"v\":", 4)) {
            p += 4;
            continue;
        }

        //framerate
        if (!strncmp(p, "\"fr\":", 5)) {
            p += 5;
            auto e = strstr(p, ",");
            if (!e) e = strstr(p, "}");
            frameRate = toFloat(p, nullptr);
            p = e;
            continue;
        }

        //start frame
        if (!strncmp(p, "\"ip\":", 5)) {
            p += 5;
            auto e = strstr(p, ",");
            if (!e) e = strstr(p, "}");
            startFrame = toFloat(p, nullptr);
            p = e;
            continue;
        }

        //end frame
        if (!strncmp(p, "\"op\":", 5)) {
            p += 5;
            auto e = strstr(p, ",");
            if (!e) e = strstr(p, "}");
            endFrame = toFloat(p, nullptr);
            p = e;
            continue;
        }

        //width
        if (!strncmp(p, "\"w\":", 4)) {
            p += 4;
            auto e = strstr(p, ",");
            if (!e) e = strstr(p, "}");
            w = toFloat(p, nullptr);
            p = e;
            continue;
        }
        //height
        if (!strncmp(p, "\"h\":", 4)) {
            p += 4;
            auto e = strstr(p, ",");
            if (!e) e = strstr(p, "}");
            h = toFloat(p, nullptr);
            p = e;
            continue;
        }
        ++p;
    }

    if (frameRate < FLOAT_EPSILON) {
        TVGLOG("LOTTIE", "Not a Lottie file? Frame rate is 0!");
        return false;
    }

    segmentEnd = frameCnt = (endFrame - startFrame);

    TVGLOG("LOTTIE", "info: frame rate = %f, duration = %f size = %f x %f", frameRate, frameCnt / frameRate, w, h);

    return true;
}


bool LottieLoader::open(const char* data, uint32_t size, const char* rpath, bool copy)
{
    if (copy) {
        content = tvg::malloc<char*>(size + 1);
        if (!content) return false;
        memcpy((char*)content, data, size);
        const_cast<char*>(content)[size] = '\0';
    } else content = data;

    this->size = size;
    this->copy = copy;

    if (!rpath) this->dirName = duplicate(".");
    else this->dirName = duplicate(rpath);

    return header();
}


bool LottieLoader::open(const char* path)
{
#ifdef THORVG_FILE_IO_SUPPORT
    auto f = fopen(path, "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);

    size = ftell(f);
    if (size == 0) {
        fclose(f);
        return false;
    }

    auto content = tvg::malloc<char*>(sizeof(char) * size + 1);
    fseek(f, 0, SEEK_SET);
    size = fread(content, sizeof(char), size, f);
    content[size] = '\0';

    fclose(f);

    this->dirName = tvg::dirname(path);
    this->content = content;
    this->copy = true;

    return header();
#else
    return false;
#endif
}


bool LottieLoader::resize(Paint* paint, float w, float h)
{
    if (!paint) return false;

    auto sx = w / this->w;
    auto sy = h / this->h;
    Matrix m = {sx, 0, 0, 0, sy, 0, 0, 0, 1};
    paint->transform(m);

    //apply the scale to the base clipper
    auto clipper = PAINT(paint)->clipper;
    if (clipper) clipper->transform(m);

    return true;
}


bool LottieLoader::read()
{
    //the loading has been already completed
    if (!LoadModule::read()) return true;

    if (!content || size == 0) return false;

    TaskScheduler::request(this);

    return true;
}


Paint* LottieLoader::paint()
{
    done();

    if (!comp) return nullptr;
    comp->initiated = true;
    return comp->root->scene;
}


bool LottieLoader::applySlot(uint32_t slotId, bool byDefault)
{
    if (!ready() || comp->slots.count == 0 || comp->slotDatas.empty()) return false;

    if (slotId == 0) {
        ARRAY_FOREACH(p, comp->slots) {
            (*p)->reset();
        }
        overridden = false;
        rebuild = true;
        return true;
    }

    LottieSlotData* slotData = nullptr;

    INLIST_FOREACH(comp->slotDatas, p) {
        if (p->id == slotId) {
            slotData = p;
            break;
        }
    }

    if (!slotData) return false;

    ARRAY_FOREACH(p, slotData->datas) {
        ARRAY_FOREACH(q, comp->slots) {
            if (strcmp((*q)->sid, p->sid)) continue;
            (*q)->apply(p->prop, byDefault);
            break;
        }
    }

    overridden = true;
    rebuild = true;
    return true;
}


bool LottieLoader::delSlot(uint32_t slotId, bool byDefault)
{
    if (!ready() || comp->slots.count == 0 || comp->slotDatas.empty()) return false;

    if (slotId == 0) {
        if (!byDefault) {
            ARRAY_FOREACH(p, comp->slots) {
                (*p)->reset();
            }
            rebuild = true;
        }

        comp->slotDatas.free();
        overridden = false;
        return true;
    }

    LottieSlotData* slotData = nullptr;

    INLIST_FOREACH(comp->slotDatas, p) {
        if (p->id == slotId) {
            slotData = p;
            break;
        }
    }

    if (!slotData) return false;

    ARRAY_FOREACH(p, slotData->datas) {
        ARRAY_FOREACH(q, comp->slots) {
            if (strcmp((*q)->sid, p->sid)) continue;
            if (!byDefault && (*q)->overridden) {
                (*q)->reset();
                rebuild = true;
            }
            break;
        }
    }

    comp->slotDatas.remove(slotData);
    delete(slotData);
    return true;
}


uint32_t LottieLoader::genSlot(const char* slots)
{
    if (!slots || !ready() || comp->slots.count == 0) return 0;

    auto temp = duplicate(slots);

    //parsing slot json
    LottieParser parser(temp, dirName, builder->expressions());
    parser.comp = comp;
    
    auto idx = 0;
    bool generated = false;
    LottieSlotData* slotData = new LottieSlotData(djb2Encode(slots));
    while (auto sid = parser.sid(idx == 0)) {
        ARRAY_FOREACH(p, comp->slots) {
            if (strcmp((*p)->sid, sid)) continue;
            auto prop = parser.slotData(*p);

            if (prop) {
                slotData->datas.push({duplicate(sid), prop});
                generated = true;
            }
            break;
        }
        ++idx;
    }

    tvg::free((char*)temp);

    if (generated) comp->slotDatas.back(slotData);
    else {
        delete(slotData);
        return 0;
    }

    return slotData->id;
}


float LottieLoader::shorten(float frameNo)
{
    //This ensures that the target frame number is reached.
    return nearbyintf((frameNo + startFrame()) * 10000.0f) * 0.0001f;
}


bool LottieLoader::frame(float no)
{
    no = shorten(no);

    //Skip update if frame diff is too small.
    if (!builder->tweening() && fabsf(this->frameNo - no) <= 0.0009f) return false;

    this->done();

    this->frameNo = no;

    builder->offTween();

    TaskScheduler::request(this);

    return true;
}


float LottieLoader::startFrame()
{
    return segmentBegin;
}


float LottieLoader::totalFrame()
{
    return segmentEnd - segmentBegin;
}


float LottieLoader::curFrame()
{
    return frameNo - startFrame();
}


float LottieLoader::duration()
{
    return (segmentEnd - segmentBegin) / frameRate;
}


void LottieLoader::sync()
{
    done();

    if (rebuild) run(0);
}


uint32_t LottieLoader::markersCnt()
{
    return ready() ? comp->markers.count : 0;
}


const char* LottieLoader::markers(uint32_t index)
{
    if (!ready() || index >= comp->markers.count) return nullptr;
    auto marker = comp->markers.begin() + index;
    return (*marker)->name;
}


Result LottieLoader::segment(float begin, float end)
{
    if (begin < 0.0f) begin = 0.0f;
    if (end > frameCnt) end = frameCnt;

    if (begin > end) return Result::InvalidArguments;

    segmentBegin = begin;
    segmentEnd = end;

    return Result::Success;
}


bool LottieLoader::segment(const char* marker, float& begin, float& end)
{
    if (!ready() || comp->markers.count == 0) return false;

    ARRAY_FOREACH(p, comp->markers) {
        if (!strcmp(marker, (*p)->name)) {
            begin = (*p)->time;
            end = (*p)->time + (*p)->duration;
            return true;
        }
    }
    return false;
}


bool LottieLoader::ready()
{
    {
        ScopedLock lock(key);
        if (comp) return true;
    }
    done();
    if (comp) return true;
    return false;
}


bool LottieLoader::tween(float from, float to, float progress)
{
    //tweening is not necessary
    if (tvg::zero(progress)) return frame(from);
    else if (tvg::equal(progress, 1.0f)) return frame(to);

    done();

    frameNo = shorten(from);

    builder->onTween(shorten(to), progress);

    TaskScheduler::request(this);

    return true;
}