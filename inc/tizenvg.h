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
#ifndef _TIZENVG_H_
#define _TIZENVG_H_

#include <memory>

#ifdef TIZENVG_BUILD
    #define TIZENVG_EXPORT __attribute__ ((visibility ("default")))
#else
    #define TIZENVG_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

namespace tizenvg
{

/**
 * @class Engine
 *
 * @ingroup TizenVG
 *
 * @brief description...
 *
 */
class TIZENVG_EXPORT Engine final
{
public:
    /**
     * @brief ...
     *
     * @param[in] arg ...
     *
     * @note ...
     *
     * @return ...
     *
     * @see ...
     */
    static int init() noexcept;
    static int term() noexcept;
};

} //namespace

#ifdef __cplusplus
}
#endif

#endif //_TIZENVG_H_
