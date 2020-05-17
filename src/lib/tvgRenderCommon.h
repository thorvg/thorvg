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

struct RenderMatrix
{
    //3x3 Matrix Elements
    float e11, e12, e13;
    float e21, e22, e23;
    float e31, e32, e33;

    static void rotate(RenderMatrix* out, float degree)
    {
        constexpr auto PI = 3.141592f;

        if (fabsf(degree) < FLT_EPSILON) return;

        auto radian = degree / 180.0f * PI;
        auto cosVal = cosf(radian);
        auto sinVal = sinf(radian);

        auto t11 = out->e11 * cosVal + out->e12 * sinVal;
        auto t12 = out->e11 * -sinVal + out->e12 * cosVal;
        auto t21 = out->e21 * cosVal + out->e22 * sinVal;
        auto t22 = out->e21 * -sinVal + out->e22 * cosVal;
        auto t31 = out->e31 * cosVal + out->e32 * sinVal;
        auto t32 = out->e31 * -sinVal + out->e32 * cosVal;

        out->e11 = t11;
        out->e12 = t12;
        out->e21 = t21;
        out->e22 = t22;
        out->e31 = t31;
        out->e32 = t32;
    }

    static void scale(RenderMatrix* out, float factor)
    {
        out->e11 *= factor;
        out->e22 *= factor;
        out->e33 *= factor;
    }

    static void identity(RenderMatrix* out)
    {
        out->e11 = 1.0f;
        out->e12 = 0.0f;
        out->e13 = 0.0f;
        out->e21 = 0.0f;
        out->e22 = 1.0f;
        out->e23 = 0.0f;
        out->e31 = 0.0f;
        out->e32 = 0.0f;
        out->e33 = 1.0f;
    }

    static void translate(RenderMatrix* out, float x, float y)
    {
        out->e31 += x;
        out->e32 += y;
    }

    static void multiply(const RenderMatrix* lhs, const RenderMatrix* rhs, RenderMatrix* out)
    {
        assert(lhs && rhs && out);

        out->e11 = lhs->e11 * rhs->e11 + lhs->e12 * rhs->e21 + lhs->e13 * rhs->e31;
        out->e12 = lhs->e11 * rhs->e12 + lhs->e12 * rhs->e22 + lhs->e13 * rhs->e32;
        out->e13 = lhs->e11 * rhs->e13 + lhs->e12 * rhs->e23 + lhs->e13 * rhs->e33;

        out->e21 = lhs->e21 * rhs->e11 + lhs->e22 * rhs->e21 + lhs->e23 * rhs->e31;
        out->e22 = lhs->e21 * rhs->e12 + lhs->e22 * rhs->e22 + lhs->e23 * rhs->e32;
        out->e23 = lhs->e21 * rhs->e13 + lhs->e22 * rhs->e23 + lhs->e23 * rhs->e33;

        out->e31 = lhs->e31 * rhs->e11 + lhs->e32 * rhs->e21 + lhs->e33 * rhs->e31;
        out->e32 = lhs->e31 * rhs->e12 + lhs->e32 * rhs->e22 + lhs->e33 * rhs->e32;
        out->e33 = lhs->e31 * rhs->e13 + lhs->e32 * rhs->e23 + lhs->e33 * rhs->e33;
    }
};

struct RenderTransform
{
    RenderMatrix m;
    float x = 0.0f;
    float y = 0.0f;
    float degree = 0.0f;  //rotation degree
    float factor = 1.0f;  //scale factor

    bool update()
    {
        //Init Status
        if (fabsf(x) <= FLT_EPSILON && fabsf(y) <= FLT_EPSILON &&
            fabsf(degree) <= FLT_EPSILON && fabsf(factor - 1) <= FLT_EPSILON) {
            return false;
        }

        RenderMatrix::identity(&m);
        RenderMatrix::scale(&m, factor);
        RenderMatrix::rotate(&m, degree);
        RenderMatrix::translate(&m, x, y);

        return true;
    }
};


class RenderMethod
{
public:
    virtual ~RenderMethod() {}
    virtual void* prepare(const Shape& shape, void* data, const RenderMatrix* transform, RenderUpdateFlag flags) = 0;
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
