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
#ifndef _TVG_GL_RENDERER_H_
#define _TVG_GL_RENDERER_H_

#include "tvgGlCommon.h"

namespace tvg
{

class GlRenderer : public RenderMethod
{
public:
    Surface surface;

    void* prepare(const Shape& shape, void* data, const RenderTransform* transform, RenderUpdateFlag flags) override;
    bool dispose(const Shape& shape, void *data) override;
    bool render(const Shape& shape, void *data) override;
    bool target(uint32_t* buffer, size_t stride, size_t w, size_t h)
    {
        return 0;
    };
    bool clear() override;
    size_t ref() override;
    size_t unref() override;

    static GlRenderer* inst();
    static int init();
    static int term();

private:
    GlRenderer(){};
    ~GlRenderer(){};

    std::unique_ptr<GlProgram>   mColorProgram;
    int32_t   mColorUniform;
    uint32_t  mVertexAttrID;
};

}

#endif /* _TVG_GL_RENDERER_H_ */
