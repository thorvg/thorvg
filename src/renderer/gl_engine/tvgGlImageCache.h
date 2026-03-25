/*
 * Copyright (c) 2020 - 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_GL_IMAGE_CACHE_H_
#define _TVG_GL_IMAGE_CACHE_H_

#include "tvgGlCommon.h"
#include "tvgInlist.h"

struct GlImageCache
{
    GLuint retain(const RenderSurface* surface, FilterMethod filter);
    GLuint release(const RenderSurface* surface, FilterMethod filter, GLuint texId);
    bool contains(const RenderSurface* surface, FilterMethod filter, GLuint texId) const;
    void drain(Array<GLuint>& textures);

    struct Entry
    {
        GLuint texId = 0;
        uint32_t refCount = 0;
    };

    struct SurfaceEntry
    {
        const RenderSurface* surface = nullptr;
        Entry bilinear;
        Entry nearest;
        SurfaceEntry* prev = nullptr;
        SurfaceEntry* next = nullptr;
    };

    static void upload(GLuint texId, const RenderSurface* surface, FilterMethod filter);
    
    tvg::Inlist<SurfaceEntry> entries;
    uint32_t surfaceCount = 0;
};

#endif /* _TVG_GL_IMAGE_CACHE_H_ */
