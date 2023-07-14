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

#include "tvgGlGeometry.h"

#include <float.h>

#include "tvgTessellator.h"

#define NORMALIZED_TOP_3D 1.0f
#define NORMALIZED_BOTTOM_3D -1.0f
#define NORMALIZED_LEFT_3D -1.0f
#define NORMALIZED_RIGHT_3D 1.0f

GlGeometry::GlGeometry()
{
    GL_CHECK(glGenVertexArrays(1, &mVao));

    assert(mVao);
}

GlGeometry::~GlGeometry()
{
    GL_CHECK(glDeleteVertexArrays(1, &mVao));

    mVao = 0;
}

bool GlGeometry::tessellate(const RenderShape& rshape, RenderUpdateFlag flag, GLStageBuffer* vertexBuffer,
                            GLStageBuffer* indexBuffer)
{
    Array<float>    vertices;
    Array<uint32_t> indices;
    Tessellator     tess(&vertices, &indices);

    tess.tessellate(&rshape, true);

    if (vertices.count == 0 || indices.count == 0) {
        return false;
    }

    mVertexBufferView = vertexBuffer->push(vertices.data, vertices.count * sizeof(float));
    mIndexBufferView = indexBuffer->push(indices.data, indices.count * sizeof(uint32_t));

    mDrawCount = indices.count;

    return true;
}


void GlGeometry::draw(RenderUpdateFlag flag)
{
    if (mDrawCount == 0) {
        return;
    }

    // TODO draw stroke based on flag
    this->bindBuffers();

    GL_CHECK(
        glDrawElements(GL_TRIANGLES, mDrawCount, GL_UNSIGNED_INT, reinterpret_cast<void*>(mIndexBufferView.offset)));

    // reset vao state
    GL_CHECK(glBindVertexArray(0));
}


void GlGeometry::updateTransform(const RenderTransform* transform, float w, float h)
{
    float model[16];

    if (transform) {
        transform->toMatrix4x4(model);
    } else {
        // identity matrix
        memset(model, 0, 16 * sizeof(float));
        model[0] = 1.f;
        model[5] = 1.f;
        model[10] = 1.f;
        model[15] = 1.f;
    }

    MVP_MATRIX(w, h);

    MULTIPLY_MATRIX(mvp, model, mTransform);
}

float* GlGeometry::getTransforMatrix()
{
    return mTransform;
}

GlSize GlGeometry::getPrimitiveSize() const
{
    return GlSize{};
}

void GlGeometry::bindBuffers()
{
    assert(mVertexBufferView.buffer);
    assert(mIndexBufferView.buffer);
    assert(mVao);

    // bind vao to make sure buffer offset is applied inside this geometry
    GL_CHECK(glBindVertexArray(mVao));

    mVertexBufferView.buffer->bind(GlGpuBuffer::Target::ARRAY_BUFFER);

    // currently only have one attribute
    // if we need to support text in the future, we may need another attribute ot carry uv information
    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3,
                                   reinterpret_cast<void*>(mVertexBufferView.offset)));

    mIndexBufferView.buffer->bind(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER);
}
