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

#include "tvgLottieModel.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/



/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

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
        auto w = fabsf(end(frameNo).x - sx);
        auto h = fabsf(end(frameNo).y - sy);
        auto radius = (w > h) ? (w + 0.375f * h) : (h + 0.375f * w);
        static_cast<RadialGradient*>(fill)->radial(sx, sy, radius);

        //TODO: focal support?
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


int32_t LottieLayer::remap(int32_t frameNo)
{
    if (timeRemap.frames) {
        frameNo = comp->frameAtTime(timeRemap(frameNo));
    }

    if (timeStretch == 1.0f) return frameNo;
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