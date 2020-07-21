/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
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
    virtual void* prepare(const Shape& shape, void* data, const RenderTransform* transform, RenderUpdateFlag flags) { return nullptr; }
    virtual bool dispose(const Shape& shape, void *data) { return false; }
    virtual bool preRender() { return false; }
    virtual bool render(const Shape& shape, void *data) { return false; }
    virtual bool postRender() { return false; }
    virtual bool clear() { return false; }
    virtual bool flush() { return false; }
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