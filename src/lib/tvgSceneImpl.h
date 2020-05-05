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

    ~Impl()
    {
        //Are you sure clear() prior to this?
        assert(paints.empty());
    }

    bool clear(RenderMethod& renderer)
    {
        for (auto paint : paints) {
            if (auto scene = dynamic_cast<Scene*>(paint)) {
                if (!SCENE_IMPL->clear(renderer)) return false;
            } else if (auto shape = dynamic_cast<Shape*>(paint)) {
                if (!SHAPE_IMPL->dispose(*shape, renderer)) return false;
            }
            delete(paint);
        }
        paints.clear();

        return true;
    }

    bool update(RenderMethod &renderer)
    {
        for(auto paint: paints) {
            if (auto scene = dynamic_cast<Scene*>(paint)) {
                if (!SCENE_IMPL->update(renderer)) return false;
            } else if (auto shape = dynamic_cast<Shape*>(paint)) {
                if (!SHAPE_IMPL->update(*shape, renderer)) return false;
            }
        }
        return true;
    }

    bool render(RenderMethod &renderer)
    {
        for(auto paint: paints) {
            if (auto scene = dynamic_cast<Scene*>(paint)) {
                if(!SCENE_IMPL->render(renderer)) return false;
            } else if (auto shape = dynamic_cast<Shape*>(paint)) {
                if(!SHAPE_IMPL->render(*shape, renderer)) return false;
            }
        }
        return true;
    }

    bool bounds(float& x, float& y, float& w, float& h)
    {
        for(auto paint: paints) {
            auto x2 = FLT_MAX;
            auto y2 = FLT_MAX;
            auto w2 = 0.0f;
            auto h2 = 0.0f;

            if (auto scene = dynamic_cast<Scene*>(paint)) {
                if (!SCENE_IMPL->bounds(x2, y2, w2, h2)) return false;
            } else if (auto shape = dynamic_cast<Shape*>(paint)) {
                if (!SHAPE_IMPL->bounds(x2, y2, w2, h2)) return false;
            }

            //Merge regions
            if (x2 < x) x = x2;
            if (x + w < x2 + w2) w = (x2 + w2) - x;
            if (y2 < y) y = x2;
            if (y + h < y2 + h2) h = (y2 + h2) - y;
        }
        return true;
    }
};

#endif //_TVG_SCENE_IMPL_H_