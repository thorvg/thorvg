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
#ifndef _TVG_CANVAS_CPP_
#define _TVG_CANVAS_CPP_

#include "tvgCommon.h"
#include "tvgShapeImpl.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Canvas::Impl
{
    vector<Paint*> paints;
    RenderMethod*  renderer;

    Impl(RenderMethod* pRenderer):renderer(pRenderer)
    {
        renderer->ref();
    }

    ~Impl()
    {
        clear();
        renderer->unref();
    }

    int push(unique_ptr<Paint> paint)
    {
        Paint* pPaint = paint.release();
        assert(pPaint);
        paints.push_back(pPaint);

        return update(pPaint);
    }

    int clear()
    {
        assert(renderer);

        for (auto paint : paints) {
            if (auto scene = dynamic_cast<Scene*>(paint)) {
                cout << "TODO: " <<  scene << endl;
            } else if (auto shape = dynamic_cast<Shape*>(paint)) {
                auto p = shape->pImpl.get();
                assert(p);
                if (!p->dispose(*shape, *renderer)) return -1;
            }
            delete(paint);
        }
        paints.clear();

        return 0;
    }

    int update()
    {
        assert(renderer);

        for(auto paint: paints) {
            if (auto scene = dynamic_cast<Scene*>(paint)) {
                cout << "TODO: " <<  scene << endl;
            } else if (auto shape = dynamic_cast<Shape*>(paint)) {
                auto p = shape->pImpl.get();
                assert(p);
                if (!p->update(*shape, *renderer)) return -1;
            }
        }

        return 0;
    }

    int update(Paint* paint)
    {
        assert(renderer);

        if (auto scene = dynamic_cast<Scene*>(paint)) {
            cout << "TODO: " <<  scene << endl;
        } else if (auto shape = dynamic_cast<Shape*>(paint)) {
            auto p = shape->pImpl.get();
            assert(p);
            if (!p->update(*shape, *renderer)) return -1;
        }

        return 0;
    }

    int draw()
    {
        assert(renderer);

        //Clear render target before drawing
        if (!renderer->clear()) return -1;

        for(auto paint: paints) {
            if (auto scene = dynamic_cast<Scene*>(paint)) {
                cout << "TODO: " <<  scene << endl;
            } else if (auto shape = dynamic_cast<Shape*>(paint)) {
                auto p = shape->pImpl.get();
                assert(p);
                if(!p->render(*shape, *renderer)) return -1;
            }
        }
        return 0;
    }

};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Canvas::Canvas(RenderMethod *pRenderer):pImpl(make_unique<Impl>(pRenderer))
{
}


Canvas::~Canvas()
{
}


int Canvas::reserve(size_t n) noexcept
{
    auto impl = pImpl.get();
    assert(impl);
    impl->paints.reserve(n);
    return 0;
}


int Canvas::push(unique_ptr<Paint> paint) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    return impl->push(move(paint));
}


int Canvas::clear() noexcept
{
    auto impl = pImpl.get();
    assert(impl);
    return impl->clear();
}


int Canvas::draw(bool async) noexcept
{
    auto impl = pImpl.get();
    assert(impl);
    return impl->draw();
}


int Canvas::update() noexcept
{
    auto impl = pImpl.get();
    assert(impl);
    return impl->update();
}


int Canvas::update(Paint* paint) noexcept
{
    auto impl = pImpl.get();
    assert(impl);
    return impl->update(paint);
}

#endif /* _TVG_CANVAS_CPP_ */
