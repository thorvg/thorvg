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

#include "tvgGlImageCache.h"

static GlImageCache::SurfaceEntry* findEntry(tvg::Inlist<GlImageCache::SurfaceEntry>& entries, const RenderSurface* surface)
{
    INLIST_FOREACH(entries, entry) {
        if (entry->surface == surface) return entry;
    }
    return nullptr;
}

static const GlImageCache::SurfaceEntry* findEntry(const tvg::Inlist<GlImageCache::SurfaceEntry>& entries, const RenderSurface* surface)
{
    INLIST_FOREACH(entries, entry) {
        if (entry->surface == surface) return entry;
    }
    return nullptr;
}

void GlImageCache::upload(GLuint texId, const RenderSurface* surface, FilterMethod filter)
{
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texId));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->data));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (filter == FilterMethod::Bilinear) ? GL_LINEAR : GL_NEAREST));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (filter == FilterMethod::Bilinear) ? GL_LINEAR : GL_NEAREST));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
}

GLuint GlImageCache::retain(const RenderSurface* surface, FilterMethod filter)
{
    auto* cache = findEntry(entries, surface);
    if (!cache) {
        cache = new SurfaceEntry;
        cache->surface = surface;
        entries.back(cache);
        ++surfaceCount;
    }
    auto& entry = (filter == FilterMethod::Bilinear) ? cache->bilinear : cache->nearest;

    if (entry.texId) {
        ++entry.refCount;
        return entry.texId;
    }

    GLuint texId = 0;
    GL_CHECK(glGenTextures(1, &texId));
    upload(texId, surface, filter);

    entry.texId = texId;
    entry.refCount = 1;
    return texId;
}

GLuint GlImageCache::release(const RenderSurface* surface, FilterMethod filter, GLuint texId)
{
    auto* cache = findEntry(entries, surface);
    if (!cache) return 0;
    auto& entry = (filter == FilterMethod::Bilinear) ? cache->bilinear : cache->nearest;
    if (entry.texId != texId) return 0;

    if (entry.refCount > 0) --entry.refCount;
    if (entry.refCount > 0) return 0;

    auto released = entry.texId;
    entry = {};

    if (!cache->bilinear.texId && !cache->nearest.texId) {
        entries.remove(cache);
        delete(cache);
        --surfaceCount;
    }

    return released;
}

bool GlImageCache::contains(const RenderSurface* surface, FilterMethod filter, GLuint texId) const
{
    auto* cache = findEntry(entries, surface);
    if (!cache) return false;
    const auto& entry = (filter == FilterMethod::Bilinear) ? cache->bilinear : cache->nearest;
    return entry.texId == texId;
}

void GlImageCache::drain(Array<GLuint>& textures)
{
    textures.reserve(textures.count + surfaceCount * 2);
    INLIST_FOREACH(entries, entry) {
        if (entry->bilinear.texId) textures.push(entry->bilinear.texId);
        if (entry->nearest.texId) textures.push(entry->nearest.texId);
    }
    entries.free();
    surfaceCount = 0;
}
