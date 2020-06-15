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
#ifndef _TVG_INITIALIZER_CPP_
#define _TVG_INITIALIZER_CPP_

#include "tvgCommon.h"
#include "tvgSwRenderer.h"
#include "tvgGlRenderer.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Result Initializer::init(CanvasEngine engine) noexcept
{
    if (engine == CanvasEngine::Sw) {
        if (!SwRenderer::init()) return Result::InsufficientCondition;
    } else if (engine == CanvasEngine::Gl) {
        if (!GlRenderer::init()) return Result::InsufficientCondition;
    } else {
        return Result::InvalidArguments;
    }

    //TODO: check modules then enable them
    //1. TVG
    //2. SVG

    return Result::Success;
}


Result Initializer::term(CanvasEngine engine) noexcept
{
    //TODO: deinitialize modules

    if (engine == CanvasEngine::Sw) {
        if (!SwRenderer::term()) return Result::InsufficientCondition;
    } else if (engine == CanvasEngine::Gl) {
        if (!GlRenderer::term()) return Result::InsufficientCondition;
    } else {
        return Result::InvalidArguments;
    }

    return Result::Success;
}

#endif /* _TVG_INITIALIZER_CPP_ */
