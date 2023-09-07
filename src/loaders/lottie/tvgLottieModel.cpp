/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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

#include "tvgFill.h"
#include "tvgLottieModel.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/



/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void LottieTrimpath::segment(int32_t frameNo, float& start, float& end)
{
    auto s = this->start(frameNo) * 0.01f;
    auto e = this->end(frameNo) * 0.01f;
    auto o = fmod(this->offset(frameNo), 360.0f) / 360.0f;  //0 ~ 1

    auto diff = fabs(s - e);
    if (mathZero(diff)) {
        start = 0.0f;
        end = 0.0f;
        return;
    }
    if (mathEqual(diff, 1.0f) || mathEqual(diff, 2.0f)) {
        start = 0.0f;
        end = 1.0f;
        return;
    }

    s += o;
    e += o;

    auto loop = true;

    //no loop
    if (s > 1.0f && e > 1.0f) loop = false;
    if (s < 0.0f && e < 0.0f) loop = false;
    if (s >= 0.0f && s <= 1.0f && e >= 0.0f  && e <= 1.0f) loop = false;

    if (s > 1.0f) s -= 1.0f;
    if (s < 0.0f) s += 1.0f;
    if (e > 1.0f) e -= 1.0f;
    if (e < 0.0f) e += 1.0f;

    if (loop) {
        start = s > e ? s : e;
        end = s < e ? s : e;
    } else {
        start = s < e ? s : e;
        end = s > e ? s : e;
    }
}


Fill* LottieGradient::fill(int32_t frameNo)
{
    Fill* fill = nullptr;

    //Linear Graident
    if (id == 1) {
        fill = LinearGradient::gen().release();
        static_cast<LinearGradient*>(fill)->linear(start(frameNo).x, start(frameNo).y, end(frameNo).x, end(frameNo).y);
    }
    //Radial Gradient
    if (id == 2) {
        fill = RadialGradient::gen().release();

        auto sx = start(frameNo).x;
        auto sy = start(frameNo).y;
        auto ex = end(frameNo).x;
        auto ey = end(frameNo).y;
        auto w = fabsf(ex - sx);
        auto h = fabsf(ey - sy);
        auto r = (w > h) ? (w + 0.375f * h) : (h + 0.375f * w);
        auto progress = this->height(frameNo) * 0.01f;

        if (mathZero(progress)) {
            P(static_cast<RadialGradient*>(fill))->radial(sx, sy, r, sx, sy, 0.0f);
        } else {
            if (mathEqual(progress, 1.0f)) progress = 0.99f;
            auto startAngle = atan2(ey - sy, ex - sx) * 180.0f / MATH_PI;
            auto angle = (startAngle + this->angle(frameNo)) * (MATH_PI / 180.0f);
            auto fx = sx + cos(angle) * progress * r;
            auto fy = sy + sin(angle) * progress * r;
            // Lottie dosen't have any focal radius concept
            P(static_cast<RadialGradient*>(fill))->radial(sx, sy, r, fx, fy, 0.0f);
        }
    }

    if (!fill) return nullptr;

    colorStops(frameNo, fill);

    return fill;
}


void LottieGroup::prepare(LottieObject::Type type)
{
    LottieObject::type = type;
    if (transform) statical &= transform->statical;
    for (auto child = children.data; child < children.end(); ++child) {
        statical &= (*child)->statical;
        if (!statical) break;
    }
}


void LottieLayer::prepare()
{
    LottieGroup::prepare(LottieObject::Layer);

    /* if layer is hidden, only useulf data is its transform matrix.
        so force it to be a Null Layer and release all resource. */
    if (!hidden) return;
    type = LottieLayer::Null;
    children.reset();
}


int32_t LottieLayer::remap(int32_t frameNo)
{
    if (timeRemap.frames) {
        frameNo = comp->frameAtTime(timeRemap(frameNo));
    }
    return (int32_t)(frameNo / timeStretch);
}


LottieComposition::~LottieComposition()
{
    delete(root);
    free(version);
    free(name);

    //delete interpolators
    for (auto i = interpolators.data; i < interpolators.end(); ++i) {
        free((*i)->key);
        free(*i);
    }

    //delete assets
    for (auto a = assets.data; a < assets.end(); ++a) {
        delete(*a);
    }
}