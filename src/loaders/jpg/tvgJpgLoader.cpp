/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.

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
#include <turbojpeg.h>
#include "tvgLoaderMgr.h"
#include "tvgJpgLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void JpgLoader::clear()
{
    if (freeData) free(data);
    data = nullptr;
    size = 0;
    freeData = false;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

JpgLoader::JpgLoader()
{
    jpegDecompressor = tjInitDecompress();
}


JpgLoader::~JpgLoader()
{
    if (freeData) free(data);
    tjDestroy(jpegDecompressor);

    //This image is shared with raster engine.
    tjFree(image);
}


bool JpgLoader::open(const string& path)
{
    clear();

    auto jpegFile = fopen(path.c_str(), "rb");
    if (!jpegFile) return false;

    auto ret = false;

    //determine size
    if (fseek(jpegFile, 0, SEEK_END) < 0) goto finalize;
    if (((size = ftell(jpegFile)) < 1)) goto finalize;
    if (fseek(jpegFile, 0, SEEK_SET)) goto finalize;

    data = (unsigned char *) malloc(size);
    if (!data) goto finalize;

    if (fread(data, size, 1, jpegFile) < 1) goto failure;

    //FIXME: Decompress header here

    ret = true;
    freeData = true;

    goto finalize;

failure:
    clear();

finalize:
    fclose(jpegFile);
    return ret;
}


bool JpgLoader::open(const char* data, uint32_t size, bool copy)
{
    clear();

    if (copy) {
        this->data = (unsigned char *) malloc(size);
        if (!this->data) return false;
        memcpy((unsigned char *)this->data, data, size);
        freeData = true;
    } else {
        this->data = (unsigned char *) data;
    }

    this->size = size;

    //FIXME: Decompress header here

    return true;
}


bool JpgLoader::read()
{
    //decompress header
    int width, height;
    int inSubsamp, inColorspace;
    if (tjDecompressHeader3(jpegDecompressor, data, size, &width, &height, &inSubsamp, &inColorspace) < 0) return false;

    if (image) tjFree(image);
    image = (unsigned char *)tjAlloc(width * height * tjPixelSize[TJPF_BGRX]);
    if (!image) return false;

    //decompress jpg image
    if (tjDecompress2(jpegDecompressor, data, size, image, width, 0, height, TJPF_BGRX, 0) < 0) {
        tjFree(image);
        image = nullptr;
        return false;
    }

    vw = w = width;
    vh = h = height;

    return true;
}


bool JpgLoader::close()
{
    clear();
    return true;
}


const uint32_t* JpgLoader::pixels()
{
    return (const uint32_t*) image;
}
