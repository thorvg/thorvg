/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_GL_STENCIL_COVER_H_
#define _TVG_GL_STENCIL_COVER_H_

#include "tvgArray.h"
#include "tvgGl.h"
#include "tvgRender.h"

struct GlShape;

struct GlStencilCoverSlot
{
    RenderRegion bounds = {};
    RenderRegion atlas = {};
    bool packed = false;

    void reset()
    {
        bounds.reset();
        atlas.reset();
        packed = false;
    }
};

struct GlStencilCover
{
    GLuint fbo = 0;
    GLuint texture = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    Array<GlStencilCoverSlot*> slots;

    void update(GlShape& sdata);
    void remove(GlStencilCoverSlot* slot);
    bool pack();
    void reset();

    GLuint getFbo() const { return fbo; }
    GLuint getTexture() const { return texture; }

    static const GlStencilCoverSlot* slot(const GlShape& sdata, RenderUpdateFlag flag);

private:
    void add(GlStencilCoverSlot* slot);
    bool ensure(uint32_t width, uint32_t height);
};

#endif /* _TVG_GL_STENCIL_COVER_H_ */
