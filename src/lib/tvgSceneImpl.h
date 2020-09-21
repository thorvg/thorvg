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

    bool dispose(RenderMethod& renderer)
    {
        for (auto paint : paints) {
            paint->pImpl->dispose(renderer);
            delete(paint);
        }
        paints.clear();

        return true;
    }

    bool update(RenderMethod &renderer, const RenderTransform* transform, RenderUpdateFlag flag)
    {
        for(auto paint: paints) {
            if (!paint->pImpl->update(renderer, transform, static_cast<uint32_t>(flag))) return false;
        }
        return true;
    }

    bool render(RenderMethod &renderer)
    {
        for(auto paint: paints) {
            if(!paint->pImpl->render(renderer)) return false;
        }
        return true;
    }

    bool bounds(float* px, float* py, float* pw, float* ph)
    {
        auto x = FLT_MAX;
        auto y = FLT_MAX;
        auto w = 0.0f;
        auto h = 0.0f;

        for(auto paint: paints) {
            auto x2 = FLT_MAX;
            auto y2 = FLT_MAX;
            auto w2 = 0.0f;
            auto h2 = 0.0f;

            if (paint->pImpl->bounds(&x2, &y2, &w2, &h2)) return false;

            //Merge regions
            if (x2 < x) x = x2;
            if (x + w < x2 + w2) w = (x2 + w2) - x;
            if (y2 < y) y = x2;
            if (y + h < y2 + h2) h = (y2 + h2) - y;
        }

        if (px) *px = x;
        if (py) *py = y;
        if (pw) *pw = w;
        if (ph) *ph = h;

        return true;
    }

    Paint* duplicate()
    {
        auto ret = Scene::gen();
        if (!ret) return nullptr;
        auto dup = ret.get()->pImpl;

        dup->paints.reserve(paints.size());

        for (auto paint: paints) {
            dup->paints.push_back(paint->duplicate());
        }

        return ret.release();
    }

    bool composite(unique_ptr<Paint> comp, CompMethod method)
    {
        auto c = comp.release();
        for(auto paint: paints) {
            if(!paint->pImpl->composite(c, method)) return false;
        }
        return false;
    }

    bool composite(Paint* comp, CompMethod method)
    {
        for(auto paint: paints) {
            if(!paint->pImpl->composite(comp, method)) return false;
        }
        return false;
    }

    Paint* composite()
    {
        return nullptr;
    }

    CompMethod compositeMethod()
    {
        return CompMethod::None;
    }
};

#endif //_TVG_SCENE_IMPL_H_
