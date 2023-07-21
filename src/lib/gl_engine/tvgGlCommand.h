
#ifndef _TVG_GL_COMMAND_H_
#define _TVG_GL_COMMAND_H_

#include "tvgGlGpuBuffer.h"
#include "tvgGlRenderTask.h"

#include <vector>

enum class BindingType
{
    kUniformBuffer,
    kTexture,
};
struct BindingResource
{
    BindingType type;
    /**
     * Binding point index.
     *  Can be a uniform location for a texture
     *  Can be a uniform buffer binding index for a uniform block
     */
    uint32_t        bindPoint = {};
    uint32_t        location = {};
    GLuint          texId = {};
    GlGpuBufferView buffer = {};
    uint32_t        bufferRange = {};

    BindingResource(uint32_t index, uint32_t location, GlGpuBufferView buffer, uint32_t range)
        : type(BindingType::kUniformBuffer),
          bindPoint(index),
          location(location),
          buffer(std::move(buffer)),
          bufferRange(range)
    {
    }
};

struct VertexLayout
{
    uint32_t index;
    uint32_t stride;
    size_t   offset;
};

class GlProgram;

struct GlCommand
{
    // pipeline shader
    GlProgram* shader;
    // index start
    uint32_t drawStart;
    // index count
    uint32_t drawCount;
    // vertex buffer view
    GlGpuBufferView              vertexBuffer;
    GlGpuBufferView              indexBuffer;
    std::vector<BindingResource> bindings;
    std::vector<VertexLayout>    vertexLayouts;

    void execute();
};

#endif  // _TVG_GL_COMMAND_H_