/*
 * Copyright (c) 2024 - 2025 the ThorVG project. All rights reserved.

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


#include "tvgMath.h"
#include "tvgCompressor.h"
#include "tvgLottieModel.h"
#include "tvgLottieExpressions.h"

#ifdef THORVG_LOTTIE_EXPRESSIONS_SUPPORT

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct ExpContent
{
    LottieExpression* exp;
    union {
        LottieObject* obj;
        LottieEffect* effect;
    };
    float frameNo;
    size_t refCnt;
};

static jerry_value_t _content(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt);

//reserved expressions specifiers
static const char* EXP_NAME = "name";
static const char* EXP_CONTENT = "content";
static const char* EXP_WIDTH = "width";
static const char* EXP_HEIGHT = "height";
static const char* EXP_CYCLE = "cycle";
static const char* EXP_PINGPONG = "pingpong";
static const char* EXP_OFFSET = "offset";
static const char* EXP_CONTINUE = "continue";
static const char* EXP_TIME = "time";
static const char* EXP_VALUE = "value";
static const char* EXP_INDEX = "index";
static const char* EXP_EFFECT= "effect";

static LottieExpressions* exps = nullptr;   //singleton instance engine


static ExpContent* _expcontent(LottieExpression* exp, float frameNo, void* obj, size_t refCnt = 1)
{
    auto data = tvg::malloc<ExpContent*>(sizeof(ExpContent));
    data->exp = exp;
    data->frameNo = frameNo;
    data->obj = (LottieObject*)obj;
    data->refCnt = refCnt;
    return data;
}


static inline ExpContent* _expcontent(ExpContent* data)
{
    ++data->refCnt;
    return data;
}


static float _rand()
{
    return (float)(rand() % 10000001) * 0.0000001f;
}


static jerry_value_t _point2d(const Point& pt)
{
    auto obj = jerry_object();
    auto v1 = jerry_number(pt.x);
    auto v2 = jerry_number(pt.y);
    jerry_object_set_index(obj, 0, v1);
    jerry_object_set_index(obj, 1, v2);
    jerry_value_free(v1);
    jerry_value_free(v2);
    return obj;
}


static jerry_value_t _color(RGB32 rgb)
{
    auto value = jerry_object();
    auto r = jerry_number((float)rgb.r);
    auto g = jerry_number((float)rgb.g);
    auto b = jerry_number((float)rgb.b);
    jerry_object_set_index(value, 0, r);
    jerry_object_set_index(value, 1, g);
    jerry_object_set_index(value, 2, b);
    jerry_value_free(r);
    jerry_value_free(g);
    jerry_value_free(b);
    return value;
}


static Point _point2d(jerry_value_t obj)
{
    auto v1 = jerry_object_get_index(obj, 0);
    auto v2 = jerry_object_get_index(obj, 1);
    Point pt = {jerry_value_as_number(v1), jerry_value_as_number(v2)};
    jerry_value_free(v1);
    jerry_value_free(v2);
    return pt;
}

static RGB32 _color(jerry_value_t obj)
{
    RGB32 out;
    auto r = jerry_object_get_index(obj, 0);
    auto g = jerry_object_get_index(obj, 1);
    auto b = jerry_object_get_index(obj, 2);
    out = {jerry_value_as_int32(r), jerry_value_as_int32(g), jerry_value_as_int32(b)};
    jerry_value_free(r);
    jerry_value_free(g);
    jerry_value_free(b);
    return out;
}


static void contentFree(void *native_p, struct jerry_object_native_info_t *info_p)
{
    if (--static_cast<ExpContent*>(native_p)->refCnt == 0) {
        tvg::free(native_p);
    }
}

static jerry_object_native_info_t freeCb {contentFree, 0, 0};
static uint32_t engineRefCnt = 0;  //Expressions Engine reference count


static char* _name(jerry_value_t args)
{
    auto arg0 = jerry_value_to_string(args);
    auto len = jerry_string_length(arg0);
    auto name = tvg::malloc<jerry_char_t*>(len * sizeof(jerry_char_t) + 1);
    jerry_string_to_buffer(arg0, JERRY_ENCODING_UTF8, name, len);
    name[len] = '\0';
    jerry_value_free(arg0);
    return (char*) name;
}


static unsigned long _idByName(jerry_value_t args)
{
    auto name = _name(args);
    auto id = djb2Encode(name);
    tvg::free(name);
    return id;
}


static jerry_value_t _toComp(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto layer = static_cast<LottieLayer*>(jerry_object_get_native_ptr(info->function, nullptr));
    return _point2d(_point2d(args[0]) * layer->cache.matrix);
}


static jerry_value_t _point(const Point& v)
{
    auto obj = _point2d(v);
    auto value = _point2d(v);
    jerry_object_set_sz(obj, EXP_VALUE, value);
    jerry_value_free(value);
    return obj;
}


static jerry_value_t _buildValue(float frameNo, LottieProperty* property)
{
    switch (property->type) {
        case LottieProperty::Type::Integer: return jerry_number((*static_cast<LottieInteger*>(property))(frameNo));
        case LottieProperty::Type::Float: return jerry_number((*static_cast<LottieFloat*>(property))(frameNo));
        case LottieProperty::Type::Scalar: return _point((*static_cast<LottieScalar*>(property))(frameNo));
        case LottieProperty::Type::Vector: return _point((*static_cast<LottieVector*>(property))(frameNo));
        case LottieProperty::Type::PathSet:
        {
            auto obj = jerry_object();
            jerry_object_set_native_ptr(obj, nullptr, property);
            return obj;
        }
        case LottieProperty::Type::Color: return _color((*static_cast<LottieColor*>(property))(frameNo));
        case LottieProperty::Type::Opacity: return jerry_number((*static_cast<LottieOpacity*>(property))(frameNo));
        default: TVGERR("LOTTIE", "Non supported type for value? = %d", (int) property->type);
    }
    return jerry_undefined();
}


static void _buildTransform(jerry_value_t context, float frameNo, LottieTransform* transform)
{
    if (!transform) return;

    auto obj = jerry_object();
    jerry_object_set_sz(context, "transform", obj);

    auto anchorPoint = _buildValue(frameNo, &transform->anchor);
    jerry_object_set_sz(obj, "anchorPoint", anchorPoint);
    jerry_value_free(anchorPoint);

    auto position = _buildValue(frameNo, &transform->position);
    jerry_object_set_sz(obj, "position", position);
    jerry_value_free(position);

    auto scale = _buildValue(frameNo, &transform->scale);
    jerry_object_set_sz(obj, "scale", scale);
    jerry_value_free(scale);

    auto rotation = _buildValue(frameNo, &transform->rotation);
    jerry_object_set_sz(obj, "rotation", rotation);
    jerry_value_free(rotation);

    auto opacity = _buildValue(frameNo, &transform->opacity);
    jerry_object_set_sz(obj, "opacity", opacity);
    jerry_value_free(opacity);

    jerry_value_free(obj);
}


static jerry_value_t _buildGroup(LottieGroup* group, float frameNo)
{
    auto obj = jerry_function_external(_content);

    //attach a transform
    ARRAY_FOREACH(p, group->children) {
        if ((*p)->type == LottieObject::Type::Transform) {
            _buildTransform(obj, frameNo, static_cast<LottieTransform*>(*p));
            break;
        }
    }
    jerry_object_set_native_ptr(obj, &freeCb, _expcontent(nullptr, frameNo, group));
    jerry_object_set_sz(obj, EXP_CONTENT, obj);
    return obj;
}


static jerry_value_t _buildPolystar(LottiePolyStar* polystar, float frameNo)
{
    auto obj = jerry_object();
    auto position = jerry_object();
    jerry_object_set_native_ptr(position, nullptr, &polystar->position);
    jerry_object_set_sz(obj, "position", position);
    jerry_value_free(position);
    auto innerRadius = jerry_number(polystar->innerRadius(frameNo));
    jerry_object_set_sz(obj, "innerRadius", innerRadius);
    jerry_value_free(innerRadius);
    auto outerRadius = jerry_number(polystar->outerRadius(frameNo));
    jerry_object_set_sz(obj, "outerRadius", outerRadius);
    jerry_value_free(outerRadius);
    auto innerRoundness = jerry_number(polystar->innerRoundness(frameNo));
    jerry_object_set_sz(obj, "innerRoundness", innerRoundness);
    jerry_value_free(innerRoundness);
    auto outerRoundness = jerry_number(polystar->outerRoundness(frameNo));
    jerry_object_set_sz(obj, "outerRoundness", outerRoundness);
    jerry_value_free(outerRoundness);
    auto rotation = jerry_number(polystar->rotation(frameNo));
    jerry_object_set_sz(obj, "rotation", rotation);
    jerry_value_free(rotation);
    auto ptsCnt = jerry_number(polystar->ptsCnt(frameNo));
    jerry_object_set_sz(obj, "points", ptsCnt);
    jerry_value_free(ptsCnt);

    return obj;
}


static jerry_value_t _buildTrimpath(LottieTrimpath* trimpath, float frameNo)
{
    jerry_value_t obj = jerry_object();
    auto start = jerry_number(trimpath->start(frameNo));
    jerry_object_set_sz(obj, "start", start);
    jerry_value_free(start);
    auto end = jerry_number(trimpath->end(frameNo));
    jerry_object_set_sz(obj, "end", end);
    jerry_value_free(end);
    auto offset = jerry_number(trimpath->offset(frameNo));
    jerry_object_set_sz(obj, EXP_OFFSET, offset);
    jerry_value_free(offset);

    return obj;
}


static jerry_value_t _effectProperty(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto data = static_cast<ExpContent*>(jerry_object_get_native_ptr(info->function, &freeCb));
    auto name = _name(args[0]);
    auto property = static_cast<LottieFxCustom*>(data->effect)->property(name);
    tvg::free(name);

    if (!property) return jerry_undefined();

    return _buildValue(data->frameNo, property);
}


static jerry_value_t _effect(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto data = static_cast<ExpContent*>(jerry_object_get_native_ptr(info->function, &freeCb));
    auto layer = static_cast<LottieLayer*>(data->obj);
    LottieEffect* effect = nullptr;

    //either name or index
    if (jerry_value_is_string(args[0])) {
        auto name = _name(args[0]);
        effect = layer->effectById(djb2Encode(name));
        tvg::free(name);
    } else {
        effect = layer->effectByIdx((int16_t)jerry_value_as_int32(args[0]));
    }

    if (!effect) return jerry_undefined();

    //find a effect property
    auto obj = jerry_function_external(_effectProperty);
    jerry_object_set_native_ptr(obj, &freeCb, _expcontent(data->exp, data->frameNo, effect));
    jerry_object_set_sz(obj, "", obj);
    return obj;
}


static void _buildLayer(jerry_value_t context, float frameNo, LottieLayer* layer, LottieLayer* comp, LottieExpression* exp)
{
    auto width = jerry_number(layer->w);
    jerry_object_set_sz(context, EXP_WIDTH, width);
    jerry_value_free(width);

    auto height = jerry_number(layer->h);
    jerry_object_set_sz(context, EXP_HEIGHT, height);
    jerry_value_free(height);

    auto index = jerry_number(layer->ix);
    jerry_object_set_sz(context, EXP_INDEX, index);
    jerry_value_free(index);

    auto parent = jerry_object();
    jerry_object_set_native_ptr(parent, nullptr, layer->parent);
    jerry_object_set_sz(context, "parent", parent);
    jerry_value_free(parent);

    auto hasParent = jerry_boolean(layer->parent ? true : false);
    jerry_object_set_sz(context, "hasParent", hasParent);
    jerry_value_free(hasParent);

    auto inPoint = jerry_number(layer->inFrame);
    jerry_object_set_sz(context, "inPoint", inPoint);
    jerry_value_free(inPoint);

    auto outPoint = jerry_number(layer->outFrame);
    jerry_object_set_sz(context, "outPoint", outPoint);
    jerry_value_free(outPoint);

    //TODO: Confirm exp->layer->comp->timeAtFrame() ?
    auto startTime = jerry_number(exp->comp->timeAtFrame(layer->startFrame));
    jerry_object_set_sz(context, "startTime", startTime);
    jerry_value_free(startTime);

    auto hasVideo = jerry_boolean(false);
    jerry_object_set_sz(context, "hasVideo", hasVideo);
    jerry_value_free(hasVideo);

    auto hasAudio = jerry_boolean(false);
    jerry_object_set_sz(context, "hasAudio", hasAudio);
    jerry_value_free(hasAudio);

    //active, #current in the animation range?

    auto enabled = jerry_boolean(!layer->hidden);
    jerry_object_set_sz(context, "enabled", enabled);
    jerry_value_free(enabled);

    auto audioActive = jerry_boolean(false);
    jerry_object_set_sz(context, "audioActive", audioActive);
    jerry_value_free(audioActive);

    //sampleImage(point, radius = [.5, .5], postEffect=true, t=time)

    _buildTransform(context, frameNo, layer->transform);

    //audioLevels, #the value of the Audio Levels property of the layer in decibels

    auto timeRemap = jerry_object();
    jerry_object_set_native_ptr(timeRemap, nullptr, &layer->timeRemap);
    jerry_object_set_sz(context, "timeRemap", timeRemap);
    jerry_value_free(timeRemap);

    //marker.key(index)
    //marker.key(name)
    //marker.nearestKey(t)
    //marker.numKeys

    if (layer->name) {
        auto name = jerry_string_sz(layer->name);
        jerry_object_set_sz(context, EXP_NAME, name);
        jerry_value_free(name);
    }

    auto toComp = jerry_function_external(_toComp);
    jerry_object_set_sz(context, "toComp", toComp);
    jerry_object_set_native_ptr(toComp, nullptr, comp);
    jerry_value_free(toComp);

    //content("name"), #look for the named property from a layer
    auto data = _expcontent(exp, frameNo, layer, 2);

    auto content = jerry_function_external(_content);
    jerry_object_set_sz(context, EXP_CONTENT, content);
    jerry_object_set_native_ptr(content, &freeCb, data);
    jerry_value_free(content);

    auto effect = jerry_function_external(_effect);
    jerry_object_set_sz(context, EXP_EFFECT, effect);
    jerry_object_set_native_ptr(effect, &freeCb, data);
    jerry_value_free(effect);
}


static jerry_value_t _addsub(const jerry_value_t args[], float addsub)
{
    //string + string
    if (jerry_value_is_string(args[0]) || jerry_value_is_string(args[1])) {
        auto a = _name(args[0]);
        auto b = _name(args[1]);
        auto ret = tvg::concat(a, b);
        auto val = jerry_string_sz(ret);
        tvg::free(ret);
        tvg::free(a);
        tvg::free(b);
        return val;
    }

    //number + number
    auto n1 = jerry_value_is_number(args[0]);
    auto n2 = jerry_value_is_number(args[1]);

    //1d + 1d
    if (n1 && n2) return jerry_number(jerry_value_as_number(args[0]) + addsub * jerry_value_as_number(args[1]));

    auto pt = _point2d(args[n1 ? 1 : 0]);

    //2d + 1d
    if (n1 || n2) {
        auto secondary = n1 ? 0 : 1;
        auto val3 = jerry_value_as_number(args[secondary]);
        if (secondary == 0) pt.x = (pt.x * addsub) + val3;
        else pt.x += (addsub * val3);
    //2d + 2d
    } else {
        pt += _point2d(args[1]) * addsub;
    }

    return _point2d(pt);
}


static jerry_value_t _muldiv(const jerry_value_t arg1, float arg2)
{
    //1d
    if (jerry_value_is_number(arg1)) return jerry_number(jerry_value_as_number(arg1) * arg2);

    //2d
    return _point2d(_point2d(arg1) * arg2);
}


static jerry_value_t _add(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    return _addsub(args, 1.0f);
}


static jerry_value_t _sub(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    return _addsub(args, -1.0f);
}


static jerry_value_t _mul(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    return _muldiv(args[0], jerry_value_as_number(args[1]));
}


static jerry_value_t _div(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    return _muldiv(args[0], 1.0f / jerry_value_as_number(args[1]));
}


static jerry_value_t _interp(float t, const jerry_value_t args[], int argsCnt)
{
    auto tMin = 0.0f;
    auto tMax = 1.0f;
    int idx = 0;

    tMin = jerry_value_as_number(args[1]);
    tMax = jerry_value_as_number(args[2]);
    idx += 2;

    t = (t - tMin) / (tMax - tMin);
    if (t < 0) t = 0.0f;
    else if (t > 1) t = 1.0f;

    //2d
    if (jerry_value_is_object(args[idx + 1]) && jerry_value_is_object(args[idx + 2])) {
        return _point2d(tvg::lerp(_point2d(args[idx + 1]), _point2d(args[idx + 2]), t));
    }

    //1d
    return jerry_number(tvg::lerp(jerry_value_as_number(args[idx + 1]), jerry_value_as_number(args[idx + 2]), t));
}


static jerry_value_t _linear(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto t = jerry_value_as_number(args[0]);
    return _interp(t, args, jerry_value_as_uint32(argsCnt));
}


static jerry_value_t _ease(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto t = jerry_value_as_number(args[0]);
    t = (t < 0.5f) ? (4 * t * t * t) : (1.0f - powf(-2.0f * t + 2.0f, 3) * 0.5f);
    return _interp(t, args, jerry_value_as_uint32(argsCnt));
}



static jerry_value_t _easeIn(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto t = jerry_value_as_number(args[0]);
    t = t * t * t;
    return _interp(t, args, jerry_value_as_uint32(argsCnt));
}


static jerry_value_t _easeOut(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto t = jerry_value_as_number(args[0]);
    t = 1.0f - powf(1.0f - t, 3);
    return _interp(t, args, jerry_value_as_uint32(argsCnt));
}


static jerry_value_t _clamp(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto num = jerry_value_as_number(args[0]);
    auto limit1 = jerry_value_as_number(args[1]);
    auto limit2 = jerry_value_as_number(args[2]);

    //clamping
    if (num < limit1) num = limit1;
    if (num > limit2) num = limit2;

    return jerry_number(num);
}


static jerry_value_t _dot(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    return jerry_number(tvg::dot(_point2d(args[0]), _point2d(args[1])));
}


static jerry_value_t _cross(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    return jerry_number(tvg::cross(_point2d(args[0]), _point2d(args[1])));
}


static jerry_value_t _normalize(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto pt = _point2d(args[0]);
    return _point2d(pt / tvg::length(pt));
}


static jerry_value_t _length(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    return jerry_number(tvg::length(_point2d(args[0])));
}


static jerry_value_t _random(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    return jerry_number(_rand());
}


static jerry_value_t _deg2rad(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    return jerry_number(deg2rad(jerry_value_as_number(args[0])));
}


static jerry_value_t _rad2deg(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    return jerry_number(rad2deg(jerry_value_as_number(args[0])));
}


static jerry_value_t _fromCompToSurface(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    TVGLOG("LOTTIE", "fromCompToSurface is not supported in expressions!");

    return jerry_undefined();
}


static jerry_value_t _content(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto data = static_cast<ExpContent*>(jerry_object_get_native_ptr(info->function, &freeCb));
    auto group = static_cast<LottieGroup*>(data->obj);
    auto target = group->content(_idByName(args[0]));
    if (!target) return jerry_undefined();

    //find the a path property(sh) in the group layer?
    switch (target->type) {
        case LottieObject::Group: return _buildGroup(static_cast<LottieGroup*>(target), data->frameNo);
        case LottieObject::Path: {
            auto obj = jerry_object();
            jerry_object_set_native_ptr(obj, nullptr, &static_cast<LottiePath*>(target)->pathset);
            jerry_object_set_sz(obj, "path", obj);
            return obj;
        }
        case LottieObject::Polystar: return _buildPolystar(static_cast<LottiePolyStar*>(target), data->frameNo);
        case LottieObject::Trimpath: return _buildTrimpath(static_cast<LottieTrimpath*>(target), data->frameNo);
        default: break;
    }
    return jerry_undefined();
}


static jerry_value_t _layer(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto data = static_cast<ExpContent*>(jerry_object_get_native_ptr(info->function, &freeCb));
    auto comp = static_cast<LottieLayer*>(data->obj);
    LottieLayer* layer;

    //layer index
    if (jerry_value_is_number(args[0])) {
        auto idx = (uint16_t)jerry_value_as_int32(args[0]);
        layer = comp->layerByIdx(idx);
        jerry_value_free(idx);
    //layer name
    } else {
        layer = comp->layerById(_idByName(args[0]));
    }

    if (!layer) return jerry_undefined();

    auto obj = jerry_object();
    jerry_object_set_native_ptr(obj, nullptr, layer);
    _buildLayer(obj, data->frameNo, layer, comp, data->exp);

    return obj;
}


static jerry_value_t _nearestKey(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto exp = static_cast<LottieExpression*>(jerry_object_get_native_ptr(info->function, nullptr));
    auto time = jerry_value_as_number(args[0]);
    auto frameNo = exp->comp->frameAtTime(time);
    auto index = jerry_number((float)exp->property->nearest(frameNo));

    auto obj = jerry_object();
    jerry_object_set_sz(obj, EXP_INDEX, index);
    jerry_value_free(index);

    return obj;
}


static jerry_value_t _property(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto data = static_cast<ExpContent*>(jerry_object_get_native_ptr(info->function, &freeCb));
    auto property = data->obj->property(jerry_value_as_int32(args[0]));
    if (!property) return jerry_undefined();
    return _buildValue(data->frameNo, property);
}


static jerry_value_t _propertyGroup(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto data = static_cast<ExpContent*>(jerry_object_get_native_ptr(info->function, &freeCb));
    auto level = jerry_value_as_int32(args[0]);

    //intermediate group
    if (level == 1) {
        auto group = jerry_function_external(_property);
        jerry_object_set_native_ptr(group, &freeCb, _expcontent(data));
        jerry_object_set_sz(group, "", group);
        return group;
    }

    TVGLOG("LOTTIE", "propertyGroup(%d)?", level);

    return jerry_undefined();
}


static jerry_value_t _valueAtTime(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto exp = static_cast<LottieExpression*>(jerry_object_get_native_ptr(info->function, nullptr));
    auto time = jerry_value_as_number(args[0]);
    auto frameNo = exp->comp->frameAtTime(time);
    return _buildValue(frameNo, exp->property);
}


static jerry_value_t _velocity(Point& prv, Point& cur, float elapsed)
{
    return _point2d({(cur.x - prv.x) / elapsed, (cur.y - prv.y) / elapsed});
}


static jerry_value_t _velocityAtTime(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto exp = static_cast<LottieExpression*>(jerry_object_get_native_ptr(info->function, nullptr));
    auto key = exp->property->nearest(exp->comp->frameAtTime(jerry_value_as_number(args[0])));
    auto pframe = exp->property->frameNo(key - 1);
    auto cframe = exp->property->frameNo(key);
    auto elapsed = (cframe - pframe) / (exp->comp->frameRate);

    //compute the velocity
    switch (exp->property->type) {
        case LottieProperty::Type::Float: {
            auto prv = (*static_cast<LottieFloat*>(exp->property))(pframe);
            auto cur = (*static_cast<LottieFloat*>(exp->property))(cframe);
            return jerry_number((cur - prv) / elapsed);
        }
        case LottieProperty::Type::Scalar: {
            auto prv = (*static_cast<LottieScalar*>(exp->property))(pframe);
            auto cur = (*static_cast<LottieScalar*>(exp->property))(cframe);
            return _velocity(prv, cur, elapsed);
        }
        case LottieProperty::Type::Vector: {
            auto prv = (*static_cast<LottieVector*>(exp->property))(pframe);
            auto cur = (*static_cast<LottieVector*>(exp->property))(cframe);
            return _velocity(prv, cur, elapsed);
        }
        default: TVGLOG("LOTTIE", "Non supported type for velocityAtTime?");
    }
    return jerry_undefined();
}


static jerry_value_t _speedAtTime(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto exp = static_cast<LottieExpression*>(jerry_object_get_native_ptr(info->function, nullptr));
    auto key = exp->property->nearest(exp->comp->frameAtTime(jerry_value_as_number(args[0])));
    auto pframe = exp->property->frameNo(key - 1);
    auto cframe = exp->property->frameNo(key);

    Point cur, prv;

    //compute the velocity
    switch (exp->property->type) {
        case LottieProperty::Type::Scalar: {
            prv = (*static_cast<LottieScalar*>(exp->property))(pframe);
            cur = (*static_cast<LottieScalar*>(exp->property))(cframe);
            break;
        }
        case LottieProperty::Type::Vector: {
            prv = (*static_cast<LottieVector*>(exp->property))(pframe);
            cur = (*static_cast<LottieVector*>(exp->property))(cframe);
            break;
        }
        default: {
            TVGLOG("LOTTIE", "Non supported type for speedAtTime?");
            return jerry_undefined();
        }
    }

    auto elapsed = (cframe - pframe) / (exp->comp->frameRate);
    auto speed = sqrtf(pow(cur.x - prv.x, 2) + pow(cur.y - prv.y, 2)) / elapsed;
    return jerry_number(speed);
}



static jerry_value_t _wiggle(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto data = static_cast<ExpContent*>(jerry_object_get_native_ptr(info->function, &freeCb));
    auto freq = jerry_value_as_number(args[0]);
    auto amp = jerry_value_as_number(args[1]);
    auto octaves = (argsCnt > 2) ? jerry_value_as_int32(args[2]) : 1;
    auto ampm = (argsCnt > 3) ? jerry_value_as_number(args[3]) : 5.0f;
    auto time = (argsCnt > 4) ? jerry_value_as_number(args[4]) : data->exp->comp->timeAtFrame(data->frameNo);

    Point result = {100.0f, 100.0f};

    for (int o = 0; o < octaves; ++o) {
        auto repeat = int(time * freq);
        auto frac = (time * freq - float(repeat)) * 1.25f;
        for (int i = 0; i < repeat; ++i) {
            result.x += (_rand() * 2.0f - 1.0f) * amp * frac;
            result.y += (_rand() * 2.0f - 1.0f) * amp * frac;
        }
        freq *= 2.0f;
        amp *= ampm;
    }
    return _point2d(result);
}


static jerry_value_t _temporalWiggle(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto data = static_cast<ExpContent*>(jerry_object_get_native_ptr(info->function, &freeCb));
    auto freq = jerry_value_as_number(args[0]);
    auto amp = jerry_value_as_number(args[1]);
    auto octaves = (argsCnt > 2) ? jerry_value_as_int32(args[2]) : 1;
    auto ampm = (argsCnt > 3) ? jerry_value_as_number(args[3]) : 5.0f;
    auto time = (argsCnt > 4) ? jerry_value_as_number(args[4]) : data->exp->comp->timeAtFrame(data->frameNo);
    auto wiggleTime = time;

    for (int o = 0; o < octaves; ++o) {
        auto repeat = int(time * freq);
        auto frac = (time * freq - float(repeat));
        for (int i = 0; i < repeat; ++i) {
            wiggleTime += (_rand() * 2.0f - 1.0f) * amp * frac;
        }
        freq *= 2.0f;
        amp *= ampm;
    }

    return _buildValue(data->exp->comp->frameAtTime(wiggleTime), data->exp->property);
}


static LottieProperty::Loop _loopCommon(const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto mode = LottieProperty::Loop::InCycle;

    if (argsCnt > 0) {
        auto name = _name(args[0]);
        if (!strcmp(name, EXP_CYCLE)) mode = LottieProperty::Loop::InCycle;
        else if (!strcmp(name, EXP_PINGPONG)) mode = LottieProperty::Loop::InPingPong;
        else if (!strcmp(name, EXP_OFFSET)) mode = LottieProperty::Loop::InOffset;
        else if (!strcmp(name, EXP_CONTINUE)) mode = LottieProperty::Loop::InContinue;
        tvg::free(name);
    }

    if (mode != LottieProperty::Loop::InCycle && mode != LottieProperty::Loop::InPingPong) {
        TVGLOG("LOTTIE", "Not supported loopIn type = %d", (int) mode);
    }
    return mode;
}


#define LOOP_OUT_OFFSET 4

static jerry_value_t _loopOut(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto data = static_cast<ExpContent*>(jerry_object_get_native_ptr(info->function, &freeCb));
    auto mode = static_cast<LottieProperty::Loop>((int) _loopCommon(args, argsCnt) + LOOP_OUT_OFFSET);
    auto key = (argsCnt > 1) ? jerry_value_as_int32(args[1]) : 0;
    return _buildValue(data->exp->property->loop(data->frameNo, key, mode, data->exp->layer->outFrame), data->exp->property);
}


static jerry_value_t _loopOutDuration(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto data = static_cast<ExpContent*>(jerry_object_get_native_ptr(info->function, &freeCb));
    auto mode = static_cast<LottieProperty::Loop>((int) _loopCommon(args, argsCnt) + LOOP_OUT_OFFSET);
    auto out = (argsCnt > 1) ? data->exp->comp->frameAtTime(jerry_value_as_number(args[1])) : FLT_MAX;
    return _buildValue(data->exp->property->loop(data->frameNo, 0, mode, out), data->exp->property);
}


static jerry_value_t _loopIn(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto data = static_cast<ExpContent*>(jerry_object_get_native_ptr(info->function, &freeCb));
    auto mode = _loopCommon(args, argsCnt);
    auto key = (argsCnt > 1) ? jerry_value_as_int32(args[1]) : 0;
    return _buildValue(data->exp->property->loop(data->frameNo, key, mode, data->exp->layer->outFrame), data->exp->property);
}


static jerry_value_t _loopInDuration(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto data = static_cast<ExpContent*>(jerry_object_get_native_ptr(info->function, &freeCb));
    auto mode = _loopCommon(args, argsCnt);
    auto in = (argsCnt > 1) ? data->exp->comp->frameAtTime(jerry_value_as_number(args[1])) : FLT_MAX;
    return _buildValue(data->exp->property->loop(data->frameNo, 0, mode, in), data->exp->property);
}


static jerry_value_t _key(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto exp = static_cast<LottieExpression*>(jerry_object_get_native_ptr(info->function, nullptr));
    auto frameNo = exp->property->frameNo(jerry_value_as_int32(args[0]));
    auto time = jerry_number(exp->comp->timeAtFrame(frameNo));
    auto value = _buildValue(frameNo, exp->property);

    auto obj = jerry_object();
    jerry_object_set_sz(obj, EXP_TIME, time);
    jerry_object_set_sz(obj, EXP_INDEX, args[0]);
    jerry_object_set_sz(obj, EXP_VALUE, value);

    //direct access, key[0], key[1]
    if (exp->property->type == LottieProperty::Type::Float) {
        jerry_object_set_index(obj, 0, value);
    } else if (exp->property->type == LottieProperty::Type::Scalar || exp->property->type == LottieProperty::Type::Vector) {
        jerry_object_set_index(obj, 0, jerry_object_get_index(value, 0));
        jerry_object_set_index(obj, 1, jerry_object_get_index(value, 1));
    }

    jerry_value_free(time);
    jerry_value_free(value);

    return obj;
}


static jerry_value_t _createPath(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    //TODO: arg1: points, arg2: inTangents, arg3: outTangents, arg4: isClosed
    auto arg1 = jerry_value_to_object(args[0]);
    auto pathset = jerry_object_get_native_ptr(arg1, nullptr);
    if (!pathset) {
        TVGERR("LOTTIE", "failed createPath()");
        return jerry_undefined();
    }

    jerry_value_free(arg1);

    auto obj = jerry_object();
    jerry_object_set_native_ptr(obj, nullptr, pathset);
    return obj;
}


static jerry_value_t _uniformPath(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto pathset = static_cast<LottiePathSet*>(jerry_object_get_native_ptr(info->function, nullptr));

    /* TODO: ThorVG prebuilds the path data for performance.
       It actually need to constructs the Array<Point> for points, inTangents, outTangents and then return here... */
    auto obj = jerry_object();
    jerry_object_set_native_ptr(obj, nullptr, pathset);
    return obj;
}


static jerry_value_t _isClosed(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    //TODO: Not used
    return jerry_boolean(true);
}


static void _buildPath(jerry_value_t context, LottieExpression* exp)
{
    //Trick for fast building path.
    auto points = jerry_function_external(_uniformPath);
    jerry_object_set_native_ptr(points, nullptr, exp->property);
    jerry_object_set_sz(context, "points", points);
    jerry_value_free(points);

    auto inTangents = jerry_function_external(_uniformPath);
    jerry_object_set_native_ptr(inTangents, nullptr, exp->property);
    jerry_object_set_sz(context, "inTangents", inTangents);
    jerry_value_free(inTangents);

    auto outTangents = jerry_function_external(_uniformPath);
    jerry_object_set_native_ptr(outTangents, nullptr, exp->property);
    jerry_object_set_sz(context, "outTangents", outTangents);
    jerry_value_free(outTangents);

    auto isClosed = jerry_function_external(_isClosed);
    jerry_object_set_native_ptr(isClosed, nullptr, exp->property);
    jerry_object_set_sz(context, "isClosed", isClosed);
    jerry_value_free(isClosed);

}


static void _buildProperty(float frameNo, jerry_value_t context, LottieExpression* exp)
{
    auto value = _buildValue(frameNo, exp->property);
    jerry_object_set_sz(context, EXP_VALUE, value);
    jerry_value_free(value);

    auto valueAtTime = jerry_function_external(_valueAtTime);
    jerry_object_set_sz(context, "valueAtTime", valueAtTime);
    jerry_object_set_native_ptr(valueAtTime, nullptr, exp);
    jerry_value_free(valueAtTime);

    auto velocity = jerry_number(0.0f);
    jerry_object_set_sz(context, "velocity", velocity);
    jerry_value_free(velocity);

    auto velocityAtTime = jerry_function_external(_velocityAtTime);
    jerry_object_set_sz(context, "velocityAtTime", velocityAtTime);
    jerry_object_set_native_ptr(velocityAtTime, nullptr, exp);
    jerry_value_free(velocityAtTime);

    auto speed = jerry_number(0.0f);
    jerry_object_set_sz(context, "speed", speed);
    jerry_value_free(speed);

    auto speedAtTime = jerry_function_external(_speedAtTime);
    jerry_object_set_sz(context, "speedAtTime", speedAtTime);
    jerry_object_set_native_ptr(speedAtTime, nullptr, exp);
    jerry_value_free(speedAtTime);

    {
        auto data =  _expcontent(exp, frameNo, exp->object, 7);

        auto wiggle = jerry_function_external(_wiggle);
        jerry_object_set_sz(context, "wiggle", wiggle);
        jerry_object_set_native_ptr(wiggle, &freeCb, data);
        jerry_value_free(wiggle);

        auto temporalWiggle = jerry_function_external(_temporalWiggle);
        jerry_object_set_sz(context, "temporalWiggle", temporalWiggle);
        jerry_object_set_native_ptr(temporalWiggle, &freeCb, data);
        jerry_value_free(temporalWiggle);

        auto propertyGroup = jerry_function_external(_propertyGroup);
        jerry_object_set_native_ptr(propertyGroup, &freeCb, data);
        jerry_object_set_sz(context, "propertyGroup", propertyGroup);
        jerry_value_free(propertyGroup);

        //propertyIndex

        auto loopIn = jerry_function_external(_loopIn);
        jerry_object_set_sz(context, "loopIn", loopIn);
        jerry_object_set_native_ptr(loopIn, &freeCb, data);
        jerry_value_free(loopIn);

        auto loopOut = jerry_function_external(_loopOut);
        jerry_object_set_sz(context, "loopOut", loopOut);
        jerry_object_set_native_ptr(loopOut, &freeCb, data);
        jerry_value_free(loopOut);

        auto loopInDuration = jerry_function_external(_loopInDuration);
        jerry_object_set_sz(context, "loopInDuration", loopInDuration);
        jerry_object_set_native_ptr(loopInDuration, &freeCb, data);
        jerry_value_free(loopInDuration);

        auto loopOutDuration = jerry_function_external(_loopOutDuration);
        jerry_object_set_sz(context, "loopOutDuration", loopOutDuration);
        jerry_object_set_native_ptr(loopOutDuration, &freeCb, data);
        jerry_value_free(loopOutDuration);

    }

    //smooth(width=.2, samples=5, t=time)

    auto key = jerry_function_external(_key);
    jerry_object_set_sz(context, "key", key);
    jerry_object_set_native_ptr(key, nullptr, exp);
    jerry_value_free(key);

    //key(markerName)

    auto nearestKey = jerry_function_external(_nearestKey);
    jerry_object_set_native_ptr(nearestKey, nullptr, exp);
    jerry_object_set_sz(context, "nearestKey", nearestKey);
    jerry_value_free(nearestKey);

    auto numKeys = jerry_number((float)exp->property->frameCnt());
    jerry_object_set_sz(context, "numKeys", numKeys);
    jerry_value_free(numKeys);

    //name

    {
        auto data = _expcontent(exp, frameNo, exp->layer, 2);

        //content("name"), #look for the named property from a layer
        auto content = jerry_function_external(_content);
        jerry_object_set_sz(context, EXP_CONTENT, content);
        jerry_object_set_native_ptr(content, &freeCb, data);
        jerry_value_free(content);

        auto effect = jerry_function_external(_effect);
        jerry_object_set_sz(context, EXP_EFFECT, effect);
        jerry_object_set_native_ptr(effect, &freeCb, data);
        jerry_value_free(effect);
    }

    auto effect = jerry_function_external(_effect);
    jerry_object_set_sz(context, EXP_EFFECT, effect);
    jerry_object_set_native_ptr(effect, &freeCb, _expcontent(exp, frameNo, exp->layer));
    jerry_value_free(effect);

    //expansions per types
    if (exp->property->type == LottieProperty::Type::PathSet) _buildPath(context, exp);
}


static jerry_value_t _comp(const jerry_call_info_t* info, const jerry_value_t args[], const jerry_length_t argsCnt)
{
    auto data = static_cast<ExpContent*>(jerry_object_get_native_ptr(info->function, &freeCb));
    auto comp = static_cast<LottieLayer*>(data->obj);
    auto layer = comp->layerById(_idByName(args[0]));

    if (!layer) return jerry_undefined();

    auto obj = jerry_object();
    jerry_object_set_native_ptr(obj, nullptr, layer);
    _buildLayer(obj, data->frameNo, layer, comp, data->exp);

    return obj;
}


static void _buildMath(jerry_value_t context)
{
    auto bm_mul = jerry_function_external(_mul);
    jerry_object_set_sz(context, "$bm_mul", bm_mul);
    jerry_value_free(bm_mul);

    auto bm_sum = jerry_function_external(_add);
    jerry_object_set_sz(context, "$bm_sum", bm_sum);
    jerry_value_free(bm_sum);

    auto bm_add = jerry_function_external(_add);
    jerry_object_set_sz(context, "$bm_add", bm_add);
    jerry_value_free(bm_add);

    auto bm_sub = jerry_function_external(_sub);
    jerry_object_set_sz(context, "$bm_sub", bm_sub);
    jerry_value_free(bm_sub);

    auto bm_div = jerry_function_external(_div);
    jerry_object_set_sz(context, "$bm_div", bm_div);
    jerry_value_free(bm_div);

    auto mul = jerry_function_external(_mul);
    jerry_object_set_sz(context, "mul", mul);
    jerry_value_free(mul);

    auto sum = jerry_function_external(_add);
    jerry_object_set_sz(context, "sum", sum);
    jerry_value_free(sum);

    auto add = jerry_function_external(_add);
    jerry_object_set_sz(context, "add", add);
    jerry_value_free(add);

    auto sub = jerry_function_external(_sub);
    jerry_object_set_sz(context, "sub", sub);
    jerry_value_free(sub);

    auto div = jerry_function_external(_div);
    jerry_object_set_sz(context, "div", div);
    jerry_value_free(div);

    auto clamp = jerry_function_external(_clamp);
    jerry_object_set_sz(context, "clamp", clamp);
    jerry_value_free(clamp);

    auto dot = jerry_function_external(_dot);
    jerry_object_set_sz(context, "dot", dot);
    jerry_value_free(dot);

    auto cross = jerry_function_external(_cross);
    jerry_object_set_sz(context, "cross", cross);
    jerry_value_free(cross);

    auto normalize = jerry_function_external(_normalize);
    jerry_object_set_sz(context, "normalize", normalize);
    jerry_value_free(normalize);

    auto length = jerry_function_external(_length);
    jerry_object_set_sz(context, "length", length);
    jerry_value_free(length);

    auto random = jerry_function_external(_random);
    jerry_object_set_sz(context, "random", random);
    jerry_value_free(random);

    auto deg2rad = jerry_function_external(_deg2rad);
    jerry_object_set_sz(context, "degreesToRadians", deg2rad);
    jerry_value_free(deg2rad);

    auto rad2deg = jerry_function_external(_rad2deg);
    jerry_object_set_sz(context, "radiansToDegrees", rad2deg);
    jerry_value_free(rad2deg);

    auto linear = jerry_function_external(_linear);
    jerry_object_set_sz(context, "linear", linear);
    jerry_value_free(linear);

    auto ease = jerry_function_external(_ease);
    jerry_object_set_sz(context, "ease", ease);
    jerry_value_free(ease);

    auto easeIn = jerry_function_external(_easeIn);
    jerry_object_set_sz(context, "easeIn", easeIn);
    jerry_value_free(easeIn);

    auto easeOut = jerry_function_external(_easeOut);
    jerry_object_set_sz(context, "easeOut", easeOut);
    jerry_value_free(easeOut);

    //lookAt
}


void LottieExpressions::buildGlobal(float frameNo, LottieExpression* exp)
{
    tvg::free(static_cast<ExpContent*>(jerry_object_get_native_ptr(comp, &freeCb)));
    jerry_object_set_native_ptr(comp, &freeCb, _expcontent(exp, frameNo, exp->layer));

    auto index = jerry_number(exp->layer->ix);
    jerry_object_set_sz(global, EXP_INDEX, index);
    jerry_value_free(index);
}


void LottieExpressions::buildComp(jerry_value_t context, float frameNo, LottieLayer* comp, LottieExpression* exp)
{
    //layer(index) / layer(name) / layer(otherLayer, reIndex)
    auto layer = jerry_function_external(_layer);
    jerry_object_set_sz(context, "layer", layer);

    jerry_object_set_native_ptr(layer, &freeCb, _expcontent(exp, frameNo, comp));
    jerry_value_free(layer);

    auto numLayers = jerry_number((float)comp->children.count);
    jerry_object_set_sz(context, "numLayers", numLayers);
    jerry_value_free(numLayers);
}


void LottieExpressions::buildComp(LottieComposition* comp, float frameNo, LottieExpression* exp)
{
    buildComp(this->comp, frameNo, comp->root, exp);

    //marker
    //marker.key(index)
    //marker.key(name)
    //marker.nearestKey(t)
    //marker.numKeys

    //activeCamera

    auto width = jerry_number(comp->w);
    jerry_object_set_sz(thisComp, EXP_WIDTH, width);
    jerry_value_free(width);

    auto height = jerry_number(comp->h);
    jerry_object_set_sz(thisComp, EXP_HEIGHT, height);
    jerry_value_free(height);

    auto duration = jerry_number(comp->duration());
    jerry_object_set_sz(thisComp, "duration", duration);
    jerry_value_free(duration);

    //ntscDropFrame
    //displayStartTime

    auto frameDuration = jerry_number(1.0f / comp->frameRate);
    jerry_object_set_sz(thisComp, "frameDuration", frameDuration);
    jerry_value_free(frameDuration);

    //shutterAngle
    //shutterPhase
    //bgColor
    //pixelAspect

    if (comp->name) {
        auto name = jerry_string((jerry_char_t*)comp->name, strlen(comp->name), JERRY_ENCODING_UTF8);
        jerry_object_set_sz(thisComp, EXP_NAME, name);
        jerry_value_free(name);
    }
}


jerry_value_t LottieExpressions::buildGlobal()
{
    global = jerry_current_realm();

    //comp(name)
    comp = jerry_function_external(_comp);
    jerry_object_set_sz(global, "comp", comp);

    //footage(name)

    thisComp = jerry_object();
    jerry_object_set_sz(global, "thisComp", thisComp);

    thisLayer = jerry_object();
    jerry_object_set_sz(global, "thisLayer", thisLayer);

    thisProperty = jerry_object();
    jerry_object_set_sz(global, "thisProperty", thisProperty);

    auto fromCompToSurface = jerry_function_external(_fromCompToSurface);
    jerry_object_set_sz(global, "fromCompToSurface", fromCompToSurface);
    jerry_value_free(fromCompToSurface);

    auto createPath = jerry_function_external(_createPath);
    jerry_object_set_sz(global, "createPath", createPath);
    jerry_value_free(createPath);

    //posterizeTime(framesPerSecond)
    //value

    return global;
}


void LottieExpressions::buildWritables(LottieExpression* exp)
{
    if (exp->writables.empty()) return;
    ARRAY_FOREACH(p, exp->writables) {
        auto writable = jerry_number(p->val);
        jerry_object_set_sz(global, p->var, writable);
        jerry_value_free(writable);
    }
}


jerry_value_t LottieExpressions::evaluate(float frameNo, LottieExpression* exp)
{
    if (exp->disabled && exp->writables.empty()) return jerry_undefined();

    buildGlobal(frameNo, exp);

    //main composition
    buildComp(exp->comp, frameNo, exp);

    //this composition
    buildComp(thisComp, frameNo, exp->layer->comp, exp);

    //update global context values
    _buildProperty(frameNo, global, exp);

    //this layer
    jerry_object_set_native_ptr(thisLayer, nullptr, exp->layer);
    _buildLayer(thisLayer, frameNo, exp->layer, exp->comp->root, exp);

    //this property
    jerry_object_set_native_ptr(thisProperty, nullptr, exp->property);
    _buildProperty(frameNo, thisProperty, exp);

    //expansions per object type
    if (exp->object->type == LottieObject::Transform) _buildTransform(global, frameNo, static_cast<LottieTransform*>(exp->object));

    //update writable values
    buildWritables(exp);

    //evaluate the code
    auto eval = jerry_eval((jerry_char_t *) exp->code, strlen(exp->code), JERRY_PARSE_NO_OPTS);

    if (jerry_value_is_exception(eval)) {
        TVGERR("LOTTIE", "Failed to dispatch the expressions!");
        exp->disabled = true;
        return jerry_undefined();
    }

    jerry_value_free(eval);

    return jerry_object_get_sz(global, "$bm_rt");
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

LottieExpressions::~LottieExpressions()
{
    jerry_value_free(thisProperty);
    jerry_value_free(thisLayer);
    jerry_value_free(thisComp);
    jerry_value_free(comp);
    jerry_value_free(global);
    jerry_cleanup();
}


LottieExpressions::LottieExpressions()
{
    jerry_init(JERRY_INIT_EMPTY);
    _buildMath(buildGlobal());
}


void LottieExpressions::update(float curTime)
{
    //time, #current time in seconds
    auto time = jerry_number(curTime);
    jerry_object_set_sz(global, EXP_TIME, time);
    jerry_value_free(time);
}


//FIXME: Threads support
#include "tvgTaskScheduler.h"

LottieExpressions* LottieExpressions::instance()
{
    //FIXME: Threads support
    if (TaskScheduler::threads() > 1) {
        TVGLOG("LOTTIE", "Lottie Expressions are not supported with tvg threads");
        return nullptr;
    }

    if (!exps) exps = new LottieExpressions;
    ++engineRefCnt;
    return exps;
}


void LottieExpressions::retrieve(LottieExpressions* instance)
{
    if (--engineRefCnt == 0) {
        delete(instance);
        exps = nullptr;
    }
}


Point LottieExpressions::toPoint2d(jerry_value_t obj)
{
    return _point2d(obj);
}


RGB32 LottieExpressions::toColor(jerry_value_t obj)
{
    return _color(obj);
}


#endif //THORVG_LOTTIE_EXPRESSIONS_SUPPORT
