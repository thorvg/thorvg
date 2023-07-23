
#include "tvgGlCommand.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <cstddef>
#include <cstdint>

void GlCommand::execute()
{
    // bind shader
    shader->load();
    // bind vertex buffer
    vertexBuffer.buffer->bind(GlGpuBuffer::Target::ARRAY_BUFFER);
    // setup attribute layout
    for (size_t i = 0; i < vertexLayouts.size(); ++i) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, vertexLayouts[i].size, GL_FLOAT, GL_FALSE, vertexLayouts[i].stride,
                              reinterpret_cast<void *>(vertexLayouts[i].offset + vertexBuffer.offset));
    }
    // bind index buffer
    indexBuffer.buffer->bind(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER);
    // bind other resources
    for (auto &binding : bindings) {
        if (binding.type == BindingType::kTexture) {
            glActiveTexture(GL_TEXTURE0 + binding.bindPoint);
            glBindTexture(GL_TEXTURE_2D, binding.texId);

            shader->setUniform1Value(binding.location, 1, reinterpret_cast<int32_t *>(&binding.bindPoint));
        } else if (binding.type == BindingType::kUniformBuffer) {
            binding.buffer.buffer->bind(GlGpuBuffer::Target::UNIFORM_BUFFER);

            glUniformBlockBinding(shader->getProgramId(), binding.location, binding.bindPoint);
            glBindBufferRange(GL_UNIFORM_BUFFER, binding.bindPoint, binding.buffer.buffer->GetBufferId(),
                              binding.buffer.offset, binding.bufferRange);
        }
    }

    // draw
    glDrawElements(GL_TRIANGLES, drawCount, GL_UNSIGNED_INT, reinterpret_cast<void *>(indexBuffer.offset));
}