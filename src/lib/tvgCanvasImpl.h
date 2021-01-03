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
#ifndef _TVG_CANVAS_IMPL_H_
#define _TVG_CANVAS_IMPL_H_

#include "tvgPaint.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Canvas::Impl
{
    Array<Paint*> paints;
    RenderMethod*  renderer;

    Impl(RenderMethod* pRenderer):renderer(pRenderer)
    {
    }

    ~Impl()
    {
        clear(true);
        delete(renderer);
    }

    Result push(unique_ptr<Paint> paint)
    {
        auto p = paint.release();
        if (!p) return Result::MemoryCorruption;
        paints.push(p);

        return update(p);
    }

    Result clear(bool free)
    {
        if (!renderer) return Result::InsufficientCondition;

        //Clear render target before drawing
        if (!renderer->clear()) return Result::InsufficientCondition;

        //free paints
        if (free) {
            for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
                (*paint)->pImpl->dispose(*renderer);
                delete(*paint);
            }
        }

        paints.clear();

        return Result::Success;
    }

    Result update(Paint* paint)
    {
        if (!renderer) return Result::InsufficientCondition;

        Array<RenderData> clips;

        //Update single paint node
        if (paint) {
            paint->pImpl->update(*renderer, nullptr, 255, clips, RenderUpdateFlag::None);
        //Update all retained paint nodes
        } else {
            for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
                (*paint)->pImpl->update(*renderer, nullptr, 255, clips, RenderUpdateFlag::None);
            }
        }
        return Result::Success;
    }

    Result draw()
    {
        if (!renderer) return Result::InsufficientCondition;

        if (!renderer->preRender()) return Result::InsufficientCondition;

        for (auto paint = paints.data; paint < (paints.data + paints.count); ++paint) {
            if (!(*paint)->pImpl->render(*renderer, 255)) return Result::InsufficientCondition;
        }

        if (!renderer->postRender()) return Result::InsufficientCondition;

        return Result::Success;
    }
};

#endif /* _TVG_CANVAS_IMPL_H_ */
