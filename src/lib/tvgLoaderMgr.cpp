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
#include "tvgLoaderMgr.h"

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

unique_ptr<Loader> findLoaderByType(FileType type)
{
    switch (type) {
        case FileType::Svg :
#ifdef THORVG_SVG_LOADER_SUPPORT
            return unique_ptr<SvgLoader>(new SvgLoader);
#endif
            break;
        default:
            break;
    }
    return nullptr;
}

unique_ptr<Loader> findLoaderByExt(const string& path)
{
    string ext = path.substr(path.find_last_of(".") + 1);
    if (!ext.compare("svg")) {
        return findLoaderByType(FileType::Svg);
    }
    return nullptr;
}

unique_ptr<Loader> LoaderMgr::loader(const string& path)
{
    unique_ptr<Loader> loader = nullptr;
    loader = findLoaderByExt(path);
    if (loader && loader->open(path.c_str())) {
        return loader;
    }
    return nullptr;
}

unique_ptr<Loader> LoaderMgr::loader(const char* data, uint32_t size)
{
    unique_ptr<Loader> loader = nullptr;
    for (int i = 0; i < static_cast<int>(FileType::Unknown); i++) {
        loader = findLoaderByType(static_cast<FileType>(i));
        if (loader && loader->open(data, size)) {
            return loader;
        }
    }
    return nullptr;
}
