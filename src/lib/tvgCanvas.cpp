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
#ifndef _TVG_CANVAS_CPP_
#define _TVG_CANVAS_CPP_

#include "tvgCommon.h"
#include "tvgCanvasImpl.h"

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Canvas::Canvas(RenderMethod *pRenderer):pImpl(make_unique<Impl>(pRenderer))
{
}


Canvas::~Canvas()
{
}


Result Canvas::reserve(uint32_t n) noexcept
{
    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;
    impl->paints.reserve(n);
    return Result::Success;
}


Result Canvas::push(unique_ptr<Paint> paint) noexcept
{
    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;
    return impl->push(move(paint));
}


Result Canvas::clear() noexcept
{
    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;
    return impl->clear();
}


Result Canvas::draw(bool async) noexcept
{
    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;
    return impl->draw();
}


Result Canvas::update() noexcept
{
    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;
    return impl->update();
}


Result Canvas::update(Paint* paint) noexcept
{
    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;
    return impl->update(paint);
}


Result Canvas::sync() noexcept
{
    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;

    if (impl->renderer->flush()) return Result::Success;

    return Result::InsufficientCondition;
}

#endif /* _TVG_CANVAS_CPP_ */
