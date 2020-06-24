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
#ifndef _TVG_SVG_LOADER_H_
#define _TVG_SVG_LOADER_H_

#include "tvgSvgLoaderCommon.h"

class SvgLoader : public Loader
{
private:
    string content;
    SvgLoaderData loaderData;

public:
    SvgLoader();
    ~SvgLoader();

    bool open(const char* path) override;
    bool read() override;
    bool close() override;
    unique_ptr<Scene> data() override;
};


#endif //_TVG_SVG_LOADER_H_
