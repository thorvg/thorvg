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

#ifndef _TVG_GL_STATE_CACHE_H_
#define _TVG_GL_STATE_CACHE_H_

#include <cstddef>
#include "tvgArray.h"
#include "tvgGl.h"

struct GlStateCache
{
    GlStateCache() = default;

    GlStateCache(const GlStateCache&) = delete;
    GlStateCache& operator=(const GlStateCache&) = delete;

    void invalidate();

    void useProgram(GLuint program);
    void bindVertexArray(GLuint vertexArray);
    void bindBuffer(GLenum target, GLuint buffer);

    void bindTexture2D(GLenum textureUnit, GLuint texture);

    void beginVertexLayout();
    void setVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                GLsizei stride, size_t offset, GLuint sourceBuffer);
    void setVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
    void endVertexLayout();

    void viewport(GLint x, GLint y, GLsizei width, GLsizei height);
    void scissor(GLint x, GLint y, GLsizei width, GLsizei height);
    void bindFramebuffer(GLenum target, GLuint framebuffer);

    void enable(GLenum capability);
    void disable(GLenum capability);

    void blendFunc(GLenum source, GLenum destination);
    void depthFunc(GLenum function);
    void depthMask(GLboolean enabled);
    void colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);

    void stencilFunc(GLenum function, GLint reference, GLuint mask);
    void stencilFuncSeparate(GLenum face, GLenum function, GLint reference, GLuint mask);
    void stencilOp(GLenum stencilFail, GLenum depthFail, GLenum depthPass);
    void stencilOpSeparate(GLenum face, GLenum stencilFail, GLenum depthFail, GLenum depthPass);

    void clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void clearStencil(GLint value);
    void clearDepth(double value);

    template<typename T>
    struct CachedValue
    {
        T value = {};
        bool valid = false;

        bool matches(const T& candidate) const
        {
            return valid && value == candidate;
        }

        void set(const T& candidate)
        {
            value = candidate;
            valid = true;
        }

        void invalidate()
        {
            valid = false;
        }
    };

    // Arguments shared by glViewport() and glScissor().
    struct RectState
    {
        GLint x = 0;
        GLint y = 0;
        GLsizei width = 0;
        GLsizei height = 0;

        bool operator==(const RectState& rhs) const
        {
            return x == rhs.x && y == rhs.y && width == rhs.width && height == rhs.height;
        }
    };

    // Arguments passed to glBlendFunc().
    struct BlendFuncState
    {
        GLenum source = 0;
        GLenum destination = 0;

        bool operator==(const BlendFuncState& rhs) const
        {
            return source == rhs.source && destination == rhs.destination;
        }
    };

    // Arguments passed to glColorMask().
    struct ColorMaskState
    {
        GLboolean red = GL_FALSE;
        GLboolean green = GL_FALSE;
        GLboolean blue = GL_FALSE;
        GLboolean alpha = GL_FALSE;

        bool operator==(const ColorMaskState& rhs) const
        {
            return red == rhs.red && green == rhs.green && blue == rhs.blue && alpha == rhs.alpha;
        }
    };

    // Arguments passed to glStencilFunc() or glStencilFuncSeparate().
    struct StencilFuncState
    {
        GLenum function = 0;
        GLint reference = 0;
        GLuint mask = 0;

        bool operator==(const StencilFuncState& rhs) const
        {
            return function == rhs.function && reference == rhs.reference && mask == rhs.mask;
        }
    };

    // Arguments passed to glStencilOp() or glStencilOpSeparate().
    struct StencilOpState
    {
        GLenum stencilFail = 0;
        GLenum depthFail = 0;
        GLenum depthPass = 0;

        bool operator==(const StencilOpState& rhs) const
        {
            return stencilFail == rhs.stencilFail && depthFail == rhs.depthFail && depthPass == rhs.depthPass;
        }
    };

    // Arguments passed to glClearColor().
    struct ClearColorState
    {
        GLfloat red = 0.0f;
        GLfloat green = 0.0f;
        GLfloat blue = 0.0f;
        GLfloat alpha = 0.0f;

        bool operator==(const ClearColorState& rhs) const
        {
            return red == rhs.red && green == rhs.green && blue == rhs.blue && alpha == rhs.alpha;
        }
    };

    // Arguments passed to glVertexAttribPointer(), including its source buffer.
    struct VertexPointerState
    {
        GLint size = 0;
        GLenum type = 0;
        GLboolean normalized = GL_FALSE;
        GLsizei stride = 0;
        size_t offset = 0;
        GLuint sourceBuffer = 0;

        bool operator==(const VertexPointerState& rhs) const
        {
            return size == rhs.size && type == rhs.type && normalized == rhs.normalized && stride == rhs.stride && offset == rhs.offset && sourceBuffer == rhs.sourceBuffer;
        }
    };

    // Arguments passed to glVertexAttrib4f().
    struct VertexConstantState
    {
        GLfloat x = 0.0f;
        GLfloat y = 0.0f;
        GLfloat z = 0.0f;
        GLfloat w = 0.0f;

        bool operator==(const VertexConstantState& rhs) const
        {
            return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
        }
    };

    // Buffer bound to a target by glBindBuffer().
    struct BufferBindingState
    {
        GLenum target = 0;           // Target passed to glBindBuffer()
        CachedValue<GLuint> buffer;  // Buffer passed to glBindBuffer()
    };

    // GL_TEXTURE_2D binding set by glBindTexture() for one texture unit.
    struct TextureBindingState
    {
        GLenum textureUnit = GL_TEXTURE0;  // Unit selected by glActiveTexture()
        CachedValue<GLuint> texture;       // Texture passed to glBindTexture()
    };

    // Capability state set by glEnable() or glDisable().
    struct CapabilityState
    {
        GLenum capability = 0;      // Capability passed to glEnable()/glDisable()
        CachedValue<bool> enabled;  // Result of glEnable()/glDisable()
    };

    // Per-index state set by glEnableVertexAttribArray(), glDisableVertexAttribArray(),
    // glVertexAttribPointer(), and glVertexAttrib4f().
    struct VertexAttributeState
    {
        CachedValue<bool> enabled;                  // glEnableVertexAttribArray(), glDisableVertexAttribArray()
        CachedValue<VertexPointerState> pointer;    // glVertexAttribPointer()
        CachedValue<VertexConstantState> constant;  // glVertexAttrib4f()
        bool known = false;                         // Index has cache-managed OpenGL state
        bool used = false;                          // Index is used by the current vertex layout
    };

    CachedValue<GLuint> program;
    CachedValue<GLuint> vertexArray;
    CachedValue<GLuint> elementArrayBuffer;
    Array<BufferBindingState> buffers;

    CachedValue<GLenum> activeTextureUnit;
    Array<TextureBindingState> textures;
    Array<VertexAttributeState> vertexAttributes;

    CachedValue<RectState> viewportState;
    CachedValue<RectState> scissorState;
    CachedValue<GLuint> readFramebuffer;
    CachedValue<GLuint> drawFramebuffer;

    Array<CapabilityState> capabilities;
    CachedValue<BlendFuncState> blendFunction;
    CachedValue<GLenum> depthFunction;
    CachedValue<GLboolean> depthWriteEnabled;
    CachedValue<ColorMaskState> colorWriteMask;

    CachedValue<StencilFuncState> frontStencilFunction;
    CachedValue<StencilFuncState> backStencilFunction;
    CachedValue<StencilOpState> frontStencilOperation;
    CachedValue<StencilOpState> backStencilOperation;

    CachedValue<ClearColorState> clearColorState;
    CachedValue<GLint> clearStencilValue;
    CachedValue<double> clearDepthValue;

    BufferBindingState& bufferBinding(GLenum target);
    TextureBindingState& textureBinding(GLenum textureUnit);
    CapabilityState& capability(GLenum value);
    VertexAttributeState& vertexAttribute(GLuint index);

    void setCapability(GLenum value, bool enabled);
    void invalidateVertexArrayState();
    void invalidateVertexConstants();
    void disableVertexAttribute(GLuint index, VertexAttributeState& attribute);
    void enableVertexAttribute(GLuint index, VertexAttributeState& attribute);
};

#endif /* _TVG_GL_STATE_CACHE_H_ */
