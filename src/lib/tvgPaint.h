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
#ifndef _TVG_PAINT_H_
#define _TVG_PAINT_H_

#include <float.h>
#include <math.h>
#include "tvgRender.h"

namespace tvg
{
    struct StrategyMethod
    {
        virtual ~StrategyMethod(){}

        virtual bool dispose(RenderMethod& renderer) = 0;
        virtual bool update(RenderMethod& renderer, const RenderTransform* transform, RenderUpdateFlag pFlag) = 0;
        virtual bool render(RenderMethod& renderer) = 0;
        virtual bool bounds(float* x, float* y, float* w, float* h) const = 0;
        virtual Paint* duplicate() = 0;
        virtual bool composite(unique_ptr<Paint> comp, CompMethod method) = 0;
        virtual bool composite(Paint* comp, CompMethod method) = 0;
        virtual Paint* composite() = 0;
        virtual CompMethod compositeMethod() = 0;
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

        Matrix transform()
        {
            if (rTransform) {
                rTransform->update();
                return rTransform->m;
            }
            return {1, 0, 0, 0, 1, 0, 0, 0, 1};
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

        Paint* duplicate()
        {
            auto ret = smethod->duplicate();

            //duplicate Transform
            if (ret && rTransform) {
                ret->pImpl->rTransform = new RenderTransform();
                if (ret->pImpl->rTransform) {
                    *ret->pImpl->rTransform = *rTransform;
                    ret->pImpl->flag |= RenderUpdateFlag::Transform;
                }
            }

            return ret;
        }

        bool composite(unique_ptr<Paint> comp, CompMethod method) const
        {
            return smethod->composite(move(comp), method);
        }

        bool composite(Paint* comp, CompMethod method) const
        {
            return smethod->composite(comp, method);
        }

        Paint* composite() const
        {
            return smethod->composite();
        }

        CompMethod compositeMethod() const
        {
            return smethod->compositeMethod();
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

        bool dispose(RenderMethod& renderer) override
        {
            return inst->dispose(renderer);
        }

        bool update(RenderMethod& renderer, const RenderTransform* transform, RenderUpdateFlag flag) override
        {
            return inst->update(renderer, transform, flag);
        }

        bool render(RenderMethod& renderer) override
        {
            return inst->render(renderer);
        }

        Paint* duplicate() override
        {
            return inst->duplicate();
        }

        bool composite(unique_ptr<Paint> comp, CompMethod method) override
        {
            return inst->composite(move(comp), method);
        }

        bool composite(Paint* comp, CompMethod method) override
        {
            return inst->composite(comp, method);
        }

        Paint* composite() override
        {
            return inst->composite();
        }

        CompMethod compositeMethod() override
        {
            return inst->compositeMethod();
        }
    };
}

#endif //_TVG_PAINT_H_
