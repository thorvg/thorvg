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
#ifndef _TVG_GLCANVAS_CPP_
#define _TVG_GLCANVAS_CPP_

#include "tvgCommon.h"
#include "tvgGlRenderer.h"
#include "tvgCanvasImpl.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct GlCanvas::Impl
{
    Impl() {}
};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

GlCanvas::GlCanvas() : Canvas(GlRenderer::inst()), pImpl(make_unique<Impl>())
{
}


GlCanvas::~GlCanvas()
{
}


int GlCanvas::sync() noexcept
{
    return 0;
}


unique_ptr<GlCanvas> GlCanvas::gen() noexcept
{
    auto canvas = unique_ptr<GlCanvas>(new GlCanvas);
    assert(canvas);

    return canvas;
}


#endif /* _TVG_GLCANVAS_CPP_ */
