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
#ifndef _TVG_RENDER_H_
#define _TVG_RENDER_H_

#include "tvgCommon.h"
#include <vector>

namespace tvg
{

struct Surface
{
    //TODO: Union for multiple types
    uint32_t* buffer;
    uint32_t stride;
    uint32_t w, h;
    uint32_t cs;
};

struct Composite {
    void* compEData;
    CompMethod method;
};

enum RenderUpdateFlag {None = 0, Path = 1, Color = 2, Gradient = 4, Stroke = 8, Transform = 16, All = 32};

struct RenderTransform
{
    Matrix m;             //3x3 Matrix Elements
    float x = 0.0f;
    float y = 0.0f;
    float degree = 0.0f;  //rotation degree
    float scale = 1.0f;   //scale factor
    bool overriding = false;  //user transform?

    bool update();
    void override(const Matrix& m);

    RenderTransform();
    RenderTransform(const RenderTransform* lhs, const RenderTransform* rhs);
};


class RenderMethod
{
public:
    virtual ~RenderMethod() {}
    virtual void* prepare(TVG_UNUSED const Shape& shape, TVG_UNUSED void* data, TVG_UNUSED const RenderTransform* transform, TVG_UNUSED vector<Composite>& compList, TVG_UNUSED RenderUpdateFlag flags) { return nullptr; }
    virtual bool dispose(TVG_UNUSED const Shape& shape, TVG_UNUSED void *data) { return true; }
    virtual bool preRender() { return true; }
    virtual bool render(TVG_UNUSED const Shape& shape, TVG_UNUSED void *data) { return true; }
    virtual bool postRender() { return true; }
    virtual bool clear() { return true; }
    virtual bool flush() { return true; }
};

}

#endif //_TVG_RENDER_H_
