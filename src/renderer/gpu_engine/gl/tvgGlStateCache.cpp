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

#include "tvgGlStateCache.h"

GlStateCache::BufferBindingState& GlStateCache::bufferBinding(GLenum target)
{
    for (auto& binding : buffers) {
        if (binding.target == target) return binding;
    }
    buffers.push({target, {}});
    return buffers.last();
}

GlStateCache::TextureBindingState& GlStateCache::textureBinding(GLenum textureUnit)
{
    for (auto& binding : textures) {
        if (binding.textureUnit == textureUnit) return binding;
    }
    textures.push({textureUnit, {}});
    return textures.last();
}

GlStateCache::CapabilityState& GlStateCache::capability(GLenum value)
{
    for (auto& state : capabilities) {
        if (state.capability == value) return state;
    }
    capabilities.push({value, {}});
    return capabilities.last();
}

void GlStateCache::setCapability(GLenum value, bool enabled)
{
    auto& state = capability(value);
    if (state.enabled.matches(enabled)) return;
    if (enabled) GL_CHECK(glEnable(value));
    else GL_CHECK(glDisable(value));
    state.enabled.set(enabled);
}

GlStateCache::VertexAttributeState& GlStateCache::vertexAttribute(GLuint index)
{
    while (vertexAttributes.count <= index)
        vertexAttributes.push(VertexAttributeState{});
    auto& attribute = vertexAttributes[index];
    attribute.known = true;
    return attribute;
}

void GlStateCache::invalidateVertexArrayState()
{
    elementArrayBuffer.invalidate();
    for (auto& attribute : vertexAttributes) {
        attribute.enabled.invalidate();
        attribute.pointer.invalidate();
        attribute.used = false;
    }
}

void GlStateCache::invalidateVertexConstants()
{
    for (auto& attribute : vertexAttributes)
        attribute.constant.invalidate();
}

void GlStateCache::disableVertexAttribute(GLuint index, VertexAttributeState& attribute)
{
    if (attribute.enabled.matches(false)) return;
    GL_CHECK(glDisableVertexAttribArray(index));
    attribute.enabled.set(false);
}

void GlStateCache::enableVertexAttribute(GLuint index, VertexAttributeState& attribute)
{
    if (attribute.enabled.matches(true)) return;
    GL_CHECK(glEnableVertexAttribArray(index));
    attribute.enabled.set(true);
}

void GlStateCache::invalidate()
{
    // This forgets CPU-side assumptions only; it deliberately issues no GL
    // reset. Applications sharing the context must restore external state
    // before GlRenderer::sync().
    program.invalidate();
    vertexArray.invalidate();
    invalidateVertexArrayState();
    invalidateVertexConstants();
    for (auto& binding : buffers)
        binding.buffer.invalidate();

    activeTextureUnit.invalidate();
    for (auto& binding : textures)
        binding.texture.invalidate();

    viewportState.invalidate();
    scissorState.invalidate();
    readFramebuffer.invalidate();
    drawFramebuffer.invalidate();

    for (auto& capability : capabilities)
        capability.enabled.invalidate();
    blendFunction.invalidate();
    blendEquationMode.invalidate();
    depthFunction.invalidate();
    depthWriteEnabled.invalidate();
    colorWriteMask.invalidate();

    frontStencilFunction.invalidate();
    backStencilFunction.invalidate();
    stencilWriteMask.invalidate();
    frontStencilOperation.invalidate();
    backStencilOperation.invalidate();

    clearColorState.invalidate();
    clearStencilValue.invalidate();
    clearDepthValue.invalidate();
}

void GlStateCache::useProgram(GLuint value)
{
    if (program.matches(value)) return;
    GL_CHECK(glUseProgram(value));
    program.set(value);
}

void GlStateCache::bindVertexArray(GLuint value)
{
    if (vertexArray.matches(value)) return;
    GL_CHECK(glBindVertexArray(value));
    vertexArray.set(value);
    invalidateVertexArrayState();
}

void GlStateCache::bindBuffer(GLenum target, GLuint buffer)
{
    if (target == GL_ELEMENT_ARRAY_BUFFER) {
        if (elementArrayBuffer.matches(buffer)) return;
        GL_CHECK(glBindBuffer(target, buffer));
        elementArrayBuffer.set(buffer);
        return;
    }

    auto& binding = bufferBinding(target);
    if (binding.buffer.matches(buffer)) return;
    GL_CHECK(glBindBuffer(target, buffer));
    binding.buffer.set(buffer);
}

void GlStateCache::bindTexture2D(GLenum textureUnit, GLuint texture)
{
    if (!activeTextureUnit.matches(textureUnit)) {
        GL_CHECK(glActiveTexture(textureUnit));
        activeTextureUnit.set(textureUnit);
    }

    auto& binding = textureBinding(textureUnit);
    if (binding.texture.matches(texture)) return;
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture));
    binding.texture.set(texture);
}

void GlStateCache::beginVertexLayout()
{
    for (auto& attribute : vertexAttributes)
        attribute.used = false;
}

void GlStateCache::setVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                          GLsizei stride, size_t offset, GLuint sourceBuffer)
{
    bindBuffer(GL_ARRAY_BUFFER, sourceBuffer);
    auto& attribute = vertexAttribute(index);
    attribute.used = true;
    // A draw with an enabled array leaves the generic attribute value undefined.
    // Replay the constant if this attribute later transitions back to constant mode.
    attribute.constant.invalidate();
    enableVertexAttribute(index, attribute);

    VertexPointerState pointer = {size, type, normalized, stride, offset, sourceBuffer};
    if (attribute.pointer.matches(pointer)) return;

    GL_CHECK(glVertexAttribPointer(index, size, type, normalized, stride, reinterpret_cast<const void*>(offset)));
    attribute.pointer.set(pointer);
}

void GlStateCache::setVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    auto& attribute = vertexAttribute(index);
    attribute.used = true;
    disableVertexAttribute(index, attribute);

    VertexConstantState constant = {x, y, z, w};
    if (attribute.constant.matches(constant)) return;

    GL_CHECK(glVertexAttrib4f(index, x, y, z, w));
    attribute.constant.set(constant);
}

void GlStateCache::endVertexLayout()
{
    for (uint32_t index = 0; index < vertexAttributes.count; ++index) {
        auto& attribute = vertexAttributes[index];
        if (!attribute.known || attribute.used) continue;
        disableVertexAttribute(index, attribute);
    }
}

void GlStateCache::viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    RectState state = {x, y, width, height};
    if (viewportState.matches(state)) return;
    GL_CHECK(glViewport(x, y, width, height));
    viewportState.set(state);
}

void GlStateCache::scissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    RectState state = {x, y, width, height};
    if (scissorState.matches(state)) return;
    GL_CHECK(glScissor(x, y, width, height));
    scissorState.set(state);
}

void GlStateCache::bindFramebuffer(GLenum target, GLuint framebuffer)
{
    if (target == GL_FRAMEBUFFER) {
        if (readFramebuffer.matches(framebuffer) && drawFramebuffer.matches(framebuffer)) return;
        GL_CHECK(glBindFramebuffer(target, framebuffer));
        readFramebuffer.set(framebuffer);
        drawFramebuffer.set(framebuffer);
        return;
    }

    if (target == GL_READ_FRAMEBUFFER) {
        if (readFramebuffer.matches(framebuffer)) return;
        GL_CHECK(glBindFramebuffer(target, framebuffer));
        readFramebuffer.set(framebuffer);
        return;
    }

    if (target == GL_DRAW_FRAMEBUFFER) {
        if (drawFramebuffer.matches(framebuffer)) return;
        GL_CHECK(glBindFramebuffer(target, framebuffer));
        drawFramebuffer.set(framebuffer);
        return;
    }

    GL_CHECK(glBindFramebuffer(target, framebuffer));
}

void GlStateCache::enable(GLenum capability)
{
    setCapability(capability, true);
}

void GlStateCache::disable(GLenum capability)
{
    setCapability(capability, false);
}

void GlStateCache::blendFunc(GLenum source, GLenum destination)
{
    BlendFuncState state = {source, destination};
    if (blendFunction.matches(state)) return;
    GL_CHECK(glBlendFunc(source, destination));
    blendFunction.set(state);
}

void GlStateCache::blendEquation(GLenum mode)
{
    if (blendEquationMode.matches(mode)) return;
    GL_CHECK(glBlendEquation(mode));
    blendEquationMode.set(mode);
}

void GlStateCache::depthFunc(GLenum function)
{
    if (depthFunction.matches(function)) return;
    GL_CHECK(glDepthFunc(function));
    depthFunction.set(function);
}

void GlStateCache::depthMask(GLboolean enabled)
{
    if (depthWriteEnabled.matches(enabled)) return;
    GL_CHECK(glDepthMask(enabled));
    depthWriteEnabled.set(enabled);
}

void GlStateCache::colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    ColorMaskState state = {red, green, blue, alpha};
    if (colorWriteMask.matches(state)) return;
    GL_CHECK(glColorMask(red, green, blue, alpha));
    colorWriteMask.set(state);
}

void GlStateCache::stencilFunc(GLenum function, GLint reference, GLuint mask)
{
    StencilFuncState state = {function, reference, mask};
    if (frontStencilFunction.matches(state) && backStencilFunction.matches(state)) return;
    GL_CHECK(glStencilFunc(function, reference, mask));
    frontStencilFunction.set(state);
    backStencilFunction.set(state);
}

void GlStateCache::stencilFuncSeparate(GLenum face, GLenum function, GLint reference, GLuint mask)
{
    StencilFuncState state = {function, reference, mask};
    if (face == GL_FRONT) {
        if (frontStencilFunction.matches(state)) return;
        GL_CHECK(glStencilFuncSeparate(face, function, reference, mask));
        frontStencilFunction.set(state);
        return;
    }

    if (face == GL_BACK) {
        if (backStencilFunction.matches(state)) return;
        GL_CHECK(glStencilFuncSeparate(face, function, reference, mask));
        backStencilFunction.set(state);
        return;
    }

    if (face == GL_FRONT_AND_BACK) {
        if (frontStencilFunction.matches(state) && backStencilFunction.matches(state)) return;
        GL_CHECK(glStencilFuncSeparate(face, function, reference, mask));
        frontStencilFunction.set(state);
        backStencilFunction.set(state);
        return;
    }

    GL_CHECK(glStencilFuncSeparate(face, function, reference, mask));
}

void GlStateCache::stencilMask(GLuint mask)
{
    if (stencilWriteMask.matches(mask)) return;
    GL_CHECK(glStencilMask(mask));
    stencilWriteMask.set(mask);
}

void GlStateCache::stencilOp(GLenum stencilFail, GLenum depthFail, GLenum depthPass)
{
    StencilOpState state = {stencilFail, depthFail, depthPass};
    if (frontStencilOperation.matches(state) && backStencilOperation.matches(state)) return;
    GL_CHECK(glStencilOp(stencilFail, depthFail, depthPass));
    frontStencilOperation.set(state);
    backStencilOperation.set(state);
}

void GlStateCache::stencilOpSeparate(GLenum face, GLenum stencilFail, GLenum depthFail, GLenum depthPass)
{
    StencilOpState state = {stencilFail, depthFail, depthPass};
    if (face == GL_FRONT) {
        if (frontStencilOperation.matches(state)) return;
        GL_CHECK(glStencilOpSeparate(face, stencilFail, depthFail, depthPass));
        frontStencilOperation.set(state);
        return;
    }

    if (face == GL_BACK) {
        if (backStencilOperation.matches(state)) return;
        GL_CHECK(glStencilOpSeparate(face, stencilFail, depthFail, depthPass));
        backStencilOperation.set(state);
        return;
    }

    if (face == GL_FRONT_AND_BACK) {
        if (frontStencilOperation.matches(state) && backStencilOperation.matches(state)) return;
        GL_CHECK(glStencilOpSeparate(face, stencilFail, depthFail, depthPass));
        frontStencilOperation.set(state);
        backStencilOperation.set(state);
        return;
    }

    GL_CHECK(glStencilOpSeparate(face, stencilFail, depthFail, depthPass));
}

void GlStateCache::clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    ClearColorState state = {red, green, blue, alpha};
    if (clearColorState.matches(state)) return;
    GL_CHECK(glClearColor(red, green, blue, alpha));
    clearColorState.set(state);
}

void GlStateCache::clearStencil(GLint value)
{
    if (clearStencilValue.matches(value)) return;
    GL_CHECK(glClearStencil(value));
    clearStencilValue.set(value);
}

void GlStateCache::clearDepth(double value)
{
    if (clearDepthValue.matches(value)) return;
#if defined(__EMSCRIPTEN__)
    GL_CHECK(glClearDepthf(static_cast<GLfloat>(value)));
#elif defined(THORVG_GL_TARGET_GLES)
    GL_CHECK(glClearDepthf(static_cast<GLfloat>(value)));
#else
    GL_CHECK(glClearDepth(value));
#endif
    clearDepthValue.set(value);
}
