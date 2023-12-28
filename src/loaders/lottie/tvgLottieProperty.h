/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

#ifndef _TVG_LOTTIE_PROPERTY_H_
#define _TVG_LOTTIE_PROPERTY_H_

#include "tvgCommon.h"
#include "tvgArray.h"
#include "tvgMath.h"
#include "tvgBezier.h"
#include "tvgLottieInterpolator.h"

struct PathSet
{
    Point* pts;
    PathCommand* cmds;
    uint16_t ptsCnt;
    uint16_t cmdsCnt;
};


struct RGB24
{
    int32_t rgb[3];
};


struct ColorStop
{
    Fill::ColorStop* data = nullptr;
    Array<float>* input = nullptr;
};


static inline RGB24 operator-(const RGB24& lhs, const RGB24& rhs)
{
    return {lhs.rgb[0] - rhs.rgb[0], lhs.rgb[1] - rhs.rgb[1], lhs.rgb[2] - rhs.rgb[2]};
}


static inline RGB24 operator+(const RGB24& lhs, const RGB24& rhs)
{
    return {lhs.rgb[0] + rhs.rgb[0], lhs.rgb[1] + rhs.rgb[1], lhs.rgb[2] + rhs.rgb[2]};
}


static inline RGB24 operator*(const RGB24& lhs, float rhs)
{
    return {(int32_t)lroundf(lhs.rgb[0] * rhs), (int32_t)lroundf(lhs.rgb[1] * rhs), (int32_t)lroundf(lhs.rgb[2] * rhs)};
}


static void copy(PathSet& pathset, Array<Point>& outPts)
{
    Array<Point> inPts;
    inPts.data = pathset.pts;
    inPts.count = pathset.ptsCnt;
    outPts.push(inPts);
    inPts.data = nullptr;
}


static void copy(PathSet& pathset, Array<PathCommand>& outCmds)
{
    Array<PathCommand> inCmds;
    inCmds.data = pathset.cmds;
    inCmds.count = pathset.cmdsCnt;
    outCmds.push(inCmds);
    inCmds.data = nullptr;
}


template<typename T>
struct LottieScalarFrame
{
    T value;                    //keyframe value
    float no;                   //frame number
    LottieInterpolator* interpolator;
    bool hold = false;           //do not interpolate.

    T interpolate(LottieScalarFrame<T>* next, float frameNo)
    {
        auto t = (frameNo - no) / (next->no - no);
        if (interpolator) t = interpolator->progress(t);

        if (hold) {
            if (t < 1.0f) return value;
            else return next->value;
        }
        return mathLerp(value, next->value, t);
    }
};


template<typename T>
struct LottieVectorFrame
{
    T value;                    //keyframe value
    float no;                   //frame number
    LottieInterpolator* interpolator;
    T outTangent, inTangent;
    float length;
    bool hasTangent = false;
    bool hold = false;

    T interpolate(LottieVectorFrame* next, float frameNo)
    {
        auto t = (frameNo - no) / (next->no - no);
        if (interpolator) t = interpolator->progress(t);

        if (hold) {
            if (t < 1.0f) return value;
            else return next->value;
        }

        if (hasTangent) {
            Bezier bz = {value, value + outTangent, next->value + inTangent, next->value};
            t = bezAt(bz, t * length, length);
            return bezPointAt(bz, t);
        } else {
            return mathLerp(value, next->value, t);
        }
    }

    float angle(LottieVectorFrame* next, float frameNo)
    {
        if (!hasTangent) return 0;
        auto t = (frameNo - no) / (next->no - no);
        if (interpolator) t = interpolator->progress(t);
        Bezier bz = {value, value + outTangent, next->value + inTangent, next->value};
        t = bezAt(bz, t * length, length);
        return -bezAngleAt(bz, t);
    }

    void prepare(LottieVectorFrame* next)
    {
        Bezier bz = {value, value + outTangent, next->value + inTangent, next->value};
        length = bezLength(bz);
    }
};


template<typename T>
struct LottieProperty
{
    //Property has an either keyframes or single value.
    Array<LottieScalarFrame<T>>* frames = nullptr;
    T value;

    LottieProperty(T v) : value(v) {}

    ~LottieProperty()
    {
        delete(frames);
    }

    LottieScalarFrame<T>& newFrame()
    {
        if (!frames) frames = new Array<LottieScalarFrame<T>>;
        if (frames->count + 1 >= frames->reserved) {
            auto old = frames->reserved;
            frames->grow(frames->count + 2);
            memset((void*)(frames->data + old), 0x00, sizeof(LottieScalarFrame<T>) * (frames->reserved - old));
        }
        ++frames->count;
        return frames->last();
    }

    LottieScalarFrame<T>& nextFrame()
    {
        return (*frames)[frames->count];
    }

    T operator()(float frameNo)
    {
        if (!frames) return value;
        if (frames->count == 1 || frameNo <= frames->first().no) return frames->first().value;
        if (frameNo >= frames->last().no) return frames->last().value;

        uint32_t low = 1;
        uint32_t high = frames->count - 1;

        while (low <= high) {
            auto mid = low + (high - low) / 2;
            auto frame = frames->data + mid;
            if (mathEqual(frameNo, frame->no)) return frame->value;
            else if (frameNo > frame->no) low = mid + 1;
            else high = mid - 1;
        }

        auto frame = frames->data + low;
        return (frame - 1)->interpolate(frame, frameNo);
    }

    float angle(float frameNo) { return 0; }
    void prepare() {}
};


struct LottiePathSet
{
    Array<LottieScalarFrame<PathSet>>* frames = nullptr;
    PathSet value;

    LottiePathSet(PathSet v) : value(v)
    {
    }

    ~LottiePathSet()
    {
        free(value.cmds);
        free(value.pts);

        if (!frames) return;
        for (auto p = frames->data; p < frames->end(); ++p) {
            free((*p).value.cmds);
            free((*p).value.pts);
        }
        free(frames->data);
        free(frames);
    }

    LottieScalarFrame<PathSet>& newFrame()
    {
        if (!frames) {
            frames = static_cast<Array<LottieScalarFrame<PathSet>>*>(calloc(1, sizeof(Array<LottieScalarFrame<PathSet>>)));
        }
        if (frames->count + 1 >= frames->reserved) {
            auto old = frames->reserved;
            frames->grow(frames->count + 2);
            memset((void*)(frames->data + old), 0x00, sizeof(LottieScalarFrame<PathSet>) * (frames->reserved - old));
        }
        ++frames->count;
        return frames->last();
    }

    LottieScalarFrame<PathSet>& nextFrame()
    {
        return (*frames)[frames->count];
    }

    bool operator()(float frameNo, Array<PathCommand>& cmds, Array<Point>& pts)
    {
        if (!frames) {
            copy(value, cmds);
            copy(value, pts);
            return true;
        }

        if (frames->count == 1 || frameNo <= frames->first().no) {
            copy(frames->first().value, cmds);
            copy(frames->first().value, pts);
            return true;
        }

        if (frameNo >= frames->last().no) {
            copy(frames->last().value, cmds);
            copy(frames->last().value, pts);
            return true;
        }

        uint32_t low = 1;
        uint32_t high = frames->count - 1;

        while (low <= high) {
            auto mid = low + (high - low) / 2;
            auto frame = frames->data + mid;
            if (mathEqual(frameNo, frame->no)) {
                copy(frame->value, cmds);
                copy(frame->value, pts);
                return true;
            } else if (frameNo > frame->no) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }

        //interpolate
        auto frame = frames->data + low;
        auto pframe = frame - 1;
        copy(pframe->value, cmds);

        auto t = (frameNo - pframe->no) / (frame->no - pframe->no);
        if (pframe->interpolator) t = pframe->interpolator->progress(t);

        if (pframe->hold) {
            if (t < 1.0f) copy(pframe->value, pts);
            else copy(frame->value, pts);
            return true;
        }

        auto s = pframe->value.pts;
        auto e = frame->value.pts;

        for (auto i = 0; i < pframe->value.ptsCnt; ++i, ++s, ++e) {
            pts.push(mathLerp(*s, *e, t));
        }
        return true;
    }

    void prepare() {}
};


struct LottieColorStop
{
    Array<LottieScalarFrame<ColorStop>>* frames = nullptr;
    ColorStop value;
    uint16_t count = 0;     //colorstop count

    ~LottieColorStop()
    {
        free(value.data);
        if (!frames) return;
        for (auto p = frames->data; p < frames->end(); ++p) {
            free((*p).value.data);
        }
        free(frames->data);
        free(frames);
    }

    LottieScalarFrame<ColorStop>& newFrame()
    {
        if (!frames) {
            frames = static_cast<Array<LottieScalarFrame<ColorStop>>*>(calloc(1, sizeof(Array<LottieScalarFrame<ColorStop>>)));
        }
        if (frames->count + 1 >= frames->reserved) {
            auto old = frames->reserved;
            frames->grow(frames->count + 2);
            memset((void*)(frames->data + old), 0x00, sizeof(LottieScalarFrame<ColorStop>) * (frames->reserved - old));
        }
        ++frames->count;
        return frames->last();
    }

    LottieScalarFrame<ColorStop>& nextFrame()
    {
        return (*frames)[frames->count];
    }

    void operator()(float frameNo, Fill* fill)
    {
        if (!frames) {
            fill->colorStops(value.data, count);
            return;
        }

        if (frames->count == 1 || frameNo <= frames->first().no) {
            fill->colorStops(frames->first().value.data, count);
            return;
        }

        if (frameNo >= frames->last().no) {
            fill->colorStops(frames->last().value.data, count);
            return;
        }

        uint32_t low = 1;
        uint32_t high = frames->count - 1;

        while (low <= high) {
            auto mid = low + (high - low) / 2;
            auto frame = frames->data + mid;
            if (mathEqual(frameNo, frame->no)) {
                fill->colorStops(frame->value.data, count);
                return;
            } else if (frameNo > frame->no) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }

        //interpolate
        auto frame = frames->data + low;
        auto pframe = frame - 1;
        auto t = (frameNo - pframe->no) / (frame->no - pframe->no);
        if (pframe->interpolator) t = pframe->interpolator->progress(t);

        if (pframe->hold) {
            if (t < 1.0f) fill->colorStops(pframe->value.data, count);
            else fill->colorStops(frame->value.data, count);
        }

        auto s = pframe->value.data;
        auto e = frame->value.data;

        Array<Fill::ColorStop> result;

        for (auto i = 0; i < count; ++i, ++s, ++e) {
            auto offset = mathLerp(s->offset, e->offset, t);
            auto r = mathLerp(s->r, e->r, t);
            auto g = mathLerp(s->g, e->g, t);
            auto b = mathLerp(s->b, e->b, t);
            auto a = mathLerp(s->a, e->a, t);
            result.push({offset, r, g, b, a});
        }
        fill->colorStops(result.data, count);
    }

    void prepare() {}
};


struct LottiePosition
{
    Array<LottieVectorFrame<Point>>* frames = nullptr;
    Point value;

    LottiePosition(Point v) : value(v)
    {
    }

    ~LottiePosition()
    {
        delete(frames);
    }

    LottieVectorFrame<Point>& newFrame()
    {
        if (!frames) frames = new Array<LottieVectorFrame<Point>>;
        if (frames->count + 1 >= frames->reserved) {
            auto old = frames->reserved;
            frames->grow(frames->count + 2);
            memset((void*)(frames->data + old), 0x00, sizeof(LottieVectorFrame<Point>) * (frames->reserved - old));
        }
        ++frames->count;
        return frames->last();
    }

    LottieVectorFrame<Point>& nextFrame()
    {
        return (*frames)[frames->count];
    }

    Point operator()(float frameNo)
    {
        if (!frames) return value;
        if (frames->count == 1 || frameNo <= frames->first().no) return frames->first().value;
        if (frameNo >= frames->last().no) return frames->last().value;

        uint32_t low = 1;
        uint32_t high = frames->count - 1;

        while (low <= high) {
            auto mid = low + (high - low) / 2;
            auto frame = frames->data + mid;
            if (mathEqual(frameNo, frame->no)) return frame->value;
            else if (frameNo > frame->no) low = mid + 1;
            else high = mid - 1;
        }

        auto frame = frames->data + low;
        return (frame - 1)->interpolate(frame, frameNo);
    }

    float angle(float frameNo)
    {
        if (!frames) return 0;
        if (frames->count == 1 || frameNo <= frames->first().no) return 0;
        if (frameNo >= frames->last().no) return 0;

        uint32_t low = 1;
        uint32_t high = frames->count - 1;

        while (low <= high) {
            auto mid = low + (high - low) / 2;
            auto frame = frames->data + mid;
            if (frameNo > frame->no) low = mid + 1;
            else high = mid - 1;
        }

        auto frame = frames->data + low;
        return (frame - 1)->angle(frame, frameNo);
    }

    void prepare()
    {
        if (!frames || frames->count < 2) return;
        for (auto frame = frames->data + 1; frame < frames->end(); ++frame) {
            (frame - 1)->prepare(frame);
        }
    }

};


using LottiePoint = LottieProperty<Point>;
using LottieFloat = LottieProperty<float>;
using LottieOpacity = LottieProperty<uint8_t>;
using LottieColor = LottieProperty<RGB24>;

#endif //_TVG_LOTTIE_PROPERTY_H_