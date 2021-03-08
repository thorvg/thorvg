#include <fstream>
#include <string>
#include "tvgLoaderMgr.h"
#include "tvgPngLoader.h"
// #include "png.h"

PngLoader::~PngLoader()
{
    if (content) {
        free((void*)content);
        content = nullptr;
    }
}

bool PngLoader::open(const string& path)
{
    png_image image;
    image.version = PNG_IMAGE_VERSION;
    image.opaque = NULL;

    if (!png_image_begin_read_from_file(&image, path.c_str())) return false;
    w = image.width;
    h = image.height;
    vw = w;
    vh = h;
    png_bytep buffer;
    image.format = PNG_FORMAT_BGRA;
    buffer = static_cast<png_bytep>(malloc(PNG_IMAGE_SIZE(image)));
    if (!buffer) {
        // out of memory, only time when libpng doesnt free its data
        png_image_free(&image);
        return false;
    }
    if (!png_image_finish_read(&image, NULL, buffer, 0, NULL)) return false;
    content = reinterpret_cast<uint32_t*>(buffer);
    return true;
}

bool PngLoader::read()
{
    return true;
}

bool PngLoader::close()
{
    return true;
}

const uint32_t* PngLoader::pixels()
{
    return this->content;
}
