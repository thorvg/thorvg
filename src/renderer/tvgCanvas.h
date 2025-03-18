/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#ifndef _TVG_CANVAS_H_
#define _TVG_CANVAS_H_

#include "tvgPaint.h"

enum Status : uint8_t {Synced = 0, Updating, Drawing, Damaged};

struct Canvas::Impl
{
    Scene* scene;
    RenderMethod* renderer;
    RenderRegion vport = {0, 0, INT32_MAX, INT32_MAX};
    Status status = Status::Synced;

    Impl() : scene(Scene::gen())
    {
        scene->ref();
    }

    ~Impl()
    {
        //make it sure any deferred jobs
        renderer->sync();

        scene->unref();
        if (renderer->unref() == 0) delete(renderer);
    }

    Result push(Paint* target, Paint* at)
    {
        //You cannot push paints during rendering.
        if (status == Status::Drawing) return Result::InsufficientCondition;

        auto ret = scene->push(target, at);
        if (ret != Result::Success) return ret;

        return update(target, true);
    }

    Result remove(Paint* paint)
    {
        if (status == Status::Drawing) return Result::InsufficientCondition;
        return scene->remove(paint);
    }

    Result update(Paint* paint, bool force)
    {
        Array<RenderData> clips;
        auto flag = RenderUpdateFlag::None;
        if (status == Status::Damaged || force) flag = RenderUpdateFlag::All;

        auto m = tvg::identity();

        if (!renderer->preUpdate()) return Result::InsufficientCondition;

        if (paint) PAINT(paint)->update(renderer, m, clips, 255, flag);
        else PAINT(scene)->update(renderer, m, clips, 255, flag);

        if (!renderer->postUpdate()) return Result::InsufficientCondition;

        status = Status::Updating;
        return Result::Success;
    }

    Result draw(bool clear)
    {
        if (status == Status::Drawing) return Result::InsufficientCondition;

        if (clear && !renderer->clear()) return Result::InsufficientCondition;

        if (scene->paints().empty()) return Result::InsufficientCondition;

        if (status == Status::Damaged) update(nullptr, false);

        if (!renderer->preRender()) return Result::InsufficientCondition;

        if (!PAINT(scene)->render(renderer) || !renderer->postRender()) return Result::InsufficientCondition;

        status = Status::Drawing;

        return Result::Success;
    }

    Result sync()
    {
        if (status == Status::Synced || status == Status::Damaged) return Result::InsufficientCondition;

        if (renderer->sync()) {
            status = Status::Synced;
            return Result::Success;
        }

        return Result::Unknown;
    }

    Result viewport(int32_t x, int32_t y, int32_t w, int32_t h)
    {
        if (status != Status::Damaged && status != Status::Synced) return Result::InsufficientCondition;

        RenderRegion val = {x, y, w, h};
        //intersect if the target buffer is already set.
        auto surface = renderer->mainSurface();
        if (surface && surface->w > 0 && surface->h > 0) {
            val.intersect({0, 0, (int32_t)surface->w, (int32_t)surface->h});
        }
        if (vport == val) return Result::Success;
        renderer->viewport(val);
        vport = val;
        status = Status::Damaged;
        return Result::Success;
    }
};

#endif /* _TVG_CANVAS_H_ */
