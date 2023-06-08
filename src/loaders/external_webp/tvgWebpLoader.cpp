/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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

#include <memory.h>
#include <webp/decode.h>

#include "tvgLoader.h"
#include "tvgWebpLoader.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void WebpLoader::clear()
{
    if (freeData) free(data);
    data = nullptr;
    size = 0;
    freeData = false;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

WebpLoader::WebpLoader()
{
}


WebpLoader::~WebpLoader()
{
    if (freeData) free(data);
    WebPFree(image);
}


bool WebpLoader::open(const string& path)
{
    clear();

    auto webpFile = fopen(path.c_str(), "rb");
    if (!webpFile) return false;

    auto ret = false;

    //determine size
    if (fseek(webpFile, 0, SEEK_END) < 0) goto finalize;
    if (((size = ftell(webpFile)) < 1)) goto finalize;
    if (fseek(webpFile, 0, SEEK_SET)) goto finalize;

    data = (unsigned char *) malloc(size);
    if (!data) goto finalize;

    freeData = true;

    if (fread(data, size, 1, webpFile) < 1) goto failure;

    int width, height;
    if (!WebPGetInfo(data, size, &width, &height)) goto failure;

    w = static_cast<float>(width);
    h = static_cast<float>(height);
    cs = ColorSpace::ARGB8888;

    ret = true;

    goto finalize;

failure:
    clear();

finalize:
    fclose(webpFile);
    return ret;
}


bool WebpLoader::open(const char* data, uint32_t size, bool copy)
{
    clear();

    if (copy) {
        this->data = (unsigned char *) malloc(size);
        if (!this->data) return false;
        memcpy((unsigned char *)this->data, data, size);
        freeData = true;
    } else {
        this->data = (unsigned char *) data;
        freeData = false;
    }

    int width, height;
    if (!WebPGetInfo(this->data, size, &width, &height)) return false;

    w = static_cast<float>(width);
    h = static_cast<float>(height);
    cs = ColorSpace::ARGB8888;
    this->size = size;
    return true;
}


bool WebpLoader::read()
{
    if (!data || w <= 0 || h <= 0) return false;

    TaskScheduler::request(this);
    return true;
}


bool WebpLoader::close()
{
    this->done();
    clear();
    return true;
}


unique_ptr<Surface> WebpLoader::bitmap()
{
    this->done();

    if (!image) return nullptr;

    //TODO: It's better to keep this surface instance in the loader side
    auto surface = new Surface;
    surface->buf8 = image;
    surface->stride = static_cast<uint32_t>(w);
    surface->w = static_cast<uint32_t>(w);
    surface->h = static_cast<uint32_t>(h);
    surface->cs = cs;
    surface->channelSize = sizeof(uint32_t);
    surface->premultiplied = false;
    surface->owner = true;
    return unique_ptr<Surface>(surface);
}


void WebpLoader::run(unsigned tid)
{
    if (image) {
        WebPFree(image);
        image = nullptr;
    }

    image = WebPDecodeBGRA(data, size, nullptr, nullptr);
}
