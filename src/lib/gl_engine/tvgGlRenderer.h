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

namespace tvg
{

class GlRenderer : public RenderMethod
{
public:
    void* prepare(const Shape& shape, void* data, const RenderMatrix* transform, RenderUpdateFlag flags) override;
    bool dispose(const Shape& shape, void *data) override;
    bool render(const Shape& shape, void *data) override;
    bool clear() override;
    size_t ref() override;
    size_t unref() override;

    static GlRenderer* inst();
    static int init();
    static int term();

private:
    GlRenderer(){};
    ~GlRenderer(){};
};

}

#endif /* _TVG_GL_RENDERER_H_ */
