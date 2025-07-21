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

#ifndef _TVG_LOTTIE_PROPERTY_H_
#define _TVG_LOTTIE_PROPERTY_H_

#include <algorithm>
#include "tvgMath.h"
#include "tvgStr.h"
#include "tvgLottieData.h"
#include "tvgLottieInterpolator.h"
#include "tvgLottieExpressions.h"
#include "tvgLottieModifier.h"


struct LottieFont;
struct LottieLayer;
struct LottieObject;
struct LottieProperty;


//default keyframe updates condition (no tweening)
#define DEFAULT_COND (!tween.active || !frames || (frames->count == 1))


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
        return tvg::lerp(value, next->value, t);
    }

    float angle(LottieScalarFrame* next, float frameNo)
    {
        return 0.0f;
    }

    void prepare(TVG_UNUSED LottieScalarFrame* next)
    {
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
            return bz.at(bz.atApprox(t * length, length));
        } else {
            return tvg::lerp(value, next->value, t);
        }
    }

    float angle(LottieVectorFrame* next, float frameNo)
    {
        if (!hasTangent) {
            Point dp = next->value - value;
            return rad2deg(tvg::atan2(dp.y, dp.x));
        }

        auto t = (frameNo - no) / (next->no - no);
        if (interpolator) t = interpolator->progress(t);
        Bezier bz = {value, value + outTangent, next->value + inTangent, next->value};
        t = bz.atApprox(t * length, length);
        return bz.angle(t >= 1.0f ? 0.99f : (t <= 0.0f ? 0.01f : t));
    }

    void prepare(LottieVectorFrame* next)
    {
        Bezier bz = {value, value + outTangent, next->value + inTangent, next->value};
        length = bz.lengthApprox();
    }
};


struct LottieExpression
{
    enum LoopMode : uint8_t { None = 0, InCycle = 1, InPingPong, InOffset, InContinue, OutCycle, OutPingPong, OutOffset, OutContinue };

    //writable expressions variable name and value.
    struct Writable {
        char* var;
        float val;
    };

    char* code;
    LottieComposition* comp;
    LottieLayer* layer;
    LottieObject* object;
    LottieProperty* property;
    Array<Writable> writables;
    bool disabled = false;

    struct {
        uint32_t key = 0;      //the keyframe number repeating to
        float in = FLT_MAX;    //looping duration in frame number
        LoopMode mode = None;
    } loop;

    LottieExpression() {}

    LottieExpression(const LottieExpression* rhs)
    {
        code = strdup(rhs->code);
        comp = rhs->comp;
        layer = rhs->layer;
        object = rhs->object;
        property = rhs->property;
        disabled = rhs->disabled;
    }

    ~LottieExpression()
    {
        ARRAY_FOREACH(p, writables) {
            tvg::free(p->var);
        }
        tvg::free(code);
    }

    bool assign(const char* var, float val)
    {
        //overwrite the existing value
        ARRAY_FOREACH(p, writables) {
            if (tvg::equal(var, p->var)) {
                p->val = val;
                return true;
            }
        }
        writables.push({tvg::duplicate(var), val});
        return true;
    }
};


//Property would have an either keyframes or single value.
struct LottieProperty
{
    enum class Type : uint8_t {Invalid = 0, Integer, Float, Scalar, Vector, PathSet, Color, Opacity, ColorStop, TextDoc, Image};

    LottieExpression* exp = nullptr;
    Type type;
    uint8_t ix;  //property index

    LottieProperty(Type type = Type::Invalid) : type(type) {}
    virtual ~LottieProperty() {}

    virtual uint32_t frameCnt() = 0;
    virtual uint32_t nearest(float frameNo) = 0;
    virtual float frameNo(int32_t key) = 0;

    bool copy(LottieProperty* rhs, bool shallow)
    {
        type = rhs->type;
        ix = rhs->ix;

        if (!rhs->exp) return false;
        if (shallow) {
            exp = rhs->exp;
            rhs->exp = nullptr;
        } else {
            exp = new LottieExpression(rhs->exp);
        }
        exp->property = this;
        return true;
    }
};


static void _copy(PathSet* pathset, Array<Point>& out, Matrix* transform)
{
    if (transform) {
        for (int i = 0; i < pathset->ptsCnt; ++i) {
            out.push(pathset->pts[i] * *transform);
        }
    } else {
        Array<Point> in;
        in.data = pathset->pts;
        in.count = pathset->ptsCnt;
        out.push(in);
        in.data = nullptr;
    }
}


static void _copy(PathSet* pathset, Array<PathCommand>& out)
{
    Array<PathCommand> in;
    in.data = pathset->cmds;
    in.count = pathset->cmdsCnt;
    out.push(in);
    in.data = nullptr;
}


template<typename T>
uint32_t _bsearch(T* frames, float frameNo)
{
    int32_t low = 0;
    int32_t high = int32_t(frames->count) - 1;

    while (low <= high) {
        auto mid = low + (high - low) / 2;
        auto frame = frames->data + mid;
        if (frameNo < frame->no) high = mid - 1;
        else low = mid + 1;
    }
    if (high < low) low = high;
    if (low < 0) low = 0;
    return low;
}


template<typename T>
uint32_t _nearest(T* frames, float frameNo)
{
    if (frames) {
        auto key = _bsearch(frames, frameNo);
        if (key == frames->count - 1) return key;
        return (fabsf(frames->data[key].no - frameNo) < fabsf(frames->data[key + 1].no - frameNo)) ? key : (key + 1);
    }
    return 0;
}


template<typename T>
float _frameNo(T* frames, int32_t key)
{
    if (!frames) return 0.0f;
    if (key < 0) key = 0;
    if (key >= (int32_t) frames->count) key = (int32_t)(frames->count - 1);
    return (*frames)[key].no;
}


template<typename T>
float _loop(T* frames, float frameNo, LottieExpression* exp)
{
    if (!frames) return frameNo;
    if (exp->loop.mode == LottieExpression::LoopMode::None) return frameNo;

    if (frameNo >= exp->loop.in || frameNo < frames->first().no || frameNo < frames->last().no) return frameNo;

    frameNo -= frames->first().no;

    switch (exp->loop.mode) {
        case LottieExpression::LoopMode::InCycle: {
            return fmodf(frameNo, frames->last().no - frames->first().no) + (*frames)[exp->loop.key].no;
        }
        case LottieExpression::LoopMode::InPingPong: {
            auto range = frames->last().no - (*frames)[exp->loop.key].no;
            auto forward = (static_cast<int>(frameNo / range) % 2) == 0 ? true : false;
            frameNo = fmodf(frameNo, range);
            return (forward ? frameNo : (range - frameNo)) + (*frames)[exp->loop.key].no;
        }
        case LottieExpression::LoopMode::OutCycle: {
            return fmodf(frameNo, (*frames)[frames->count - 1 - exp->loop.key].no - frames->first().no) + frames->first().no;
        }
        case LottieExpression::LoopMode::OutPingPong: {
            auto range = (*frames)[frames->count - 1 - exp->loop.key].no - frames->first().no;
            auto forward = (static_cast<int>(frameNo / range) % 2) == 0 ? true : false;
            frameNo = fmodf(frameNo, range);
            return (forward ? frameNo : (range - frameNo)) + frames->first().no;
        }
        default: break;
    }
    return frameNo;
}


template<typename Frame, typename Value, LottieProperty::Type PType = LottieProperty::Type::Invalid, bool Scalar = 1>
struct LottieGenericProperty : LottieProperty
{
    using MyProperty = LottieGenericProperty<Frame, Value, PType, Scalar>;

    //Property has an either keyframes or single value.
    Array<Frame>* frames = nullptr;
    Value value;

    LottieGenericProperty(Value v) : LottieProperty(PType), value(v) {}

    LottieGenericProperty() : LottieProperty(PType) {}

    LottieGenericProperty(const MyProperty& rhs)
    {
        copy(const_cast<MyProperty&>(rhs));
    }

    ~LottieGenericProperty()
    {
        release();
    }

    void release()
    {
        delete(frames);
        frames = nullptr;
        if (exp) {
            delete(exp);
            exp = nullptr;
        }
    }

    uint32_t nearest(float frameNo) override
    {
        return _nearest(frames, frameNo);
    }

    uint32_t frameCnt() override
    {
        return frames ? frames->count : 1;
    }

    float frameNo(int32_t key) override
    {
        return _frameNo(frames, key);
    }

    Frame& newFrame()
    {
        if (!frames) frames = new Array<Frame>;
        if (frames->count + 1 >= frames->reserved) {
            auto old = frames->reserved;
            frames->grow(frames->count + 2);
            memset((void*)(frames->data + old), 0x00, sizeof(Frame) * (frames->reserved - old));
        }
        ++frames->count;
        return frames->last();
    }

    Frame& nextFrame()
    {
        return (*frames)[frames->count];
    }

    Value operator()(float frameNo, LottieExpressions* exps = nullptr)
    {
        //overriding with expressions
        if (exps && exp) {
            Value out{};
            frameNo = _loop(frames, frameNo, exp);
            if (exps->result<MyProperty>(frameNo, out, exp)) return out;
        }

        if (!frames) return value;
        if (frames->count == 1 || frameNo <= frames->first().no) return frames->first().value;
        if (frameNo >= frames->last().no) return frames->last().value;

        auto frame = frames->data + _bsearch(frames, frameNo);
        if (tvg::equal(frame->no, frameNo)) return frame->value;
        return frame->interpolate(frame + 1, frameNo);
    }

    Value operator()(float frameNo, Tween& tween, LottieExpressions* exps)
    {
        if (DEFAULT_COND) return operator()(frameNo, exps);
        return tvg::lerp(operator()(frameNo, exps), operator()(tween.frameNo, exps), tween.progress);
    }

    void copy(MyProperty& rhs, bool shallow = true)
    {
        if (LottieProperty::copy(&rhs, shallow)) return;

        if (rhs.frames) {
            if (shallow) {
                frames = rhs.frames;
                rhs.frames = nullptr;
            } else {
                frames = new Array<Frame>;
                *frames = *rhs.frames;
            }
        } else {
            frames = nullptr;
            value = rhs.value;
        }
    }

    float angle(float frameNo)
    {
        if (!frames || frames->count == 1) return 0;

        if (frameNo <= frames->first().no) return frames->first().angle(frames->data + 1, frames->first().no);
        if (frameNo >= frames->last().no) {
            auto frame = frames->data + frames->count - 2;
            return frame->angle(frame + 1, frames->last().no);
        }

        auto frame = frames->data + _bsearch(frames, frameNo);
        return frame->angle(frame + 1, frameNo);
    }

    float angle(float frameNo, Tween& tween)
    {
        if (DEFAULT_COND) return angle(frameNo);
        return tvg::lerp(angle(frameNo), angle(tween.frameNo), tween.progress);
    }

    void prepare()
    {
        if (Scalar) return;
        if (!frames || frames->count < 2) return;
        for (auto frame = frames->begin() + 1; frame < frames->end(); ++frame) {
            (frame - 1)->prepare(frame);
        }
    }
};


struct LottiePathSet : LottieProperty
{
    Array<LottieScalarFrame<PathSet>>* frames = nullptr;
    PathSet value;

    LottiePathSet() : LottieProperty(LottieProperty::Type::PathSet) {}

    ~LottiePathSet()
    {
        release();
    }

    void release()
    {
        if (exp) {
            delete(exp);
            exp = nullptr;
        }

        tvg::free(value.cmds);
        tvg::free(value.pts);

        if (!frames) return;

        ARRAY_FOREACH(p, *frames) {
            tvg::free((*p).value.cmds);
            tvg::free((*p).value.pts);
        }
        tvg::free(frames->data);
        tvg::free(frames);
    }

    uint32_t nearest(float frameNo) override
    {
        return _nearest(frames, frameNo);
    }

    uint32_t frameCnt() override
    {
        return frames ? frames->count : 1;
    }

    float frameNo(int32_t key) override
    {
        return _frameNo(frames, key);
    }

    LottieScalarFrame<PathSet>& newFrame()
    {
        if (!frames) {
            frames = tvg::calloc<Array<LottieScalarFrame<PathSet>>*>(1, sizeof(Array<LottieScalarFrame<PathSet>>));
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

    //return false means requiring the interpolation
    bool dispatch(float frameNo, PathSet*& path, LottieScalarFrame<PathSet>*& frame, float& t)
    {
        if (!frames) path = &value;
        else if (frames->count == 1 || frameNo <= frames->first().no) path = &frames->first().value;
        else if (frameNo >= frames->last().no) path = &frames->last().value;
        else {
            frame = frames->data + _bsearch(frames, frameNo);
            if (tvg::equal(frame->no, frameNo)) path = &frame->value;
            else if (frame->value.ptsCnt != (frame + 1)->value.ptsCnt) {
                path = &frame->value;
            } else {
                t = (frameNo - frame->no) / ((frame + 1)->no - frame->no);
                if (frame->interpolator) t = frame->interpolator->progress(t);
                if (frame->hold) path = &(frame + ((t < 1.0f) ? 0 : 1))->value;
                else return false;
            }
        }
        return true;
    }

    bool modifiedPath(float frameNo, RenderPath& out, Matrix* transform, LottieModifier* modifier)
    {
        PathSet* path;
        LottieScalarFrame<PathSet>* frame;
        RenderPath temp;
        float t;

        if (dispatch(frameNo, path, frame, t)) {
            if (modifier) return modifier->modifyPath(path->cmds, path->cmdsCnt, path->pts, path->ptsCnt, transform, out);
            _copy(path, out.cmds);
            _copy(path, out.pts, transform);
            return true;
        }

        //interpolate 2 frames
        auto s = frame->value.pts;
        auto e = (frame + 1)->value.pts;
        auto interpPts = tvg::malloc<Point*>(frame->value.ptsCnt * sizeof(Point));
        auto p = interpPts;

        for (auto i = 0; i < frame->value.ptsCnt; ++i, ++s, ++e, ++p) {
            *p = tvg::lerp(*s, *e, t);
            if (transform) *p *= *transform;
        }

        if (modifier) modifier->modifyPath(frame->value.cmds, frame->value.cmdsCnt, interpPts, frame->value.ptsCnt, nullptr, out);

        tvg::free(interpPts);

        return true;
    }

    bool defaultPath(float frameNo, RenderPath& out, Matrix* transform)
    {
        PathSet* path;
        LottieScalarFrame<PathSet>* frame;
        float t;

        if (dispatch(frameNo, path, frame, t)) {
            _copy(path, out.cmds);
            _copy(path, out.pts, transform);
            return true;
        }

        //interpolate 2 frames
        auto s = frame->value.pts;
        auto e = (frame + 1)->value.pts;

        for (auto i = 0; i < frame->value.ptsCnt; ++i, ++s, ++e) {
            auto pt = tvg::lerp(*s, *e, t);
            if (transform) pt *= *transform;
            out.pts.push(pt);
        }
        _copy(&frame->value, out.cmds);
        return true;
    }

    bool tweening(float frameNo, RenderPath& out, Matrix* transform, LottieModifier* modifier, Tween& tween, LottieExpressions* exps)
    {
        RenderPath to;  //used as temp as well.
        auto pivot = out.pts.count;
        if (!operator()(frameNo, out, transform, exps)) return false;
        if (!operator()(tween.frameNo, to, transform, exps)) return false;

        auto from = out.pts.data + pivot;
        if (to.pts.count != out.pts.count - pivot) TVGLOG("LOTTIE", "Tweening has different numbers of points in consecutive frames.");

        for (uint32_t i = 0; i < std::min(to.pts.count, (out.pts.count - pivot)); ++i) {
            from[i] = tvg::lerp(from[i], to.pts[i], tween.progress);
        }

        if (!modifier) return true;

        //Apply modifiers
        to.clear();
        return modifier->modifyPath(to.cmds.data, to.cmds.count, to.pts.data, to.pts.count, transform, out);
    }

    bool operator()(float frameNo, RenderPath& out, Matrix* transform, LottieExpressions* exps, LottieModifier* modifier = nullptr)
    {
        //overriding with expressions
        if (exps && exp) {
            frameNo = _loop(frames, frameNo, exp);
            if (exps->result<LottiePathSet>(frameNo, out, transform, modifier, exp)) return true;
        }

        if (modifier) return modifiedPath(frameNo, out, transform, modifier);
        else return defaultPath(frameNo, out, transform);
    }

    bool operator()(float frameNo, RenderPath& out, Matrix* transform, Tween& tween, LottieExpressions* exps, LottieModifier* modifier = nullptr)
    {
        if (DEFAULT_COND) return operator()(frameNo, out, transform, exps, modifier);
        return tweening(frameNo, out, transform, modifier, tween, exps);
    }
};


struct LottieColorStop : LottieProperty
{
    Array<LottieScalarFrame<ColorStop>>* frames = nullptr;
    ColorStop value;
    uint16_t count = 0;     //colorstop count
    bool populated = false;

    LottieColorStop() : LottieProperty(LottieProperty::Type::ColorStop) {}

    LottieColorStop(const LottieColorStop& rhs)
    {
        copy(const_cast<LottieColorStop&>(rhs));
    }

    ~LottieColorStop()
    {
        release();
    }

    void release()
    {
        if (exp) {
            delete(exp);
            exp = nullptr;
        }

        if (value.data) {
            tvg::free(value.data);
            value.data = nullptr;
        }

        if (!frames) return;

        ARRAY_FOREACH(p, *frames) {
            tvg::free((*p).value.data);
        }
        tvg::free(frames->data);
        tvg::free(frames);
        frames = nullptr;
    }

    uint32_t nearest(float frameNo) override
    {
        return _nearest(frames, frameNo);
    }

    uint32_t frameCnt() override
    {
        return frames ? frames->count : 1;
    }

    float frameNo(int32_t key) override
    {
        return _frameNo(frames, key);
    }

    LottieScalarFrame<ColorStop>& newFrame()
    {
        if (!frames) {
            frames = tvg::calloc<Array<LottieScalarFrame<ColorStop>>*>(1, sizeof(Array<LottieScalarFrame<ColorStop>>));
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

    Result tweening(float frameNo, Fill* fill, Tween& tween, LottieExpressions* exps)
    {
        auto frame = frames->data + _bsearch(frames, frameNo);
        if (tvg::equal(frame->no, frameNo)) return fill->colorStops(frame->value.data, count);

        //from
        operator()(frameNo, fill, exps);

        //to
        auto dup = fill->duplicate();
        operator()(tween.frameNo, dup, exps);

        //interpolate
        const Fill::ColorStop* from;
        auto fromCnt = fill->colorStops(&from);

        const Fill::ColorStop* to;
        auto toCnt = fill->colorStops(&to);

        if (fromCnt != toCnt) TVGLOG("LOTTIE", "Tweening has different numbers of color data in consecutive frames.");

        for (uint32_t i = 0; i < std::min(fromCnt, toCnt); ++i) {
            const_cast<Fill::ColorStop*>(from)->offset = tvg::lerp(from->offset, to->offset, tween.progress);
            const_cast<Fill::ColorStop*>(from)->r = tvg::lerp(from->r, to->r, tween.progress);
            const_cast<Fill::ColorStop*>(from)->g = tvg::lerp(from->g, to->g, tween.progress);
            const_cast<Fill::ColorStop*>(from)->b = tvg::lerp(from->b, to->b, tween.progress);
            const_cast<Fill::ColorStop*>(from)->a = tvg::lerp(from->a, to->a, tween.progress);
        }

        return Result::Success;
    }

    Result operator()(float frameNo, Fill* fill, LottieExpressions* exps = nullptr)
    {
        //overriding with expressions
        if (exps && exp) {
            frameNo = _loop(frames, frameNo, exp);
            if (exps->result<LottieColorStop>(frameNo, fill, exp)) return Result::Success;
        }

        if (!frames) return fill->colorStops(value.data, count);

        if (frames->count == 1 || frameNo <= frames->first().no) {
            return fill->colorStops(frames->first().value.data, count);
        }

        if (frameNo >= frames->last().no) return fill->colorStops(frames->last().value.data, count);

        auto frame = frames->data + _bsearch(frames, frameNo);
        if (tvg::equal(frame->no, frameNo)) return fill->colorStops(frame->value.data, count);

        //interpolate
        auto t = (frameNo - frame->no) / ((frame + 1)->no - frame->no);
        if (frame->interpolator) t = frame->interpolator->progress(t);

        if (frame->hold) {
            if (t < 1.0f) fill->colorStops(frame->value.data, count);
            else fill->colorStops((frame + 1)->value.data, count);
        }

        auto s = frame->value.data;
        auto e = (frame + 1)->value.data;

        Array<Fill::ColorStop> result;

        for (auto i = 0; i < count; ++i, ++s, ++e) {
            auto offset = tvg::lerp(s->offset, e->offset, t);
            auto r = tvg::lerp(s->r, e->r, t);
            auto g = tvg::lerp(s->g, e->g, t);
            auto b = tvg::lerp(s->b, e->b, t);
            auto a = tvg::lerp(s->a, e->a, t);
            result.push({offset, r, g, b, a});
        }
        return fill->colorStops(result.data, count);
    }

    Result operator()(float frameNo, Fill* fill, Tween& tween, LottieExpressions* exps)
    {
        if (DEFAULT_COND) return operator()(frameNo, fill, exps);
        return tweening(frameNo, fill, tween, exps);
    }

    void copy(LottieColorStop& rhs, bool shallow = true)
    {
        if (LottieProperty::copy(&rhs, shallow)) return;

        if (rhs.frames) {
            if (shallow) {
                frames = rhs.frames;
                rhs.frames = nullptr;
            } else {
                frames = new Array<LottieScalarFrame<ColorStop>>;
                *frames = *rhs.frames;
            }
        } else {
            frames = nullptr;
            value = rhs.value;
            rhs.value = ColorStop();
        }
        populated = rhs.populated;
        count = rhs.count;
    }

    void prepare() {}
};


struct LottieTextDoc : LottieProperty
{
    Array<LottieScalarFrame<TextDocument>>* frames = nullptr;
    TextDocument value;

    LottieTextDoc() : LottieProperty(LottieProperty::Type::TextDoc) {}

    LottieTextDoc(const LottieTextDoc& rhs)
    {
        copy(const_cast<LottieTextDoc&>(rhs));
    }

    ~LottieTextDoc()
    {
        release();
    }

    void release()
    {
        if (exp) {
            delete(exp);
            exp = nullptr;
        }

        if (value.text) {
            tvg::free(value.text);
            value.text = nullptr;
        }
        if (value.name) {
            tvg::free(value.name);
            value.name = nullptr;
        }

        if (!frames) return;

        ARRAY_FOREACH(p, *frames) {
            tvg::free((*p).value.text);
            tvg::free((*p).value.name);
        }
        delete(frames);
        frames = nullptr;
    }

    uint32_t nearest(float frameNo) override
    {
        return _nearest(frames, frameNo);
    }

    uint32_t frameCnt() override
    {
        return frames ? frames->count : 1;
    }

    float frameNo(int32_t key) override
    {
        return _frameNo(frames, key);
    }

    LottieScalarFrame<TextDocument>& newFrame()
    {
        if (!frames) frames = new Array<LottieScalarFrame<TextDocument>>;
        if (frames->count + 1 >= frames->reserved) {
            auto old = frames->reserved;
            frames->grow(frames->count + 2);
            memset((void*)(frames->data + old), 0x00, sizeof(LottieScalarFrame<TextDocument>) * (frames->reserved - old));
        }
        ++frames->count;
        return frames->last();
    }

    LottieScalarFrame<TextDocument>& nextFrame()
    {
        return (*frames)[frames->count];
    }

    TextDocument& operator()(float frameNo)
    {
        if (!frames) return value;
        if (frames->count == 1 || frameNo <= frames->first().no) return frames->first().value;
        if (frameNo >= frames->last().no) return frames->last().value;

        auto frame = frames->data + _bsearch(frames, frameNo);
        return frame->value;
    }

    TextDocument& operator()(float frameNo, LottieExpressions* exps)
    {
        auto& out = operator()(frameNo);

        //overriding with expressions
        if (exps && exp) {
            frameNo = _loop(frames, frameNo, exp);
            exps->result(frameNo, out, exp);
        }

        return out;
    }

    void copy(LottieTextDoc& rhs, bool shallow = true)
    {
        if (LottieProperty::copy(&rhs, shallow)) return;

        if (rhs.frames) {
            if (shallow) {
                frames = rhs.frames;
                rhs.frames = nullptr;
            } else {
                frames = new Array<LottieScalarFrame<TextDocument>>;
                *frames = *rhs.frames;
            }
        } else {
            frames = nullptr;
            value = rhs.value;
            rhs.value.text = nullptr;
            rhs.value.name = nullptr;
        }
    }

    void prepare() {}
};


struct LottieBitmap : LottieProperty
{
    union {
        char* b64Data = nullptr;
        char* path;
    };
    char* mimeType = nullptr;
    uint32_t size = 0;
    float width = 0.0f;
    float height = 0.0f;

    LottieBitmap() : LottieProperty(LottieProperty::Type::Image) {}

    LottieBitmap(const LottieBitmap& rhs)
    {
        copy(const_cast<LottieBitmap&>(rhs));
    }

    ~LottieBitmap()
    {
        release();
    }

    void release()
    {
        tvg::free(b64Data);
        tvg::free(mimeType);

        b64Data = nullptr;
        mimeType = nullptr;
    }

    uint32_t frameCnt() override { return 0; }
    uint32_t nearest(float frameNo) override { return 0; }
    float frameNo(int32_t key) override { return 0; }

    void copy(LottieBitmap& rhs, bool shallow = true)
    {
        if (LottieProperty::copy(&rhs, shallow)) return;

        if (shallow) {
            b64Data = rhs.b64Data;
            mimeType = rhs.mimeType;
            rhs.b64Data = nullptr;
            rhs.mimeType = nullptr;
        } else {
            //TODO: optimize here by avoiding data copy
            TVGLOG("LOTTIE", "Shallow copy of the image data!");
            b64Data = duplicate(rhs.b64Data);
            if (rhs.mimeType) mimeType = duplicate(rhs.mimeType);
        }
        size = rhs.size;
        width = rhs.width;
        height = rhs.height;
    }
};

using LottieFloat = LottieGenericProperty<LottieScalarFrame<float>, float, LottieProperty::Type::Float>;
using LottieInteger = LottieGenericProperty<LottieScalarFrame<int8_t>, int8_t, LottieProperty::Type::Integer>;
using LottieScalar = LottieGenericProperty<LottieScalarFrame<Point>, Point, LottieProperty::Type::Scalar>;
using LottieVector = LottieGenericProperty<LottieVectorFrame<Point>, Point, LottieProperty::Type::Vector, 0>;
using LottieColor = LottieGenericProperty<LottieScalarFrame<RGB32>, RGB32, LottieProperty::Type::Color>;
using LottieOpacity = LottieGenericProperty<LottieScalarFrame<uint8_t>, uint8_t, LottieProperty::Type::Opacity>;

#endif //_TVG_LOTTIE_PROPERTY_H_
