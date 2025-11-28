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

#if defined(__linux__)
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
#endif

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#define SPACE_GLYPH_IDX 1
#define LINE_FEED_GLYPH_IDX 10
#define DOT_GLYPH_IDX 46


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

    reader.data = tvg::malloc<uint8_t>(reader.size);

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


static size_t _codepoints(char** utf8)
{
    auto p = *utf8;

    if (!(*p & 0x80U)) {
        (*utf8) += 1;
        return *p;
    } else if ((*p & 0xe0U) == 0xc0U) {
        (*utf8) += 2;
        return ((*p & 0x1fU) << 6) + (*(p + 1) & 0x3fU);
    } else if ((*p & 0xf0U) == 0xe0U) {
        (*utf8) += 3;
        return ((*p & 0x0fU) << 12) + ((*(p + 1) & 0x3fU) << 6) + (*(p + 2) & 0x3fU);
    } else if ((*p & 0xf8U) == 0xf0U) {
        (*utf8) += 4;
        return ((*p & 0x07U) << 18) + ((*(p + 1) & 0x3fU) << 12) + ((*(p + 2) & 0x3fU) << 6) + (*(p + 3) & 0x3fU);
    }
    //error case
    (*utf8) += 1;
    return 0;
}


static void _build(const RenderPath& in, const Point& cursor, const Point& kerning, RenderPath& out)
{
    out.cmds.push(in.cmds);
    out.pts.grow(in.pts.count);
    ARRAY_FOREACH(p, in.pts) {
        out.pts.push(*p + cursor + kerning);
    }
}


static void _align(const Point& align, const Point& box, const Point& cursor, uint32_t begin, uint32_t end, RenderPath& out)
{
    if (align.x > 0.0f || align.y > 0.0f) {
        auto shift = (box - cursor) * align;
        for (auto p = out.pts.data + begin; p < out.pts.data + end; p++) {
            *p += shift;
        }
    }
}


static void _alignX(float align, float box, float x, uint32_t begin, uint32_t end, RenderPath& out)
{
    if (align > 0.0f) {
        auto shift = (box - x) * align;
        for (auto p = out.pts.data + begin; p < out.pts.data + end; p++) {
            p->x += shift;
        }
    }
}


static void _alignY(float align, float box, float y, uint32_t begin, uint32_t end, RenderPath& out)
{
    if (align > 0.0f) {
        auto shift = (box - y) * align;
        for (auto p = out.pts.data + begin; p < out.pts.data + end; p++) {
            p->y += shift;
        }
    }
}


uint32_t TtfLoader::feedLine(FontMetrics& fm, float box, float x, uint32_t begin, uint32_t end, Point& cursor, uint32_t& loc, RenderPath& out)
{
    _alignX(fm.align.x, box, x, begin, end, out); //align the given line
    cursor.x = 0.0f;
    cursor.y += reader.metrics.hhea.advance * fm.spacing.y;
    ++loc;
    return out.pts.count;
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


TtfGlyphMetrics* TtfLoader::request(uint32_t code)
{
    if (code == 0) return nullptr;

    auto it = glyphs.find(code);
    if (it == glyphs.end()) {
        auto it = glyphs.emplace(code, TtfGlyphMetrics{}).first;
        auto rtgm = &it->second;
        if (!reader.convert(rtgm->path, *rtgm, reader.glyph(code, rtgm), {0.0f, 0.0f}, 1U)) {
            TVGERR("TTF", "invalid glyph id, codepoint(0x%x)", code);
            glyphs.erase(it);
            return nullptr;
        }
        return rtgm;
    }
    return &it->second;
}


void TtfLoader::wrapNone(FontMetrics& fm, const Point& box, char* utf8, RenderPath& out)
{
    TtfGlyphMetrics* ltgm = nullptr;  //left side glyph between the two adjacent glyphs
    Point cursor = {};
    uint32_t line = 0;  //the begin pos of the last line among path
    uint32_t loc = 1;  //line counter

    while (*utf8) {
        auto code = _codepoints(&utf8);
        if (code == LINE_FEED_GLYPH_IDX) {
            line = feedLine(fm, box.x, cursor.x, line, out.pts.count, cursor, loc, out);
            continue;
        }
        auto rtgm = request(code);  //right side glyph between the two adjacent glyphs
        if (!rtgm) continue;

        Point kerning{};
        if (ltgm) reader.kerning(ltgm->idx, rtgm->idx, kerning);

        _build(rtgm->path, cursor, kerning, out);
        cursor.x +=  (rtgm->advance + kerning.x) * fm.spacing.x;

        if (cursor.x > fm.size.x) fm.size.x = cursor.x;  //text horizontal size

        //store the base glyph width for italic transform
        if (!ltgm && rtgm->w > 0.0f) static_cast<TtfMetrics*>(fm.engine)->baseWidth = rtgm->w;
        ltgm = rtgm;

    }
    fm.size.y = height(loc, fm.spacing.y);
    _alignY(fm.align.y, box.y, fm.size.y, 0, line, out); //before the last line
    _align(fm.align, box, {cursor.x, fm.size.y}, line, out.pts.count, out);  //last line
}


void TtfLoader::wrapChar(FontMetrics& fm, const Point& box, char* utf8, RenderPath& out)
{
    TtfGlyphMetrics* ltgm = nullptr;  //left side glyph between the two adjacent glyphs
    uint32_t line = 0;  //the begin pos of the last line among path
    uint32_t loc = 1;   //line counter
    Point cursor = {};

    while (*utf8) {
        auto code = _codepoints(&utf8);
        if (code == LINE_FEED_GLYPH_IDX) {
            line = feedLine(fm, box.x, cursor.x, line, out.pts.count, cursor, loc, out);
            continue;
        }
        auto rtgm = request(code); //right side glyph between the two adjacent glyphs
        if (!rtgm) continue;

        Point kerning{};
        if (ltgm) reader.kerning(ltgm->idx, rtgm->idx, kerning);

        auto xadv = (rtgm->advance + kerning.x) * fm.spacing.x;

        //normal scenario
        if (xadv < box.x) {
            if (cursor.x + xadv > box.x) {
                line = feedLine(fm, box.x, cursor.x, line, out.pts.count, cursor, loc, out);
            }
            _build(rtgm->path, cursor, kerning, out);
            cursor.x += xadv;
        //not enough layout space, force pushing
        } else {
            _build(rtgm->path, cursor, kerning, out);
            line = feedLine(fm, box.x, cursor.x, line, out.pts.count, cursor, loc, out);
        }

        if (cursor.x > fm.size.x) fm.size.x = cursor.x;  //text horizontal size

        //store the base glyph width for italic transform
        if (!ltgm && rtgm->w > 0.0f) static_cast<TtfMetrics*>(fm.engine)->baseWidth = rtgm->w;
        ltgm = rtgm;
    }
    fm.size.y = height(loc, fm.spacing.y);
    _alignY(fm.align.y, box.y, fm.size.y, 0, line, out); //before the last line
    _align(fm.align, box, {cursor.x, fm.size.y}, line, out.pts.count, out);  //last line
}


void TtfLoader::wrapWord(FontMetrics& fm, const Point& box, char* utf8, RenderPath& out, bool smart)
{
    TtfGlyphMetrics* ltgm = nullptr;  //left side glyph between the two adjacent glyphs
    auto line = 0;  //the begin pos of the last line among path
    auto word = 0;  //the begin pos of the last word among path
    auto wadv = 0.0f;  //word advance size
    auto hadv = reader.metrics.hhea.advance * fm.spacing.y;  //line advance size
    uint32_t loc = 1;  //line counter
    Point cursor = {};

    while (*utf8) {
        auto code = _codepoints(&utf8);
        if (code == LINE_FEED_GLYPH_IDX) {
            line = feedLine(fm, box.x, cursor.x, line, out.pts.count, cursor, loc, out);
            continue;
        }
        auto rtgm = request(code); //right side glyph between the two adjacent glyphs
        if (!rtgm) continue;

        Point kerning{};
        if (ltgm) reader.kerning(ltgm->idx, rtgm->idx, kerning);

        auto xadv = (rtgm->advance + kerning.x) * fm.spacing.x;

        //try line-wrap
        if (cursor.x + xadv > box.x) {
            //wrapping only if enough space for the current word
            if ((cursor.x - wadv) + xadv < box.x) {
                _alignX(fm.align.x, box.x, wadv, line, word, out);  //align the current line
                //shift the wrapping word to the next line
                for (auto p = out.pts.begin() + word; p < out.pts.end(); p++) {
                    p->x -= wadv;
                    p->y += hadv;
                }
                cursor.x -= wadv;
                cursor.y += hadv;
                line = word;
                wadv = 0;
                ++loc;
            //not enougth space, line wrap by character
            } else if (smart) {
                line = feedLine(fm, box.x, cursor.x, line, out.pts.count, cursor, loc, out);
            }
        }
        _build(rtgm->path, cursor, kerning, out);
        cursor.x += xadv;

        //capture the word start
        if (rtgm->idx == SPACE_GLYPH_IDX) {
            word = out.pts.count;
            wadv = cursor.x;
        }

        if (cursor.x > fm.size.x) fm.size.x = cursor.x;  //text horizontal size

        //store the base glyph width for italic transform
        if (!ltgm && rtgm->w > 0.0f) static_cast<TtfMetrics*>(fm.engine)->baseWidth = rtgm->w;
        ltgm = rtgm;
    }
    fm.size.y = height(loc, fm.spacing.y);
    _alignY(fm.align.y, box.y, fm.size.y, 0, line, out); //before the last line
    _align(fm.align, box, {cursor.x, fm.size.y}, line, out.pts.count, out);  //last line
}


void TtfLoader::wrapEllipsis(FontMetrics& fm, const Point& box, char* utf8, RenderPath& out)
{
    TtfGlyphMetrics* ltgm = nullptr;  //left side glyph between the two adjacent glyphs
    auto line = 0;  //the begin pos of the last line among path
    Point cursor = {};
    struct {
        uint32_t pts = 0;
        uint32_t cmds = 0;
        float xadv = 0.0f;
    } capture;   //the last path for reverting the last character if the ... space is not enough
    uint32_t loc = 1;  //line counter
    auto stop = false;

    while (*utf8) {
        auto code = _codepoints(&utf8);
        if (code == LINE_FEED_GLYPH_IDX) {
            line = feedLine(fm, box.x, cursor.x, line, out.pts.count, cursor, loc, out);
            continue;
        }
        auto rtgm = request(code); //right side glyph between the two adjacent glyphs
        if (!rtgm) continue;

        Point kerning{};
        if (ltgm) reader.kerning(ltgm->idx, rtgm->idx, kerning);

        auto xadv = (rtgm->advance + kerning.x) * fm.spacing.x;

        //normal case
        if (cursor.x + xadv < box.x) {
            capture = {out.pts.count, out.cmds.count, xadv};
            _build(rtgm->path, cursor, kerning, out);
            cursor.x += xadv;
        //ellipsis
        } else {
            rtgm = request(DOT_GLYPH_IDX); //right side glyph between the two adjacent glyphs
            if (!rtgm) {
                TVGERR("TTF", "Cannot support ... since no glyph data");
                return;
            }
            kerning = {};
            reader.kerning(rtgm->idx, rtgm->idx, kerning);
            //not enough space, revert one character back
            if (cursor.x + (rtgm->advance + kerning.x) * 3 > box.x) {
                out.pts.count = capture.pts;
                out.cmds.count = capture.cmds;
                cursor.x -= capture.xadv;
            }
            //append ...
            auto tmp = (rtgm->advance + kerning.x) * fm.spacing.x;
            for (int i = 0; i < 3; ++i) {
                _build(rtgm->path, cursor, kerning, out);
                cursor.x += tmp;
            }
            stop = true;
        }

        if (cursor.x > fm.size.x) fm.size.x = cursor.x;  //text horizontal size

        //store the base glyph width for italic transform
        if (!ltgm && rtgm->w > 0.0f) static_cast<TtfMetrics*>(fm.engine)->baseWidth = rtgm->w;

        if (stop) break;  //stop the process if the ellipsis is applied

        ltgm = rtgm;
    }
    fm.size.y = height(loc, fm.spacing.y);
    _alignY(fm.align.y, box.y, fm.size.y, 0, line, out); //before the last line
    _align(fm.align, box, {cursor.x, fm.size.y}, line, out.pts.count, out);  //last line
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


void TtfLoader::transform(Paint* paint, FontMetrics& fm, float italicShear)
{
    auto scale = 1.0f / fm.scale;
    Matrix m = {scale, -italicShear * scale, italicShear * static_cast<TtfMetrics*>(fm.engine)->baseWidth * scale, 0, scale, reader.metrics.hhea.ascent * scale, 0, 0, 1};
    paint->transform(m);
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
        reader.data = tvg::malloc<uint8_t>(size);
        if (!reader.data) return false;
        memcpy((char*)reader.data, data, reader.size);
        freeData = true;
    } else reader.data = (uint8_t*)data;

    return reader.header();
}


bool TtfLoader::get(FontMetrics& fm, char* text, RenderPath& out)
{
    constexpr const auto DPI = 96.0f / 72.0f;   //dpi base?

    out.clear();

    if (!text || fm.fontSize == 0.0f) return false;

    fm.scale = reader.metrics.unitsPerEm / (fm.fontSize * DPI);
    fm.size = {};
    if (!fm.engine) fm.engine = tvg::calloc<TtfMetrics>(1, sizeof(TtfMetrics));

    auto box = fm.box * fm.scale;

    if (fm.wrap == TextWrap::None || fm.box.x == 0.0f) wrapNone(fm, box, text, out);
    else if (fm.wrap == TextWrap::Character) wrapChar(fm, box, text, out);
    else if (fm.wrap == TextWrap::Word) wrapWord(fm, box, text, out, false);
    else if (fm.wrap == TextWrap::Smart) wrapWord(fm, box, text, out, true);
    else if (fm.wrap == TextWrap::Ellipsis) wrapEllipsis(fm, box, text, out);
    else return false;

    return true;
}


void TtfLoader::release(FontMetrics& fm)
{
    tvg::free(fm.engine);
    fm.engine = nullptr;
}


void TtfLoader::copy(const FontMetrics& in, FontMetrics& out)
{
    release(out);
    out = in;
    if (in.engine) out.engine = tvg::calloc<TtfMetrics>(1, sizeof(TtfMetrics));
    *static_cast<TtfMetrics*>(out.engine) = *static_cast<TtfMetrics*>(in.engine);
}
