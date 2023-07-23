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

bool GlGeometry::tessellate(const RenderShape& rshape, RenderUpdateFlag flag, TessContext* context)
{
    if (flag & RenderUpdateFlag::Color) {
        uint8_t r, g, b, a;
        rshape.fillColor(&r, &g, &b, &a);

        if (a > 0) {
            Array<float>    vertices;
            Array<uint32_t> indices;

            Tessellator tess(&vertices, &indices);

            tess.tessellate(&rshape, true);

            float color[4];
            color[0] = r / 255.f;
            color[1] = g / 255.f;
            color[2] = b / 255.f;
            color[3] = a / 255.f;

            mCmds.insert({RenderUpdateFlag::Color, generateColorCMD(color, vertices, indices, context)});
        }
    }

    if ((flag & RenderUpdateFlag::Stroke) && rshape.strokeWidth() > 0.f) {

        uint8_t r, g, b, a;
        rshape.strokeColor(&r, &g, &b, &a);

        if (a > 0) {

            Array<float>    vertices;
            Array<uint32_t> indices;

            Stroker stroker(&vertices, &indices);

            stroker.stroke(&rshape);

            float color[4];
            color[0] = r / 255.f;
            color[1] = g / 255.f;
            color[2] = b / 255.f;
            color[3] = a / 255.f;

            mCmds.insert({RenderUpdateFlag::Stroke, generateColorCMD(color, vertices, indices, context)});
        }
    }

    return true;
}

void GlGeometry::bind()
{
    assert(mVao);

    glBindVertexArray(mVao);
}

void GlGeometry::unBind()
{
    glBindVertexArray(0);
}

void GlGeometry::draw(RenderUpdateFlag flag)
{
    auto it = mCmds.find(flag);

    if (it == mCmds.end()) {
        return;
    }
    it->second.execute();
    // if (flag == RenderUpdateFlag::Color && mFillDrawCount == 0) {
    //     return;
    // }

    // if (flag == RenderUpdateFlag::Stroke && mStrokeDrawCount == 0) {
    //     return;
    // }

    // // TODO draw stroke based on flag
    // this->bindBuffers();

    // if (flag == RenderUpdateFlag::Color) {

    //     GL_CHECK(glDrawElements(GL_TRIANGLES, mFillDrawCount, GL_UNSIGNED_INT,
    //                             reinterpret_cast<void*>(mIndexBufferView.offset)));
    // } else if (flag == RenderUpdateFlag::Stroke) {
    //     GL_CHECK(
    //         glDrawElements(GL_TRIANGLES, mStrokeDrawCount, GL_UNSIGNED_INT,
    //                        reinterpret_cast<void*>(mIndexBufferView.offset + mStrokeDrawStart * sizeof(uint32_t))));
    // }

    // // reset vao state
    // GL_CHECK(glBindVertexArray(0));
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


GlSize GlGeometry::getPrimitiveSize() const
{
    return GlSize{};
}

GlCommand GlGeometry::generateColorCMD(float color[4], const Array<float>& vertices, const Array<uint32_t>& indices,
                                       TessContext* context)
{
    GlCommand cmd;

    cmd.shader = context->shaders[PipelineType::kSolidColor].get();
    cmd.vertexBuffer = context->vertexBuffer->push(vertices.data, vertices.count * sizeof(float));
    cmd.indexBuffer = context->indexBuffer->push(indices.data, indices.count * sizeof(uint32_t));
    cmd.drawCount = indices.count;
    cmd.drawStart = 0;
    // attribute layout
    cmd.vertexLayouts.emplace_back(VertexLayout{0, 3, 3 * sizeof(float), 0});
    // uniforms
    // matrix
    {
        int32_t loc = cmd.shader->getUniformBlockIndex("Matrix");
        cmd.bindings.emplace_back(
            BindingResource(0, loc, context->uniformBuffer->push(mTransform, 16 * sizeof(float)), 16 * sizeof(float)));
    }
    // color
    {
        int32_t loc = cmd.shader->getUniformBlockIndex("ColorInfo");

        cmd.bindings.emplace_back(
            BindingResource(1, loc, context->uniformBuffer->push(color, 4 * sizeof(float)), 4 * sizeof(float)));
    }

    return cmd;
}

// void GlGeometry::bindBuffers()
// {
//     assert(mVertexBufferView.buffer);
//     assert(mIndexBufferView.buffer);
//     assert(mVao);

//     GL_CHECK(glBindVertexArray(mVao));

//     mVertexBufferView.buffer->bind(GlGpuBuffer::Target::ARRAY_BUFFER);

//     GL_CHECK(glEnableVertexAttribArray(0));
//     GL_CHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3,
//                                    reinterpret_cast<void*>(mVertexBufferView.offset)));

//     mIndexBufferView.buffer->bind(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER);
// }
