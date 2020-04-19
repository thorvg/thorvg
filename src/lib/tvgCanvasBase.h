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
#ifndef _TVG_CANVAS_BASE_CPP_
#define _TVG_CANVAS_BASE_CPP_

#include "tvgCommon.h"


struct CanvasBase
{
    vector<PaintNode*> nodes;
    RenderMethod*      renderer;

    CanvasBase(RenderMethod *pRenderer):renderer(pRenderer)
    {
        renderer->ref();
    }

    ~CanvasBase()
    {
       clear();
       renderer->unref();
    }

    int reserve(size_t n)
    {
        nodes.reserve(n);

        return 0;
    }

    int clear()
    {
        assert(renderer);

        auto ret = 0;

        for (auto node : nodes) {
            if (SceneNode *scene = dynamic_cast<SceneNode *>(node)) {
                cout << "TODO: " <<  scene << endl;
            } else if (ShapeNode *shape = dynamic_cast<ShapeNode *>(node)) {
                ret |= renderer->dispose(*shape, shape->engine());
            }
            delete(node);
        }
        nodes.clear();

        return ret;
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

    int push(unique_ptr<PaintNode> paint)
    {
        PaintNode *node = paint.release();
        assert(node);
        nodes.push_back(node);
        return node->update(renderer);
    }

    int draw()
    {
        assert(renderer);

        auto ret = 0;

        for(auto node: nodes) {
            if (SceneNode *scene = dynamic_cast<SceneNode *>(node)) {
                cout << "TODO: " <<  scene << endl;
            } else if (ShapeNode *shape = dynamic_cast<ShapeNode *>(node)) {
                ret |= renderer->render(*shape, shape->engine());
            }
        }
        return ret;
    }

};

#endif /* _TVG_CANVAS_BASE_CPP_ */
