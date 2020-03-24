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

#include <iostream>
#include <cassert>
#include "tizenvg.h"

using namespace std;
using namespace tizenvg;

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
namespace tizenvg
{

class EngineImpl
{
 public:
    static int init() {
        return 0;
    }

    static int term() {
        return 0;
    }
};

}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

int Engine::init() noexcept
{
    return EngineImpl::init();
}

int Engine::term() noexcept
{
    return EngineImpl::term();
}

#endif /* _TVG_ENGINE_CPP_ */
