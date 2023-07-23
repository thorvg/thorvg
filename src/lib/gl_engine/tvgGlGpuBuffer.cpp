/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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

#include "tvgGlGpuBuffer.h"
#include <SDL2/SDL_opengl_glext.h>

#include <cmath>
#include <cstdint>
#include <cstring>

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

GlGpuBuffer::GlGpuBuffer()
{
    GL_CHECK(glGenBuffers(1, &mGlBufferId));
    assert(mGlBufferId != GL_INVALID_VALUE);
}


GlGpuBuffer::~GlGpuBuffer()
{
    if (mGlBufferId) {
        GL_CHECK(glDeleteBuffers(1, &mGlBufferId));
    }
}


void GlGpuBuffer::updateBufferData(Target target, uint32_t size, const void* data)
{
    GL_CHECK(glBindBuffer(static_cast<uint32_t>(target), mGlBufferId));
    GL_CHECK(glBufferData(static_cast<uint32_t>(target), size, data, GL_STATIC_DRAW));
}


void GlGpuBuffer::unbind(Target target)
{
    GL_CHECK(glBindBuffer(static_cast<uint32_t>(target), 0));
}

void GlGpuBuffer::bind(Target target)
{
    GL_CHECK(glBindBuffer(static_cast<uint32_t>(target), mGlBufferId));
}

GLStageBuffer::GLStageBuffer(GlGpuBuffer::Target target)
    : mBufferTarget(target), mOffsetAlign(1), mGpuBuffer(new GlGpuBuffer), mStageBuffer()
{
    // a little reserve buffer
    mStageBuffer.reserve(512);

    if (mBufferTarget == GlGpuBuffer::Target::UNIFORM_BUFFER) {
        GLint tmp = 0;
        glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &tmp);

        mOffsetAlign = tmp;
    }
}

GlGpuBufferView GLStageBuffer::push(void* data, uint32_t length)
{
    alignOffset();

    GlGpuBufferView result{};

    result.buffer = this->mGpuBuffer.get();
    result.offset = this->mStageBuffer.count;

    if (this->mStageBuffer.reserved - this->mStageBuffer.count < length) {
        this->mStageBuffer.grow(std::max(length, this->mStageBuffer.reserved));
    }

    std::memcpy(this->mStageBuffer.data + result.offset, data, length);

    this->mStageBuffer.count += length;

    return result;
}

void GLStageBuffer::copyToGPU()
{
    if (mStageBuffer.count == 0) {
        return;
    }

    mGpuBuffer->updateBufferData(mBufferTarget, mStageBuffer.count, mStageBuffer.data);

    mGpuBuffer->unbind(mBufferTarget);

    mStageBuffer.clear();
}

void GLStageBuffer::alignOffset()
{
    if (mBufferTarget != GlGpuBuffer::Target::UNIFORM_BUFFER) {
        return;
    }

    if (mStageBuffer.count % mOffsetAlign == 0) {
        return;
    }

    uint32_t offset = mOffsetAlign - mStageBuffer.count % mOffsetAlign;

    // check if we need to grow
    if (mStageBuffer.count + offset > mStageBuffer.reserved) {
        mStageBuffer.grow(std::max(mOffsetAlign, mStageBuffer.reserved));
    }

    mStageBuffer.count += offset;
}
