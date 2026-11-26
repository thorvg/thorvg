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

#include "tvgGlGpuBuffer.h"

/************************************************************************/
/* GlGpuBuffer Implementation                                           */
/************************************************************************/

static GLint _getGpuBufferAlign() 
{
    static GLint offset = 0;
    if (!offset) {
        GL_CHECK(glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &offset));
    }
    return offset;
}

void GlGpuBuffer::updateBufferData(Target target, uint32_t size, const void* data)
{
    GL_CHECK(glBufferData(static_cast<uint32_t>(target), size, data, GL_STATIC_DRAW));
}

GlGpuBuffer::GlGpuBuffer()
{
    GL_CHECK(glGenBuffers(1, &mGlBufferId));
    assert(mGlBufferId != 0);
}

GlGpuBuffer::~GlGpuBuffer()
{
    GL_CHECK(glDeleteBuffers(1, &mGlBufferId));
}

/************************************************************************/
/* GlStageBuffer Implementation                                         */
/************************************************************************/

GlStageBuffer::GlStageBuffer()
{
    memset(mImageOpacityOffsets, 0xff, sizeof(mImageOpacityOffsets));
    GL_CHECK(glGenVertexArrays(1, &mVao));
}

GlStageBuffer::~GlStageBuffer()
{
    glDeleteVertexArrays(1, &mVao);
}

uint32_t GlStageBuffer::reserve(Array<uint8_t>& buffer, uint32_t size, void** dst)
{
    auto offset = buffer.count;
    if (buffer.reserved - offset < size) buffer.grow(max(size, buffer.reserved));
    *dst = buffer.data + offset;
    buffer.count += size;
    return offset;
}

uint32_t GlStageBuffer::push(Array<uint8_t>& buffer, const void* data, uint32_t size)
{
    void* dst = nullptr;
    auto offset = reserve(buffer, size, &dst);
    if (size > 0) memcpy(dst, data, size);
    return offset;
}

uint32_t GlStageBuffer::push(void* data, uint32_t size, bool alignGpuOffset)
{
    if (alignGpuOffset) alignOffset(size);
    return push(mStageBuffer, data, size);
}

uint32_t GlStageBuffer::getImageOpacityOffset(uint8_t opacity)
{
    auto& offset = mImageOpacityOffsets[opacity];
    if (offset == UINT32_MAX) {
        GlImageOpacityBlock block = {opacity, 0, 0, 0};
        offset = push(block, sizeof(block), true);
    }
    return offset;
}

bool GlStageBuffer::flushToGPU(GlStateCache& state)
{
    memset(mImageOpacityOffsets, 0xff, sizeof(mImageOpacityOffsets));

    if ((mStageBuffer.empty() && mAuxBuffer.empty()) || mIndexBuffer.empty()) {
        mStageBuffer.clear();
        mAuxBuffer.clear();
        mIndexBuffer.clear();
        return false;
    }

    state.bindVertexArray(mVao);

    if (!mStageBuffer.empty()) {
        state.bindBuffer(static_cast<uint32_t>(GlGpuBuffer::Target::ARRAY_BUFFER), mGpuBuffer.getBufferId());
        mGpuBuffer.updateBufferData(GlGpuBuffer::Target::ARRAY_BUFFER, mStageBuffer.count, mStageBuffer.data);
    }

    if (!mAuxBuffer.empty()) {
        state.bindBuffer(static_cast<uint32_t>(GlGpuBuffer::Target::ARRAY_BUFFER), mGpuAuxBuffer.getBufferId());
        mGpuAuxBuffer.updateBufferData(GlGpuBuffer::Target::ARRAY_BUFFER, mAuxBuffer.count, mAuxBuffer.data);
    }

    state.bindBuffer(static_cast<uint32_t>(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER), mGpuIndexBuffer.getBufferId());
    mGpuIndexBuffer.updateBufferData(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER, mIndexBuffer.count, mIndexBuffer.data);

    mStageBuffer.clear();
    mAuxBuffer.clear();
    mIndexBuffer.clear();

    return true;
}

void GlStageBuffer::bind(GlStateCache& state)
{
    state.bindVertexArray(mVao);
    state.bindBuffer(static_cast<uint32_t>(GlGpuBuffer::Target::ARRAY_BUFFER), mGpuBuffer.getBufferId());
    state.bindBuffer(static_cast<uint32_t>(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER), mGpuIndexBuffer.getBufferId());
}

void GlStageBuffer::unbind(GlStateCache& state)
{
    state.bindVertexArray(mVao);
    state.bindBuffer(static_cast<uint32_t>(GlGpuBuffer::Target::ARRAY_BUFFER), 0);
    GL_CHECK(glBindBuffer(GL_UNIFORM_BUFFER, 0));
    state.bindBuffer(static_cast<uint32_t>(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER), 0);
    state.bindVertexArray(0);
}

void GlStageBuffer::alignOffset(uint32_t size)
{
    uint32_t alignment = _getGpuBufferAlign();
    if (mStageBuffer.count % alignment == 0) return;

    uint32_t offset = alignment - mStageBuffer.count % alignment;
    if (mStageBuffer.count + offset + size > mStageBuffer.reserved) {
        mStageBuffer.grow(max(offset + size, mStageBuffer.reserved));
    }
    mStageBuffer.count += offset;
}
