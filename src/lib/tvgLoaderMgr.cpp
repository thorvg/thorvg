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

#ifdef THORVG_PNG_LOADER_SUPPORT
    #include "tvgPngLoader.h"
#endif

#include "tvgRawLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static int initCnt = 0;


static Loader* _find(FileType type)
{
    switch(type) {
        case FileType::Svg: {
#ifdef THORVG_SVG_LOADER_SUPPORT
            return new SvgLoader;
#endif
            break;
        }
        case FileType::Png: {
#ifdef THORVG_PNG_LOADER_SUPPORT
            return new PngLoader;
#endif
            break;
        }
        case FileType::Raw: {
            return new RawLoader;
            break;
        }
        default: {
            break;
        }
    }
    return nullptr;
}


static Loader* _find(const string& path)
{
    auto ext = path.substr(path.find_last_of(".") + 1);
    if (!ext.compare("svg")) return _find(FileType::Svg);
    if (!ext.compare("png")) return _find(FileType::Png);
    return nullptr;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


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


unique_ptr<Loader> LoaderMgr::loader(const string& path)
{
    if (auto loader = _find(path)) {
        if (loader->open(path)) return unique_ptr<Loader>(loader);
        else delete(loader);
    }
    return nullptr;
}


unique_ptr<Loader> LoaderMgr::loader(const char* data, uint32_t size)
{
    for (int i = 0; i < static_cast<int>(FileType::Unknown); i++) {
        auto loader = _find(static_cast<FileType>(i));
        if (loader) {
            if (loader->open(data, size)) return unique_ptr<Loader>(loader);
            else delete(loader);
        }
    }
    return nullptr;
}


unique_ptr<Loader> LoaderMgr::loader(const uint32_t *data, uint32_t w, uint32_t h, bool copy)
{
    for (int i = 0; i < static_cast<int>(FileType::Unknown); i++) {
        auto loader = _find(static_cast<FileType>(i));
        if (loader) {
            if (loader->open(data, w, h, copy)) return unique_ptr<Loader>(loader);
            else delete(loader);
        }
    }
    return nullptr;
}
