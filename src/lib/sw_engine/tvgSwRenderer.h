/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef _TVG_SW_RENDERER_H_
#define _TVG_SW_RENDERER_H_

#include <vector>
#include "tvgRender.h"

struct SwSurface;
struct SwTask;

namespace tvg
{

class SwRenderer : public RenderMethod
{
public:
    void* prepare(const Shape& shape, void* data, const RenderTransform* transform, vector<Composite>& compList, RenderUpdateFlag flags) override;
    bool dispose(const Shape& shape, void *data) override;
    bool preRender() override;
    bool postRender() override;
    bool target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h, uint32_t cs);
    bool clear() override;
    bool render(const Shape& shape, void *data) override;

    static SwRenderer* gen();
    static bool init();
    static bool term();

private:
    SwSurface* surface = nullptr;
    vector<SwTask*> tasks;

    SwRenderer(){};
    ~SwRenderer();
};

}

#endif /* _TVG_SW_RENDERER_H_ */
