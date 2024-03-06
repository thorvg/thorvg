/*
 * Copyright (c) 2020 - 2024 the ThorVG project. All rights reserved.

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
#include "tvgGlGeometry.h"
#include "tvgGlTessellator.h"
#include "tvgGlRenderTask.h"

GlGeometry::~GlGeometry()
{
}

bool GlGeometry::tesselate(const RenderShape& rshape, RenderUpdateFlag flag)
{
    if (flag & (RenderUpdateFlag::Color | RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) {
        fillVertex.clear();
        fillIndex.clear();


        BWTessellator bwTess{&fillVertex, &fillIndex};

        bwTess.tessellate(&rshape);

        mStencilFill = true;
    }

    if (flag & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
        strokeVertex.clear();
        strokeIndex.clear();

        Stroker stroke{&strokeVertex, &strokeIndex};
        stroke.stroke(&rshape);
    }

    return true;
}

bool GlGeometry::tesselate(const Surface* image, const RenderMesh* mesh, RenderUpdateFlag flag)
{
    if (flag & RenderUpdateFlag::Image) {
        fillVertex.clear();
        fillIndex.clear();

        if (mesh && mesh->triangleCnt) {
            fillVertex.reserve(mesh->triangleCnt * 3 * 5);
            fillIndex.reserve(mesh->triangleCnt * 3);

            uint32_t index = 0;
            for (uint32_t i = 0; i < mesh->triangleCnt; i++) {
                fillVertex.push(mesh->triangles[i].vertex[0].pt.x);
                fillVertex.push(mesh->triangles[i].vertex[0].pt.y);

                fillVertex.push(mesh->triangles[i].vertex[0].uv.x);
                fillVertex.push(mesh->triangles[i].vertex[0].uv.y);

                fillVertex.push(mesh->triangles[i].vertex[1].pt.x);
                fillVertex.push(mesh->triangles[i].vertex[1].pt.y);

                fillVertex.push(mesh->triangles[i].vertex[1].uv.x);
                fillVertex.push(mesh->triangles[i].vertex[1].uv.y);

                fillVertex.push(mesh->triangles[i].vertex[2].pt.x);
                fillVertex.push(mesh->triangles[i].vertex[2].pt.y);

                fillVertex.push(mesh->triangles[i].vertex[2].uv.x);
                fillVertex.push(mesh->triangles[i].vertex[2].uv.y);

                fillIndex.push(index);
                fillIndex.push(index + 1);
                fillIndex.push(index + 2);
                index += 3;
            }

        } else {
            fillVertex.reserve(5 * 4);
            fillIndex.reserve(6);

            float left = 0.f;
            float top = 0.f;
            float right = image->w;
            float bottom = image->h;

            // left top point
            fillVertex.push(left);
            fillVertex.push(top);

            fillVertex.push(0.f);
            fillVertex.push(1.f);
            // left bottom point
            fillVertex.push(left);
            fillVertex.push(bottom);

            fillVertex.push(0.f);
            fillVertex.push(0.f);
            // right top point
            fillVertex.push(right);
            fillVertex.push(top);

            fillVertex.push(1.f);
            fillVertex.push(1.f);
            // right bottom point
            fillVertex.push(right);
            fillVertex.push(bottom);

            fillVertex.push(1.f);
            fillVertex.push(0.f);

            fillIndex.push(0);
            fillIndex.push(1);
            fillIndex.push(2);

            fillIndex.push(2);
            fillIndex.push(1);
            fillIndex.push(3);
        }
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
    if (flag & RenderUpdateFlag::Image) {
        // image has two attribute: [pos, uv]
        task->addVertexLayout(GlVertexLayout{0, 2, 4 * sizeof(float), vertexOffset});
        task->addVertexLayout(GlVertexLayout{1, 2, 4 * sizeof(float), vertexOffset + 2 * sizeof(float)});
    } else {
        task->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), vertexOffset});
    }
    task->setDrawRange(indexOffset, indexBuffer->count);
    task->setViewport(viewport);
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

void GlGeometry::setViewport(const RenderRegion& viewport)
{
    this->viewport = viewport;
}

float* GlGeometry::getTransforMatrix()
{
    return mTransform;
}

bool GlGeometry::needStencilCover(RenderUpdateFlag flag)
{
    if (flag & RenderUpdateFlag::Stroke) return false;
    if (flag & RenderUpdateFlag::GradientStroke) return false;
    if (flag & RenderUpdateFlag::Image) return false;

    return mStencilFill;
}
