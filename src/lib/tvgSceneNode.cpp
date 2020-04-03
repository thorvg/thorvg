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
#ifndef _TVG_SCENE_NODE_CPP_
#define _TVG_SCENE_NODE_CPP_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct SceneNode::Impl
{

};



/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SceneNode :: SceneNode() : pImpl(make_unique<Impl>())
{

}


SceneNode :: ~SceneNode()
{
    cout << "SceneNode(" << this << ") destroyed!" << endl;
}


unique_ptr<SceneNode> SceneNode::gen() noexcept
{
    return unique_ptr<SceneNode>(new SceneNode);
}


int SceneNode :: push(unique_ptr<ShapeNode> shape) noexcept
{
    return 0;
}


int SceneNode :: update(RasterMethod* engine) noexcept
{

    return 0;
}

#endif /* _TVG_SCENE_NODE_CPP_ */
