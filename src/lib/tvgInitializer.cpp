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
#include "tvgLoaderMgr.h"

#ifdef THORVG_SW_RASTER_SUPPORT
    #include "tvgSwRenderer.h"
#endif

#ifdef THORVG_GL_RASTER_SUPPORT
    #include "tvgGlRenderer.h"
#endif


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Result Initializer::init(CanvasEngine engine) noexcept
{
    auto nonSupport = true;

    if (engine == CanvasEngine::Sw) {
        #ifdef THORVG_SW_RASTER_SUPPORT
            if (!SwRenderer::init()) return Result::InsufficientCondition;
            nonSupport = false;
        #endif
    } else if (engine == CanvasEngine::Gl) {
        #ifdef THORVG_GL_RASTER_SUPPORT
            if (!GlRenderer::init()) return Result::InsufficientCondition;
            nonSupport = false;
        #endif
    } else {
        return Result::InvalidArguments;
    }

    if (nonSupport) return Result::NonSupport;

    if (!LoaderMgr::init()) return Result::Unknown;

    return Result::Success;
}


Result Initializer::term(CanvasEngine engine) noexcept
{
    auto nonSupport = true;

    if (engine == CanvasEngine::Sw) {
        #ifdef THORVG_SW_RASTER_SUPPORT
            if (!SwRenderer::term()) return Result::InsufficientCondition;
            nonSupport = false;
        #endif
    } else if (engine == CanvasEngine::Gl) {
        #ifdef THORVG_GL_RASTER_SUPPORT
            if (!GlRenderer::term()) return Result::InsufficientCondition;
            nonSupport = false;
        #endif
    } else {
        return Result::InvalidArguments;
    }

    if (nonSupport) return Result::NonSupport;

    if (!LoaderMgr::term()) return Result::Unknown;

    return Result::Success;
}

#endif /* _TVG_INITIALIZER_CPP_ */
