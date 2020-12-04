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
#ifndef _TVG_SCENE_IMPL_H_
#define _TVG_SCENE_IMPL_H_

#include <vector>
#include "tvgPaint.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Scene::Impl
{
    vector<Paint*> paints;
    uint32_t opacity;

    bool dispose(RenderMethod& renderer)
    {
        for (auto paint : paints) {
            paint->pImpl->dispose(renderer);
            delete(paint);
        }
        paints.clear();

        return true;
    }

    void* update(RenderMethod &renderer, const RenderTransform* transform, uint32_t opacity, vector<Composite>& compList, RenderUpdateFlag flag)
    {
        this->opacity = opacity;

        /* Overriding opacity value. If this scene is half-translucent,
           It must do intermeidate composition with that opacity value. */
        if (opacity < 255 && opacity > 0) opacity = 255;

        /* FXIME: it requires to return list of children engine data
           This is necessary for scene composition */
        void* edata = nullptr;

        for (auto paint : paints) {
            edata = paint->pImpl->update(renderer, transform, opacity, compList, static_cast<uint32_t>(flag));
        }

        return edata;
    }

    bool render(RenderMethod &renderer)
    {
        void* ctx = nullptr;

        //Half translucent. This requires intermediate composition.
        if (opacity < 255 && opacity > 0) {
            float x, y, w, h;
            if (!bounds(&x, &y, &w, &h)) return false;
            if (x < 0) {
                w += x;
                x = 0;
            }
            if (y < 0) {
                h += y;
                y = 0;
            }
            ctx = renderer.beginComposite(roundf(x), roundf(y), roundf(w), roundf(h));
        }

        for (auto paint : paints) {
            if (!paint->pImpl->render(renderer)) return false;
        }     

        if (ctx) return renderer.endComposite(ctx, opacity);

        return true;
    }

    bool bounds(float* px, float* py, float* pw, float* ph)
    {
        if (paints.size() == 0) return false;

        auto x1 = FLT_MAX;
        auto y1 = FLT_MAX;
        auto x2 = 0.0f;
        auto y2 = 0.0f;

        for (auto paint : paints) {
            auto x = FLT_MAX;
            auto y = FLT_MAX;
            auto w = 0.0f;
            auto h = 0.0f;

            if (!paint->pImpl->bounds(&x, &y, &w, &h)) continue;

            //Merge regions
            if (x < x1) x1 = x;
            if (x2 < x + w) x2 = (x + w);
            if (y < y1) y1 = y;
            if (y2 < y + h) y2 = (y + h);
        }

        if (px) *px = x1;
        if (py) *py = y1;
        if (pw) *pw = (x2 - x1);
        if (ph) *ph = (y2 - y1);

        return true;
    }

    Paint* duplicate()
    {
        auto ret = Scene::gen();
        if (!ret) return nullptr;
        auto dup = ret.get()->pImpl;

        dup->paints.reserve(paints.size());

        for (auto paint : paints) {
            dup->paints.push_back(paint->duplicate());
        }

        return ret.release();
    }
};

#endif //_TVG_SCENE_IMPL_H_
