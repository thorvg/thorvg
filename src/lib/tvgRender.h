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
#include "tvgArray.h"

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

struct ClipPath {
    void* edata;
    CompositeMethod method;
};

enum RenderUpdateFlag {None = 0, Path = 1, Color = 2, Gradient = 4, Stroke = 8, Transform = 16, Image = 32, All = 64};

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
    virtual void* prepare(const Shape& shape, void* data, const RenderTransform* transform, uint32_t opacity, Array<ClipPath>& clips, RenderUpdateFlag flags) = 0;
    virtual void* prepare(const Picture& picture, void* data, uint32_t *buffer, const RenderTransform* transform, uint32_t opacity, Array<ClipPath>& clips, RenderUpdateFlag flags) = 0;
    virtual void* beginComposite(uint32_t x, uint32_t y, uint32_t w, uint32_t h) = 0;
    virtual bool endComposite(void* ctx, uint32_t opacity) = 0;
    virtual bool dispose(void *data) = 0;
    virtual bool preRender() = 0;
    virtual bool render(const Shape& shape, void *data) = 0;
    virtual bool render(const Picture& picture, void *data) = 0;
    virtual bool postRender() = 0;
    virtual bool renderRegion(void* data, uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h) = 0;
    virtual bool clear() = 0;
    virtual bool sync() = 0;
};

}

#endif //_TVG_RENDER_H_
