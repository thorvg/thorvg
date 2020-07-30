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
        virtual bool update(RenderMethod& renderer, const RenderTransform* transform, RenderUpdateFlag pFlag) = 0;
        virtual bool render(RenderMethod& renderer) = 0;
        virtual bool bounds(float* x, float* y, float* w, float* h) const = 0;
    };

    struct Paint::Impl
    {
        StrategyMethod* smethod = nullptr;
        RenderTransform *rTransform = nullptr;
        uint32_t flag = RenderUpdateFlag::None;

        ~Impl() {
            if (smethod) delete(smethod);
            if (rTransform) delete(rTransform);
        }

        void method(StrategyMethod* method)
        {
            smethod = method;
        }

        bool rotate(float degree)
        {
            if (rTransform) {
                if (fabsf(degree - rTransform->degree) <= FLT_EPSILON) return true;
            } else {
                if (fabsf(degree) <= FLT_EPSILON) return true;
                rTransform = new RenderTransform();
                if (!rTransform) return false;
            }
            rTransform->degree = degree;
            if (!rTransform->overriding) flag |= RenderUpdateFlag::Transform;

            return true;
        }

        bool scale(float factor)
        {
            if (rTransform) {
                if (fabsf(factor - rTransform->scale) <= FLT_EPSILON) return true;
            } else {
                if (fabsf(factor) <= FLT_EPSILON) return true;
                rTransform = new RenderTransform();
                if (!rTransform) return false;
            }
            rTransform->scale = factor;
            if (!rTransform->overriding) flag |= RenderUpdateFlag::Transform;

            return true;
        }

        bool translate(float x, float y)
        {
            if (rTransform) {
                if (fabsf(x - rTransform->x) <= FLT_EPSILON && fabsf(y - rTransform->y) <= FLT_EPSILON) return true;
            } else {
                if (fabsf(x) <= FLT_EPSILON && fabsf(y) <= FLT_EPSILON) return true;
                rTransform = new RenderTransform();
                if (!rTransform) return false;
            }
            rTransform->x = x;
            rTransform->y = y;
            if (!rTransform->overriding) flag |= RenderUpdateFlag::Transform;

            return true;
        }

        bool transform(const Matrix& m)
        {
            if (!rTransform) {
                rTransform = new RenderTransform();
                if (!rTransform) return false;
            }
            rTransform->override(m);
            flag |= RenderUpdateFlag::Transform;

            return true;
        }

        bool bounds(float* x, float* y, float* w, float* h) const
        {
            return smethod->bounds(x, y, w, h);
        }

        bool dispose(RenderMethod& renderer)
        {
            return smethod->dispose(renderer);
        }

        bool update(RenderMethod& renderer, const RenderTransform* pTransform, uint32_t pFlag)
        {
            if (flag & RenderUpdateFlag::Transform) {
                if (!rTransform) return false;
                if (!rTransform->update()) {
                    delete(rTransform);
                    rTransform = nullptr;
                }
            }

            auto newFlag = static_cast<RenderUpdateFlag>(pFlag | flag);
            flag = RenderUpdateFlag::None;

            if (rTransform && pTransform) {
                RenderTransform outTransform(pTransform, rTransform);
                return smethod->update(renderer, &outTransform, newFlag);
            } else {
                auto outTransform = pTransform ? pTransform : rTransform;
                return smethod->update(renderer, outTransform, newFlag);
            }
        }

        bool render(RenderMethod& renderer)
        {
            return smethod->render(renderer);
        }
    };


    template<class T>
    struct PaintMethod : StrategyMethod
    {
        T* inst = nullptr;

        PaintMethod(T* _inst) : inst(_inst) {}
        ~PaintMethod(){}

        bool bounds(float* x, float* y, float* w, float* h) const override
        {
            return inst->bounds(x, y, w, h);
        }

        bool dispose(RenderMethod& renderer)
        {
            return inst->dispose(renderer);
        }

        bool update(RenderMethod& renderer, const RenderTransform* transform, RenderUpdateFlag flag)
        {
            return inst->update(renderer, transform, flag);
        }

        bool render(RenderMethod& renderer)
        {
            return inst->render(renderer);
        }
    };
}

#endif //_TVG_PAINT_H_