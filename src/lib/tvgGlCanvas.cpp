/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef _TVG_GLCANVAS_CPP_
#define _TVG_GLCANVAS_CPP_

#include "tvgCommon.h"
#include "tvgCanvasImpl.h"

#ifdef THORVG_GL_RASTER_SUPPORT
    #include "tvgGlRenderer.h"
#else
    class GlRenderer : public RenderMethod
    {
        //Non Supported. Dummy Class */
    };
#endif

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct GlCanvas::Impl
{
    Impl() {}
};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

#ifdef THORVG_GL_RASTER_SUPPORT
GlCanvas::GlCanvas() : Canvas(GlRenderer::inst()), pImpl(make_unique<Impl>())
#else
GlCanvas::GlCanvas() : Canvas(nullptr), pImpl(make_unique<Impl>())
#endif
{
}



GlCanvas::~GlCanvas()
{
}


Result GlCanvas::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h) noexcept
{
#ifdef THORVG_GL_RASTER_SUPPORT
    //We know renderer type, avoid dynamic_cast for performance.
    auto renderer = static_cast<GlRenderer*>(Canvas::pImpl.get()->renderer);
    if (!renderer) return Result::MemoryCorruption;

    if (!renderer->target(buffer, stride, w, h, 0)) return Result::Unknown;

    return Result::Success;
#endif
    return Result::NonSupport;
}


unique_ptr<GlCanvas> GlCanvas::gen() noexcept
{
#ifdef THORVG_GL_RASTER_SUPPORT
    auto canvas = unique_ptr<GlCanvas>(new GlCanvas);
    assert(canvas);

    return canvas;
#endif
    return nullptr;
}


#endif /* _TVG_GLCANVAS_CPP_ */
