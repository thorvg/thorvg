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


#include "tvgStr.h"
#include "tvgTtfLoader.h"

#if defined(_WIN32) && (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
    #include <windows.h>
#elif defined(__linux__)
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
#endif

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#ifdef THORVG_FILE_IO_SUPPORT

#if defined(_WIN32) && (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)

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

#elif defined(__linux__)

static bool _map(TtfLoader* loader, const char* path)
{
    auto& reader = loader->reader;

    auto fd = open(path, O_RDONLY);
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
#else
static bool _map(TtfLoader* loader, const char* path)
{
    auto& reader = loader->reader;

    auto f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);

    reader.size = ftell(f);
    if (reader.size == 0) {
        fclose(f);
        return false;
    }

    reader.data = tvg::malloc<uint8_t*>(reader.size);

    fseek(f, 0, SEEK_SET);
    auto ret = fread(reader.data, sizeof(char), reader.size, f);
    if (ret < reader.size) {
        fclose(f);
        return false;
    }

    fclose(f);

    return true;
}


static void _unmap(TtfLoader* loader)
{
    auto& reader = loader->reader;
    tvg::free(reader.data);
    reader.data = nullptr;
    reader.size = 0;
}
#endif

#endif //THORVG_FILE_IO_SUPPORT


static size_t _codepoints(char* utf8, uint32_t& code)
{
    if (!(*utf8 & 0x80U)) {
        code = *utf8;
        return 1;
    } else if ((*utf8 & 0xe0U) == 0xc0U) {
        code = ((*utf8 & 0x1fU) << 6) + (*(utf8 + 1) & 0x3fU);
        return 2;
    } else if ((*utf8 & 0xf0U) == 0xe0U) {
        code = ((*utf8 & 0x0fU) << 12) + ((*(utf8 + 1) & 0x3fU) << 6) + (*(utf8 + 2) & 0x3fU);
        return 3;
    } else if ((*utf8 & 0xf8U) == 0xf0U) {
        code = ((*utf8 & 0x07U) << 18) + ((*(utf8 + 1) & 0x3fU) << 12) + ((*(utf8 + 2) & 0x3fU) << 6) + (*(utf8 + 3) & 0x3fU);
        return 4;
    }
    code = 0;
    return 0;
}


void TtfLoader::clear()
{
    if (nomap) {
        if (freeData) tvg::free(reader.data);
        reader.data = nullptr;
        reader.size = 0;
        freeData = false;
        nomap = false;
    } else {
#ifdef THORVG_FILE_IO_SUPPORT
        _unmap(this);
#endif
    }

    tvg::free(name);
    name = nullptr;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


float TtfLoader::transform(Paint* paint, FontMetrics* metrics, float fontSize, float italicShear)
{
    auto dpi = 96.0f / 72.0f;   //dpi base?
    auto scale = fontSize * dpi / reader.metrics.unitsPerEm;
    Matrix m = {scale, -italicShear * scale, italicShear * static_cast<TtfMetrics*>(metrics)->baseWidth * scale, 0, scale, 0, 0, 0, 1};
    paint->transform(m);

    return scale;
}


TtfLoader::TtfLoader() : FontLoader(FileType::Ttf)
{
}


TtfLoader::~TtfLoader()
{
    clear();
}


bool TtfLoader::open(const char* path)
{
#ifdef THORVG_FILE_IO_SUPPORT
    clear();
    if (!_map(this, path)) return false;

    name = tvg::filename(path);

    return reader.header();
#else
    return false;
#endif
}


bool TtfLoader::open(const char* data, uint32_t size, TVG_UNUSED const char* rpath, bool copy)
{
    reader.size = size;
    nomap = true;

    if (copy) {
        reader.data = tvg::malloc<uint8_t*>(size);
        if (!reader.data) return false;
        memcpy((char*)reader.data, data, reader.size);
        freeData = true;
    } else reader.data = (uint8_t*)data;

    return reader.header();
}


bool TtfLoader::read(RenderPath& path, char* text, FontMetrics* out)
{
    path.clear();

    if (!text) return false;

    auto utf8 = text;  //supposed to be valid utf8
    auto metrics = static_cast<TtfMetrics*>(out);
    Point offset = {0.0f, reader.metrics.hhea.ascent};
    Point kerning = {0.0f, 0.0f};
    auto lglyph = INVALID_GLYPH;
    TtfGlyph glyph;
    uint32_t code;

    while (*utf8) {
        utf8 += _codepoints(utf8, code);
        if (code == 0) break;
        auto rglyph = reader.glyph(code, glyph);
        if (rglyph == INVALID_GLYPH) continue;
        if (lglyph != INVALID_GLYPH) reader.kerning(lglyph, rglyph, kerning);
        if (!reader.convert(path, glyph, offset, kerning, 1U)) break;
        offset.x += (glyph.advance + kerning.x);
        //store the first glyph with outline min size for italic transform.
        if (lglyph == INVALID_GLYPH && glyph.offset) metrics->baseWidth = glyph.w;
        lglyph = rglyph;
    }

    metrics->w = offset.x;
    metrics->h = reader.metrics.hhea.ascent - reader.metrics.hhea.descent;

    return true;
}