/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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

#ifndef _TVG_WEBP_LOADER_H_
#define _TVG_WEBP_LOADER_H_

#include "tvgLoader.h"
#include "tvgTaskScheduler.h"

#ifdef _WIN32
# include <windows.h>
#else
# include <dlfcn.h>
#endif  // _WIN32

// Dynamic function pointer types
using webp_decode_bgra_f = uint8_t* (*)(const uint8_t* data, size_t data_size, int* width, int* height);
using webp_get_info_f = int (*)(const uint8_t* data, size_t data_size, int* width, int* height);
using webp_free_f = void (*)(void* ptr);


#ifdef __cplusplus
extern "C" {
#endif

// Dynamic library wrapper functions
extern uint8_t* webp_decode_bgra(const uint8_t* data, size_t data_size, int* width, int* height);
extern int      webp_get_info(const uint8_t* data, size_t data_size, int* width, int* height);
extern void     webp_free(void* ptr);

#ifdef __cplusplus
}
#endif


class WebpLoader : public ImageLoader, public Task
{
public:
    WebpLoader();
    ~WebpLoader();

    bool open(const char* path) override;
    bool open(const char* data, uint32_t size, const char* rpath, bool copy) override;
    bool read() override;

    RenderSurface* bitmap() override;

private:
    void run(unsigned tid) override;

    unsigned char* data = nullptr;
    unsigned long size = 0;
    bool freeData = false;

#ifdef THORVG_MODULE_SUPPORT
    // Dynamic Loader implementation
    #ifdef _WIN32
    HMODULE dl_handle{nullptr};
    #else
    void *dl_handle{nullptr};
    #endif
    void init();
    bool moduleLoad();
    void moduleFree();
#endif

    webp_decode_bgra_f webpDecodeBgra{nullptr};
    webp_get_info_f    webpGetInfo{nullptr};
    webp_free_f        webpFree{nullptr};
};

#endif //_TVG_WEBP_LOADER_H_
