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
#include "tvgGlProgram.h"

static constexpr int32_t PREP_PREMULTIPLY = 1 << 0;
static constexpr int32_t PREP_BGR = 1 << 1;

static constexpr char TEX_PREPROCESS_VERT_SHADER[] = R"(void main(){vec2 p=vec2(gl_VertexID==2?3.0:-1.0,gl_VertexID==1?3.0:-1.0);gl_Position=vec4(p,0.0,1.0);})";

static constexpr char TEX_PREPROCESS_FRAG_SHADER[] = R"(uniform sampler2D uTexture;uniform int uFlags;out vec4 FragColor;void main(){vec4 c=texelFetch(uTexture,ivec2(gl_FragCoord.xy),0);if((uFlags&2)!=0)c=c.bgra;if((uFlags&1)!=0)c.rgb*=c.a;FragColor=c;})";

void TextureMgr::requestPreprocess(GLuint texId, uint32_t width, uint32_t height, int32_t flags)
{
    if (!texId) return;
    for (uint32_t i = 0; i < prepRequests.count; ++i) {
        if (prepRequests[i].texId != texId) continue;
        if (flags && width && height) prepRequests[i] = {texId, width, height, flags};
        else {
            prepRequests[i] = prepRequests.last();
            prepRequests.pop();
        }
        return;
    }
    if (flags && width && height) prepRequests.push(Request{texId, width, height, flags});
}

bool TextureMgr::flushPreprocess(GlStateCache& state, GLint restoreFbo)
{
    if (prepRequests.empty()) return true;

    if (!prepProgram) {
        prepProgram = new GlProgram(TEX_PREPROCESS_VERT_SHADER, TEX_PREPROCESS_FRAG_SHADER);
        GL_CHECK(prepFlagsLoc = glGetUniformLocation(prepProgram->getProgramId(), "uFlags"));
    }
    if (!prepStagingFbo) {
        GL_CHECK(glGenFramebuffers(1, &prepStagingFbo));
    }
    if (!prepTargetFbo) {
        GL_CHECK(glGenFramebuffers(1, &prepTargetFbo));
    }
    if (!prepVao) {
        GL_CHECK(glGenVertexArrays(1, &prepVao));
    }
    if (!prepStagingTex) {
        GL_CHECK(glGenTextures(1, &prepStagingTex));
    }
    if (!prepProgram || !prepStagingFbo || !prepTargetFbo || !prepVao || !prepStagingTex) return false;

    state.bindFramebuffer(GL_FRAMEBUFFER, prepStagingFbo);
    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state.disable(GL_BLEND);
    state.disable(GL_CULL_FACE);
    state.disable(GL_DITHER);
    state.disable(GL_RASTERIZER_DISCARD);
    state.disable(GL_SCISSOR_TEST);
#if defined(THORVG_GL_TARGET_GL)
    state.disable(GL_COLOR_LOGIC_OP);
#endif

    state.useProgram(prepProgram->getProgramId());
    prepProgram->setSampler(GlShaderUniform::Texture, 0);
    state.bindVertexArray(prepVao);

    int32_t currentFlags = 0;
    ARRAY_FOREACH(p, prepRequests)
    {
        auto& request = *p;
        if (request.width > prepStagingWidth || request.height > prepStagingHeight) {
            auto attach = (prepStagingWidth == 0);
            state.bindTexture2D(TVG_GL_TEXTURE_SETUP_UNIT, prepStagingTex);
            // This texture is only a non-layered level-0 FBO attachment and is never sampled,
            // so sampling parameters and mipmap completeness do not affect the framebuffer.
            GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, request.width, request.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
            prepStagingWidth = request.width;
            prepStagingHeight = request.height;
            if (attach) {
                GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, prepStagingTex, 0));
            }
        }
        state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, prepStagingFbo);
        state.viewport(0, 0, request.width, request.height);
        state.bindTexture2D(TVG_GL_TEXTURE_SETUP_UNIT, request.texId);
        if (currentFlags != request.flags) {
            currentFlags = request.flags;
            GL_CHECK(glUniform1iv(prepFlagsLoc, 1, &currentFlags));
        }
        GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, 3));

        state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, prepTargetFbo);
        GL_CHECK(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, request.texId, 0));
        GL_CHECK(glBlitFramebuffer(0, 0, request.width, request.height, 0, 0, request.width, request.height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    }
    prepRequests.clear();

    GL_CHECK(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0));
    state.bindTexture2D(TVG_GL_TEXTURE_SETUP_UNIT, 0);
    state.bindVertexArray(0);
    state.bindFramebuffer(GL_FRAMEBUFFER, restoreFbo);
    state.enable(GL_DITHER);
    return true;
}

TextureMgr::SurfaceEntry* TextureMgr::find(const RenderSurface* surface)
{
    INLIST_FOREACH(surfaces, entry)
    {
        if (entry->surface == surface) return entry;
    }
    return nullptr;
}

void TextureMgr::upload(GlStateCache& state, GLuint texId, const RenderSurface* surface, FilterMethod filter, bool initialize)
{
    int32_t flags = 0;
    if (surface->channelSize == sizeof(uint32_t)) {
        auto supported = true;
        if (surface->cs == ColorSpace::ARGB8888 || surface->cs == ColorSpace::ARGB8888S) flags |= PREP_BGR;
        else if (surface->cs != ColorSpace::ABGR8888 && surface->cs != ColorSpace::ABGR8888S) supported = false;
        if (supported && !surface->premultiplied) flags |= PREP_PREMULTIPLY;
    }

    state.bindTexture2D(TVG_GL_TEXTURE_SETUP_UNIT, texId);
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->data));
    if (initialize) {
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (filter == FilterMethod::Bilinear) ? GL_LINEAR : GL_NEAREST));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (filter == FilterMethod::Bilinear) ? GL_LINEAR : GL_NEAREST));
    }
    state.bindTexture2D(TVG_GL_TEXTURE_SETUP_UNIT, 0);

    requestPreprocess(texId, surface->w, surface->h, flags);
}

GLuint TextureMgr::retain(GlStateCache& state, const RenderSurface* surface, FilterMethod filter)
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
    upload(state, texId, surface, filter, true);

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
    requestPreprocess(texId, 0, 0, 0);

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
    prepRequests.clear();
    delete prepProgram;
    prepProgram = nullptr;
    if (prepStagingFbo) {
        GL_CHECK(glDeleteFramebuffers(1, &prepStagingFbo));
    }
    if (prepTargetFbo) {
        GL_CHECK(glDeleteFramebuffers(1, &prepTargetFbo));
    }
    if (prepStagingTex) {
        GL_CHECK(glDeleteTextures(1, &prepStagingTex));
    }
    if (prepVao) {
        GL_CHECK(glDeleteVertexArrays(1, &prepVao));
    }
    prepStagingFbo = prepTargetFbo = prepStagingTex = prepVao = 0;
    prepStagingWidth = prepStagingHeight = 0;
    prepFlagsLoc = -1;
    if (++stamp == 0) stamp = 1;  // avoid zero stamp, which is used to indicate stale cache.
    if (!textures.empty()) {
        GL_CHECK(glDeleteTextures(textures.count, textures.data));
    }
}
