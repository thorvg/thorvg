/*
 * Copyright (c) 2020 - 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_LOADER_H_
#define _TVG_LOADER_H_

#include <atomic>
#include "tvgCommon.h"
#include "tvgRender.h"
#include "tvgInlist.h"
#include "tvgStr.h"
#include "tvgAccessor.h"

namespace tvg
{

struct AssetResolver
{
    std::function<bool(Paint* paint, const char* src, void* data)> func;
    void* data;
};

enum class DataOwnership : uint8_t
{
    Borrow = 0,  // the caller keeps it alive, thus shareable by its address
    Copy,        // the loader duplicates and releases it
    Transfer     // the loader takes it over, the caller must not touch it anymore
};

struct SourceData
{
    uint8_t* data = nullptr;
    uint32_t size = 0;
    bool owned = false;  // release the data on clear()

    ~SourceData()
    {
        clear();
    }

    bool assign(const char* data, uint32_t size, bool copy, bool terminate = false)
    {
        clear();

        if (copy) {
            this->data = tvg::malloc<uint8_t>(terminate ? size + 1 : size);
            if (!this->data) return false;
            memcpy(this->data, data, size);
            if (terminate) this->data[size] = '\0';
            owned = true;
        } else this->data = (uint8_t*)data;

        this->size = size;

        return true;
    }

    void own(char* data, uint32_t size)
    {
        clear();
        this->data = (uint8_t*)data;
        this->size = size;
        owned = true;
    }

    void clear()
    {
        if (owned) tvg::free(data);
        data = nullptr;
        size = 0;
        owned = false;
    }

    const char* text() const
    {
        return (const char*)data;
    }
};

struct LoaderOps
{
    Type caller;  // which requests this?
};

struct PictureOps : LoaderOps
{
    AssetResolver* resolver;
    const char* rpath;  // decide the relative path file if the file is loaded from memory
    bool accessible;    // allow the accessor

    PictureOps(AssetResolver* resolver, const char* rpath, bool accessible) :
        LoaderOps{Type::Picture}, resolver(resolver), rpath(rpath), accessible(accessible) {}
};

struct Loader
{
    INLIST_ITEM(Loader);

    // Use either hashkey(data) or hashpath(path)
    uintptr_t hashkey = 0;
    char* hashpath = nullptr;

    SourceData src;              // encoded source data
    FileType type;               // current loader file type
    atomic<uint16_t> sharing{};  // reference count
    bool readied = false;        // read done already
    bool cached = false;         // cached for sharing

    Loader(FileType type) : type(type) {}

    virtual ~Loader()
    {
        tvg::free(hashpath);
    }

    bool allowCache()
    {
        if (type == FileType::Lot) return false;
        if (type == FileType::Gif) return false;
        if (type == FileType::Media) return false;

        return true;
    }

    bool cache(uintptr_t data)
    {
        if (!allowCache()) return false;
        hashkey = data;
        cached = true;
        return true;
    }

    bool cache(const char* path)
    {
        if (!allowCache()) return false;

        hashpath = tvg::duplicate(path);
        cached = true;
        return true;
    }

    bool adopt(const char* data)  // takes over the borrowed source data
    {
        if (src.data != (uint8_t*)data) return false;
        src.owned = true;
        return true;
    }

    virtual bool open(const char* path, const LoaderOps* ops) { return false; }
    virtual bool open(const char* data, uint32_t size, const LoaderOps* ops, bool copy) { return false; }
    virtual bool resize(Paint* paint, float w, float h) { return false; }
    virtual bool sync() { return false; };  // finish immediately if any async update jobs, return true if something has been updated.

    virtual bool read()
    {
        if (readied) return false;
        readied = true;
        return true;
    }

    virtual bool close()
    {
        if (sharing == 0) return true;
        --sharing;
        return false;
    }

    char* open(const char* path, uint32_t& size, bool text = false)
    {
#ifdef THORVG_FILE_IO_SUPPORT
        auto f = fopen(path, text ? "r" : "rb");
        if (!f) return nullptr;

        fseek(f, 0, SEEK_END);

        size = ftell(f);
        if (size == 0) {
            fclose(f);
            return nullptr;
        }

        auto content = tvg::malloc<char>(sizeof(char) * (text ? size + 1 : size));
        fseek(f, 0, SEEK_SET);
        size = fread(content, sizeof(char), size, f);
        if (text) content[size] = '\0';

        fclose(f);

        return content;
#endif
        return nullptr;
    }
};

struct ImageLoader : Loader
{
    float w = 0, h = 0;  // default image size
    bool playable;       // true if this loader supports playback

    ImageLoader(FileType type, bool playable = false) : Loader(type), playable(playable) {}

    virtual const AccessorEntity* access(uint32_t id) { return nullptr; }
    virtual void access(AccessorCallback& cb) {}
    virtual Paint* paint() { return nullptr; }
    virtual RenderSurface* bitmap() { return nullptr; }
};

struct BitmapLoader : ImageLoader
{
    static atomic<ColorSpace> cs;  // desired value

    RenderSurface surface;

    BitmapLoader(FileType type, bool playable = false) : ImageLoader(type, playable) {}

    RenderSurface* bitmap() override
    {
        return surface.data ? &surface : nullptr;
    }
};

struct AnimLoader : ImageLoader
{
    float segmentBegin = 0.0f;
    float segmentEnd;  // initialize the value with the total frame number

    AnimLoader(FileType type) : ImageLoader(type, true) {}
    virtual ~AnimLoader() {}

    virtual bool frame(float no) = 0;  // set the current frame number
    virtual float totalFrame() = 0;  // return the total frame count
    virtual float curFrame() = 0;  // return the current frame number
    virtual float duration() = 0;  // return the animation duration in seconds
    virtual Result segment(float begin, float end) = 0;

    void segment(float* begin, float* end)
    {
        if (begin) *begin = segmentBegin;
        if (end) *end = segmentEnd;
    }
};

struct FontMetrics
{
    Point size;  // text width, height
    float scale;
    Point align{}, box{}, spacing{1.0f, 1.0f};
    float fontSize = 0.0f;
    uint32_t lines = 1;  // line count
    TextWrap wrap = TextWrap::None;

    void* engine = nullptr;  // engine extension

    ~FontMetrics()
    {
        tvg::free(engine);
    }
};

struct FontLoader : Loader
{
    static constexpr const float DPI = 96.0f / 72.0f;  // dpi base?

    char* name = nullptr;

    FontLoader(FileType type) : Loader(type) {}

    using Loader::read;

    virtual bool get(FontMetrics& fm, char* text, uint32_t len, RenderPath& out) = 0;
    virtual void transform(Paint* paint, FontMetrics& fm, float italicShear) = 0;
    virtual void release(FontMetrics& fm) = 0;
    virtual void metrics(const FontMetrics& fm, TextMetrics& out) = 0;
    virtual bool metrics(const FontMetrics& fm, const char* ch, GlyphMetrics& out, const char** next) = 0;
    virtual void copy(const FontMetrics& in, FontMetrics& out) = 0;
};
}

#endif //_TVG_LOADER_H_
