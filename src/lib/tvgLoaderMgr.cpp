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
#ifndef _TVG_LOADER_MGR_CPP_
#define _TVG_LOADER_MGR_CPP_

#include "tvgCommon.h"

#ifdef THORVG_SVG_LOADER_SUPPORT
    #include "tvgSvgLoader.h"
#endif

static int initCnt = 0;

bool LoaderMgr::init()
{
    if (initCnt > 0) return true;
    ++initCnt;

    //TODO:

    return true;
}

bool LoaderMgr::term()
{
    --initCnt;
    if (initCnt > 0) return true;

    //TODO:

    return true;
}

unique_ptr<Loader> LoaderMgr::loader(const char* path)
{
#ifdef THORVG_SVG_LOADER_SUPPORT
    return unique_ptr<SvgLoader>(new SvgLoader);
#endif
    cout << "Non supported format: " << path << endl;
    return unique_ptr<Loader>(nullptr);
}

#endif //_TVG_LOADER_MGR_CPP_