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

#ifdef THORVG_GL_RASTER_SUPPORT
    #include "tvgGlRenderer.h"
#endif

GlCanvas::GlCanvas() = default;

GlCanvas::~GlCanvas()
{
#ifdef THORVG_GL_RASTER_SUPPORT
    GlRenderer::term();
#endif
}


Result GlCanvas::target(void* context, int32_t id, uint32_t w, uint32_t h, ColorSpace cs) noexcept
{
#ifdef THORVG_GL_RASTER_SUPPORT
    if (cs != ColorSpace::ABGR8888S) return Result::NonSupport;

    if (pImpl->status != Status::Damaged && pImpl->status != Status::Synced) {
        return Result::InsufficientCondition;
    }

    //We know renderer type, avoid dynamic_cast for performance.
    auto renderer = static_cast<GlRenderer*>(pImpl->renderer);
    if (!renderer) return Result::MemoryCorruption;

    if (!renderer->target(context, id, w, h)) return Result::Unknown;
    pImpl->vport = {{0, 0}, {(int32_t)w, (int32_t)h}};
    renderer->viewport(pImpl->vport);

    //Paints must be updated again with this new target.
    pImpl->status = Status::Damaged;

    return Result::Success;
#endif
    return Result::NonSupport;
}


GlCanvas* GlCanvas::gen() noexcept
{
#ifdef THORVG_GL_RASTER_SUPPORT
    if (engineInit > 0) {
        auto renderer = GlRenderer::gen(TaskScheduler::threads());
        if (!renderer) return nullptr;
        renderer->ref();
        auto ret = new GlCanvas;
        ret->pImpl->renderer = renderer;
        return ret;
    }
#endif
    return nullptr;
}
