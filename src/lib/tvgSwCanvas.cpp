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
#include "tvgSwRenderer.h"
#include "tvgCanvasImpl.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct SwCanvas::Impl
{
    Impl() {}
};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SwCanvas::SwCanvas() : Canvas(SwRenderer::inst()), pImpl(make_unique<Impl>())
{
}


SwCanvas::~SwCanvas()
{
}


Result SwCanvas::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h) noexcept
{
    //We know renderer type, avoid dynamic_cast for performance.
    auto renderer = static_cast<SwRenderer*>(Canvas::pImpl.get()->renderer);
    if (!renderer) return Result::MemoryCorruption;

    if (!renderer->target(buffer, stride, w, h)) return Result::InvalidArguments;

    return Result::Success;
}


unique_ptr<SwCanvas> SwCanvas::gen() noexcept
{
    auto canvas = unique_ptr<SwCanvas>(new SwCanvas);
    assert(canvas);

   return canvas;
}

#endif /* _TVG_SWCANVAS_CPP_ */
