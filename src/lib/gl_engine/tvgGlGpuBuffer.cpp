#include "tvgGlCommon.h"
#include "tvgGlGpuBuffer.h"

#include <assert.h>

GlGpuBuffer::GlGpuBuffer()
{
    GL_CHECK(glGenBuffers(1, &mGlBufferId));
    assert(mGlBufferId != GL_INVALID_VALUE);
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
    GL_CHECK(glBindBuffer(static_cast<uint32_t>(target), mGlBufferId));
    GL_CHECK(glBufferData(static_cast<uint32_t>(target), size, data, GL_STATIC_DRAW));
}


void GlGpuBuffer::unbind(Target target)
{
    GL_CHECK(glBindBuffer(static_cast<uint32_t>(target), 0));
}