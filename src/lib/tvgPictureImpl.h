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
#ifndef _TVG_PICTURE_IMPL_H_
#define _TVG_PICTURE_IMPL_H_

#include <string>
#include "tvgPaint.h"
#include "tvgLoaderMgr.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Picture::Impl
{
    unique_ptr<Loader> loader = nullptr;
    Paint* paint = nullptr;
    uint32_t *pixels = nullptr;
    Picture *picture = nullptr;
    void *rdata = nullptr;              //engine data
    float w = 0, h = 0;
    bool resizing = false;

    Impl(Picture* p) : picture(p)
    {
    }

    bool dispose(RenderMethod& renderer)
    {
        if (paint) {
            paint->pImpl->dispose(renderer);
            delete(paint);

            return true;
        }
        else if (pixels) {
            return renderer.dispose(rdata);
        }
        return false;
    }

    void resize()
    {
        auto sx = w / loader->vw;
        auto sy = h / loader->vh;

        if (loader->preserveAspect) {
            //Scale
            auto scale = sx < sy ? sx : sy;
            paint->scale(scale);
            //Align
            auto vx = loader->vx * scale;
            auto vy = loader->vy * scale;
            auto vw = loader->vw * scale;
            auto vh = loader->vh * scale;
            if (vw > vh) vy -= (h - vh) * 0.5f;
            else vx -= (w - vw) * 0.5f;
            paint->translate(-vx, -vy);
        } else {
            //Align
            auto vx = loader->vx * sx;
            auto vy = loader->vy * sy;
            auto vw = loader->vw * sx;
            auto vh = loader->vh * sy;
            if (vw > vh) vy -= (h - vh) * 0.5f;
            else vx -= (w - vw) * 0.5f;

            Matrix m = {sx, 0, -vx, 0, sy, -vy, 0, 0, 1};
            paint->transform(m);
        }
        resizing = false;
    }

    uint32_t reload()
    {
        if (loader) {
            if (!paint) {
                auto scene = loader->scene();
                if (scene) {
                    paint = scene.release();
                    loader->close();
                    if (w != loader->w && h != loader->h) resize();
                    if (paint) return RenderUpdateFlag::None;
                }
            }
            if (!pixels) {
                pixels = const_cast<uint32_t*>(loader->pixels());
                if (pixels) return RenderUpdateFlag::Image;
            }
        }
        return RenderUpdateFlag::None;
    }

    void* update(RenderMethod &renderer, const RenderTransform* transform, uint32_t opacity, Array<RenderData>& clips, RenderUpdateFlag pFlag)
    {
        auto flag = reload();

        if (pixels) rdata = renderer.prepare(*picture, rdata, transform, opacity, clips, static_cast<RenderUpdateFlag>(pFlag | flag));
        else if (paint) {
            if (resizing) resize();
            rdata = paint->pImpl->update(renderer, transform, opacity, clips, static_cast<RenderUpdateFlag>(pFlag | flag));
        }
        return rdata;
    }

    bool render(RenderMethod &renderer, uint32_t opacity)
    {
        if (pixels) return renderer.renderImage(rdata);
        else if (paint) return paint->pImpl->render(renderer, opacity);
        return false;
    }

    bool viewbox(float* x, float* y, float* w, float* h)
    {
        if (!loader) return false;
        if (x) *x = loader->vx;
        if (y) *y = loader->vy;
        if (w) *w = loader->vw;
        if (h) *h = loader->vh;
        return true;
    }

    bool size(uint32_t w, uint32_t h)
    {
        this->w = w;
        this->h = h;
        resizing = true;
        return true;
    }

    bool bounds(float* x, float* y, float* w, float* h)
    {
        if (!paint) return false;
        return paint->pImpl->bounds(x, y, w, h);
    }

    bool bounds(RenderMethod& renderer, uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h)
    {
        if (rdata) return renderer.region(rdata, x, y, w, h);
        if (paint) return paint->pImpl->bounds(renderer, x, y, w, h);
        return false;
    }

    Result load(const string& path)
    {
        if (loader) loader->close();
        loader = LoaderMgr::loader(path);
        if (!loader) return Result::NonSupport;
        if (!loader->read()) return Result::Unknown;
        w = loader->w;
        h = loader->h;
        return Result::Success;
    }

    Result load(const char* data, uint32_t size)
    {
        if (loader) loader->close();
        loader = LoaderMgr::loader(data, size);
        if (!loader) return Result::NonSupport;
        if (!loader->read()) return Result::Unknown;
        w = loader->w;
        h = loader->h;
        return Result::Success;
    }

    Result load(uint32_t* data, uint32_t w, uint32_t h, bool copy)
    {
        if (loader) loader->close();
        loader = LoaderMgr::loader(data, w, h, copy);
        if (!loader) return Result::NonSupport;
        return Result::Success;
    }

    Paint* duplicate()
    {
        reload();

        if (!paint) return nullptr;
        auto ret = Picture::gen();
        if (!ret) return nullptr;
        auto dup = ret.get()->pImpl;
        dup->paint = paint->duplicate();

        return ret.release();
    }
};

#endif //_TVG_PICTURE_IMPL_H_
