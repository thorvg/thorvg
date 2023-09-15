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
#include "tvgGlRenderTask.h"

#define NORMALIZED_TOP_3D 1.0f
#define NORMALIZED_BOTTOM_3D -1.0f
#define NORMALIZED_LEFT_3D -1.0f
#define NORMALIZED_RIGHT_3D 1.0f

GlGeometry::~GlGeometry()
{
}

bool GlGeometry::tesselate(const RenderShape& rshape, RenderUpdateFlag flag)
{
    if (flag & (RenderUpdateFlag::Color | RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) {
        fillVertex.clear();
        fillIndex.clear();

        Tessellator tess{&fillVertex, &fillIndex};
        tess.tessellate(&rshape, true);
    }

    if (flag & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
        strokeVertex.clear();
        strokeIndex.clear();

        Stroker stroke{&strokeVertex, &strokeIndex};
        stroke.stroke(&rshape);
    }

    return true;
}


void GlGeometry::disableVertex(uint32_t location)
{
    GL_CHECK(glDisableVertexAttribArray(location));
}


bool GlGeometry::draw(GlRenderTask* task, GlStageBuffer* gpuBuffer, RenderUpdateFlag flag)
{

    if (flag == RenderUpdateFlag::None) {
        return false;
    }

    Array<float>* vertexBuffer = nullptr;
    Array<uint32_t>* indexBuffer = nullptr;

    if (flag & RenderUpdateFlag::Stroke) {
        vertexBuffer = &strokeVertex;
        indexBuffer = &strokeIndex;
    } else {
        vertexBuffer = &fillVertex;
        indexBuffer = &fillIndex;
    }

    if (indexBuffer->count == 0) return false;

    uint32_t vertexOffset = gpuBuffer->push(vertexBuffer->data, vertexBuffer->count * sizeof(float));
    uint32_t indexOffset = gpuBuffer->push(indexBuffer->data, indexBuffer->count * sizeof(uint32_t));

    // vertex layout
    task->addVertexLayout(GlVertexLayout{0, 3, 3 * sizeof(float), vertexOffset});
    task->setDrawRange(indexOffset, indexBuffer->count);

    return true;
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
