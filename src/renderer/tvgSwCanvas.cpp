/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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
#include "tvgTaskScheduler.h"
#include "tvgLoadModule.h"

#ifdef THORVG_SW_RASTER_SUPPORT
    #include "tvgSwRenderer.h"
#endif


SwCanvas::SwCanvas() = default;

SwCanvas::~SwCanvas()
{
#ifdef THORVG_SW_RASTER_SUPPORT
    SwRenderer::term();
#endif
}


Result SwCanvas::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h, ColorSpace cs) noexcept
{
#ifdef THORVG_SW_RASTER_SUPPORT
    if (cs == ColorSpace::Unknown) return Result::InvalidArguments;
    if (cs == ColorSpace::Grayscale8) return Result::NonSupport;

    if (pImpl->status != Status::Damaged && pImpl->status != Status::Synced) {
        return Result::InsufficientCondition;
    }

    //We know renderer type, avoid dynamic_cast for performance.
    auto renderer = static_cast<SwRenderer*>(pImpl->renderer);
    if (!renderer) return Result::MemoryCorruption;

    if (!renderer->target(buffer, stride, w, h, cs)) return Result::InvalidArguments;
    pImpl->vport = {{0, 0}, {(int32_t)w, (int32_t)h}};
    renderer->viewport(pImpl->vport);

    //FIXME: The value must be associated with an individual canvas instance.
    ImageLoader::cs = static_cast<ColorSpace>(cs);

    //Paints must be updated again with this new target.
    pImpl->status = Status::Damaged;

    return Result::Success;
#endif
    return Result::NonSupport;
}


SwCanvas* SwCanvas::gen() noexcept
{
#ifdef THORVG_SW_RASTER_SUPPORT
    if (engineInit > 0) {
        auto renderer = SwRenderer::gen(TaskScheduler::threads());
        renderer->ref();
        auto ret = new SwCanvas;
        ret->pImpl->renderer = renderer;
        return ret;
    }
#endif
    return nullptr;
}
