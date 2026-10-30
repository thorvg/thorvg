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

#include "tvgGlCoverageAtlas.h"
#include "tvgGlCommon.h"
#include "tvgGlStateCache.h"

#ifdef THORVG_ANALYTIC_AA_SUPPORT

struct GlCoverageAtlas::Page
{
    GLuint texture = 0;
    GlCoverageFormat format;
    uint32_t width, height;
    uint8_t* data = nullptr;
    Array<RenderRegion> free;
    RenderRegion dirty{};
    bool initialized = false;

    ~Page() { tvg::free(data); }

    bool allocate(uint32_t w, uint32_t h, GlCoverageRegion& region)
    {
        for (uint32_t i = 0; i < free.count; ++i) {
            auto rect = free[i];
            if (rect.w() < w || rect.h() < h) continue;
            free[i] = free.last();
            free.pop();
            region = {texture, {rect.xMin, rect.yMin, rect.xMin + int32_t(w), rect.yMin + int32_t(h)}};
            if (rect.w() > w) free.push({region.rect.xMax, rect.yMin, rect.xMax, region.rect.yMax});
            if (rect.h() > h) free.push({rect.xMin, region.rect.yMax, rect.xMax, rect.yMax});
            return true;
        }
        return false;
    }

    void recycle(RenderRegion rect)
    {
        for (uint32_t i = 0; i < free.count;) {
            auto& other = free[i];
            auto horizontal = rect.yMin == other.yMin && rect.yMax == other.yMax &&
                              (rect.xMax == other.xMin || other.xMax == rect.xMin);
            auto vertical = rect.xMin == other.xMin && rect.xMax == other.xMax &&
                            (rect.yMax == other.yMin || other.yMax == rect.yMin);
            if (!horizontal && !vertical) { ++i; continue; }
            rect.add(other);
            free[i] = free.last();
            free.pop();
            i = 0;
        }
        free.push(rect);
    }
};

uint32_t GlCoverageAtlas::maxTextureSize()
{
    if (!maxSize) {
        GLint size;
        GL_CHECK(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size));
        maxSize = size;
    }
    return maxSize;
}

GlCoverageAtlas::Page* GlCoverageAtlas::find(GLuint texture)
{
    ARRAY_FOREACH(page, pages) {
        if ((*page)->texture == texture) return *page;
    }
    return nullptr;
}

bool GlCoverageAtlas::update(const GlCoverageData& packed, GlCoverageRegion& region)
{
    auto page = find(region.texture);
    if (page && (page->format != packed.format || region.rect.w() != packed.width || region.rect.h() != packed.height)) {
        retire(region);
        page = nullptr;
    }
    if (!page) {
        ARRAY_FOREACH(candidate, pages) {
            if ((*candidate)->format == packed.format && (*candidate)->allocate(packed.width, packed.height, region)) {
                page = *candidate;
                break;
            }
        }
    }
    auto bytes = packed.format == GlCoverageFormat::Runs ? 8u : 1u;
    if (!page) {
        page = new Page;
        page->format = packed.format;
        // Start at 64 KiB per page; long run rows use wide, shallow pages.
        page->width = std::max(packed.width, std::min(maxTextureSize(), bytes == 8 ? 2048u : 256u));
        page->height = std::max(packed.height, std::min(maxSize, 65536u / (page->width * bytes)));
        page->data = tvg::calloc<uint8_t>(size_t(page->width) * page->height, bytes);
        GL_CHECK(glGenTextures(1, &page->texture));
        if (!page->data || !page->texture) {
            if (page->texture) glDeleteTextures(1, &page->texture);
            delete page;
            return false;
        }
        page->free.push({0, 0, int32_t(page->width), int32_t(page->height)});
        page->allocate(packed.width, packed.height, region);
        pages.push(page);
    }

    auto src = static_cast<const uint8_t*>(packed.data);
    auto dst = page->data + (size_t(region.rect.yMin) * page->width + region.rect.xMin) * bytes;
    for (uint32_t y = 0; y < packed.height; ++y) {
        memcpy(dst, src, size_t(packed.width) * bytes);
        src += size_t(packed.width) * bytes;
        dst += size_t(page->width) * bytes;
    }
    if (page->dirty.valid()) page->dirty.add(region.rect);
    else page->dirty = region.rect;
    return true;
}

void GlCoverageAtlas::flush(GlStateCache& state)
{
    auto pending = false;
    ARRAY_FOREACH(page, pages) pending |= (*page)->dirty.valid();
    if (!pending) return;

    // Preserve the host unpack state once for the whole upload batch.
    const GLenum parameters[] = {GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_PIXELS, GL_UNPACK_SKIP_ROWS};
    GLint unpack[4], buffer;
    for (uint32_t i = 0; i < 4; ++i) {
        glGetIntegerv(parameters[i], &unpack[i]);
        glPixelStorei(parameters[i], i == 0 ? 1 : 0);
    }
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#ifdef THORVG_GL_TARGET_GL
    GLint swapBytes;
    glGetIntegerv(GL_UNPACK_SWAP_BYTES, &swapBytes);
    glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
#endif

    ARRAY_FOREACH(entry, pages) {
        auto page = *entry;
        if (!page->dirty.valid()) continue;
        auto runs = page->format == GlCoverageFormat::Runs;
        auto format = runs ? GL_RGBA_INTEGER : GL_RED;
        auto type = runs ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;
        state.bindTexture2D(TVG_GL_TEXTURE_SETUP_UNIT, page->texture);
        if (!page->initialized) {
            GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, runs ? GL_RGBA16UI : GL_R8, page->width, page->height, 0, format, type, nullptr));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
            page->initialized = true;
        }
        auto& rect = page->dirty;
        auto data = page->data + (size_t(rect.yMin) * page->width + rect.xMin) * (runs ? 8 : 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, page->width);
        GL_CHECK(glTexSubImage2D(GL_TEXTURE_2D, 0, rect.xMin, rect.yMin, rect.w(), rect.h(), format, type, data));
        page->dirty = {};
    }

    for (uint32_t i = 0; i < 4; ++i) glPixelStorei(parameters[i], unpack[i]);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
#ifdef THORVG_GL_TARGET_GL
    glPixelStorei(GL_UNPACK_SWAP_BYTES, swapBytes);
#endif
}

void GlCoverageAtlas::retire(GlCoverageRegion& region)
{
    if (region.texture) retired.push(region);
    region = {};
}

void GlCoverageAtlas::reclaim()
{
    ARRAY_FOREACH(region, retired) find(region->texture)->recycle(region->rect);
    retired.clear();
}

void GlCoverageAtlas::clear(GlStateCache& state)
{
    ARRAY_FOREACH(page, pages) {
        GL_CHECK(glDeleteTextures(1, &(*page)->texture));
        delete *page;
    }
    pages.clear();
    retired.clear();
    maxSize = 0;
    state.invalidate();
}

#endif //THORVG_ANALYTIC_AA_SUPPORT
