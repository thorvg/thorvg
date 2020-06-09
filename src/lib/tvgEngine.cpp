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
#ifndef _TVG_ENGINE_CPP_
#define _TVG_ENGINE_CPP_

#include "tvgCommon.h"
#include "tvgSwRenderer.h"
#include "tvgGlRenderer.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Result Engine::init() noexcept
{
    //TODO: Initialize Raster engines by configuration.

    int ret = 0;
    ret |= SwRenderer::init();
    ret |= GlRenderer::init();

    return Result::Success;
}


Result Engine::term() noexcept
{
    int ret = 0;
    ret |= SwRenderer::term();
    ret |= GlRenderer::term();

    return Result::Success;
}

#endif /* _TVG_ENGINE_CPP_ */
