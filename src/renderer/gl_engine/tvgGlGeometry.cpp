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
}

bool GlGeometry::tesselate(const RenderShape& rshape, RenderUpdateFlag flag, GlStageBuffer* gpuBuffer)
{
    mFillVertexOffset = 0;
    mStrokeVertexOffset = 0;
    mFillIndexOffset = 0;
    mStrokeIndexOffset = 0;
    mFillCount = 0;
    mStrokeCount = 0;

    if (flag & (RenderUpdateFlag::Color | RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) {
        Array<float> vertex;
        Array<uint32_t> index;

        tvg::Tessellator tess{&vertex, &index};
        tess.tessellate(&rshape, true);

        mFillCount = index.count;

        mFillVertexOffset = gpuBuffer->push(vertex.data, vertex.count * sizeof(float));
        mFillIndexOffset = gpuBuffer->push(index.data, index.count * sizeof(uint32_t));
    }

    if (flag & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
        Array<float> vertex;
        Array<uint32_t> index;

        tvg::Stroker stroke{&vertex, &index};
        stroke.stroke(&rshape);

        mStrokeCount = index.count;

        mStrokeVertexOffset = gpuBuffer->push(vertex.data, vertex.count * sizeof(float));
        mStrokeIndexOffset = gpuBuffer->push(index.data, index.count * sizeof(uint32_t));
    }

    return true;
}


void GlGeometry::disableVertex(uint32_t location)
{
    GL_CHECK(glDisableVertexAttribArray(location));
}


void GlGeometry::draw(const uint32_t location, RenderUpdateFlag flag)
{

    if (flag == RenderUpdateFlag::None) {
        return;
    }


    uint32_t vertexOffset = (flag == RenderUpdateFlag::Stroke) ? mStrokeVertexOffset : mFillVertexOffset;
    uint32_t indexOffset = (flag == RenderUpdateFlag::Stroke) ? mStrokeIndexOffset : mFillIndexOffset;
    uint32_t count = (flag == RenderUpdateFlag::Stroke) ? mStrokeCount : mFillCount;

    GL_CHECK(glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(vertexOffset)));
    GL_CHECK(glEnableVertexAttribArray(location));

    GL_CHECK(glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, reinterpret_cast<void*>(indexOffset)));
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
