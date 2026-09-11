/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

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

#include "tvgMath.h"
#include "tvgLottieCommon.h"
#include "tvgLottieInterpolator.h"
#include "tvgLottieExpressions.h"
#include "tvgLottieModifier.h"
#include "tvgLottieTween.h"

struct LottieLayer;
struct LottieObject;
struct LottieProperty;

//default keyframe updates condition (no tweening)
#define DEFAULT_COND (!tween.active || !frames || (frames->count == 1))

enum Dim : uint8_t { DimX = 0, DimY = 1, DimCnt = 2 };

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

    float angle(LottieScalarFrame* next, float frameNo) { return 0.0f; }
    void prepare(TVG_UNUSED LottieScalarFrame* next) {}
    void setInterpolator(LottieInterpolator* ip, TVG_UNUSED Dim dim) { interpolator = ip; }
};

template<typename T>
struct LottieVectorFrame
{
    T value;                    //keyframe value
    float no;                   //frame number
    LottieInterpolator* interpolator[DimCnt] = {};
    T outTangent, inTangent;
    float length;
    bool hasTangent = false;
    bool hold = false;

    Point progress(float t)
    {
        auto tx = interpolator[DimX] ? interpolator[DimX]->progress(t) : t;
        auto ty = interpolator[DimY] ? interpolator[DimY]->progress(t) : tx;
        return {tx, ty};
    }

    T interpolate(LottieVectorFrame* next, float frameNo)
    {
        auto t = (frameNo - no) / (next->no - no);
        auto p = progress(t);

        if (hold) {
            if (p.x < 1.0f) return value;
            return next->value;
        }

        if (hasTangent) {
            Bezier bz = {value, value + outTangent, next->value + inTangent, next->value};
            return bz.at(bz.atApprox(p.x * length, length));
        }

        return {tvg::lerp(value.x, next->value.x, p.x), tvg::lerp(value.y, next->value.y, p.y)};
    }

    float angle(LottieVectorFrame* next, float frameNo)
    {
        auto t = (frameNo - no) / (next->no - no);

        //spatial bezier
        if (hasTangent) {
            if (interpolator[DimX]) t = interpolator[DimX]->progress(t);
            Bezier bz = {value, value + outTangent, next->value + inTangent, next->value};
            t = bz.atApprox(t * length, length);
            return bz.angle(t >= 1.0f ? 0.99f : (t <= 0.0f ? 0.01f : t));
        }

        //per-dimension easing
        if (interpolator[DimY]) {
            auto p1 = progress(t);
            auto p2 = progress(tvg::clamp(t + 0.001f, 0.0f, 1.0f));
            Point dp = {(next->value.x - value.x) * (p2.x - p1.x), (next->value.y - value.y) * (p2.y - p1.y)};
            return rad2deg(tvg::atan(dp));
        }

        return rad2deg(tvg::atan(next->value - value));
    }

    void prepare(LottieVectorFrame* next)
    {
        Bezier bz = {value, value + outTangent, next->value + inTangent, next->value};
        length = bz.lengthApprox();
    }

    void setInterpolator(LottieInterpolator* ip, Dim dim) { interpolator[dim] = ip; }
};

struct LottieExpression
{
    char* code;
    LottieComposition* comp;
    LottieLayer* layer;
    LottieObject* object;
    LottieProperty* property;
    bool disabled = false;

    LottieExpression() {}

    LottieExpression(const LottieExpression* rhs)
    {
        code = tvg::duplicate(rhs->code);
        comp = rhs->comp;
        layer = rhs->layer;
        object = rhs->object;
        property = rhs->property;
        disabled = rhs->disabled;
    }

    ~LottieExpression()
    {
        tvg::free(code);
    }
};

//Property would have an either keyframes or single value.
struct LottieProperty
{
    enum class Type : uint8_t
    {
        Invalid = 0,
        Integer,
        Float,
        Scalar,
        Vector,
        PathSet,
        Color,
        Opacity,
        ColorStop,
        TextDoc,
        Image,
        Scalar3
    };
    enum class Loop : uint8_t {None = 0, InCycle = 1, InPingPong, InOffset, InContinue, OutCycle, OutPingPong, OutOffset, OutContinue};

    LottieExpression* exp = nullptr;
    Type type;
    uint8_t ix = 0;  //property index
    unsigned long sid = 0; //property sid for slot

    LottieProperty(Type type = Type::Invalid) : type(type) {}
    virtual ~LottieProperty() {}

    virtual uint32_t frameCnt() = 0;
    virtual uint32_t nearest(float frameNo) = 0;
    virtual float frameNo(int32_t key) = 0;
    virtual float loop(float frameNo, uint32_t key, Loop mode, float inout) = 0;
    bool copy(LottieProperty* rhs, bool shallow);

    static uint32_t bsearch(const float* firstNo, uint32_t count, uint32_t stride, float frameNo);
    static uint32_t nearest(const float* firstNo, uint32_t count, uint32_t stride, float frameNo);
    static float frameNo(const float* firstNo, uint32_t count, uint32_t stride, int32_t key);
    static float loop(float frameNo, LottieProperty::Loop mode, float inout, float firstNo, float inNo, float outNo);
};

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
        return frames ? LottieProperty::nearest(&frames->data->no, frames->count, sizeof(Frame), frameNo) : 0;
    }

    uint32_t frameCnt() override
    {
        return frames ? frames->count : 1;
    }

    float frameNo(int32_t key) override
    {
        return frames ? LottieProperty::frameNo(&frames->data->no, frames->count, sizeof(Frame), key) : 0.0f;
    }

    float loop(float frameNo, uint32_t key, Loop mode, float inout) override
    {
        if (!frames || mode == Loop::None) return frameNo;
        return LottieProperty::loop(frameNo, mode, inout, frames->first().no, (*frames)[key].no, (*frames)[frames->count - 1 - key].no);
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
            if (exps->result<MyProperty>(frameNo, out, exp)) return out;
        }

        if (!frames) return value;
        if (frames->count == 1 || frameNo <= frames->first().no) return frames->first().value;
        if (frameNo >= frames->last().no) return frames->last().value;

        auto key = LottieProperty::bsearch(&frames->data->no, frames->count, sizeof(Frame), frameNo);
        auto frame = frames->data + key;
        if (tvg::equal(frame->no, frameNo)) return frame->value;
        return frame->interpolate(frame + 1, frameNo);
    }

    Value operator()(float frameNo, LottieTween& tween, LottieExpressions* exps)
    {
        if (DEFAULT_COND) return (*this)(frameNo, exps);
        // tweening
        auto key = tween.key(this, frameNo);
        if (!tween.inited(key)) tween.capture(key, (*this)(frameNo, exps));
        return tween.run(key, (*this)(tween.to, exps));
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

        auto key = LottieProperty::bsearch(&frames->data->no, frames->count, sizeof(Frame), frameNo);
        auto frame = frames->data + key;
        return frame->angle(frame + 1, frameNo);
    }

    float angle(float frameNo, LottieTween& tween)
    {
        if (DEFAULT_COND) return angle(frameNo);
        // tweening
        auto key = tween.key(this, frameNo);
        if (!tween.inited(key)) tween.capture(key, angle(frameNo));
        return tween.run(key, angle(tween.to));
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

    LottiePathSet(const LottiePathSet& rhs)
    {
        copy(const_cast<LottiePathSet&>(rhs));
    }

    ~LottiePathSet()
    {
        release();
    }

    uint32_t nearest(float frameNo) override
    {
        return frames ? LottieProperty::nearest(&frames->data->no, frames->count, sizeof(*frames->data), frameNo) : 0;
    }

    uint32_t frameCnt() override
    {
        return frames ? frames->count : 1;
    }

    float frameNo(int32_t key) override
    {
        return frames ? LottieProperty::frameNo(&frames->data->no, frames->count, sizeof(*frames->data), key) : 0.0f;
    }

    float loop(float frameNo, uint32_t key, Loop mode, float inout) override
    {
        if (!frames || mode == Loop::None) return frameNo;
        return LottieProperty::loop(frameNo, mode, inout, frames->first().no, (*frames)[key].no, (*frames)[frames->count - 1 - key].no);
    }

    LottieScalarFrame<PathSet>& nextFrame()
    {
        return (*frames)[frames->count];
    }

    void release();
    void copy(LottiePathSet& rhs, bool shallow = true);
    LottieScalarFrame<PathSet>& newFrame();
    bool dispatch(float frameNo, PathSet*& path, LottieScalarFrame<PathSet>*& frame, float& t);
    void modifiedPath(float frameNo, RenderPath& out, Matrix* transform, LottieModifier* modifier);
    void defaultPath(float frameNo, RenderPath& out, Matrix* transform);
    void operator()(float frameNo, RenderPath& out, Matrix* transform, LottieExpressions* exps, LottieModifier* modifier = nullptr);
    void operator()(float frameNo, RenderPath& out, Matrix* transform, LottieTween& tween, LottieExpressions* exps, LottieModifier* modifier = nullptr);
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

    uint32_t nearest(float frameNo) override
    {
        return frames ? LottieProperty::nearest(&frames->data->no, frames->count, sizeof(*frames->data), frameNo) : 0;
    }

    uint32_t frameCnt() override
    {
        return frames ? frames->count : 1;
    }

    float frameNo(int32_t key) override
    {
        return frames ? LottieProperty::frameNo(&frames->data->no, frames->count, sizeof(*frames->data), key) : 0.0f;
    }

    float loop(float frameNo, uint32_t key, Loop mode, float inout) override
    {
        if (!frames || mode == Loop::None) return frameNo;
        return LottieProperty::loop(frameNo, mode, inout, frames->first().no, (*frames)[key].no, (*frames)[frames->count - 1 - key].no);
    }

    LottieScalarFrame<ColorStop>& nextFrame()
    {
        return (*frames)[frames->count];
    }

    void prepare() {}
    void release();
    LottieScalarFrame<ColorStop>& newFrame();
    Result operator()(float frameNo, Fill* fill, LottieExpressions* exps = nullptr);
    void operator()(float frameNo, Fill* fill, LottieTween& tween, LottieExpressions* exps);
    void copy(LottieColorStop& rhs, bool shallow = true);
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

    uint32_t nearest(float frameNo) override
    {
        return frames ? LottieProperty::nearest(&frames->data->no, frames->count, sizeof(*frames->data), frameNo) : 0;
    }

    uint32_t frameCnt() override
    {
        return frames ? frames->count : 1;
    }

    float frameNo(int32_t key) override
    {
        return frames ? LottieProperty::frameNo(&frames->data->no, frames->count, sizeof(*frames->data), key) : 0.0f;
    }

    float loop(float frameNo, uint32_t key, Loop mode, float inout) override
    {
        if (!frames || mode == Loop::None) return frameNo;
        return LottieProperty::loop(frameNo, mode, inout, frames->first().no, (*frames)[key].no, (*frames)[frames->count - 1 - key].no);
    }

    LottieScalarFrame<TextDocument>& nextFrame()
    {
        return (*frames)[frames->count];
    }

    void prepare() {}
    void release();
    LottieScalarFrame<TextDocument>& newFrame();
    TextDocument& operator()(float frameNo);
    TextDocument& operator()(float frameNo, LottieExpressions* exps);
    void copy(LottieTextDoc& rhs, bool shallow = true);
};

struct LottieAsset : LottieProperty, AssetSrc
{
    float width = 0.0f;
    float height = 0.0f;

    LottieAsset() : LottieProperty(LottieProperty::Type::Image) {}

    LottieAsset(const LottieAsset& rhs)
    {
        copy(const_cast<LottieAsset&>(rhs));
    }

    uint32_t frameCnt() override { return 0; }
    uint32_t nearest(float frameNo) override { return 0; }
    float frameNo(int32_t key) override { return 0.0f; }
    float loop(float frameNo, TVG_UNUSED uint32_t key, TVG_UNUSED Loop mode, TVG_UNUSED float inout) override { return frameNo; }
    void copy(LottieAsset& rhs, bool shallow = true);
};

using LottieFloat = LottieGenericProperty<LottieScalarFrame<float>, float, LottieProperty::Type::Float>;
using LottieInteger = LottieGenericProperty<LottieScalarFrame<int8_t>, int8_t, LottieProperty::Type::Integer>;
using LottieScalar = LottieGenericProperty<LottieScalarFrame<Point>, Point, LottieProperty::Type::Scalar>;
using LottieVector = LottieGenericProperty<LottieVectorFrame<Point>, Point, LottieProperty::Type::Vector, 0>;
using LottieScalar3 = LottieGenericProperty<LottieScalarFrame<Point3>, Point3, LottieProperty::Type::Scalar3>;
using LottieColor = LottieGenericProperty<LottieScalarFrame<RGB32>, RGB32, LottieProperty::Type::Color>;
using LottieOpacity = LottieGenericProperty<LottieScalarFrame<uint8_t>, uint8_t, LottieProperty::Type::Opacity>;

#endif //_TVG_LOTTIE_PROPERTY_H_
