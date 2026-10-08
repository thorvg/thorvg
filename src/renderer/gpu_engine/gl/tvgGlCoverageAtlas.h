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

#ifndef _TVG_GL_COVERAGE_ATLAS_H_
#define _TVG_GL_COVERAGE_ATLAS_H_

#include "tvgGl.h"
#include "tvgGlCoverage.h"

#ifdef THORVG_ANALYTIC_AA_SUPPORT

struct GlStateCache;

// Non-owning location; the atlas owns the texture.
struct GlCoverageRegion
{
    GLuint texture = 0;
    tvg::RenderRegion rect{};
};

// Coverage maps share pages of the same format; image textures use TextureMgr.
struct GlCoverageAtlas
{
    uint32_t maxTextureSize();
    // Copies packed bytes into CPU page storage; flush() uploads them to GL.
    bool update(const GlCoverageData& data, GlCoverageRegion& region);
    void flush(GlStateCache& state);
    void retire(GlCoverageRegion& region);  // Invalidates the location; reuse waits for reclaim().
    void reclaim();  // Only after queued draws have been submitted or discarded.
    void clear(GlStateCache& state);

private:
    struct Page;
    Page* find(GLuint texture);
    tvg::Array<Page*> pages;
    tvg::Array<GlCoverageRegion> retired;
    uint32_t maxSize = 0;
};

#endif //THORVG_ANALYTIC_AA_SUPPORT
#endif //_TVG_GL_COVERAGE_ATLAS_H_
