/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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
#include <float.h>
#include <math.h>
#include "tvgSwCommon.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#define GRADIENT_STOP_SIZE 1024
#define FIXPT_BITS 8
#define FIXPT_SIZE (1<<FIXPT_BITS)


static bool _updateColorTable(SwFill* fill, const Fill* fdata, SwSurface* surface, uint32_t opacity)
{
    if (!fill->ctable) {
        fill->ctable = static_cast<uint32_t*>(malloc(GRADIENT_STOP_SIZE * sizeof(uint32_t)));
        if (!fill->ctable) return false;
    }

    const Fill::ColorStop* colors;
    auto cnt = fdata->colorStops(&colors);
    if (cnt == 0 || !colors) return false;

    auto pColors = colors;

    auto a = (pColors->a * opacity) / 255;
    if (a < 255) fill->translucent = true;

    auto r = ALPHA_MULTIPLY(pColors->r, a);
    auto g = ALPHA_MULTIPLY(pColors->g, a);
    auto b = ALPHA_MULTIPLY(pColors->b, a);

    auto rgba = surface->blender.join(r, g, b, a);
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
        auto delta = 1.0f / (next->offset - curr->offset);
        a = (next->a * opacity) / 255;
        if (!fill->translucent && a < 255) fill->translucent = true;

        auto r = ALPHA_MULTIPLY(next->r, a);
        auto g = ALPHA_MULTIPLY(next->g, a);
        auto b = ALPHA_MULTIPLY(next->b, a);

        auto rgba2 = surface->blender.join(r, g, b, a);

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


bool _prepareLinear(SwFill* fill, const LinearGradient* linear, const Matrix* transform)
{
    float x1, x2, y1, y2;
    if (linear->linear(&x1, &y1, &x2, &y2) != Result::Success) return false;

    if (transform) {
        auto sx = sqrt(pow(transform->e11, 2) + pow(transform->e21, 2));
        auto sy = sqrt(pow(transform->e12, 2) + pow(transform->e22, 2));
        auto cx = (x2 - x1) * 0.5f + x1;
        auto cy = (y2 - y1) * 0.5f + y1;
        auto dx = x1 - cx;
        auto dy = y1 - cy;
        x1 = dx * transform->e11 + dy * transform->e12 + transform->e13 + (cx * sx);
        y1 = dx * transform->e21 + dy * transform->e22 + transform->e23 + (cy * sy);
        dx = x2 - cx;
        dy = y2 - cy;
        x2 = dx * transform->e11 + dy * transform->e12 + transform->e13 + (cx * sx);
        y2 = dx * transform->e21 + dy * transform->e22 + transform->e23 + (cy * sy);
    }

    fill->linear.dx = x2 - x1;
    fill->linear.dy = y2 - y1;
    fill->linear.len = fill->linear.dx * fill->linear.dx + fill->linear.dy * fill->linear.dy;

    if (fill->linear.len < FLT_EPSILON) return true;

    fill->linear.dx /= fill->linear.len;
    fill->linear.dy /= fill->linear.len;
    fill->linear.offset = -fill->linear.dx * x1 -fill->linear.dy * y1;

    return true;
}


bool _prepareRadial(SwFill* fill, const RadialGradient* radial, const Matrix* transform)
{
    float radius;
    if (radial->radial(&fill->radial.cx, &fill->radial.cy, &radius) != Result::Success) return false;
    if (radius < FLT_EPSILON) return true;

    fill->sx = 1.0f;
    fill->sy = 1.0f;

    if (transform) {
        auto tx = fill->radial.cx * transform->e11 + fill->radial.cy * transform->e12 + transform->e13;
        auto ty = fill->radial.cx * transform->e21 + fill->radial.cy * transform->e22 + transform->e23;
        fill->radial.cx = tx;
        fill->radial.cy = ty;

        auto sx = sqrt(pow(transform->e11, 2) + pow(transform->e21, 2));
        auto sy = sqrt(pow(transform->e12, 2) + pow(transform->e22, 2));

        //FIXME; Scale + Rotation is not working properly
        radius *= sx;

        if (abs(sx - sy) > FLT_EPSILON) {
            fill->sx = sx;
            fill->sy = sy;
        }
    }

    fill->radial.a = radius * radius;
    fill->radial.inv2a = pow(1 / (2 * fill->radial.a), 2);

    return true;
}


static inline uint32_t _clamp(const SwFill* fill, int32_t pos)
{
    switch (fill->spread) {
        case FillSpread::Pad: {
            if (pos >= GRADIENT_STOP_SIZE) pos = GRADIENT_STOP_SIZE - 1;
            else if (pos < 0) pos = 0;
            break;
        }
        case FillSpread::Repeat: {
            if (pos < 0) pos = GRADIENT_STOP_SIZE + pos;
            pos = pos % GRADIENT_STOP_SIZE;
            break;
        }
        case FillSpread::Reflect: {
            auto limit = GRADIENT_STOP_SIZE * 2;
            pos = pos % limit;
            if (pos < 0) pos = limit + pos;
            if (pos >= GRADIENT_STOP_SIZE) pos = (limit - pos - 1);
            break;
        }
    }
    return pos;
}


static inline uint32_t _fixedPixel(const SwFill* fill, int32_t pos)
{
    int32_t i = (pos + (FIXPT_SIZE / 2)) >> FIXPT_BITS;
    return fill->ctable[_clamp(fill, i)];
}


static inline uint32_t _pixel(const SwFill* fill, float pos)
{
    auto i = static_cast<int32_t>(pos * (GRADIENT_STOP_SIZE - 1) + 0.5f);
    return fill->ctable[_clamp(fill, i)];
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void fillFetchRadial(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len)
{
    //Rotation
    auto rx = (x + 0.5f - fill->radial.cx) * fill->sy;
    auto ry = (y + 0.5f - fill->radial.cy) * fill->sx;
    auto inv2a = fill->radial.inv2a;
    auto rxy = rx * rx + ry * ry;
    auto rxryPlus = 2 * rx;
    auto det = (-4 * fill->radial.a * -rxy) * inv2a;
    auto detDelta = (4 * fill->radial.a * (rxryPlus + 1.0f)) * inv2a;
    auto detDelta2 = (4 * fill->radial.a * 2.0f) * inv2a;

   for (uint32_t i = 0 ; i < len ; ++i) {
        *dst = _pixel(fill, sqrt(det));
        ++dst;
        det += detDelta;
        detDelta += detDelta2;
    }
}


void fillFetchLinear(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t offset, uint32_t len)
{
    //Rotation
    float rx = x + 0.5f;
    float ry = y + 0.5f;
    float t = (fill->linear.dx * rx + fill->linear.dy * ry + fill->linear.offset) * (GRADIENT_STOP_SIZE - 1);
    float inc = (fill->linear.dx) * (GRADIENT_STOP_SIZE - 1);

    if (abs(inc) < FLT_EPSILON) {
        auto color = _fixedPixel(fill, static_cast<int32_t>(t * FIXPT_SIZE));
        rasterRGBA32(dst, color, offset, len);
        return;
    }

    dst += offset;

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


bool fillGenColorTable(SwFill* fill, const Fill* fdata, const Matrix* transform, SwSurface* surface, uint32_t opacity, bool ctable)
{
    if (!fill) return false;

    fill->spread = fdata->spread();

    if (ctable) {
        if (!_updateColorTable(fill, fdata, surface, opacity)) return false;
    }

    if (fdata->id() == FILL_ID_LINEAR) {
        return _prepareLinear(fill, static_cast<const LinearGradient*>(fdata), transform);
    } else if (fdata->id() == FILL_ID_RADIAL) {
        return _prepareRadial(fill, static_cast<const RadialGradient*>(fdata), transform);
    }

    //LOG: What type of gradient?!

    return false;
}


void fillReset(SwFill* fill)
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