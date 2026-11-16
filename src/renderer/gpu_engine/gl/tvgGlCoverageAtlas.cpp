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
#include "tvgGlProgram.h"

#ifdef THORVG_ANALYTIC_AA_SUPPORT

// One instance per directed edge, including its allocation's pixel bounds.
struct GlCoverageEdge
{
    Point a, b;
    float bounds[4];
};

static const char* EDGE_VERT = R"(
layout(location = 0) in vec4 aEdge;
layout(location = 1) in vec4 aBounds;
uniform vec2 uSize;
flat out vec4 vEdge;
void main()
{
    const vec2 corners[6] = vec2[6](vec2(0,0), vec2(0,1), vec2(1,0),
                                   vec2(1,0), vec2(0,1), vec2(1,1));
    vec2 lo = aBounds.xy;
    vec2 hi = aBounds.zw;
    if (aEdge.y != aEdge.w) {
        lo = max(lo, floor(min(aEdge.xy, aEdge.zw)));
        hi.y = min(hi.y, ceil(max(aEdge.y, aEdge.w)));
    }
    hi = max(lo, hi);
    vec2 p = mix(lo, hi, corners[gl_VertexID]);
    gl_Position = vec4(p * uSize - 1.0, 0.0, 1.0);
    vEdge = aEdge;
}
)";

static const char* EDGE_FRAG = R"(
flat in vec4 vEdge;
out float FragColor;
void main()
{
    // Horizontal instances clear entire allocations before additive draws.
    if (vEdge.y == vEdge.w) { FragColor = 0.0; return; }
    vec2 a = vEdge.xy, b = vEdge.zw;
    float direction = sign(b.y - a.y);
    if (a.y > b.y) { vec2 t = a; a = b; b = t; }
    vec2 pixel = floor(gl_FragCoord.xy);
    vec2 y = clamp(vec2(pixel.y, pixel.y + 1.0), a.y, b.y);
    vec2 x = a.x + (y - a.y) * ((b.x - a.x) / (b.y - a.y)) - pixel.x;
    float lo = min(x.x, x.y), hi = max(x.x, x.y);
    float area;
    if (hi <= 0.0) area = 1.0;
    else if (lo >= 1.0) area = 0.0;
    else if (hi - lo < 0.00001) area = clamp(1.0 - (lo + hi) * 0.5, 0.0, 1.0);
    else {
        // Integrate clamp(1-x,0,1) without subtracting large squared values.
        vec2 c = clamp(vec2(lo, hi), 0.0, 1.0);
        area = (max(-lo, 0.0) + (c.y - c.x) * (1.0 - (c.x + c.y) * 0.5)) / (hi - lo);
    }
    FragColor = direction * (y.y - y.x) * area;
}
)";

struct GlCoverageAtlas::Page
{
    GLuint texture = 0;
    GlCoverageFormat format;
    uint32_t width, height;
    uint8_t* data = nullptr;
    Array<RenderRegion> free;
    RenderRegion dirty{};
    bool initialized = false;
    Array<GlCoverageEdge> edges;
    Array<RenderRegion> clears;

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

bool GlCoverageAtlas::gpuSupported()
{
    if (gpuSupport < 0) {
#ifdef __EMSCRIPTEN__
        auto context = emscripten_webgl_get_current_context();
        gpuSupport = emscripten_webgl_enable_extension(context, "EXT_color_buffer_float") &&
                     emscripten_webgl_enable_extension(context, "EXT_float_blend");
#elif defined(THORVG_GL_TARGET_GL)
        gpuSupport = 1;  // Floating-point rendering and blending are core in GL 3.3.
#else
        gpuSupport = 0;  // Native GLES capability discovery is outside this prototype.
#endif
    }
    return gpuSupport != 0;
}

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
    for (auto page : pages) {
        if (page->texture == texture) return page;
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
        for (auto candidate : pages) {
            if (candidate->format == packed.format && candidate->allocate(packed.width, packed.height, region)) {
                page = candidate;
                break;
            }
        }
    }
    auto gpu = packed.format == GlCoverageFormat::Edges;
    auto bytes = packed.format == GlCoverageFormat::Runs ? 8u : 1u;
    if (!page) {
        page = new Page;
        page->format = packed.format;
        // Start at 64 KiB per page; long run rows use wide, shallow pages.
        page->width = std::max(packed.width, std::min(maxTextureSize(), gpu ? 1024u : bytes == 8 ? 2048u : 256u));
        page->height = std::max(packed.height, std::min(maxSize, gpu ? 1024u : 65536u / (page->width * bytes)));
        if (!gpu) page->data = tvg::calloc<uint8_t>(size_t(page->width) * page->height, bytes);
        GL_CHECK(glGenTextures(1, &page->texture));
        if ((!gpu && !page->data) || !page->texture) {
            if (page->texture) glDeleteTextures(1, &page->texture);
            delete page;
            return false;
        }
        page->free.push({0, 0, int32_t(page->width), int32_t(page->height)});
        page->allocate(packed.width, packed.height, region);
        pages.push(page);
    }

    if (gpu) return true;
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

bool GlCoverageAtlas::updateEdges(const Array<Point>& edges, const RenderRegion& bounds, GlCoverageRegion& region)
{
    if (bounds.w() > maxTextureSize() || bounds.h() > maxSize) return false;
    // A second update before flush must not sum old and new versions together.
    if (auto page = find(region.texture)) {
        for (const auto& rect : page->clears) {
            if (rect == region.rect) { retire(region); break; }
        }
    }
    GlCoverageData packed;
    packed.format = GlCoverageFormat::Edges;
    packed.width = bounds.w();
    packed.height = bounds.h();
    if (!update(packed, region)) return false;
    auto page = find(region.texture);
    auto rect = region.rect;
    page->clears.push(rect);
    Point offset = {float(rect.xMin - bounds.xMin), float(rect.yMin - bounds.yMin)};
    page->edges.grow(edges.count / 2);
    for (uint32_t i = 0; i < edges.count; i += 2) {
        page->edges.push({edges[i] + offset, edges[i + 1] + offset,
                          {float(rect.xMin), float(rect.yMin), float(rect.xMax), float(rect.yMax)}});
    }
    return true;
}

void GlCoverageAtlas::flushEdges(GlStateCache& state)
{
    auto pending = false;
    for (auto page : pages) pending |= !page->clears.empty();
    if (!pending) return;
    if (!edgeProgram) {
        edgeProgram = new GlProgram(EDGE_VERT, EDGE_FRAG);
        glGenFramebuffers(1, &edgeFramebuffer);
        glGenBuffers(1, &edgeBuffer);
        glGenVertexArrays(1, &edgeVao);
    }
    GLint readFbo, drawFbo, unpackBuffer;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, edgeFramebuffer);
    state.bindVertexArray(edgeVao);
    state.useProgram(edgeProgram->getProgramId());
    state.disable(GL_DEPTH_TEST);
    state.disable(GL_STENCIL_TEST);
    state.disable(GL_SCISSOR_TEST);
    state.disable(GL_CULL_FACE);
    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBlendEquation(GL_FUNC_ADD);
    state.blendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
    Array<GlCoverageEdge> instances;
    for (auto page : pages) {
        if (page->clears.empty()) continue;
        state.bindTexture2D(TVG_GL_TEXTURE_SETUP_UNIT, page->texture);
        if (!page->initialized) {
            // Half-float summation loses subpixel residuals under winding cancellation.
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, page->width, page->height, 0, GL_RED, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            page->initialized = true;
        }
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, page->texture, 0);
        state.viewport(0, 0, page->width, page->height);
        float size[] = {2.0f / page->width, 2.0f / page->height};
        glUniform2fv(glGetUniformLocation(edgeProgram->getProgramId(), "uSize"), 1, size);
        instances.clear();
        instances.reserve(page->clears.count + page->edges.count);
        for (const auto& rect : page->clears) instances.push({{}, {}, {float(rect.xMin), float(rect.yMin), float(rect.xMax), float(rect.yMax)}});
        instances.push(page->edges);
        state.bindBuffer(GL_ARRAY_BUFFER, edgeBuffer);
        glBufferData(GL_ARRAY_BUFFER, instances.count * sizeof(GlCoverageEdge), instances.data, GL_STREAM_DRAW);
        state.beginVertexLayout();
        for (uint32_t i = 0; i < 2; ++i) {
            state.setVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, sizeof(GlCoverageEdge), i * sizeof(float) * 4, edgeBuffer);
            state.setVertexAttribDivisor(i, 1);
        }
        state.endVertexLayout();
        state.disable(GL_BLEND);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, page->clears.count);
        for (uint32_t i = 0; i < 2; ++i) {
            state.setVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, sizeof(GlCoverageEdge),
                                         page->clears.count * sizeof(GlCoverageEdge) + i * sizeof(float) * 4, edgeBuffer);
        }
        state.enable(GL_BLEND);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, page->edges.count);
        page->edges.clear();
        page->clears.clear();
    }
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
}

void GlCoverageAtlas::flush(GlStateCache& state)
{
    flushEdges(state);
    auto pending = false;
    for (auto page : pages) pending |= page->dirty.valid();
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

    for (auto page : pages) {
        if (!page->dirty.valid()) continue;
        auto runs = page->format == GlCoverageFormat::Runs;
        auto format = runs ? GL_RGBA_INTEGER : GL_RED_INTEGER;
        auto type = runs ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;
        state.bindTexture2D(TVG_GL_TEXTURE_SETUP_UNIT, page->texture);
        if (!page->initialized) {
            GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, runs ? GL_RGBA16UI : GL_R8UI, page->width, page->height, 0, format, type, nullptr));
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
    for (auto& region : retired) find(region.texture)->recycle(region.rect);
    retired.clear();
}

void GlCoverageAtlas::clear(GlStateCache& state)
{
    delete edgeProgram;
    edgeProgram = nullptr;
    if (edgeFramebuffer) glDeleteFramebuffers(1, &edgeFramebuffer);
    if (edgeBuffer) glDeleteBuffers(1, &edgeBuffer);
    if (edgeVao) glDeleteVertexArrays(1, &edgeVao);
    edgeFramebuffer = edgeBuffer = edgeVao = 0;
    gpuSupport = -1;
    for (auto page : pages) {
        GL_CHECK(glDeleteTextures(1, &page->texture));
        delete page;
    }
    pages.clear();
    retired.clear();
    maxSize = 0;
    state.invalidate();
}

#endif //THORVG_ANALYTIC_AA_SUPPORT
