/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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


#include "tvgTtfLoader.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
#endif

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#ifdef _WIN32

static bool _map(TtfLoader* loader, const string& path)
{
    auto& reader = loader->reader;

	auto file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE) return false;

	DWORD high;
	auto low = GetFileSize(file, &high);
	if (low == INVALID_FILE_SIZE) {
		CloseHandle(file);
		return false;
	}

	reader.size = (uint32_t)((size_t)high << (8 * sizeof(DWORD)) | low);

	loader->mapping = (uint8_t*)CreateFileMapping(file, NULL, PAGE_READONLY, high, low, NULL);

	CloseHandle(file);

	if (!loader->mapping) return false;

	loader->reader.data = (uint8_t*) MapViewOfFile(loader->mapping, FILE_MAP_READ, 0, 0, 0);
	if (!loader->reader.data) {
		CloseHandle(loader->mapping);
		loader->mapping = nullptr;
		return false;
	}
	return true;
}

static void _unmap(TtfLoader* loader)
{
    auto& reader = loader->reader;

	if (reader.data) {
		UnmapViewOfFile(reader.data);
		reader.data = nullptr;
	}
	if (loader->mapping) {
		CloseHandle(loader->mapping);
		loader->mapping = nullptr;
	}
}

#else

static bool _map(TtfLoader* loader, const string& path)
{
    auto& reader = loader->reader;

    auto fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;

    struct stat info;
    if (fstat(fd, &info) < 0) {
        close(fd);
        return false;
    }

    reader.data = (uint8_t*)mmap(NULL, (size_t) info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (reader.data == (uint8_t*)-1) return false;
    reader.size = (uint32_t) info.st_size;
    close(fd);

    return true;
}


static void _unmap(TtfLoader* loader)
{
    auto& reader = loader->reader;
    if (reader.data == (uint8_t*) -1) return;
    munmap((void *) reader.data, reader.size);
    reader.data = (uint8_t*)-1;
    reader.size = 0;
}
#endif


static uint32_t* _codepoints(const char* text, size_t n)
{
    uint32_t c;
    size_t i = 0;

    auto utf8 = text;
    //preserve approximate enough space.
    auto utf32 = (uint32_t*) malloc(sizeof(uint32_t) * n + 1);

    while(*utf8) {
        if (!(*utf8 & 0x80U)) {
            utf32[i++] = *utf8++;
        } else if ((*utf8 & 0xe0U) == 0xc0U) {
            c = (*utf8++ & 0x1fU) << 6;
            utf32[i++] = c + (*utf8++ & 0x3fU);
        } else if ((*utf8 & 0xf0U) == 0xe0U) {
            c = (*utf8++ & 0x0fU) << 12;
            c += (*utf8++ & 0x3fU) << 6;
            utf32[i++] = c + (*utf8++ & 0x3fU);
        } else if ((*utf8 & 0xf8U) == 0xf0U) {
            c = (*utf8++ & 0x07U) << 18;
            c += (*utf8++ & 0x3fU) << 12;
            c += (*utf8++ & 0x3fU) << 6;
            c += (*utf8++ & 0x3fU);
            utf32[i++] = c;
        } else {
            free(utf32);
            return nullptr;
        }
    }
    utf32[i - 1] = 0;   //end of the unicdoe
    return utf32;
}


void TtfLoader::clear()
{
    _unmap(this);
    shape = nullptr;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


bool TtfLoader::resize(Paint* paint, float sx, TVG_UNUSED float sy)
{
    if (!paint) return false;
    auto shift = 0.0f;
    auto dpi = 96.0f / 72.0f;   //dpi base?
    scale = sx * dpi / reader.metrics.unitsPerEm;
    if (italic) shift = -scale * 0.18f;  //experimental decision.
    Matrix m = {scale, shift, -(shift * reader.metrics.minw), 0, scale, 0, 0, 0, 1};
    paint->transform(m);

    return true;
}


TtfLoader::TtfLoader() : FontLoader(FileType::Ttf)
{
}


TtfLoader::~TtfLoader()
{
    clear();
}


bool TtfLoader::open(const string& path)
{
    clear();
    if (!_map(this, path)) return false;
    return reader.header();
}


bool TtfLoader::request(Shape* shape, char* text, bool italic)
{
    this->shape = shape;
    this->text = text;
    this->italic = italic;

    return true;
}


bool TtfLoader::read()
{
    if (!text) return false;

    shape->reset();
    shape->fill(FillRule::EvenOdd);

    auto n = strlen(text);
    auto code = _codepoints(text, n);

    //TODO: optimize with the texture-atlas?
    TtfGlyphMetrics gmetrics;
    Point offset = {0.0f, reader.metrics.hhea.ascent};
    Point kerning = {0.0f, 0.0f};
    auto lglyph = INVALID_GLYPH;

    size_t idx = 0;
    while (code[idx] && idx < n) {
        auto rglyph = reader.glyph(code[idx], gmetrics);
        if (rglyph != INVALID_GLYPH) {
            if (lglyph != INVALID_GLYPH) reader.kerning(lglyph, rglyph, kerning);
            if (!reader.convert(shape, gmetrics, offset, kerning)) break;
        }
        offset.x += (gmetrics.advanceWidth + kerning.x);
        lglyph = rglyph;
        //store the first glyph min size for italic transform.
        if (idx == 0) reader.metrics.minw = gmetrics.minw;
        ++idx;
    }

    free(code);

    return true;
}
