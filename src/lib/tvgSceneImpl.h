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

#include "tvgPaint.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Scene::Impl
{
    Array<Paint*> paints;

    bool dispose(RenderMethod& renderer)
    {
        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            (*paint)->pImpl->dispose(renderer);
            delete(*paint);
        }
        paints.clear();

        return true;
    }

    void* update(RenderMethod &renderer, const RenderTransform* transform, uint32_t opacity, Array<RenderData>& clips, RenderUpdateFlag flag)
    {
        /* Overriding opacity value. If this scene is half-translucent,
           It must do intermeidate composition with that opacity value. */
        if (opacity > 0) opacity = 255;

        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            (*paint)->pImpl->update(renderer, transform, opacity, clips, static_cast<uint32_t>(flag));
        }

        /* FXIME: it requires to return list of children engine data
           This is necessary for scene composition */
        return nullptr;
    }

    bool render(RenderMethod& renderer, uint32_t opacity)
    {
        Compositor* cmp = nullptr;

        //Half translucent. This condition requires intermediate composition.
        if ((opacity < 255 && opacity > 0) && (paints.count > 1)) {
            uint32_t x, y, w, h;
            if (!bounds(renderer, &x, &y, &w, &h)) return false;
            cmp = renderer.target(x, y, w, h);
            renderer.beginComposite(cmp, CompositeMethod::None, opacity);
        }

        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            if (!(*paint)->pImpl->render(renderer, opacity)) return false;
        }

        if (cmp) renderer.endComposite(cmp);

        return true;
    }

    bool bounds(RenderMethod& renderer, uint32_t* px, uint32_t* py, uint32_t* pw, uint32_t* ph)
    {
        if (paints.count == 0) return false;

        uint32_t x1 = UINT32_MAX;
        uint32_t y1 = UINT32_MAX;
        uint32_t x2 = 0;
        uint32_t y2 = 0;

        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            uint32_t x = UINT32_MAX;
            uint32_t y = UINT32_MAX;
            uint32_t w = 0;
            uint32_t h = 0;

            if (!(*paint)->pImpl->bounds(renderer, &x, &y, &w, &h)) continue;

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

    bool bounds(float* px, float* py, float* pw, float* ph)
    {
        if (paints.count == 0) return false;

        auto x1 = FLT_MAX;
        auto y1 = FLT_MAX;
        auto x2 = 0.0f;
        auto y2 = 0.0f;

        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            auto x = FLT_MAX;
            auto y = FLT_MAX;
            auto w = 0.0f;
            auto h = 0.0f;

            if (!(*paint)->pImpl->bounds(&x, &y, &w, &h)) continue;

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

        dup->paints.reserve(paints.count);

        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            dup->paints.push((*paint)->duplicate());
        }

        return ret.release();
    }
};

#endif //_TVG_SCENE_IMPL_H_
