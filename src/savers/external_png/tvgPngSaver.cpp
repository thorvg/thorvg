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

#include <cstring>
#include <memory>
#include <vector>
#include "tvgStr.h"
#include "tvgPngSaver.h"

#include <png.h>

#define WIDTH_8K 7680
#define HEIGHT_8K 4320
#define SIZE_8K 33177600	// WIDTH_8K x HEIGHT_8K

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
namespace tvg
{

void _buildPng(const string& fileName, const uint32_t width, const uint32_t height, uint32_t* buffer)
{
#ifdef THORVG_FILE_IO_SUPPORT
	png_image pngimg;
    memset(&pngimg, 0, sizeof(pngimg));
    pngimg.version = PNG_IMAGE_VERSION;
    pngimg.opaque  = NULL;
    pngimg.width   = width;
    pngimg.height  = height;
    pngimg.format  = PNG_FORMAT_RGBA;

	const png_int_32 row_stride = static_cast<png_int_32>(width) * 4;

    if (!png_image_write_to_file(&pngimg, fileName.c_str(), 0, static_cast<const void*>(buffer), row_stride, nullptr)) {
		TVGERR("PNG_SAVER", "encoder error %s", pngimg.message);
	}
	png_image_free(&pngimg);
#endif // THORVG_FILE_IO_SUPPORT
}

void PngSaver::run(unsigned tid)
{
	auto canvas = std::unique_ptr<SwCanvas>(SwCanvas::gen());
	if (!canvas) return;

	auto w = static_cast<uint32_t>(vsize[0]);
	auto h = static_cast<uint32_t>(vsize[1]);

	buffer = tvg::realloc<uint32_t*>(buffer, sizeof(uint32_t) * w * h);
	canvas->target(buffer, w, w, h, ColorSpace::ABGR8888S);
	if (bg) {
		canvas->push(bg);
	}
	canvas->push(target);

	canvas->update();
	if (canvas->draw(true) == tvg::Result::Success) {
		canvas->sync();
	}

	_buildPng(path, w, h, buffer);
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

PngSaver::~PngSaver()
{
	close();
}

bool PngSaver::close()
{
	this->done();

	if (bg) bg->unref();
	bg = nullptr;

	if (target) target->unref();
	target = nullptr;

	tvg::free(path);
	path = nullptr;

	tvg::free(buffer);
	buffer = nullptr;

	return true;
}

bool PngSaver::save(Paint* paint, Paint* bg, const char* filename, TVG_UNUSED uint32_t quality)
{
	if (paint == nullptr) return false;

	close();

	auto x = 0.0f, y = 0.0f, fw = 0.0f, fh = 0.0f;
	paint->bounds(&x, &y, &fw, &fh);

	// cut off the negative space
	if (x < 0) fw += x;
	if (y < 0) fh += y;

	auto w = static_cast<uint32_t>(ceilf(fw));
	auto h = static_cast<uint32_t>(ceilf(fh));

	if (w < FLOAT_EPSILON || h < FLOAT_EPSILON) {
		TVGLOG("PNG_SAVER", "Saving png(%p) has zero view size.", paint);
		return false;
	}

	if (w * h > SIZE_8K && paint->type() == tvg::Type::Picture) {
		float scale = fw / fh;
		if (scale > 1) {
			w = WIDTH_8K;
			h = static_cast<uint32_t>(w / scale);
		}
		else {
			h = HEIGHT_8K;
			w = static_cast<uint32_t>(h * scale);
		}
		TVGLOG(
			"PNG_SAVER",
			"Warning: The Picture width and/or height values exceed the 8k resolution. To avoid the heap overflow, the "
			"conversion to the PNG file made in  %dx%d resolution",
			w, h);
		static_cast<tvg::Picture*>(paint)->size(static_cast<float>(w), static_cast<float>(h));
	}
	vsize[0] = w;
	vsize[1] = h;

	if (!filename) return false;
	this->path = duplicate(filename);
	this->target = paint;
	this->target->ref();

	if (bg) {
		bg->ref();
		this->bg = bg;
	}

	TaskScheduler::request(this);

	return true;
}

bool PngSaver::save(Animation* animation, Paint* bg, const char* filename, TVG_UNUSED uint32_t quality, uint32_t fps)
{
	TVGLOG("PNG_SAVER", "Animation is not supported.");
	return false;
}

}	 // namespace tvg
