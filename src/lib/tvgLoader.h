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
#ifndef _TVG_LOADER_H_
#define _TVG_LOADER_H_

namespace tvg
{

class Loader
{
public:
    //default view box, if any.
    float vx = 0;
    float vy = 0;
    float vw = 0;
    float vh = 0;

    virtual ~Loader() {}

    virtual bool open(const char* path) = 0;
    virtual bool read() = 0;
    virtual bool close() = 0;
    virtual unique_ptr<Scene> data() = 0;
};

}

#endif //_TVG_LOADER_H_