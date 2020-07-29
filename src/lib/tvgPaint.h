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
#ifndef _TVG_PAINT_H_
#define _TVG_PAINT_H_

namespace tvg
{
    struct ITransformMethod
    {
        virtual ~ITransformMethod(){}
        virtual bool rotate(float degree) = 0;
        virtual bool scale(float factor) = 0;
        virtual bool translate(float x, float y) = 0;
        virtual bool transform(const Matrix& m) = 0;
        virtual bool bounds(float* x, float* y, float* w, float* h) const = 0;
    };

    struct Paint::Impl
    {
        ITransformMethod* ts = nullptr;

        ~Impl() {
            if (ts) delete(ts);
        }
    };


    template<class T>
    struct TransformMethod : ITransformMethod
    {
        T* _inst = nullptr;

        TransformMethod(T* inst) : _inst(inst) {}
        ~TransformMethod(){}

        bool rotate(float degree) override
        {
            return _inst->rotate(degree);
        }

        bool scale(float factor) override
        {
            return _inst->scale(factor);
        }

        bool translate(float x, float y) override
        {
            return _inst->translate(x, y);
        }

        bool transform(const Matrix& m) override
        {
            return _inst->transform(m);
        }

        bool bounds(float* x, float* y, float* w, float* h) const override
        {
            return _inst->bounds(x, y, w, h);
        }
    };
}

#endif //_TVG_PAINT_H_