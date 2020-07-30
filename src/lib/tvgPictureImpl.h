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
#include "tvgSceneImpl.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Picture::Impl
{
    unique_ptr<Loader> loader = nullptr;
    Scene* scene = nullptr;
    RenderTransform *rTransform = nullptr;
    uint32_t flag = RenderUpdateFlag::None;

    ~Impl()
    {
        if (rTransform) delete(rTransform);
    }

    bool dispose(RenderMethod& renderer)
    {
        if (!scene) return false;

        scene->IMPL->dispose(renderer);
        delete(scene);

        return true;
    }

    bool update(RenderMethod &renderer, const RenderTransform* pTransform, uint32_t pFlag)
    {
        if (loader) {
            auto scene = loader->data();
            if (scene) {
                this->scene = scene.release();
                if (!this->scene) return false;
                loader->close();
            }
        }

        if (!scene) return false;

        if (flag & RenderUpdateFlag::Transform) {
            if (!rTransform) return false;
            if (!rTransform->update()) {
                delete(rTransform);
                rTransform = nullptr;
            }
        }

        auto ret = true;

        if (rTransform && pTransform) {
            RenderTransform outTransform(pTransform, rTransform);
            ret = scene->IMPL->update(renderer, &outTransform, flag);
        } else {
            auto outTransform = pTransform ? pTransform : rTransform;
            ret = scene->IMPL->update(renderer, outTransform, flag);
        }

        flag = RenderUpdateFlag::None;

        return ret;
    }

    bool render(RenderMethod &renderer)
    {
        if (!scene) return false;
        return scene->IMPL->render(renderer);
    }


    bool size(float* w, float* h)
    {
        if (loader) {
            if (w) *w = loader->vw;
            if (h) *h = loader->vh;
            return true;
        }

        return false;
    }

    bool bounds(float* x, float* y, float* w, float* h)
    {
        if (!scene) return false;
        return scene->IMPL->bounds(x, y, w, h);
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
        flag |= RenderUpdateFlag::Transform;

        return true;
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
        flag |= RenderUpdateFlag::Transform;

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
        flag |= RenderUpdateFlag::Transform;

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