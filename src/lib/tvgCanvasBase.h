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

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct CanvasBase
{
    vector<PaintNode*> nodes;
    RasterMethod*      raster;

    CanvasBase(RasterMethod *pRaster):raster(pRaster)
    {

    }

    ~CanvasBase()
    {
       clear();
    }

    int reserve(size_t n)
    {
        nodes.reserve(n);

        return 0;
    }

    int clear()
    {
        for (auto node : nodes) {
            delete(node);
        }
        nodes.clear();

        return 0;
    }

    int push(unique_ptr<PaintNode> paint)
    {
        PaintNode *node = paint.release();
        assert(node);

        nodes.push_back(node);
#if 0
        if (SceneNode *scene = dynamic_cast<SceneNode *>(node)) {

        } else if (ShapeNode *shape = dynamic_cast<ShapeNode *>(node)) {
            return shape->update(raster);
        }
#else
        if (ShapeNode *shape = dynamic_cast<ShapeNode *>(node)) {
            return shape->update(raster);
        }
#endif
        cout << "What type of PaintNode? = " << node << endl;

        return -1;
    }

};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

#endif /* _TVG_CANVAS_BASE_CPP_ */
