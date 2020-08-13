/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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

#ifndef _TVG_GL_RENDERER_H_
#define _TVG_GL_RENDERER_H_

#include "tvgGlCommon.h"
#include "tvgGlProgram.h"


class GlRenderer : public RenderMethod
{
public:
    Surface surface;

    void* prepare(const Shape& shape, void* data, const RenderTransform* transform, RenderUpdateFlag flags) override;
    bool dispose(const Shape& shape, void *data) override;
    bool preRender() override;
    bool render(const Shape& shape, void *data) override;
    bool postRender() override;
    bool target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h);
    bool flush() override;
    bool clear() override;
    uint32_t ref() override;
    uint32_t unref() override;

    static GlRenderer* inst();
    static int init();
    static int term();

private:
    GlRenderer(){};
    ~GlRenderer(){};

    void initShaders();
    void drawPrimitive(GlGeometry& geometry, float r, float g, float b, float a, uint32_t primitiveIndex, RenderUpdateFlag flag);

    unique_ptr<GlProgram>   mColorProgram;
    int32_t   mColorUniformLoc;
    uint32_t  mVertexAttrLoc;
};

#endif /* _TVG_GL_RENDERER_H_ */
