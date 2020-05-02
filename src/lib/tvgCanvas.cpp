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


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Canvas::Impl
{
    vector<PaintNode*> nodes;
    RenderMethod*      renderer;

    Impl(RenderMethod *pRenderer):renderer(pRenderer)
    {
        renderer->ref();
    }

    ~Impl()
    {
        clearPaints();
        renderer->unref();
    }

    int reserve(size_t n)
    {
        nodes.reserve(n);

        return 0;
    }

    int push(unique_ptr<PaintNode> paint)
    {
        PaintNode *node = paint.release();
        assert(node);
        nodes.push_back(node);
        return node->update(renderer);
    }

    bool clearRender()
    {
        assert(renderer);
        return renderer->clear());
    }

    int clearPaints()
    {
        assert(renderer);

        for (auto node : nodes) {
            if (SceneNode *scene = dynamic_cast<SceneNode *>(node)) {
                cout << "TODO: " <<  scene << endl;
            } else if (ShapeNode *shape = dynamic_cast<ShapeNode *>(node)) {
                if (!renderer->dispose(*shape, shape->engine())) return -1;
            }
            delete(node);
        }
        nodes.clear();

        return 0;
    }

    int update()
    {
        assert(renderer);

        auto ret = 0;

        for(auto node: nodes) {
            ret |= node->update(renderer);
        }

        return ret;
    }

    int draw()
    {
        assert(renderer);

        for(auto node: nodes) {
            if (SceneNode *scene = dynamic_cast<SceneNode *>(node)) {
                cout << "TODO: " <<  scene << endl;
            } else if (ShapeNode *shape = dynamic_cast<ShapeNode *>(node)) {
                if (!renderer->render(*shape, shape->engine())) return -1;
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
    return impl->reserve(n);
}


int Canvas::push(unique_ptr<PaintNode> paint) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    return impl->push(move(paint));
}


int Canvas::clear(bool clearPaints) noexcept
{
    auto impl = pImpl.get();
    assert(impl);
    auto ret = 0;
    if (clearPaints) ret |= impl->clearPaints();
    ret |= impl->clearRender();
    return ret;
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


RenderMethod* Canvas::engine() noexcept
{
    auto impl = pImpl.get();
    assert(impl);
    return impl->renderer;
}

#endif /* _TVG_CANVAS_CPP_ */
