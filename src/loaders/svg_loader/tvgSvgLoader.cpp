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
#ifndef _TVG_SVG_LOADER_CPP_
#define _TVG_SVG_LOADER_CPP_

#include "tvgSvgLoader.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/



/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SvgLoader::SvgLoader()
{
    cout << "SvgLoader()" << endl;
}


SvgLoader::~SvgLoader()
{
    cout << "~SvgLoader()" << endl;
}


bool SvgLoader::open(const char* path)
{
    //TODO:
    cout << "SvgLoader::open()" << endl;

    return false;
}


bool SvgLoader::read()
{
    //TODO:
    cout << "SvgLoader::read()" << endl;

    return false;
}


bool SvgLoader::close()
{
    //TODO:
    cout << "SvgLoader::close()" << endl;

    return false;
}


unique_ptr<Scene> SvgLoader::data()
{
    //TODO:
    cout << "SvgLoader::data()" << endl;

    return nullptr;
}

#endif //_TVG_SVG_LOADER_CPP_