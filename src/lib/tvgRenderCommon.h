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
#ifndef _TVG_RENDER_COMMON_H_
#define _TVG_RENDER_COMMON_H_

namespace tvg
{

struct Surface
{
    //TODO: Union for multiple types
    uint32_t* buffer;
    size_t stride;
    size_t w, h;
};

enum RenderUpdateFlag {None = 0, Path = 1, Fill = 2, Transform = 4, All = 8};

struct RenderTransform
{
    float e11, e12, e13;
    float e21, e22, e23;
    float e31, e32, e33;

    void identity()
    {
        e11 = 1.0f;
        e12 = 0.0f;
        e13 = 0.0f;
        e21 = 0.0f;
        e22 = 1.0f;
        e23 = 0.0f;
        e31 = 0.0f;
        e32 = 0.0f;
        e33 = 1.0f;
    }

    void rotate(float degree)
    {
        constexpr auto PI = 3.141592f;

        if (fabsf(degree) <= FLT_EPSILON) return;

        auto radian = degree / 180.0f * PI;
        auto cosVal = cosf(radian);
        auto sinVal = sinf(radian);

        auto t11 = e11 * cosVal + e12 * sinVal;
        auto t12 = e11 * -sinVal + e12 * cosVal;
        auto t21 = e21 * cosVal + e22 * sinVal;
        auto t22 = e21 * -sinVal + e22 * cosVal;
        auto t31 = e31 * cosVal + e32 * sinVal;
        auto t32 = e31 * -sinVal + e32 * cosVal;

        e11 = t11;
        e12 = t12;
        e21 = t21;
        e22 = t22;
        e31 = t31;
        e32 = t32;
    }

    void translate(float x, float y)
    {
        e31 += x;
        e32 += y;
    }

    void scale(float factor)
    {
        e11 *= factor;
        e22 *= factor;
        e33 *= factor;
    }

    RenderTransform& operator*=(const RenderTransform rhs)
    {
        e11 = e11 * rhs.e11 + e12 * rhs.e21 + e13 * rhs.e31;
        e12 = e11 * rhs.e12 + e12 * rhs.e22 + e13 * rhs.e32;
        e13 = e11 * rhs.e13 + e12 * rhs.e23 + e13 * rhs.e33;

        e21 = e21 * rhs.e11 + e22 * rhs.e21 + e23 * rhs.e31;
        e22 = e21 * rhs.e12 + e22 * rhs.e22 + e23 * rhs.e32;
        e23 = e21 * rhs.e13 + e22 * rhs.e23 + e23 * rhs.e33;

        e31 = e31 * rhs.e11 + e32 * rhs.e21 + e33 * rhs.e31;
        e32 = e31 * rhs.e12 + e32 * rhs.e22 + e33 * rhs.e32;
        e33 = e31 * rhs.e13 + e32 * rhs.e23 + e33 * rhs.e33;

        return *this;
    }
};


class RenderMethod
{
public:
    virtual ~RenderMethod() {}
    virtual void* prepare(const Shape& shape, void* data, const RenderTransform* transform, RenderUpdateFlag flags) = 0;
    virtual bool dispose(const Shape& shape, void *data) = 0;
    virtual bool render(const Shape& shape, void *data) = 0;
    virtual bool clear() = 0;
    virtual size_t ref() = 0;
    virtual size_t unref() = 0;
};

struct RenderInitializer
{
    RenderMethod* pInst = nullptr;
    size_t refCnt = 0;
    bool initialized = false;

    static int init(RenderInitializer& renderInit, RenderMethod* engine)
    {
        assert(engine);
        if (renderInit.pInst || renderInit.refCnt > 0) return -1;
        renderInit.pInst = engine;
        renderInit.refCnt = 0;
        renderInit.initialized = true;
        return 0;
    }

    static int term(RenderInitializer& renderInit)
    {
        if (!renderInit.pInst || !renderInit.initialized) return -1;

        renderInit.initialized = false;

        //Still it's refered....
        if (renderInit.refCnt > 0) return  0;
        delete(renderInit.pInst);
        renderInit.pInst = nullptr;

        return 0;
    }

    static size_t unref(RenderInitializer& renderInit)
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

    static size_t ref(RenderInitializer& renderInit)
    {
        return ++renderInit.refCnt;
    }

};

}

#endif //_TVG_RENDER_COMMON_H_
