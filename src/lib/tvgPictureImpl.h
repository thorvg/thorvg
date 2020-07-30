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
#ifndef _TVG_PICTURE_IMPL_H_
#define _TVG_PICTURE_IMPL_H_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Picture::Impl
{
    unique_ptr<Loader> loader = nullptr;
    Paint* paint = nullptr;

    bool dispose(RenderMethod& renderer)
    {
        if (!paint) return false;

        paint->IMPL->dispose(renderer);
        delete(paint);

        return true;
    }

    bool update(RenderMethod &renderer, const RenderTransform* transform, RenderUpdateFlag flag)
    {
        if (loader) {
            auto scene = loader->data();
            if (scene) {
                this->paint = scene.release();
                if (!this->paint) return false;
                loader->close();
            }
        }

        if (!paint) return false;

        return paint->IMPL->update(renderer, transform, flag);
    }

    bool render(RenderMethod &renderer)
    {
        if (!paint) return false;
        return paint->IMPL->render(renderer);
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

    bool bounds(float* x, float* y, float* w, float* h)
    {
        if (!paint) return false;
        return paint->IMPL->bounds(x, y, w, h);
    }

    Result load(const string& path)
    {
        if (loader) loader->close();
        loader = LoaderMgr::loader(path.c_str());
        if (!loader || !loader->open(path.c_str())) return Result::NonSupport;
        if (!loader->read()) return Result::Unknown;
        return Result::Success;
    }
};

#endif //_TVG_PICTURE_IMPL_H_