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
#ifndef _TVG_SVG_SCENE_BUILDER_H_
#define _TVG_SVG_SCENE_BUILDER_H_

#include "tvgSvgLoaderCommon.h"

class SvgSceneBuilder
{
private:
    struct {
        int x, y;
        uint32_t w, h;
    } viewBox;
    int      ref;
    uint32_t w, h;                 //Default size
    bool     staticViewBox;
    bool     preserveAspect;       //Used in SVG
    bool     shareable;

public:
    SvgSceneBuilder();
    ~SvgSceneBuilder();

    unique_ptr<Scene> build(SvgNode* node);
};

#endif //_TVG_SVG_SCENE_BUILDER_H_
