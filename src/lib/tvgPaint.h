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
    struct StrategyMethod
    {
        virtual ~StrategyMethod(){}

        virtual bool dispose(RenderMethod& renderer) = 0;
        virtual bool update(RenderMethod& renderer, const RenderTransform* pTransform, uint32_t pFlag) = 0;
        virtual bool render(RenderMethod& renderer) = 0;

        virtual bool rotate(float degree) = 0;
        virtual bool scale(float factor) = 0;
        virtual bool translate(float x, float y) = 0;
        virtual bool transform(const Matrix& m) = 0;
        virtual bool bounds(float* x, float* y, float* w, float* h) const = 0;

    };

    struct Paint::Impl
    {
        StrategyMethod* smethod = nullptr;

        ~Impl() {
            if (smethod) delete(smethod);
        }

        void method(StrategyMethod* method)
        {
            smethod = method;
        }

        StrategyMethod* method()
        {
            return smethod;
        }
    };


    template<class T>
    struct PaintMethod : StrategyMethod
    {
        T* inst = nullptr;

        PaintMethod(T* _inst) : inst(_inst) {}
        ~PaintMethod(){}

        bool rotate(float degree) override
        {
            return inst->rotate(degree);
        }

        bool scale(float factor) override
        {
            return inst->scale(factor);
        }

        bool translate(float x, float y) override
        {
            return inst->translate(x, y);
        }

        bool transform(const Matrix& m) override
        {
            return inst->transform(m);
        }

        bool bounds(float* x, float* y, float* w, float* h) const override
        {
            return inst->bounds(x, y, w, h);
        }

        bool dispose(RenderMethod& renderer)
        {
            return inst->dispose(renderer);
        }

        bool update(RenderMethod& renderer, const RenderTransform* pTransform, uint32_t pFlag)
        {
            return inst->update(renderer, pTransform, pFlag);
        }

        bool render(RenderMethod& renderer)
        {
            return inst->render(renderer);
        }
    };
}

#endif //_TVG_PAINT_H_