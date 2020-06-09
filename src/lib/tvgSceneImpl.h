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
#ifndef _TVG_SCENE_IMPL_H_
#define _TVG_SCENE_IMPL_H_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Scene::Impl
{
    vector<Paint*> paints;
    RenderTransform *transform = nullptr;
    uint32_t flag = RenderUpdateFlag::None;

    ~Impl()
    {
        //Are you sure clear() prior to this?
        assert(paints.empty());
        if (transform) delete(transform);
    }

    bool clear(RenderMethod& renderer)
    {
        for (auto paint : paints) {
            if (paint->id == PAINT_ID_SCENE) {
                //We know renderer type, avoid dynamic_cast for performance.
                auto scene = static_cast<Scene*>(paint);
                if (!SCENE_IMPL->clear(renderer)) return false;
            } else {
                auto shape = static_cast<Shape*>(paint);
                if (!SHAPE_IMPL->dispose(*shape, renderer)) return false;
            }
            delete(paint);
        }
        paints.clear();

        return true;
    }

    bool updateInternal(RenderMethod &renderer, const RenderTransform* transform, uint32_t flag)
    {
        for(auto paint: paints) {
            if (paint->id == PAINT_ID_SCENE) {
                //We know renderer type, avoid dynamic_cast for performance.
                auto scene = static_cast<Scene*>(paint);
                if (!SCENE_IMPL->update(renderer, transform, flag)) return false;
            } else {
                auto shape = static_cast<Shape*>(paint);
                if (!SHAPE_IMPL->update(*shape, renderer, transform, flag)) return false;
            }
        }
        return true;
    }

    bool update(RenderMethod &renderer, const RenderTransform* pTransform = nullptr, uint32_t pFlag = 0)
    {
        if (flag & RenderUpdateFlag::Transform) {
            if (!transform) return false;
            if (!transform->update()) {
                delete(transform);
                transform = nullptr;
            }
        }

        auto ret = true;

        if (transform && pTransform) {
            RenderTransform outTransform(pTransform, transform);
            ret = updateInternal(renderer, &outTransform, pFlag | flag);
        } else {
            auto outTransform = pTransform ? pTransform : transform;
            ret = updateInternal(renderer, outTransform, pFlag | flag);
        }

        flag = RenderUpdateFlag::None;

        return ret;
    }

    bool render(RenderMethod &renderer)
    {
        for(auto paint: paints) {
            if (paint->id == PAINT_ID_SCENE) {
                //We know renderer type, avoid dynamic_cast for performance.
                auto scene = static_cast<Scene*>(paint);
                if(!SCENE_IMPL->render(renderer)) return false;
            } else {
                auto shape = static_cast<Shape*>(paint);
                if(!SHAPE_IMPL->render(*shape, renderer)) return false;
            }
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

            if (paint->id == PAINT_ID_SCENE) {
                //We know renderer type, avoid dynamic_cast for performance.
                auto scene = static_cast<Scene*>(paint);
                if (!SCENE_IMPL->bounds(&x2, &y2, &w2, &h2)) return false;
            } else {
                auto shape = static_cast<Shape*>(paint);
                if (!SHAPE_IMPL->bounds(&x2, &y2, &w2, &h2)) return false;
            }

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

    bool scale(float factor)
    {
        if (transform) {
            if (fabsf(factor - transform->factor) <= FLT_EPSILON) return true;
        } else {
            if (fabsf(factor) <= FLT_EPSILON) return true;
            transform = new RenderTransform();
            if (!transform) return false;
        }
        transform->factor = factor;
        flag |= RenderUpdateFlag::Transform;

        return true;
    }

    bool rotate(float degree)
    {
        if (transform) {
            if (fabsf(degree - transform->degree) <= FLT_EPSILON) return true;
        } else {
            if (fabsf(degree) <= FLT_EPSILON) return true;
            transform = new RenderTransform();
            if (!transform) return false;
        }
        transform->degree = degree;
        flag |= RenderUpdateFlag::Transform;

        return true;
    }

    bool translate(float x, float y)
    {
        if (transform) {
            if (fabsf(x - transform->x) <= FLT_EPSILON && fabsf(y - transform->y) <= FLT_EPSILON) return true;
        } else {
            if (fabsf(x) <= FLT_EPSILON && fabsf(y) <= FLT_EPSILON) return true;
            transform = new RenderTransform();
            if (!transform) return false;
        }
        transform->x = x;
        transform->y = y;
        flag |= RenderUpdateFlag::Transform;

        return true;
    }
};

#endif //_TVG_SCENE_IMPL_H_