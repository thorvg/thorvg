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
#include "tvgCanvasImpl.h"

#ifdef THORVG_SW_RASTER_SUPPORT
    #include "tvgSwRenderer.h"
#else
    class SwRenderer : public RenderMethod
    {
        //Non Supported. Dummy Class */
    };
#endif

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

#ifdef THORVG_SW_RASTER_SUPPORT
SwCanvas::SwCanvas() : Canvas(SwRenderer::inst()), pImpl(make_unique<Impl>())
#else
SwCanvas::SwCanvas() : Canvas(nullptr), pImpl(make_unique<Impl>())
#endif
{
}


SwCanvas::~SwCanvas()
{
}


Result SwCanvas::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h) noexcept
{
#ifdef THORVG_SW_RASTER_SUPPORT
    //We know renderer type, avoid dynamic_cast for performance.
    auto renderer = static_cast<SwRenderer*>(Canvas::pImpl.get()->renderer);
    if (!renderer) return Result::MemoryCorruption;

    if (!renderer->target(buffer, stride, w, h)) return Result::InvalidArguments;

    return Result::Success;
#endif
    return Result::NonSupport;
}


unique_ptr<SwCanvas> SwCanvas::gen() noexcept
{
#ifdef THORVG_SW_RASTER_SUPPORT
    auto canvas = unique_ptr<SwCanvas>(new SwCanvas);
    assert(canvas);

    return canvas;
#endif
    return unique_ptr<SwCanvas>(nullptr);
}

#endif /* _TVG_SWCANVAS_CPP_ */
