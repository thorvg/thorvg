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

#include "tvgMath.h"
#include "tvgLottieProperty.h"

/************************************************************************/
/* LottieProperty                                                       */
/************************************************************************/

bool LottieProperty::copy(LottieProperty* rhs, bool shallow)
{
    type = rhs->type;
    ix = rhs->ix;
    sid = rhs->sid;

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

float LottieProperty::loop(float frameNo, LottieProperty::Loop mode, float inout, float firstNo, float inNo, float outNo)
{
    frameNo -= firstNo;

    switch (mode) {
        case LottieProperty::Loop::InCycle:
            return fmodf(frameNo, inout - firstNo) + inNo;
        case LottieProperty::Loop::InPingPong: {
            auto range = inout - inNo;
            auto forward = (static_cast<int>(frameNo / range) % 2) == 0;
            frameNo = fmodf(frameNo, range);
            return (forward ? frameNo : (range - frameNo)) + inNo;
        }
        case LottieProperty::Loop::OutCycle:
            return fmodf(frameNo, outNo - firstNo) + firstNo;
        case LottieProperty::Loop::OutPingPong: {
            auto range = outNo - firstNo;
            auto forward = (static_cast<int>(frameNo / range) % 2) == 0;
            frameNo = fmodf(frameNo, range);
            return (forward ? frameNo : (range - frameNo)) + firstNo;
        }
        default:
            return frameNo;
    }
}

/************************************************************************/
/* LottieColorStop                                                      */
/************************************************************************/

void LottieColorStop::release()
{
    delete(exp);
    exp = nullptr;

    tvg::free(value.data);
    value.data = nullptr;

    delete(value.input);
    value.input = nullptr;

    if (!frames) return;

    ARRAY_FOREACH(p, *frames) {
        tvg::free((*p).value.data);
        delete((*p).value.input);
    }
    tvg::free(frames->data);
    tvg::free(frames);
    frames = nullptr;
}

LottieScalarFrame<ColorStop>& LottieColorStop::newFrame()
{
    if (!frames) {
        frames = tvg::calloc<Array<LottieScalarFrame<ColorStop>>>(1, sizeof(Array<LottieScalarFrame<ColorStop>>));
    }
    if (frames->count + 1 >= frames->reserved) {
        auto old = frames->reserved;
        frames->grow(frames->count + 2);
        memset((void*)(frames->data + old), 0x00, sizeof(LottieScalarFrame<ColorStop>) * (frames->reserved - old));
    }
    ++frames->count;
    return frames->last();
}

Result LottieColorStop::operator()(float frameNo, Fill* fill, LottieExpressions* exps)
{
    //overriding with expressions
    if (exps && exp) {
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
        result.push(tvg::lerp(*s, *e, t));
    }
    return fill->colorStops(result.data, count);
}

void LottieColorStop::operator()(float frameNo, Fill* fill, LottieTween& tween, LottieExpressions* exps)
{
    if (DEFAULT_COND) {
        (*this)(frameNo, fill, exps);
    } else {  // tweening
        auto key = tween.key(this, frameNo);
        if (!tween.inited(key)) {
            (*this)(frameNo, fill, exps);
            tween.capture(key, fill);
        }
        (*this)(tween.to, fill, exps);
        tween.run(key, fill);
    }
}

void LottieColorStop::copy(LottieColorStop& rhs, bool shallow)
{
    if (LottieProperty::copy(&rhs, shallow)) return;

    //the rhs colorstop is supposed be populated already.

    if (rhs.frames) {
        if (shallow) {
            frames = rhs.frames;
            rhs.frames = nullptr;
        } else {
            frames = tvg::calloc<Array<LottieScalarFrame<ColorStop>>>(1, sizeof(Array<LottieScalarFrame<ColorStop>>));
            *frames = *rhs.frames;
            for (uint32_t i = 0; i < (*rhs.frames).count; ++i) {
                (*frames)[i].value.copy((*rhs.frames)[i].value, rhs.count);
            }
        }
    } else {
        frames = nullptr;
        if (shallow) {
            value = rhs.value;
            rhs.value = ColorStop();
        } else {
            value.copy(rhs.value, rhs.count);
        }
    }
    populated = rhs.populated;
    count = rhs.count;
}

/************************************************************************/
/* LottiePathSet                                                        */
/************************************************************************/

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

LottieScalarFrame<PathSet>& LottiePathSet::newFrame()
{
    if (!frames) {
        frames = tvg::calloc<Array<LottieScalarFrame<PathSet>>>(1, sizeof(Array<LottieScalarFrame<PathSet>>));
    }
    if (frames->count + 1 >= frames->reserved) {
        auto old = frames->reserved;
        frames->grow(frames->count + 2);
        memset((void*)(frames->data + old), 0x00, sizeof(LottieScalarFrame<PathSet>) * (frames->reserved - old));
    }
    ++frames->count;
    return frames->last();
}

void LottiePathSet::operator()(float frameNo, RenderPath& out, Matrix* transform, LottieExpressions* exps, LottieModifier* modifier)
{
    //overriding with expressions
    if (exps && exp && exps->result<LottiePathSet>(frameNo, out, transform, modifier, exp)) return;
    if (modifier) modifiedPath(frameNo, out, transform, modifier);
    else defaultPath(frameNo, out, transform);
}

void LottiePathSet::operator()(float frameNo, RenderPath& out, Matrix* transform, LottieTween& tween, LottieExpressions* exps, LottieModifier* modifier)
{
    if (DEFAULT_COND) {
        (*this)(frameNo, out, transform, exps, modifier);
    } else {  // tweening
        auto key = tween.key(this, frameNo);
        if (!tween.inited(key)) {
            auto& tmp = RenderPath::scratch();
            (*this)(frameNo, tmp, transform, exps);
            tween.capture(key, tmp);
        }
        auto& tmp = RenderPath::scratch();
        (*this)(tween.to, tmp, transform, exps);
        tween.run(key, tmp, out, modifier);
    }
}

//return false means requiring the interpolation
bool LottiePathSet::dispatch(float frameNo, PathSet*& path, LottieScalarFrame<PathSet>*& frame, float& t)
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

void LottiePathSet::defaultPath(float frameNo, RenderPath& out, Matrix* transform)
{
    PathSet* path;
    LottieScalarFrame<PathSet>* frame;
    float t;

    if (dispatch(frameNo, path, frame, t)) {
        _copy(path, out.cmds);
        _copy(path, out.pts, transform);
        return;
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
}

void LottiePathSet::modifiedPath(float frameNo, RenderPath& out, Matrix* transform, LottieModifier* modifier)
{
    PathSet* path;
    LottieScalarFrame<PathSet>* frame;
    float t;

    if (dispatch(frameNo, path, frame, t)) {
        if (modifier) {
            RenderPath in;
            path->convert(in);
            modifier->path(in, out, transform);
            in.dismiss();
        } else {
            _copy(path, out.cmds);
            _copy(path, out.pts, transform);
        }
        return;
    }

    // interpolation
    auto s = frame->value.pts;
    auto e = (frame + 1)->value.pts;
    auto backup = frame->value.pts;
    frame->value.pts = tvg::malloc<Point>(frame->value.ptsCnt * sizeof(Point));
    auto p = frame->value.pts;

    for (auto i = 0; i < frame->value.ptsCnt; ++i, ++s, ++e, ++p) {
        *p = tvg::lerp(*s, *e, t);
        if (transform) *p *= *transform;
    }

    if (modifier) {
        RenderPath in;
        frame->value.convert(in);
        modifier->path(in, out, nullptr);
        in.dismiss();
    }

    std::swap(frame->value.pts, backup);
    tvg::free(backup);
}

void LottiePathSet::release()
{
    if (exp) {
        delete(exp);
        exp = nullptr;
    }

    tvg::free(value.cmds);
    tvg::free(value.pts);
    value = PathSet();

    if (!frames) return;

    ARRAY_FOREACH(p, *frames) {
        tvg::free((*p).value.cmds);
        tvg::free((*p).value.pts);
    }
    tvg::free(frames->data);
    tvg::free(frames);
    frames = nullptr;
}

void LottiePathSet::copy(LottiePathSet& rhs, bool shallow)
{
    if (LottieProperty::copy(&rhs, shallow)) return;

    if (rhs.frames) {
        if (shallow) {
            frames = rhs.frames;
            rhs.frames = nullptr;
        } else {
            frames = tvg::calloc<Array<LottieScalarFrame<PathSet>>>(1, sizeof(Array<LottieScalarFrame<PathSet>>));
            *frames = *rhs.frames;
            for (uint32_t i = 0; i < rhs.frames->count; ++i) {
                (*frames)[i].value.copy((*rhs.frames)[i].value);
            }
        }
    } else {
        frames = nullptr;
        if (shallow) {
            value = rhs.value;
            rhs.value = PathSet();
        } else value.copy(rhs.value);
    }
}

/************************************************************************/
/* LottieTextDoc                                                        */
/************************************************************************/

void LottieTextDoc::release()
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

LottieScalarFrame<TextDocument>& LottieTextDoc::newFrame()
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


TextDocument& LottieTextDoc::operator()(float frameNo)
{
    if (!frames) return value;
    if (frames->count == 1 || frameNo <= frames->first().no) return frames->first().value;
    if (frameNo >= frames->last().no) return frames->last().value;

    auto frame = frames->data + _bsearch(frames, frameNo);
    return frame->value;
}

TextDocument& LottieTextDoc::operator()(float frameNo, LottieExpressions* exps)
{
    auto& out = (*this)(frameNo);

    //overriding with expressions
    if (exps && exp) exps->result(frameNo, out, exp);

    return out;
}

void LottieTextDoc::copy(LottieTextDoc& rhs, bool shallow)
{
    if (LottieProperty::copy(&rhs, shallow)) return;

    if (rhs.frames) {
        if (shallow) {
            frames = rhs.frames;
            rhs.frames = nullptr;
        } else {
            frames = new Array<LottieScalarFrame<TextDocument>>;
            *frames = *rhs.frames;
            for (uint32_t i = 0; i < (*rhs.frames).count; ++i) {
                (*frames)[i].value.copy((*rhs.frames)[i].value);
            }
        }
    } else {
        frames = nullptr;
        if (shallow) {
            value = rhs.value;
            rhs.value.text = nullptr;
            rhs.value.name = nullptr;
        } else {
            value.copy(rhs.value);
        }
    }
}

/************************************************************************/
/* LottieAsset                                                          */
/************************************************************************/

void LottieAsset::copy(LottieAsset& rhs, bool shallow)
{
    if (LottieProperty::copy(&rhs, shallow)) return;

    release();

    if (shallow) {
        data = rhs.data;
        mimeType = rhs.mimeType;
        rhs.data = rhs.mimeType = nullptr;
    } else {
        // TODO: make it shareable data without copy?
        if (rhs.size > 0) data = static_cast<char*>(memcpy(tvg::malloc<char>(rhs.size), rhs.data, rhs.size));
        else path = tvg::duplicate(rhs.path);
        mimeType = tvg::duplicate(rhs.mimeType);
    }
    size = rhs.size;
    external = rhs.external;
    width = rhs.width;
    height = rhs.height;
}
