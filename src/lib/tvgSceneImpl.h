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
};

#endif //_TVG_SCENE_IMPL_H_