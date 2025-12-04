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

#include "tvgGlTextureMgr.h"

static inline uint32_t _premultiply(uint32_t c)
{
    auto a = (c >> 24);
    return (c & 0xff000000) + ((((c >> 8) & 0xff) * a) & 0xff00) + ((((c & 0x00ff00ff) * a) >> 8) & 0x00ff00ff);
}

static inline bool _needsPremultiply(const RenderSurface* surface)
{
    return !surface->premultiplied && surface->channelSize == sizeof(uint32_t);
}

TextureMgr::SurfaceEntry* TextureMgr::find(const RenderSurface* surface)
{
    INLIST_FOREACH(surfaces, entry)
    {
        if (entry->surface == surface) return entry;
    }
    return nullptr;
}

void TextureMgr::upload(GLuint texId, const RenderSurface* surface, FilterMethod filter)
{
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texId));
    if (_needsPremultiply(surface)) {
        auto buffer = (uint32_t*)malloc(surface->w * surface->h * sizeof(uint32_t));
        auto h = static_cast<int32_t>(surface->h);
        #pragma omp parallel for
        for (int32_t y = 0; y < h; ++y) {
            auto src = surface->buf32 + surface->stride * y;
            auto dst = buffer + surface->w * y;
            for (uint32_t x = 0; x < surface->w; ++x) {
                auto c = src[x];
                if ((c >> 24) < 255) dst[x] = _premultiply(c);
                else dst[x] = c;
            }
        }
        GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer));
        free(buffer);
    } else {
        GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->data));
    }
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (filter == FilterMethod::Bilinear) ? GL_LINEAR : GL_NEAREST));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (filter == FilterMethod::Bilinear) ? GL_LINEAR : GL_NEAREST));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
}

GLuint TextureMgr::retain(const RenderSurface* surface, FilterMethod filter)
{
    auto* surfaceEntry = find(surface);
    if (!surfaceEntry) {
        surfaceEntry = new SurfaceEntry;
        surfaceEntry->surface = surface;
        surfaces.back(surfaceEntry);
    }
    auto& entry = (filter == FilterMethod::Bilinear) ? surfaceEntry->bilinear : surfaceEntry->nearest;

    if (entry.texId) {
        ++entry.refCnt;
        return entry.texId;
    }

    GLuint texId = 0;
    GL_CHECK(glGenTextures(1, &texId));
    upload(texId, surface, filter);

    entry.texId = texId;
    entry.refCnt = 1;
    return texId;
}

GLuint TextureMgr::release(const RenderSurface* surface, FilterMethod filter, GLuint texId)
{
    auto* surfaceEntry = find(surface);
    if (!surfaceEntry) return 0;
    auto& entry = (filter == FilterMethod::Bilinear) ? surfaceEntry->bilinear : surfaceEntry->nearest;
    if (entry.texId != texId) return 0;

    if (entry.refCnt > 0) --entry.refCnt;
    if (entry.refCnt > 0) return 0;

    texId = entry.texId;
    entry.texId = 0;
    entry.refCnt = 0;

    if (!surfaceEntry->bilinear.texId && !surfaceEntry->nearest.texId) {
        surfaces.remove(surfaceEntry);
        delete (surfaceEntry);
    }

    return texId;
}

void TextureMgr::clear()
{
    Array<GLuint> textures;
    textures.reserve(textures.count + surfaces.count * 2);
    INLIST_FOREACH(surfaces, entry)
    {
        if (entry->bilinear.texId) textures.push(entry->bilinear.texId);
        if (entry->nearest.texId) textures.push(entry->nearest.texId);
    }
    surfaces.free();
    if (++stamp == 0) stamp = 1;  // avoid zero stamp, which is used to indicate stale cache.
    if (!textures.empty()) GL_CHECK(glDeleteTextures(textures.count, textures.data));
}
