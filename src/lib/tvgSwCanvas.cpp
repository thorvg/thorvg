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
#ifndef _TVG_SWCANVAS_CPP_
#define _TVG_SWCANVAS_CPP_

#include "tvgCommon.h"
#include "tvgCanvasBase.h"
#include "tvgSwRenderer.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct SwCanvas::Impl : CanvasBase
{
    Impl() : CanvasBase(SwRenderer::inst()) {}
};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

int SwCanvas::target(uint32_t* buffer, size_t stride, size_t height) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    dynamic_cast<SwRenderer*>(impl->renderer)->target(buffer, stride, height);

    return 0;
}


int SwCanvas::draw(bool async) noexcept
{
    auto impl = pImpl.get();
    assert(impl);
    return impl->draw();
}


int SwCanvas::sync() noexcept
{
    return 0;
}


int SwCanvas::push(unique_ptr<PaintNode> paint) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    return impl->push(move(paint));
}


int SwCanvas::clear() noexcept
{
    auto impl = pImpl.get();
    assert(impl);
    return impl->clear();
}


SwCanvas::SwCanvas() : pImpl(make_unique<Impl>())
{
}


SwCanvas::~SwCanvas()
{
}


unique_ptr<SwCanvas> SwCanvas::gen(uint32_t* buffer, size_t stride, size_t height) noexcept
{
    auto canvas = unique_ptr<SwCanvas>(new SwCanvas);
    assert(canvas);

    int ret = canvas.get()->target(buffer, stride, height);
    if (ret > 0)  return nullptr;

   return canvas;
}


int SwCanvas::update() noexcept
{
    auto impl = pImpl.get();
    assert(impl);
    return impl->update();
}


RenderMethod* SwCanvas::engine() noexcept
{
    auto impl = pImpl.get();
    assert(impl);
    return impl->renderer;
}

#endif /* _TVG_SWCANVAS_CPP_ */
