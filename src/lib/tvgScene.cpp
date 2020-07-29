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
    _id = PAINT_ID_SCENE;

    Paint::pImpl.get()->ts = pImpl.get()->transformMethod();
}


Scene::~Scene()
{
}


unique_ptr<Scene> Scene::gen() noexcept
{
    return unique_ptr<Scene>(new Scene);
}


Result Scene::push(unique_ptr<Paint> paint) noexcept
{
    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;

    auto p = paint.release();
    if (!p) return Result::MemoryCorruption;
    impl->paints.push_back(p);

    return Result::Success;
}


Result Scene::reserve(uint32_t size) noexcept
{
    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;

    impl->paints.reserve(size);

    return Result::Success;
}


Result Scene::load(const std::string& path) noexcept
{
    if (path.empty()) return Result::InvalidArguments;

    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;

     return impl->load(path);
}

#endif /* _TVG_SCENE_CPP_ */