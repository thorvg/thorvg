/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "aa_product_renderer.h"

#include "tvgCanvas.h"
#include "tvgGlRenderer.h"

namespace aa_poc
{
namespace
{

GlAaMode rendererMode(ProductAaMode mode)
{
    switch (mode) {
        case ProductAaMode::NoAa: return GlAaMode::NoAa;
        case ProductAaMode::Msaa4: return GlAaMode::Msaa4;
        case ProductAaMode::FlatDirect: return GlAaMode::FlatDirect;
        case ProductAaMode::CurveDirect: return GlAaMode::CurveDirect;
        case ProductAaMode::FlatMask: return GlAaMode::FlatMask;
        case ProductAaMode::CurveMask: return GlAaMode::CurveMask;
        case ProductAaMode::Hybrid: return GlAaMode::Hybrid;
    }
    return GlAaMode::NoAa;
}

ProductAaMode productMode(GlAaMode mode)
{
    switch (mode) {
        case GlAaMode::NoAa: return ProductAaMode::NoAa;
        case GlAaMode::Msaa4: return ProductAaMode::Msaa4;
        case GlAaMode::FlatDirect: return ProductAaMode::FlatDirect;
        case GlAaMode::CurveDirect: return ProductAaMode::CurveDirect;
        case GlAaMode::FlatMask: return ProductAaMode::FlatMask;
        case GlAaMode::CurveMask: return ProductAaMode::CurveMask;
        case GlAaMode::Hybrid: return ProductAaMode::Hybrid;
    }
    return ProductAaMode::NoAa;
}

GlRenderer* renderer(tvg::GlCanvas& canvas)
{
    return static_cast<GlRenderer*>(canvas.pImpl->renderer);
}

const GlRenderer* renderer(const tvg::GlCanvas& canvas)
{
    return static_cast<const GlRenderer*>(canvas.pImpl->renderer);
}

}  // namespace

bool setProductAaMode(tvg::GlCanvas& canvas, ProductAaMode mode)
{
    return renderer(canvas)->aaMode(rendererMode(mode));
}

ProductAaStats productAaStats(const tvg::GlCanvas& canvas)
{
    const auto& source = renderer(canvas)->aaStats();
    return {
        productMode(source.mode),
        source.rootSamples,
        source.noAa,
        source.msaa4,
        source.flatDirect,
        source.curveDirect,
        source.flatMask,
        source.curveMask,
        source.fallback,
    };
}

void resetProductAaStats(tvg::GlCanvas& canvas)
{
    renderer(canvas)->resetAaStats();
}

}  // namespace aa_poc
