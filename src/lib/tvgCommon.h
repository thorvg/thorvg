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
#ifndef _TVG_COMMON_H_
#define _TVG_COMMON_H_

#include <iostream>
#include <cassert>
#include <vector>
#include <math.h>
#include "tizenvg.h"

using namespace std;
using namespace tvg;

namespace tvg
{

class RenderMethod
{
public:
    enum UpdateFlag { None = 0, Path = 1, Fill = 2, All = 3 };
    virtual ~RenderMethod() {}
    virtual void* prepare(const ShapeNode& shape, void* data, UpdateFlag flags) = 0;
    virtual void* dispose(const ShapeNode& shape, void *data) = 0;
    virtual size_t ref() = 0;
    virtual size_t unref() = 0;
};

struct RenderInitializer
{
    RenderMethod* pInst = nullptr;
    size_t refCnt = 0;
    bool initialized = false;

    static int init(RenderInitializer& renderInit, RenderMethod* engine)
    {
        assert(engine);
        if (renderInit.pInst || renderInit.refCnt > 0) return -1;
        renderInit.pInst = engine;
        renderInit.refCnt = 0;
        renderInit.initialized = true;
        return 0;
    }

    static int term(RenderInitializer& renderInit)
    {
        if (!renderInit.pInst || !renderInit.initialized) return -1;

        renderInit.initialized = false;

        //Still it's refered....
        if (renderInit.refCnt > 0) return  0;
        delete(renderInit.pInst);
        renderInit.pInst = nullptr;

        return 0;
    }

    static size_t unref(RenderInitializer& renderInit)
    {
        assert(renderInit.refCnt > 0);
        --renderInit.refCnt;

        //engine has been requested to termination
        if (!renderInit.initialized && renderInit.refCnt == 0) {
            if (renderInit.pInst) {
                delete(renderInit.pInst);
                renderInit.pInst = nullptr;
            }
        }
        return renderInit.refCnt;
    }

    static RenderMethod* inst(RenderInitializer& renderInit)
    {
        assert(renderInit.pInst);
        return renderInit.pInst;
    }

    static size_t ref(RenderInitializer& renderInit)
    {
        return ++renderInit.refCnt;
    }

};

}

#endif //_TVG_COMMON_H_
