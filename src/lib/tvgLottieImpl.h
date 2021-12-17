/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All rights reserved.

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
#ifndef _TVG_LOTTIE_IMPL_H_
#define _TVG_LOTTIE_IMPL_H_

#include <string>
#include "tvgRender.h"
#include "tvgPictureImpl.h"
#include "tvgAnimationImpl.h"
#include "thorvg_lottie.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct LottieIterator : Iterator
{
    Paint* paint = nullptr;
    Paint* ptr = nullptr;

    LottieIterator(Paint* p) : paint(p) {}

    const Paint* next() override
    {
        if (!ptr) ptr = paint;
        else ptr = nullptr;
        return ptr;
    }

    uint32_t count() override
    {
        if (paint) return 1;
        else return 0;
    }

    void begin() override
    {
        ptr = nullptr;
    }
};


struct Lottie::Impl
{
    Picture::Impl *pictureImpl = nullptr;
    Animation::Impl *animationImpl = nullptr;

    ~Impl()
    {
    }

    bool dispose(RenderMethod& renderer)
    {
        return pictureImpl->dispose(renderer);
    }

    void* update(RenderMethod &renderer, const RenderTransform* pTransform, uint32_t opacity, Array<RenderData>& clips, RenderUpdateFlag pFlag)
    {
        auto flag = RenderUpdateFlag::None;

        //Do update frame
        if ( pictureImpl->loader->frame != animationImpl->frameNum) {
            flag = RenderUpdateFlag::Image;
            pictureImpl->loader->frame = animationImpl->frameNum;

            if (!pictureImpl->loader->read()) return nullptr;
        }
        //
        return pictureImpl->update(renderer, pTransform, opacity, clips, flag);
    }

    bool render(RenderMethod &renderer)
    {
        return pictureImpl->render(renderer);
    }

    bool bounds(float* x, float* y, float* w, float* h)
    {
        return pictureImpl->bounds(x, y, w, h);
    }

    RenderRegion bounds(RenderMethod& renderer)
    {
        return pictureImpl->bounds(renderer);
    }

    Result load(const string& path)
    {
        Result ret = pictureImpl->load(path);
        if (pictureImpl->loader)
          animationImpl->totalFrameNum =pictureImpl->loader->totalFrame;
        return ret;
    }

    Paint* duplicate()
    {
        auto ret = Lottie::gen();
        auto dup = ret.get()->pImpl;

        if (pictureImpl->paint) dup->pictureImpl->paint = pictureImpl->paint->duplicate(); 


        dup->pictureImpl->loader = pictureImpl->loader;
        if (pictureImpl->surface) {
            dup->pictureImpl->surface = static_cast<Surface*>(malloc(sizeof(Surface)));
            *dup->pictureImpl->surface = *(pictureImpl->surface);
        }
        dup->pictureImpl->w = pictureImpl->w;
        dup->pictureImpl->h = pictureImpl->h;
        dup->pictureImpl->resizing = pictureImpl->resizing;


        dup->animationImpl->frameNum = animationImpl->frameNum;
        dup->animationImpl->totalFrameNum = animationImpl->totalFrameNum;

        return ret.release();
    }

    Iterator* iterator()
    {
        return new LottieIterator(pictureImpl->paint);
    }
};

#endif //_TVG_LOTTIE_IMPL_H_
