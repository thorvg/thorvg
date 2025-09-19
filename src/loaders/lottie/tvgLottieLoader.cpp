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

LottieCustomSlot::~LottieCustomSlot()
{
    ARRAY_FOREACH(p, props) {
        delete(p->prop);
    }
}


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
            auto slotcode = gen(parser.slots, true);
            apply(slotcode, true);
            del(slotcode, true);
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


bool LottieLoader::apply(uint32_t slotcode, bool byDefault)
{
    if (curSlot == slotcode) return true;

    if (!ready() || comp->slots.count == 0) return false;

    auto applied = false;

    // Reset all slots if slotcode is 0
    if (slotcode == 0) {
        ARRAY_FOREACH(p, comp->slots) (*p)->reset();
        applied = true;
    } else {
        //Find the custom slot with the slotcode
        INLIST_FOREACH(this->slots, slot) {
            if (slot->code != slotcode) continue;
            //apply the custom slot property to the targets.
            ARRAY_FOREACH(p, slot->props) {
                p->target->apply(p->prop, byDefault);
            }
            applied = true;
            break;
        }
    }
    curSlot = slotcode;
    if (applied) rebuild = true;
    return applied;
}


bool LottieLoader::del(uint32_t slotcode, bool byDefault)
{
    if (comp->slots.empty() || slotcode == 0 || !ready()) return false;

    // Search matching value and remove
    INLIST_SAFE_FOREACH(this->slots, slot) {
        if (slot->code != slotcode) continue;
        if (!byDefault) {
            ARRAY_FOREACH(p, slot->props) {
                p->target->reset();
            }
            rebuild = true;
        }
        this->slots.remove(slot);
        delete(slot);
        break;
    }
    return true;
}


uint32_t LottieLoader::gen(const char* slots, bool byDefault)
{
    if (!slots || !ready() || comp->slots.empty()) return 0;

    //parsing slot json
    auto temp = byDefault ? slots : duplicate(slots);
    LottieParser parser(temp, dirName, builder->expressions());
    parser.comp = comp;
    
    auto idx = 0;
    auto custom = new LottieCustomSlot(djb2Encode(slots));

    //Generates list of the custom slot overriding
    while (auto sid = djb2Encode(parser.sid(idx == 0))) {
        //Associates the overrding target to apply for the current custom slot
        ARRAY_FOREACH(p, comp->slots) {
            if ((*p)->sid != sid) continue;  //find target
            if (auto prop = parser.parse(*p)) custom->props.push({prop, *p});
            break;
        }
        ++idx;
    }

    tvg::free((char*)temp);

    //Success, valid custom slot.
    if (custom->props.count > 0) {
        this->slots.back(custom);
        return custom->code;
    }

    delete(custom);
    return 0;
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

    if (comp) comp->clear();     //clear synchronously

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

    if (comp) comp->clear();     //clear synchronously

    TaskScheduler::request(this);

    return true;
}


bool LottieLoader::assign(const char* layer, uint32_t ix, const char* var, float val)
{
    if (!ready() || !comp->expressions) return false;
    comp->root->assign(layer, ix, var, val);

    return true;
}


bool LottieLoader::quality(uint8_t value)
{
    if (!ready()) return false;
    if (comp->quality != value) {
        comp->quality = value;
        rebuild = true;
    }
    return true;
}


void LottieLoader::set(const AssetResolver* resolver)
{
    builder->resolver = resolver;
}
