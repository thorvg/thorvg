/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#ifndef _TVG_SW_FILL_CPP_
#define _TVG_SW_FILL_CPP_

#include "tvgSwCommon.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#define GRADIENT_STOP_SIZE 1024
#define FIXPT_BITS 8
#define FIXPT_SIZE (1<<FIXPT_BITS)


static bool _updateColorTable(SwFill* fill, const Fill* fdata)
{
    assert(fill && fdata);

    if (!fill->ctable) {
        fill->ctable = static_cast<uint32_t*>(malloc(GRADIENT_STOP_SIZE * sizeof(uint32_t)));
        assert(fill->ctable);
    }

    const Fill::ColorStop* colors;
    auto cnt = fdata->colorStops(&colors);
    if (cnt == 0 || !colors) return false;

    auto pColors = colors;

    if (pColors->a < 255) fill->translucent = true;

    auto rgba = COLOR_ARGB_JOIN(pColors->r, pColors->g, pColors->b, pColors->a);
    auto inc = 1.0f / static_cast<float>(GRADIENT_STOP_SIZE);
    auto pos = 1.5f * inc;
    uint32_t i = 0;

    fill->ctable[i++] = rgba;

    while (pos <= pColors->offset) {
        fill->ctable[i] = fill->ctable[i - 1];
        ++i;
        pos += inc;
    }

    for (uint32_t j = 0; j < cnt - 1; ++j) {
        auto curr = colors + j;
        auto next = curr + 1;
        assert(curr && next);
        auto delta = 1.0f / (next->offset - curr->offset);
        if (next->a < 255) fill->translucent = true;
        auto rgba2 = COLOR_ARGB_JOIN(next->r, next->g, next->b, next->a);

        while (pos < next->offset && i < GRADIENT_STOP_SIZE) {
            auto t = (pos - curr->offset) * delta;
            auto dist = static_cast<int32_t>(256 * t);
            auto dist2 = 256 - dist;
            fill->ctable[i] = COLOR_INTERPOLATE(rgba, dist2, rgba2, dist);
            ++i;
            pos += inc;
        }
        rgba = rgba2;
    }

    for (; i < GRADIENT_STOP_SIZE; ++i)
        fill->ctable[i] = rgba;

    //Make sure the lat color stop is represented at the end of the table
    fill->ctable[GRADIENT_STOP_SIZE - 1] = rgba;

    return true;
}


bool _prepareLinear(SwFill* fill, const LinearGradient* linear, const RenderTransform* transform)
{
    assert(fill && linear);

    float x1, x2, y1, y2;
    if (linear->linear(&x1, &y1, &x2, &y2) != Result::Success) return false;

    if (transform) {
        auto cx = (x2 - x1) * 0.5f + x1;
        auto cy = (y2 - y1) * 0.5f + y1;
        auto dx = x1 - cx;
        auto dy = y1 - cy;
        x1 = dx * transform->e11 + dy * transform->e12 + transform->e31;
        y1 = dx * transform->e21 + dy * transform->e22 + transform->e32;
        dx = x2 - cx;
        dy = y2 - cy;
        x2 = dx * transform->e11 + dy * transform->e12 + transform->e31;
        y2 = dx * transform->e21 + dy * transform->e22 + transform->e32;
    }

    fill->linear.dx = x2 - x1;
    fill->linear.dy = y2 - y1;
    fill->linear.len = fill->linear.dx * fill->linear.dx + fill->linear.dy * fill->linear.dy;

    if (fill->linear.len < FLT_EPSILON) return true;

    fill->linear.dx /= fill->linear.len;
    fill->linear.dy /= fill->linear.len;
    fill->linear.offset = -fill->linear.dx * x1 - fill->linear.dy * y1;

    return true;
}


bool _prepareRadial(SwFill* fill, const RadialGradient* radial, const RenderTransform* transform)
{
    assert(fill && radial);

    float radius;
    if (radial->radial(&fill->radial.cx, &fill->radial.cy, &radius) != Result::Success) return false;
    if (radius < FLT_EPSILON) return true;

    if (transform) {
        auto tx = fill->radial.cx * transform->e11 + fill->radial.cy * transform->e12 + transform->e31;
        auto ty = fill->radial.cx * transform->e21 + fill->radial.cy * transform->e22 + transform->e32;
        fill->radial.cx = tx;
        fill->radial.cy = ty;
        radius *= transform->e33;
    }

    fill->radial.a = radius * radius;
    fill->radial.inv2a = pow(1 / (2 * fill->radial.a), 2);

    return true;
}


static inline uint32_t _clamp(const SwFill* fill, uint32_t pos)
{
    switch (fill->spread) {
        case FillSpread::Pad: {
            if (pos >= GRADIENT_STOP_SIZE) pos = GRADIENT_STOP_SIZE - 1;
            break;
        }
        case FillSpread::Repeat: {
            pos = pos % GRADIENT_STOP_SIZE;
            break;
        }
        case FillSpread::Reflect: {
            auto limit = GRADIENT_STOP_SIZE * 2;
            pos = pos % limit;
            if (pos >= GRADIENT_STOP_SIZE) pos = (limit - pos - 1);
            break;
        }
    }
    return pos;
}


static inline uint32_t _fixedPixel(const SwFill* fill, uint32_t pos)
{
    auto i = (pos + (FIXPT_SIZE / 2)) >> FIXPT_BITS;
    return fill->ctable[_clamp(fill, i)];
}


static inline uint32_t _pixel(const SwFill* fill, float pos)
{
    auto i = static_cast<uint32_t>(pos * (GRADIENT_STOP_SIZE - 1) + 0.5f);
    return fill->ctable[_clamp(fill, i)];
}


static inline void _write(uint32_t *dst, uint32_t val, uint32_t len)
{
   if (len <= 0) return;

   // Cute hack to align future memcopy operation
   // and do unroll the loop a bit. Not sure it is
   // the most efficient, but will do for now.
   auto n = (len + 7) / 8;

   switch (len & 0x07) {
        case 0: do { *dst++ = val;
        case 7:      *dst++ = val;
        case 6:      *dst++ = val;
        case 5:      *dst++ = val;
        case 4:      *dst++ = val;
        case 3:      *dst++ = val;
        case 2:      *dst++ = val;
        case 1:      *dst++ = val;
        } while (--n > 0);
    }
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void fillFetchRadial(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len)
{
    if (fill->radial.a < FLT_EPSILON) return;

    //TODO: Rotation???
    auto rx = x + 0.5f - fill->radial.cx;
    auto ry = y + 0.5f - fill->radial.cy;
    auto inv2a = fill->radial.inv2a;
    auto rxy = rx * rx + ry * ry;
    auto rxryPlus = 2 * rx;
    auto det = (-4 * fill->radial.a * -rxy) * inv2a;
    auto detDelta = (4 * fill->radial.a * (rxryPlus + 1.0f)) * inv2a;
    auto detDelta2 = (4 * fill->radial.a * 2.0f) * inv2a;

   for (uint32_t i = 0 ; i < len ; ++i)
     {
        *dst = _pixel(fill, sqrt(det));
        ++dst;
        det += detDelta;
        detDelta += detDelta2;
     }
}


void fillFetchLinear(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len)
{
    if (fill->linear.len < FLT_EPSILON) return;

    //TODO: Rotation???
    auto rx = x + 0.5f;
    auto ry = y + 0.5f;
    auto t = (fill->linear.dx * rx + fill->linear.dy * ry + fill->linear.offset) * (GRADIENT_STOP_SIZE - 1);
    auto inc = (fill->linear.dx) * (GRADIENT_STOP_SIZE - 1);

    if (fabsf(inc) < FLT_EPSILON) {
        auto color = _fixedPixel(fill, static_cast<uint32_t>(t * FIXPT_SIZE));
        _write(dst, color, len);
        return;
    }

    auto vMax = static_cast<float>(INT32_MAX >> (FIXPT_BITS + 1));
    auto vMin = -vMax;
    auto v = t + (inc * len);

    //we can use fixed point math
    if (v < vMax && v > vMin) {
        auto t2 = static_cast<uint32_t>(t * FIXPT_SIZE);
        auto inc2 = static_cast<uint32_t>(inc * FIXPT_SIZE);
        for (uint32_t j = 0; j < len; ++j) {
            *dst = _fixedPixel(fill, t2);
            ++dst;
            t2 += inc2;
        }
    //we have to fallback to float math
    } else {
        while (dst < dst + len) {
            *dst = _pixel(fill, t / GRADIENT_STOP_SIZE);
            ++dst;
            t += inc;
        }
    }
}


bool fillGenColorTable(SwFill* fill, const Fill* fdata, const RenderTransform* transform, bool ctable)
{
    if (!fill) return false;

    assert(fdata);

    fill->spread = fdata->spread();

    if (ctable) {
        if (!_updateColorTable(fill, fdata)) return false;
    }

    if (fdata->id() == FILL_ID_LINEAR) {
        return _prepareLinear(fill, static_cast<const LinearGradient*>(fdata), transform);
    } else if (fdata->id() == FILL_ID_RADIAL) {
        return _prepareRadial(fill, static_cast<const RadialGradient*>(fdata), transform);
    }

    cout << "What type of gradient?!" << endl;

    return false;
}


void fillReset(SwFill* fill, const Fill* fdata)
{
    if (fill->ctable) {
        free(fill->ctable);
        fill->ctable = nullptr;
    }
    fill->translucent = false;
}


void fillFree(SwFill* fill)
{
    if (!fill) return;

    if (fill->ctable) free(fill->ctable);

    free(fill);
}

#endif /* _TVG_SW_FILL_CPP_ */