/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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

#include <float.h>
#include "tvgPaint.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct SceneIterator : Iterator
{
    Array<Paint*>* paints;
    uint32_t idx = 0;

    SceneIterator(Array<Paint*>* p) : paints(p)
    {
    }

    const Paint* next() override
    {
        if (idx >= paints->count) return nullptr;
        return paints->data[idx++];
    }

    uint32_t count() override
    {
        return paints->count;
    }

    void begin() override
    {
        idx = 0;
    }
};

struct Scene::Impl
{
    Array<Paint*> paints;
    uint8_t opacity;                     //for composition
    RenderMethod* renderer = nullptr;    //keep it for explicit clear
    RenderData rd = nullptr;
    Scene* scene = nullptr;

    Impl(Scene* s) : scene(s)
    {
    }

    ~Impl()
    {
        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            delete(*paint);
        }
    }

    bool dispose(RenderMethod& renderer)
    {
        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            (*paint)->pImpl->dispose(renderer);
        }

        auto ret = renderer.dispose(rd);
        this->renderer = nullptr;
        this->rd = nullptr;

        return ret;
    }

    bool needComposition(uint32_t opacity)
    {
        if (opacity == 0 || paints.count == 0) return false;

        //Masking may require composition (even if opacity == 255)
        auto compMethod = scene->composite(nullptr);
        if (compMethod != CompositeMethod::None && compMethod != CompositeMethod::ClipPath) return true;

        //Half translucent requires intermediate composition.
        if (opacity == 255) return false;

        //If scene has several children or only scene, it may require composition.
        if (paints.count > 1) return true;
        if (paints.count == 1 && (*paints.data)->identifier() == TVG_CLASS_ID_SCENE) return true;
        return false;
    }

    RenderData update(RenderMethod &renderer, const RenderTransform* transform, uint32_t opacity, Array<RenderData>& clips, RenderUpdateFlag flag, bool clipper)
    {
        /* Overriding opacity value. If this scene is half-translucent,
           It must do intermeidate composition with that opacity value. */
        this->opacity = static_cast<uint8_t>(opacity);
        if (needComposition(opacity)) opacity = 255;

        this->renderer = &renderer;

        if (clipper) {
            Array<RenderData> rds;
            rds.reserve(paints.count);
            for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
                rds.push((*paint)->pImpl->update(renderer, transform, opacity, clips, flag, true));
            }
            rd = renderer.prepare(rds, rd, transform, opacity, clips, flag);
            return rd;
        } else {
            for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
                (*paint)->pImpl->update(renderer, transform, opacity, clips, flag, false);
            }
            return nullptr;
        }
    }

    bool render(RenderMethod& renderer)
    {
        Compositor* cmp = nullptr;

        if (needComposition(opacity)) {
            cmp = renderer.target(bounds(renderer), renderer.colorSpace());
            renderer.beginComposite(cmp, CompositeMethod::None, opacity);
        }

        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            if (!(*paint)->pImpl->render(renderer)) return false;
        }

        if (cmp) renderer.endComposite(cmp);

        return true;
    }

    RenderRegion bounds(RenderMethod& renderer) const
    {
        if (paints.count == 0) return {0, 0, 0, 0};

        int32_t x1 = INT32_MAX;
        int32_t y1 = INT32_MAX;
        int32_t x2 = 0;
        int32_t y2 = 0;

        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            auto region = (*paint)->pImpl->bounds(renderer);

            //Merge regions
            if (region.x < x1) x1 = region.x;
            if (x2 < region.x + region.w) x2 = (region.x + region.w);
            if (region.y < y1) y1 = region.y;
            if (y2 < region.y + region.h) y2 = (region.y + region.h);
        }

        return {x1, y1, (x2 - x1), (y2 - y1)};
    }

    bool bounds(float* px, float* py, float* pw, float* ph)
    {
        if (paints.count == 0) return false;

        auto x1 = FLT_MAX;
        auto y1 = FLT_MAX;
        auto x2 = -FLT_MAX;
        auto y2 = -FLT_MAX;

        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            auto x = FLT_MAX;
            auto y = FLT_MAX;
            auto w = 0.0f;
            auto h = 0.0f;

            if ((*paint)->bounds(&x, &y, &w, &h, true) != tvg::Result::Success) continue;

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

        auto dup = ret.get()->pImpl;

        dup->paints.reserve(paints.count);

        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            dup->paints.push((*paint)->duplicate());
        }

        return ret.release();
    }

    void clear(bool free)
    {
        auto dispose = renderer ? true : false;

        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            if (dispose) (*paint)->pImpl->dispose(*renderer);
            if (free) delete(*paint);
        }
        paints.clear();
        renderer = nullptr;
    }

    Iterator* iterator()
    {
        return new SceneIterator(&paints);
    }
};

#endif //_TVG_SCENE_IMPL_H_
