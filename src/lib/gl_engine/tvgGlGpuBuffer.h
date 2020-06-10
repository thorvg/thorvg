#ifndef _TVG_GL_GPU_BUFFER_H_
#define _TVG_GL_GPU_BUFFER_H_

#include <stdlib.h>
#include <GLES2/gl2.h>

class GlGpuBuffer
{
public:
    enum class Target
    {
        ARRAY_BUFFER = GL_ARRAY_BUFFER,
        ELEMENT_ARRAY_BUFFER = GL_ARRAY_BUFFER
    };

    GlGpuBuffer();
    ~GlGpuBuffer();
    void updateBufferData(Target target, uint32_t size, const void* data);
    void unbind(Target target);
private:
    uint32_t    mGlBufferId = 0;

};

#endif /* _TVG_GL_GPU_BUFFER_H_ */

