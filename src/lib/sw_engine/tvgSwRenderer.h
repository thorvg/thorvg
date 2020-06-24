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
#ifndef _TVG_SW_RENDERER_H_
#define _TVG_SW_RENDERER_H_

#include <queue>
#include <future>
#include <thread>

namespace tvg
{

struct SwTask;

class SwRenderer : public RenderMethod
{
public:
    void* prepare(const Shape& shape, void* data, const RenderTransform* transform, RenderUpdateFlag flags) override;
    bool dispose(const Shape& shape, void *data) override;
    bool preRender() override;
    bool render(const Shape& shape, void *data) override;
    bool postRender() override;
    bool target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h);
    bool clear() override;
    bool flush() override;
    uint32_t ref() override;
    uint32_t unref() override;

    static SwRenderer* inst();
    static int init();
    static int term();

    void doRender();  //Internally used for threading

private:
    Surface surface;
    future<void> progress;
    queue<SwTask*> prepareTasks;
    queue<SwTask*> renderTasks;

    SwRenderer(){};
    ~SwRenderer();
};

}

#endif /* _TVG_SW_RENDERER_H_ */
