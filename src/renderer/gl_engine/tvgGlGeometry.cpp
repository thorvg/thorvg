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

#include <float.h>
#include "tvgGlGpuBuffer.h"
#include "tvgGlGeometry.h"
#include "tvgGlTessellator.h"

#define NORMALIZED_TOP_3D 1.0f
#define NORMALIZED_BOTTOM_3D -1.0f
#define NORMALIZED_LEFT_3D -1.0f
#define NORMALIZED_RIGHT_3D 1.0f

GlGeometry::~GlGeometry()
{
    if (mVao) {
        glDeleteVertexArrays(1, &mVao);
    }
}

bool GlGeometry::tesselate(const RenderShape& rshape, RenderUpdateFlag flag)
{
    mFillVertexOffset = 0;
    mStrokeVertexOffset = 0;
    mFillIndexOffset = 0;
    mStrokeIndexOffset = 0;
    mFillCount = 0;
    mStrokeCount = 0;

    mStaveVertex.clear();
    mStageIndex.clear();

    if (flag & (RenderUpdateFlag::Color | RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) {
        mFillVertexOffset = mStaveVertex.count * sizeof(float);
        mFillIndexOffset = mStageIndex.count * sizeof(uint32_t);

        Array<float> vertex;
        Array<uint32_t> index;

        tvg::Tessellator tess{&vertex, &index};
        tess.tessellate(&rshape, true);

        mFillCount = index.count;

        mStaveVertex.push(vertex);
        mStageIndex.push(index);
    }

    if (flag & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
        mStrokeVertexOffset = mStaveVertex.count * sizeof(float);
        mStrokeIndexOffset = mStageIndex.count * sizeof(uint32_t);

        Array<float> vertex;
        Array<uint32_t> index;

        tvg::Stroker stroke{&vertex, &index};
        stroke.stroke(&rshape);

        mStrokeCount = index.count;

        mStaveVertex.push(vertex);
        mStageIndex.push(index);
    }

    return true;
}


void GlGeometry::disableVertex(uint32_t location)
{
    GL_CHECK(glDisableVertexAttribArray(location));
    mVertexBuffer->unbind(GlGpuBuffer::Target::ARRAY_BUFFER);
    mIndexBuffer->unbind(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER);
}


void GlGeometry::draw(const uint32_t location, RenderUpdateFlag flag)
{

    if (flag == RenderUpdateFlag::None) {
        return;
    }

    if (mVao == 0) glGenVertexArrays(1, &mVao);
    glBindVertexArray(mVao);

    updateBuffer(location);

    uint32_t vertexOffset = (flag == RenderUpdateFlag::Stroke) ? mStrokeVertexOffset : mFillVertexOffset;
    uint32_t indexOffset = (flag == RenderUpdateFlag::Stroke) ? mStrokeIndexOffset : mFillIndexOffset;
    uint32_t count = (flag == RenderUpdateFlag::Stroke) ? mStrokeCount : mFillCount;

    GL_CHECK(glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(vertexOffset)));
    GL_CHECK(glEnableVertexAttribArray(location));

    GL_CHECK(glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, reinterpret_cast<void*>(indexOffset)));
}


void GlGeometry::updateBuffer(uint32_t location)
{
    if (mVertexBuffer == nullptr) mVertexBuffer = std::make_unique<GlGpuBuffer>();
    if (mIndexBuffer == nullptr) mIndexBuffer = std::make_unique<GlGpuBuffer>();

    mVertexBuffer->updateBufferData(GlGpuBuffer::Target::ARRAY_BUFFER, mStaveVertex.count * sizeof(float), mStaveVertex.data);
    mIndexBuffer->updateBufferData(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER, mStageIndex.count * sizeof(uint32_t), mStageIndex.data);
}


void GlGeometry::updateTransform(const RenderTransform* transform, float w, float h)
{
    float modelMatrix[16];
    if (transform) {
        GET_MATRIX44(transform->m, modelMatrix);
    } else {
        memset(modelMatrix, 0, 16 * sizeof(float));
        modelMatrix[0] = 1.f;
        modelMatrix[5] = 1.f;
        modelMatrix[10] = 1.f;
        modelMatrix[15] = 1.f;
    }

    MVP_MATRIX();
    MULTIPLY_MATRIX(mvp, modelMatrix, mTransform)
}

float* GlGeometry::getTransforMatrix()
{
    return mTransform;
}
