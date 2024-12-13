/*
 * Copyright (c) 2020 - 2024 the ThorVG project. All rights reserved.

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

#include "tvgRender.h"
#include "tvgMath.h"

#define PAINT(A) PIMPL(A, Paint)

namespace tvg
{
    enum ContextFlag : uint8_t {Default = 0, FastTrack = 1};

    struct Iterator
    {
        virtual ~Iterator() {}
        virtual const Paint* next() = 0;
        virtual uint32_t count() = 0;
        virtual void begin() = 0;
    };

    struct Mask
    {
        Paint* target;
        Paint* source;
        MaskMethod method;
    };

    struct Paint::Impl
    {
        Paint* paint = nullptr;
        Mask* maskData = nullptr;
        Paint* clipper = nullptr;
        RenderMethod* renderer = nullptr;
        struct {
            Matrix m;                 //input matrix
            Matrix cm;                //multipled parents matrix
            float degree;             //rotation degree
            float scale;              //scale factor
            bool overriding;          //user transform?

            void update()
            {
                if (overriding) return;
                m.e11 = 1.0f;
                m.e12 = 0.0f;
                m.e21 = 0.0f;
                m.e22 = 1.0f;
                m.e31 = 0.0f;
                m.e32 = 0.0f;
                m.e33 = 1.0f;
                tvg::scale(&m, scale, scale);
                tvg::rotate(&m, degree);
            }
        } tr;
        BlendMethod blendMethod;
        uint8_t renderFlag;
        uint8_t ctxFlag;
        uint8_t opacity;
        uint8_t refCnt = 0;       //reference count

        Impl(Paint* pnt) : paint(pnt)
        {
            reset();
        }

        virtual ~Impl()
        {
            if (maskData) {
                maskData->target->unref();
                free(maskData);
            }
            if (clipper) clipper->unref();
            if (renderer && (renderer->unref() == 0)) delete(renderer);
        }

        uint8_t ref()
        {
            if (refCnt == UINT8_MAX) TVGERR("RENDERER", "Reference Count Overflow!");
            else ++refCnt;
            return refCnt;
        }

        uint8_t unref(bool free)
        {
            if (refCnt > 0) --refCnt;
            else TVGERR("RENDERER", "Corrupted Reference Count!");

            if (free && refCnt == 0) {
                //TODO: use the global dismiss function?
                delete(this);
                return 0;
            }

            return refCnt;
        }

        void update(RenderUpdateFlag flag)
        {
            renderFlag |= flag;
        }

        bool transform(const Matrix& m)
        {
            if (&tr.m != &m) tr.m = m;
            tr.overriding = true;
            renderFlag |= RenderUpdateFlag::Transform;

            return true;
        }

        Matrix& transform(bool origin = false)
        {
            //update transform
            if (renderFlag & RenderUpdateFlag::Transform) tr.update();
            if (origin) return tr.cm;
            return tr.m;
        }

        void clip(Paint* clp)
        {
            if (clipper) clipper->unref(clipper != clp);
            clipper = clp;
            if (!clp) return;

            clipper->ref();
        }

        bool mask(Paint* target, MaskMethod method)
        {
            //Invalid case
            if ((!target && method != MaskMethod::None) || (target && method == MaskMethod::None)) return false;

            if (maskData) {
                maskData->target->unref(maskData->target != target);
                //Reset scenario
                if (!target && method == MaskMethod::None) {
                    free(maskData);
                    maskData = nullptr;
                    return true;
                }
            } else {
                if (!target && method == MaskMethod::None) return true;
                maskData = static_cast<Mask*>(malloc(sizeof(Mask)));
            }
            target->ref();
            maskData->target = target;
            maskData->source = paint;
            maskData->method = method;
            return true;
        }

        MaskMethod mask(const Paint** target) const
        {
            if (maskData) {
                if (target) *target = maskData->target;
                return maskData->method;
            } else {
                if (target) *target = nullptr;
                return MaskMethod::None;
            }
        }

        void reset()
        {
            if (clipper) {
                clipper->unref();
                clipper = nullptr;
            }

            if (maskData) {
                maskData->target->unref();
                free(maskData);
                maskData = nullptr;
            }

            tvg::identity(&tr.m);
            tr.degree = 0.0f;
            tr.scale = 1.0f;
            tr.overriding = false;

            blendMethod = BlendMethod::Normal;
            renderFlag = RenderUpdateFlag::None;
            ctxFlag = ContextFlag::Default;
            opacity = 255;
            paint->id = 0;
        }

        bool rotate(float degree)
        {
            if (tr.overriding) return false;
            if (tvg::equal(degree, tr.degree)) return true;
            tr.degree = degree;
            renderFlag |= RenderUpdateFlag::Transform;

            return true;
        }

        bool scale(float factor)
        {
            if (tr.overriding) return false;
            if (tvg::equal(factor, tr.scale)) return true;
            tr.scale = factor;
            renderFlag |= RenderUpdateFlag::Transform;

            return true;
        }

        bool translate(float x, float y)
        {
            if (tr.overriding) return false;
            if (tvg::equal(x, tr.m.e13) && tvg::equal(y, tr.m.e23)) return true;
            tr.m.e13 = x;
            tr.m.e23 = y;
            renderFlag |= RenderUpdateFlag::Transform;

            return true;
        }

        void blend(BlendMethod method)
        {
            if (blendMethod != method) {
                blendMethod = method;
                renderFlag |= RenderUpdateFlag::Blend;
            }
        }

        RenderRegion bounds(RenderMethod* renderer) const;
        Iterator* iterator();
        bool bounds(float* x, float* y, float* w, float* h, bool transformed, bool stroking, bool origin = false);
        RenderData update(RenderMethod* renderer, const Matrix& pm, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag pFlag, bool clipper = false);
        bool render(RenderMethod* renderer);
        Paint* duplicate(Paint* ret = nullptr);
    };
}

#endif //_TVG_PAINT_H_