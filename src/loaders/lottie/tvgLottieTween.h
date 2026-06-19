/*
 * Copyright (c) 2026 the ThorVG project. All rights reserved.

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

#ifndef _TVG_LOTTIE_TWEEN_
#define _TVG_LOTTIE_TWEEN_

#include <unordered_map>
#include "tvgMath.h"
#include "tvgLottieCommon.h"

struct LottieProperty;

struct LottieTween
{
private:
    union Value {
        float vFloat;
        int8_t vInteger;
        Point vScalar;
        Point3 vScalar3;
        RGB32 vColor;
        Fill* vFill;
        PathSet vPath = {};
        uint8_t vOpacity;
    };

    struct ValueSet
    {
        Value from;        // "from" scene data
        Value cur;         // current captured scene data
        uint8_t type = 0;  // 0: general, 1: path, 2: fill
    };

    std::unordered_map<LottieProperty*, ValueSet> data;

    void release()
    {
        // delete the fill values
        for (auto& entry : data) {
            auto& value = entry.second;
            if (value.type == 0) {
                continue;
            } else if (value.type == 1) {  // path
                free(value.from.vPath.pts);
                free(value.from.vPath.cmds);
                free(value.cur.vPath.pts);
                free(value.cur.vPath.cmds);
            } else {  // fill
                delete (value.from.vFill);
                delete (value.cur.vFill);
            }
        }
        data.clear();
    }

    void convert(float v, Value& out) { out.vFloat = v; }
    void convert(int8_t v, Value& out) { out.vInteger = v; }
    void convert(const Point& v, Value& out) { out.vScalar = v; }
    void convert(const Point3& v, Value& out) { out.vScalar3 = v; }
    void convert(const RGB32& v, Value& out) { out.vColor = v; }
    void convert(uint8_t v, Value& out) { out.vOpacity = v; }

    void convert(const Value& v, float& out) { out = v.vFloat; }
    void convert(const Value& v, int8_t& out) { out = v.vInteger; }
    void convert(const Value& v, Point& out) { out = v.vScalar; }
    void convert(const Value& v, Point3& out) { out = v.vScalar3; }
    void convert(const Value& v, RGB32& out) { out = v.vColor; }
    void convert(const Value& v, uint8_t& out) { out = v.vOpacity; }


public:
    float to = 0.0f;        // used as "to frame number"
    float progress = 0.0f;  // [0 - 1]
    bool active = false;
    bool inited = false;    // this tweening is just initiated, need a capture
    bool chaining = false;  // if true, dynamic tweening chaining (dynamic tweening -> dynamic tweening)
    bool legacy = false;    // legacy tween mode

    ~LottieTween()
    {
        release();
    }

    // dynamic
    void on(float to)
    {
        this->to = to;
        data.reserve(300);
        active = true;
        if (inited) chaining = true;
        else inited = true;
        legacy = false;
    }

    // legacy
    void on(float to, float progress)
    {
        this->to = to;
        this->progress = progress;
        active = legacy = true;
    }

    void off()
    {
        active = legacy = inited = chaining = false;
        release();
    }

    // swap the "from" with the latest captured data
    void swap(LottieProperty* key)
    {
        auto& set = data[key];
        std::swap(set.from, set.cur);
        inited = false;
    }

    /************************************************************************/
    /* for General Values                                                   */
    /************************************************************************/

    template<typename T>
    void capture(LottieProperty* key, const T& from)
    {
        convert(from, data[key].from);
        inited = false;
    }

    template<typename T>
    T run(LottieProperty* key, const T& to)
    {
        auto& set = data[key];
        T from;
        convert(set.from, from);
        auto cur = tvg::lerp(from, to, progress);
        convert(cur, set.cur);
        return cur;
    }

    /************************************************************************/
    /* for PathSet Value                                                    */
    /************************************************************************/

    void capture(LottieProperty* key, RenderPath& from, uint32_t ptsPivot, uint32_t cmdsPivot)
    {
        auto& set = data[key];
        auto& path = set.from.vPath;
        path.ptsCnt =  from.pts.count - ptsPivot;
        memcpy(&path.pts, from.pts.data + ptsPivot, path.ptsCnt);
        path.cmdsCnt =  from.cmds.count - cmdsPivot;
        memcpy(&path.cmds, from.cmds.data + cmdsPivot, path.cmdsCnt);

        set.type = 1;
        inited = false;
    }

    // interpolate "from" & "to" then out to "toPath"
    void run(LottieProperty* key, RenderPath& toPath, uint32_t ptsPivot, uint32_t cmdsPivot, LottieModifier* modifier)
    {
        auto& set = data[key];
        auto& from = set.from.vPath;
        auto toPtsCnt = (toPath.pts.count - ptsPivot);

        if (from.ptsCnt != toPtsCnt) TVGLOG("LOTTIE", "Tweening has different numbers of points in consecutive frames.");

        // interpolate
        auto out = toPath.pts.data + ptsPivot;
        auto cnt =  std::min(toPtsCnt, (uint32_t)from.ptsCnt);
        for (uint32_t i = 0; i < cnt; ++i) {
            out[i] = tvg::lerp(out[i], toPath.pts[i], progress);
        }

        // TODO: apply modifier
        //if (modifier) modifier->path(toPath, fromPath, nullptr);

        // capture the current
        auto& curPath = set.cur.vPath;
        curPath.ptsCnt = toPath.pts.count - ptsPivot;
        memcpy(&curPath.pts, toPath.pts.data + ptsPivot, cnt);
    }

    /************************************************************************/
    /* for ColorStop Value                                                  */
    /************************************************************************/

    void capture(LottieProperty* key, Fill* from)
    {
        auto& set = data[key];
        set.from.vFill = from->duplicate();
        set.type = 2;
        inited = false;
    }

    // interpolate "from" & "to" then out to "to"
    void run(LottieProperty* key, Fill* toFill)
    {
        auto& set = data[key];
        auto fromFill = set.from.vFill;

        // interpolate
        const Fill::ColorStop* from;
        auto fromCnt = fromFill->colorStops(&from);

        const Fill::ColorStop* to;
        auto toCnt = toFill->colorStops(&to);

        if (fromCnt != toCnt) TVGLOG("LOTTIE", "Tweening has different numbers of color data in consecutive frames.");

        const Fill::ColorStop* out;
        toFill->colorStops(&out);

        for (uint32_t i = 0; i < std::min(fromCnt, toCnt); ++i) {
            const_cast<Fill::ColorStop*>(out)->offset = tvg::lerp(from->offset, to->offset, progress);
            const_cast<Fill::ColorStop*>(out)->r = tvg::lerp(from->r, to->r, progress);
            const_cast<Fill::ColorStop*>(out)->g = tvg::lerp(from->g, to->g, progress);
            const_cast<Fill::ColorStop*>(out)->b = tvg::lerp(from->b, to->b, progress);
            const_cast<Fill::ColorStop*>(out)->a = tvg::lerp(from->a, to->a, progress);
        }

        //capture the current
        delete (set.cur.vFill);
        set.cur.vFill = toFill->duplicate();
    }
};

#endif  //_TVG_LOTTIE_TWEEN_
