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

#include "tvgGlGpuBuffer.h"

#define NORMALIZED_TOP_3D 1.0f
#define NORMALIZED_BOTTOM_3D -1.0f
#define NORMALIZED_LEFT_3D -1.0f
#define NORMALIZED_RIGHT_3D 1.0f

bool GlGeometry::tessellate(TVG_UNUSED const RenderShape& rshape, float viewWd, float viewHt, RenderUpdateFlag flag) {
    return true;
}

void GlGeometry::disableVertex(uint32_t location) {
    GL_CHECK(glDisableVertexAttribArray(location));
    mGpuVertexBuffer->unbind(GlGpuBuffer::Target::ARRAY_BUFFER);
    mGpuIndexBuffer->unbind(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER);
    glBindVertexArray(0);
}

void GlGeometry::draw(const uint32_t location, RenderUpdateFlag flag) {
    // GL_CHECK(glDrawElements(GL_TRIANGLES, geometry.indices.size(), GL_UNSIGNED_INT, 0));
}

void GlGeometry::updateBuffer(uint32_t location, const VertexDataArray& vertexArray) {
    if (mGpuVertexBuffer.get() == nullptr) {
        mGpuVertexBuffer = std::make_unique<GlGpuBuffer>();
    }

    if (mGpuIndexBuffer.get() == nullptr) {
        mGpuIndexBuffer = std::make_unique<GlGpuBuffer>();
        glGenVertexArrays(1, &mVao);
    }

    mGpuVertexBuffer->updateBufferData(GlGpuBuffer::Target::ARRAY_BUFFER,
                                       vertexArray.vertices.size() * sizeof(VertexData), vertexArray.vertices.data());
    glBindVertexArray(mVao);
    // no need to do this every time
    GL_CHECK(glEnableVertexAttribArray(location));
    GL_CHECK(glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), 0));

    mGpuIndexBuffer->updateBufferData(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER,
                                      vertexArray.indices.size() * sizeof(uint32_t), vertexArray.indices.data());
}

GlPoint GlGeometry::normalizePoint(const GlPoint& pt, float viewWd, float viewHt) {
    GlPoint p;
    p.x = (pt.x * 2.0f / viewWd) - 1.0f;
    p.y = -1.0f * ((pt.y * 2.0f / viewHt) - 1.0f);
    return p;
}

void GlGeometry::addGeometryPoint(VertexDataArray& geometry, const GlPoint& pt, float viewWd, float viewHt,
                                  float opacity) {
    VertexData tv = {normalizePoint(pt, viewWd, viewHt), opacity};
    geometry.vertices.push_back(tv);
}

void GlGeometry::updateTransform(const RenderTransform* transform, float w, float h) {
    if (transform) {
        mTransform.x = transform->x;
        mTransform.y = transform->y;
        mTransform.angle = transform->degree;
        mTransform.scale = transform->scale;
    }

    mTransform.w = w;
    mTransform.h = h;
    GET_TRANSFORMATION(NORMALIZED_LEFT_3D, NORMALIZED_TOP_3D, mTransform.matrix);
}

float* GlGeometry::getTransforMatrix() {
    return mTransform.matrix;
}

GlSize GlGeometry::getPrimitiveSize() const {
    return GlSize{};
}
