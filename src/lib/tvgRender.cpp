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
#ifndef _TVG_RENDER_CPP_
#define _TVG_RENDER_CPP_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void RenderTransform::override(const Matrix& m)
{
    this->m = m;

    if (m.e11 == 0.0f && m.e12 == 0.0f && m.e13 == 0.0f &&
        m.e21 == 0.0f && m.e22 == 0.0f && m.e23 == 0.0f &&
        m.e31 == 0.0f && m.e32 == 0.0f && m.e33 == 0.0f) {
        overriding = false;
    } else overriding = true;
}


bool RenderTransform::update()
{
    constexpr auto PI = 3.141592f;

    if (overriding) return true;

    //Init Status
    if (fabsf(x) <= FLT_EPSILON && fabsf(y) <= FLT_EPSILON &&
        fabsf(degree) <= FLT_EPSILON && fabsf(factor - 1) <= FLT_EPSILON) {
        return false;
    }

    //identity
    m.e11 = 1.0f;
    m.e12 = 0.0f;
    m.e13 = 0.0f;
    m.e21 = 0.0f;
    m.e22 = 1.0f;
    m.e23 = 0.0f;
    m.e31 = 0.0f;
    m.e32 = 0.0f;
    m.e33 = 1.0f;

    //scale
    m.e11 *= factor;
    m.e22 *= factor;
    m.e33 *= factor;

    //rotation
    if (fabsf(degree) > FLT_EPSILON) {
        auto radian = degree / 180.0f * PI;
        auto cosVal = cosf(radian);
        auto sinVal = sinf(radian);

        auto t11 = m.e11 * cosVal + m.e12 * sinVal;
        auto t12 = m.e11 * -sinVal + m.e12 * cosVal;
        auto t21 = m.e21 * cosVal + m.e22 * sinVal;
        auto t22 = m.e21 * -sinVal + m.e22 * cosVal;
        auto t31 = m.e31 * cosVal + m.e32 * sinVal;
        auto t32 = m.e31 * -sinVal + m.e32 * cosVal;

        m.e11 = t11;
        m.e12 = t12;
        m.e21 = t21;
        m.e22 = t22;
        m.e31 = t31;
        m.e32 = t32;
    }

    m.e31 += x;
    m.e32 += y;

    return true;
}


RenderTransform::RenderTransform()
{
}


RenderTransform::RenderTransform(const RenderTransform* lhs, const RenderTransform* rhs)
{
    assert(lhs && rhs);

    auto dx = rhs->x * lhs->factor;
    auto dy = rhs->y * lhs->factor;
    auto tx = dx * lhs->m.e11 + dy * lhs->m.e12 + lhs->m.e13;
    auto ty = dx * lhs->m.e21 + dy * lhs->m.e22 + lhs->m.e23;

    x = lhs->x + tx;
    y = lhs->y + ty;
    degree = lhs->degree + rhs->degree;
    factor = lhs->factor * rhs->factor;

    update();
}

#endif //_TVG_RENDER_CPP_
