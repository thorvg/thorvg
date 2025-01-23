/*
 * Copyright (c) 2024 - 2025 the ThorVG project. All rights reserved.
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

#include "tvgWebpLoader.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#ifdef THORVG_MODULE_SUPPORT
#define WEBP_MODULE_PLUGIN_PATH ("loaders/external_webp/" WEBP_MODULE_PLUGIN)

void WebpLoader::init() {
    TVGLOG("DYLIB", "WebpLoader::init()");
    
    #ifdef _WIN32
    webpDecodeBgra = reinterpret_cast<webp_decode_bgra_f>(GetProcAddress(dl_handle, "webp_decode_bgra"));
    webpGetInfo = reinterpret_cast<webp_get_info_f>(GetProcAddress(dl_handle, "webp_get_info"));
    webpFree = reinterpret_cast<webp_free_f>(GetProcAddress(dl_handle, "webp_free"));
    #else
    webpDecodeBgra = reinterpret_cast<webp_decode_bgra_f>(dlsym(dl_handle, "webp_decode_bgra"));
    webpGetInfo = reinterpret_cast<webp_get_info_f>(dlsym(dl_handle, "webp_get_info"));
    webpFree = reinterpret_cast<webp_free_f>(dlsym(dl_handle, "webp_free"));
    #endif
}

bool WebpLoader::moduleLoad() {
    TVGLOG("DYLIB", "WebpLoader::moduleLoad()");
    #ifdef _WIN32
    dl_handle = LoadLibrary(WEBP_MODULE_PLUGIN_PATH);
    #else
    dl_handle = dlopen(WEBP_MODULE_PLUGIN_PATH, RTLD_LAZY);
    #endif
    return (dl_handle == nullptr);
}

void WebpLoader::moduleFree() {
    if (!dl_handle) return;

    TVGLOG("DYLIB", "WebpLoader::moduleFree()");

    #ifdef _WIN32
    FreeLibrary(dl_handle);
    #else
    dlclose(dl_handle);
    #endif
}
#endif

void WebpLoader::clear()
{
    if (freeData) free(data);
    data = nullptr;
    freeData = false;
}


void WebpLoader::run(unsigned tid)
{
    if (surface.cs == ColorSpace::ARGB8888 || surface.cs == ColorSpace::ARGB8888S) {
        surface.buf8 = webpDecodeBgra(data, size, nullptr, nullptr);
        surface.cs = ColorSpace::ARGB8888;
    } else  {
        surface.buf8 = webpDecodeBgra(data, size, nullptr, nullptr);
        surface.cs = ColorSpace::ABGR8888;
    }

    surface.stride = static_cast<uint32_t>(w);
    surface.w = static_cast<uint32_t>(w);
    surface.h = static_cast<uint32_t>(h);
    surface.channelSize = sizeof(uint32_t);
    surface.premultiplied = true;

    clear();
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

WebpLoader::WebpLoader() : ImageLoader(FileType::Webp)
{
#ifdef THORVG_MODULE_SUPPORT
    if (moduleLoad()) {
        TVGERR("DYLIB", "WebpLoader::WebpLoader() : moduleLoad() failed");
        return;
    }

    init();

    if (!webpDecodeBgra) TVGERR("DYLIB", "WebpLoader::webpDecodeBgra() : can't find symbol");
    if (!webpGetInfo) TVGERR("DYLIB", "WebpLoader::webpGetInfo() : can't find symbol");
    if (!webpFree) TVGERR("DYLIB", "WebpLoader::webpFree() : can't find symbol");
#else
    webpDecodeBgra = webp_decode_bgra;
    webpGetInfo = webp_get_info;
    webpFree = webp_free;
#endif
}


WebpLoader::~WebpLoader()
{
    clear();
    free(surface.buf8);

#ifdef THORVG_MODULE_SUPPORT
    moduleFree();
#endif
}


bool WebpLoader::open(const char* path)
{
#ifdef THORVG_FILE_IO_SUPPORT
    auto f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);

    size = ftell(f);
    if (size == 0) {
        fclose(f);
        return false;
    }

    data = (uint8_t*)malloc(size);

    fseek(f, 0, SEEK_SET);
    auto ret = fread(data, sizeof(char), size, f);
    if (ret < size) {
        fclose(f);
        return false;
    }

    fclose(f);

    int width, height;
    if (!webpGetInfo(data, size, &width, &height)) return false;

    w = static_cast<float>(width);
    h = static_cast<float>(height);
    freeData = true;

    return true;
#else
    return false;
#endif
}


bool WebpLoader::open(const char* data, uint32_t size, TVG_UNUSED const char* rpath, bool copy)
{
    if (copy) {
        this->data = (uint8_t*) malloc(size);
        if (!this->data) return false;
        memcpy((uint8_t*)this->data, data, size);
        freeData = true;
    } else {
        this->data = (uint8_t*) data;
        freeData = false;
    }

    int width, height;
    if (!webpGetInfo(this->data, size, &width, &height)) return false;

    w = static_cast<float>(width);
    h = static_cast<float>(height);
    this->size = size;

    return true;
}


bool WebpLoader::read()
{
    if (!LoadModule::read()) return true;

    if (!data || w == 0 || h == 0) return false;

    surface.cs = this->cs;

    TaskScheduler::request(this);

    return true;
}


bool WebpLoader::close()
{
    if (!LoadModule::close()) return false;
    this->done();
    return true;
}


RenderSurface* WebpLoader::bitmap()
{
    this->done();

    return ImageLoader::bitmap();
}