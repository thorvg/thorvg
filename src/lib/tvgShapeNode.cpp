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
#ifndef _TVG_SHAPE_NODE_CPP_
#define _TVG_SHAPE_NODE_CPP_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct ShapeFill
{
};


struct ShapeStroke
{
};


struct ShapePath
{
};


struct ShapeTransform
{
    var e[4*4];
};


struct ShapeChildren
{
    vector<unique_ptr<ShapeNode>> v;
};


struct ShapeNode::Impl
{
    ShapeChildren *children = nullptr;
    ShapeTransform *transform = nullptr;
    ShapeFill *fill = nullptr;
    ShapeStroke *stroke = nullptr;
    ShapePath *path = nullptr;
    uint32_t color = 0;
    uint32_t id = 0;


    ~Impl()
    {
        if (path) delete(path);
        if (stroke) delete(stroke);
        if (fill) delete(fill);
        if (transform) delete(transform);
        if (children) delete(children);
    }

};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

ShapeNode :: ShapeNode() : pImpl(make_unique<Impl>())
{

}


ShapeNode :: ~ShapeNode()
{
    cout << "ShapeNode(" << this << ") destroyed!" << endl;
}


unique_ptr<ShapeNode> ShapeNode::gen()
{
    return unique_ptr<ShapeNode>(new ShapeNode);
}


int ShapeNode :: prepare() noexcept
{
    return 0;
}


#endif //_TVG_SHAPE_NODE_CPP_
