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
        virtual ~StrategyMethod() {}

        virtual bool dispose(RenderMethod& renderer) = 0;
        virtual void* update(RenderMethod& renderer, const RenderTransform* transform, uint32_t opacity, Array<RenderData>& clips, RenderUpdateFlag pFlag) = 0;   //Return engine data if it has.
        virtual bool render(RenderMethod& renderer, uint32_t opacity) = 0;
        virtual bool bounds(float* x, float* y, float* w, float* h) const = 0;
        virtual bool bounds(RenderMethod& renderer, uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h) const = 0;
        virtual Paint* duplicate() = 0;
    };

    struct Paint::Impl
    {
        StrategyMethod* smethod = nullptr;
        RenderTransform *rTransform = nullptr;
        uint32_t flag = RenderUpdateFlag::None;

        Paint* cmpTarget = nullptr;
        CompositeMethod cmpMethod = CompositeMethod::None;

        uint8_t opacity = 255;

        ~Impl() {
            if (cmpTarget) delete(cmpTarget);
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

        bool bounds(RenderMethod& renderer, uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h) const
        {
            return smethod->bounds(renderer, x, y, w, h);
        }

        bool dispose(RenderMethod& renderer)
        {
            if (cmpTarget) cmpTarget->pImpl->dispose(renderer);
            return smethod->dispose(renderer);
        }

        void* update(RenderMethod& renderer, const RenderTransform* pTransform, uint32_t opacity, Array<RenderData>& clips, uint32_t pFlag)
        {
            if (flag & RenderUpdateFlag::Transform) {
                if (!rTransform) return nullptr;
                if (!rTransform->update()) {
                    delete(rTransform);
                    rTransform = nullptr;
                }
            }

            void *cmpData = nullptr;

            if (cmpTarget) {
                cmpData = cmpTarget->pImpl->update(renderer, pTransform, 255, clips, pFlag);
                if (cmpMethod == CompositeMethod::ClipPath) clips.push(cmpData);
            }

            void *edata = nullptr;
            auto newFlag = static_cast<RenderUpdateFlag>(pFlag | flag);
            flag = RenderUpdateFlag::None;
            opacity = (opacity * this->opacity) / 255;

            if (rTransform && pTransform) {
                RenderTransform outTransform(pTransform, rTransform);
                edata = smethod->update(renderer, &outTransform, opacity, clips, newFlag);
            } else {
                auto outTransform = pTransform ? pTransform : rTransform;
                edata = smethod->update(renderer, outTransform, opacity, clips, newFlag);
            }

            if (cmpData) clips.pop();

            return edata;
        }

        bool render(RenderMethod& renderer, uint32_t opacity)
        {
            Compositor* cmp = nullptr;

            /* Note: only ClipPath is processed in update() step.
               Create a composition image. */
            if (cmpTarget && cmpMethod != CompositeMethod::ClipPath) {
                uint32_t x, y, w, h;
                if (!cmpTarget->pImpl->bounds(renderer, &x, &y, &w, &h)) return false;
                cmp = renderer.target(x, y, w, h);
                renderer.beginComposite(cmp, CompositeMethod::None, 255);
                cmpTarget->pImpl->render(renderer, 255);
            }

            if (cmp) renderer.beginComposite(cmp, CompositeMethod::AlphaMask, cmpTarget->pImpl->opacity);

            auto ret = smethod->render(renderer, ((opacity * this->opacity) / 255));

            if (cmp) renderer.endComposite(cmp);

            return ret;
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

            ret->pImpl->opacity = opacity;

            return ret;
        }

        bool composite(Paint* target, CompositeMethod method)
        {
            if (!target && method != CompositeMethod::None) return false;
            if (target && method == CompositeMethod::None) return false;
            cmpTarget = target;
            cmpMethod = method;
            return true;
        }
    };


    template<class T>
    struct PaintMethod : StrategyMethod
    {
        T* inst = nullptr;

        PaintMethod(T* _inst) : inst(_inst) {}
        ~PaintMethod() {}

        bool bounds(float* x, float* y, float* w, float* h) const override
        {
            return inst->bounds(x, y, w, h);
        }

        bool bounds(RenderMethod& renderer, uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h) const override
        {
            return inst->bounds(renderer, x, y, w, h);
        }

        bool dispose(RenderMethod& renderer) override
        {
            return inst->dispose(renderer);
        }

        void* update(RenderMethod& renderer, const RenderTransform* transform, uint32_t opacity, Array<RenderData>& clips, RenderUpdateFlag flag) override
        {
            return inst->update(renderer, transform, opacity, clips, flag);
        }

        bool render(RenderMethod& renderer, uint32_t opacity) override
        {
            return inst->render(renderer, opacity);
        }

        Paint* duplicate() override
        {
            return inst->duplicate();
        }
    };
}

#endif //_TVG_PAINT_H_
