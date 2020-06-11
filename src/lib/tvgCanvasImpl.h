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
#ifndef _TVG_CANVAS_IMPL_H_
#define _TVG_CANVAS_IMPL_H_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Canvas::Impl
{
    vector<Paint*> paints;
    RenderMethod*  renderer;

    Impl(RenderMethod* pRenderer):renderer(pRenderer)
    {
        renderer->ref();
    }

    ~Impl()
    {
        clear();
        renderer->unref();
    }

    Result push(unique_ptr<Paint> paint)
    {
        auto p = paint.release();
        assert(p);
        paints.push_back(p);

        return update(p);
    }

    Result clear()
    {
        assert(renderer);

        for (auto paint : paints) {
            if (paint->id() == PAINT_ID_SCENE) {
                //We know renderer type, avoid dynamic_cast for performance.
                auto scene = static_cast<Scene*>(paint);
                if (!SCENE_IMPL->clear(*renderer)) return Result::InsufficientCondition;
            } else {
                auto shape = static_cast<Shape*>(paint);
                if (!SHAPE_IMPL->dispose(*shape, *renderer)) return Result::InsufficientCondition;
            }
            delete(paint);
        }
        paints.clear();

        return Result::Success;
    }

    Result update()
    {
        assert(renderer);

        for(auto paint: paints) {
            if (paint->id() == PAINT_ID_SCENE) {
                //We know renderer type, avoid dynamic_cast for performance.
                auto scene = static_cast<Scene*>(paint);
                if (!SCENE_IMPL->update(*renderer, nullptr)) return Result::InsufficientCondition;
            } else {
                auto shape = static_cast<Shape*>(paint);
                if (!SHAPE_IMPL->update(*shape, *renderer, nullptr)) return Result::InsufficientCondition;
            }
        }
        return Result::Success;
    }

    Result update(Paint* paint)
    {
        assert(renderer);

        if (paint->id() == PAINT_ID_SCENE) {
            //We know renderer type, avoid dynamic_cast for performance.
            auto scene = static_cast<Scene*>(paint);
            if (!SCENE_IMPL->update(*renderer)) return Result::InsufficientCondition;
        } else {
            auto shape = static_cast<Shape*>(paint);
            if (!SHAPE_IMPL->update(*shape, *renderer)) return Result::InsufficientCondition;
        }
        return Result::Success;
    }

    Result draw()
    {
        assert(renderer);

        //Clear render target before drawing
        if (!renderer->clear()) return Result::InsufficientCondition;

        for(auto paint: paints) {
           if (paint->id() == PAINT_ID_SCENE) {
                //We know renderer type, avoid dynamic_cast for performance.
                auto scene = static_cast<Scene*>(paint);
                if(!SCENE_IMPL->render(*renderer)) return Result::InsufficientCondition;
            } else {
                auto shape = static_cast<Shape*>(paint);
                if(!SHAPE_IMPL->render(*shape, *renderer)) return Result::InsufficientCondition;
            }
        }
        return Result::Success;
    }
};

#endif /* _TVG_CANVAS_IMPL_H_ */