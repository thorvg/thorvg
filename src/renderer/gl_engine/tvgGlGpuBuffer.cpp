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

#include <math.h>
#include <string.h>

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

GlGpuBuffer::GlGpuBuffer()
{
    GL_CHECK(glGenBuffers(1, &mGlBufferId));
    assert(mGlBufferId != 0);
}


GlGpuBuffer::~GlGpuBuffer()
{
    if (mGlBufferId)
    {
        GL_CHECK(glDeleteBuffers(1, &mGlBufferId));
    }
}


void GlGpuBuffer::updateBufferData(Target target, uint32_t size, const void* data)
{
    GL_CHECK(glBufferData(static_cast<uint32_t>(target), size, data, GL_STATIC_DRAW));
}

void GlGpuBuffer::bind(Target target)
{
    GL_CHECK(glBindBuffer(static_cast<uint32_t>(target), mGlBufferId));
}

void GlGpuBuffer::unbind(Target target)
{
    GL_CHECK(glBindBuffer(static_cast<uint32_t>(target), 0));
}

GlStageBuffer::~GlStageBuffer()
{
    if (mVao) {
        glDeleteVertexArrays(1, &mVao);
        mVao = 0;
    }
}

uint32_t GlStageBuffer::push(void *data, uint32_t size)
{
    uint32_t offset = mStageBuffer.count;

    if (this->mStageBuffer.reserved - this->mStageBuffer.count < size) {
        this->mStageBuffer.grow(max(size, this->mStageBuffer.reserved));
    }

    memcpy(this->mStageBuffer.data + offset, data, size);

    this->mStageBuffer.count += size;

    return offset;
}

void GlStageBuffer::flushToGPU()
{
    if (mStageBuffer.empty()) return;

    if (!mGpuBuffer) {
        mGpuBuffer.reset(new GlGpuBuffer);
        GL_CHECK(glGenVertexArrays(1, &mVao));
    }

    mGpuBuffer->bind(GlGpuBuffer::Target::ARRAY_BUFFER);
    mGpuBuffer->updateBufferData(GlGpuBuffer::Target::ARRAY_BUFFER, mStageBuffer.count, mStageBuffer.data);
    mGpuBuffer->unbind(GlGpuBuffer::Target::ARRAY_BUFFER);
}

void GlStageBuffer::bind()
{
    glBindVertexArray(mVao);
    mGpuBuffer->bind(GlGpuBuffer::Target::ARRAY_BUFFER);
    mGpuBuffer->bind(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER);
}

void GlStageBuffer::unbind()
{
    glBindVertexArray(0);
    mGpuBuffer->unbind(GlGpuBuffer::Target::ARRAY_BUFFER);
    mGpuBuffer->unbind(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER);
}
