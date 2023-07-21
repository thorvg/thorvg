
#include "tvgGlCommand.h"

void GlCommand::execute()
{
    // bind vertex buffer
    vertexBuffer.buffer->bind(GlGpuBuffer::Target::ARRAY_BUFFER);

}