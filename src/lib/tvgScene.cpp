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
#ifndef _TVG_SCENE_CPP_
#define _TVG_SCENE_CPP_

#include "tvgCommon.h"

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Scene::Scene() : pImpl(make_unique<Impl>())
{

}


Scene::~Scene()
{
}


unique_ptr<Scene> Scene::gen() noexcept
{
    return unique_ptr<Scene>(new Scene);
}


int Scene::push(unique_ptr<Paint> paint) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    auto p = paint.release();
    assert(p);
    impl->paints.push_back(p);

    return 0;
}


int Scene::reserve(size_t size) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    impl->paints.reserve(size);

    return 0;
}


int Scene::scale(float factor) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    return impl->scale(factor);
}


int Scene::rotate(float degree) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    return impl->rotate(degree);
}


int Scene::translate(float x, float y) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    return impl->translate(x, y);
}


int Scene::bounds(float& x, float& y, float& w, float& h) const noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    x = FLT_MAX;
    y = FLT_MAX;
    w = 0;
    h = 0;

    if (!impl->bounds(x, y, w, h)) return -1;

    return 0;
}

#endif /* _TVG_SCENE_CPP_ */