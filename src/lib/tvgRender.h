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
    virtual void* prepare(TVG_UNUSED const Shape& shape, TVG_UNUSED void* data, TVG_UNUSED const RenderTransform* transform, TVG_UNUSED RenderUpdateFlag flags) { return nullptr; }
    virtual bool dispose(TVG_UNUSED const Shape& shape, TVG_UNUSED void *data) { return true; }
    virtual bool preRender() { return true; }
    virtual bool render(TVG_UNUSED const Shape& shape, TVG_UNUSED void *data) { return true; }
    virtual bool postRender() { return true; }
    virtual bool clear() { return true; }
    virtual bool flush() { return true; }
    virtual uint32_t ref() { return 0; }
    virtual uint32_t unref() { return 0; }
};

struct RenderInitializer
{
    RenderMethod* pInst = nullptr;
    uint32_t refCnt = 0;
    bool initialized = false;

    static bool init(RenderInitializer& renderInit, RenderMethod* engine)
    {
        assert(engine);
        if (renderInit.pInst || renderInit.refCnt > 0) return false;
        renderInit.pInst = engine;
        renderInit.refCnt = 0;
        renderInit.initialized = true;
        return true;
    }

    static bool term(RenderInitializer& renderInit)
    {
        if (!renderInit.pInst || !renderInit.initialized) return false;

        renderInit.initialized = false;

        //Still it's refered....
        if (renderInit.refCnt > 0) return  true;
        delete(renderInit.pInst);
        renderInit.pInst = nullptr;

        return true;
    }

    static uint32_t unref(RenderInitializer& renderInit)
    {
        assert(renderInit.refCnt > 0);
        --renderInit.refCnt;

        //engine has been requested to termination
        if (!renderInit.initialized && renderInit.refCnt == 0) {
            if (renderInit.pInst) {
                delete(renderInit.pInst);
                renderInit.pInst = nullptr;
            }
        }
        return renderInit.refCnt;
    }

    static RenderMethod* inst(RenderInitializer& renderInit)
    {
        assert(renderInit.pInst);
        return renderInit.pInst;
    }

    static uint32_t ref(RenderInitializer& renderInit)
    {
        return ++renderInit.refCnt;
    }

};

}

#endif //_TVG_RENDER_H_