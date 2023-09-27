/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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

#include "tvgCanvas.h"

#ifdef THORVG_WGPU_RASTER_SUPPORT
    #include "tvgWgpuRenderer.h"
#else
    class WgpuRenderer : public RenderMethod
    {
        //Non Supported. Dummy Class */
    };
#endif

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct WgpuCanvas::Impl
{
};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

// webgpy canvas constructor
#ifdef THORVG_WGPU_RASTER_SUPPORT
WgpuCanvas::WgpuCanvas() : Canvas(WgpuRenderer::gen()), pImpl(new Impl)
#else
WgpuCanvas::WgpuCanvas() : Canvas(nullptr), pImpl(new Impl)
#endif
{
}

// webgpy canvas destructor
WgpuCanvas::~WgpuCanvas()
{
    delete pImpl;
}

// set target buffer
Result WgpuCanvas::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h) noexcept
{
#ifdef THORVG_WGPU_RASTER_SUPPORT
    //We know renderer type, avoid dynamic_cast for performance.
    auto renderer = static_cast<WgpuRenderer*>(Canvas::pImpl->renderer);
    if (!renderer) return Result::MemoryCorruption;

    if (!renderer->target(buffer, stride, w, h)) return Result::Unknown;

    //Paints must be updated again with this new target.
    Canvas::pImpl->needRefresh();

    return Result::Success;
#endif
    return Result::NonSupport;
}

// generate canvas instance
unique_ptr<WgpuCanvas> WgpuCanvas::gen() noexcept
{
#ifdef THORVG_WGPU_RASTER_SUPPORT
    return unique_ptr<WgpuCanvas>(new WgpuCanvas);
#endif
    return nullptr;
}
