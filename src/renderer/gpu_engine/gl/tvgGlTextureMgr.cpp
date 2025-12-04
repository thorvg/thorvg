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
#include "tvgGlShaderSrc.h"

static constexpr auto PREMULTIPLY_FRAG_SHADER = R"(
uniform sampler2D uSrcTexture;
in vec2 vUV;
out vec4 FragColor;

void main()
{
    vec4 color = texture(uSrcTexture, vUV);
    FragColor = vec4(color.rgb * color.a, color.a);
}
)";

struct GlUploadState
{
    GLint readFbo = 0;
    GLint drawFbo = 0;
    GLint viewport[4] = {};
    GLint scissor[4] = {};
    GLint activeTexture = GL_TEXTURE0;
    GLint activeTextureBinding = 0;
    GLint texture0Binding = 0;
    GLint vao = 0;
    GLint arrayBuffer = 0;
    GLint program = 0;
    GLint blend = 0;
    GLint depth = 0;
    GLint scissorTest = 0;
    GLint stencil = 0;

    GlUploadState()
    {
        GL_CHECK(glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo));
        GL_CHECK(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo));
        GL_CHECK(glGetIntegerv(GL_VIEWPORT, viewport));
        GL_CHECK(glGetIntegerv(GL_SCISSOR_BOX, scissor));
        GL_CHECK(glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture));
        GL_CHECK(glGetIntegerv(GL_TEXTURE_BINDING_2D, &activeTextureBinding));
        texture0Binding = activeTextureBinding;
        if (activeTexture != GL_TEXTURE0) {
            GL_CHECK(glActiveTexture(GL_TEXTURE0));
            GL_CHECK(glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0Binding));
            GL_CHECK(glActiveTexture(activeTexture));
        }
        GL_CHECK(glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao));
        GL_CHECK(glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer));
        GL_CHECK(glGetIntegerv(GL_CURRENT_PROGRAM, &program));
        GL_CHECK(glGetIntegerv(GL_BLEND, &blend));
        GL_CHECK(glGetIntegerv(GL_DEPTH_TEST, &depth));
        GL_CHECK(glGetIntegerv(GL_SCISSOR_TEST, &scissorTest));
        GL_CHECK(glGetIntegerv(GL_STENCIL_TEST, &stencil));
    }

    void restore() const
    {
        GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFbo)));
        GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFbo)));
        GL_CHECK(glViewport(viewport[0], viewport[1], viewport[2], viewport[3]));
        GL_CHECK(glScissor(scissor[0], scissor[1], scissor[2], scissor[3]));
        if (blend) GL_CHECK(glEnable(GL_BLEND));
        else GL_CHECK(glDisable(GL_BLEND));
        if (depth) GL_CHECK(glEnable(GL_DEPTH_TEST));
        else GL_CHECK(glDisable(GL_DEPTH_TEST));
        if (scissorTest) GL_CHECK(glEnable(GL_SCISSOR_TEST));
        else GL_CHECK(glDisable(GL_SCISSOR_TEST));
        if (stencil) GL_CHECK(glEnable(GL_STENCIL_TEST));
        else GL_CHECK(glDisable(GL_STENCIL_TEST));
        GL_CHECK(glUseProgram(static_cast<GLuint>(program)));
        GlProgram::unload();
        GL_CHECK(glBindVertexArray(static_cast<GLuint>(vao)));
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer)));
        GL_CHECK(glActiveTexture(GL_TEXTURE0));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture0Binding)));
        if (activeTexture != GL_TEXTURE0) {
            GL_CHECK(glActiveTexture(static_cast<GLenum>(activeTexture)));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(activeTextureBinding)));
        } else {
            GL_CHECK(glActiveTexture(static_cast<GLenum>(activeTexture)));
        }
    }
};

TextureMgr::SurfaceEntry* TextureMgr::find(const RenderSurface* surface)
{
    INLIST_FOREACH(surfaces, entry)
    {
        if (entry->surface == surface) return entry;
    }
    return nullptr;
}

void TextureMgr::premultiply(GLuint dstTexId, const RenderSurface* surface)
{
    if (!preprocessProgram) preprocessProgram = new GlProgram(BLIT_VERT_SHADER, PREMULTIPLY_FRAG_SHADER);
    if (!preprocessSrcTex) GL_CHECK(glGenTextures(1, &preprocessSrcTex));
    if (!preprocessFbo) GL_CHECK(glGenFramebuffers(1, &preprocessFbo));
    if (!preprocessVao) {
        static constexpr float vertices[] = {
            -1.0f,  1.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 1.0f, 0.0f,
        };

        GL_CHECK(glGenVertexArrays(1, &preprocessVao));
        GL_CHECK(glBindVertexArray(preprocessVao));
        GL_CHECK(glGenBuffers(1, &preprocessVbo));
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, preprocessVbo));
        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW));
        GL_CHECK(glEnableVertexAttribArray(0));
        GL_CHECK(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr));
        GL_CHECK(glEnableVertexAttribArray(1));
        GL_CHECK(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float))));
    }

    GL_CHECK(glActiveTexture(GL_TEXTURE0));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, preprocessSrcTex));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->data));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, preprocessFbo));
    GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTexId, 0));
    GL_CHECK(glViewport(0, 0, surface->w, surface->h));
    GL_CHECK(glDisable(GL_SCISSOR_TEST));
    GL_CHECK(glDisable(GL_BLEND));
    GL_CHECK(glDisable(GL_DEPTH_TEST));
    GL_CHECK(glDisable(GL_STENCIL_TEST));

    preprocessProgram->load();
    int textureUnit = 0;
    preprocessProgram->setUniform1Value(preprocessProgram->getUniformLocation("uSrcTexture"), 1, &textureUnit);

    GL_CHECK(glBindVertexArray(preprocessVao));
    GL_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0));
}

void TextureMgr::upload(Entry& entry, const RenderSurface* surface, FilterMethod filter)
{
    GlUploadState state;

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, entry.texId));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (filter == FilterMethod::Bilinear) ? GL_LINEAR : GL_NEAREST));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (filter == FilterMethod::Bilinear) ? GL_LINEAR : GL_NEAREST));

    if (!surface->premultiplied && surface->channelSize == sizeof(uint32_t) &&
        (surface->cs == ColorSpace::ABGR8888S || surface->cs == ColorSpace::ARGB8888S)) {
        GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
        premultiply(entry.texId, surface);
    } else {
        GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->data));
    }

    state.restore();
}

void TextureMgr::upload(GLuint texId, const RenderSurface* surface, FilterMethod filter)
{
    if (auto* surfaceEntry = find(surface)) {
        auto& entry = (filter == FilterMethod::Bilinear) ? surfaceEntry->bilinear : surfaceEntry->nearest;
        if (entry.texId == texId) {
            upload(entry, surface, filter);
            return;
        }
    }

    Entry entry;
    entry.texId = texId;
    upload(entry, surface, filter);
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

    entry.texId = texId;
    upload(entry, surface, filter);
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
    clearPreprocess();
}

void TextureMgr::clearPreprocess()
{
    if (preprocessProgram) {
        delete preprocessProgram;
        preprocessProgram = nullptr;
    }
    if (preprocessSrcTex) {
        GL_CHECK(glDeleteTextures(1, &preprocessSrcTex));
        preprocessSrcTex = 0;
    }
    if (preprocessFbo) {
        GL_CHECK(glDeleteFramebuffers(1, &preprocessFbo));
        preprocessFbo = 0;
    }
    if (preprocessVao) {
        GL_CHECK(glDeleteVertexArrays(1, &preprocessVao));
        preprocessVao = 0;
    }
    if (preprocessVbo) {
        GL_CHECK(glDeleteBuffers(1, &preprocessVbo));
        preprocessVbo = 0;
    }
}
