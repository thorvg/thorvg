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

class SwRenderer : public RenderMethod
{
public:
    void* prepare(const ShapeNode& shape, void* data, UpdateFlag flags) override;
    void* dispose(const ShapeNode& shape, void *data) override;
    size_t ref() override;
    size_t unref() override;

    static SwRenderer* inst();
    static int init();
    static int term();

private:
    SwRenderer(){};
    ~SwRenderer(){};
};

#endif /* _TVG_SW_RENDERER_H_ */
