/*
 * Copyright (c) 2021 - 2024 the ThorVG project. All rights reserved.

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

#include "tvgCommon.h"
#include "tvgStr.h"
#include "tvgSaveModule.h"
#include "tvgPaint.h"

#ifdef THORVG_GIF_SAVER_SUPPORT
    #include "tvgGifSaver.h"
#endif

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Saver::Impl
{
    SaveModule* saveModule = nullptr;
    Paint* bg = nullptr;

    ~Impl()
    {
        delete(saveModule);
        delete(bg);
    }
};


static SaveModule* _find(FileType type)
{
    switch(type) {
        case FileType::Gif: {
#ifdef THORVG_GIF_SAVER_SUPPORT
            return new GifSaver;
#endif
            break;
        }
        default: {
            break;
        }
    }

#ifdef THORVG_LOG_ENABLED
    const char *format;
    switch(type) {
        case FileType::Gif: {
            format = "GIF";
            break;
        }
        default: {
            format = "???";
            break;
        }
    }
    TVGLOG("RENDERER", "%s format is not supported", format);
#endif
    return nullptr;
}


static SaveModule* _find(const char* filename)
{
    auto ext = strExtension(filename);
    if (ext && !strcmp(ext, "gif")) return _find(FileType::Gif);
    return nullptr;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Saver::Saver() : pImpl(new Impl())
{
}


Saver::~Saver()
{
    delete(pImpl);
}


Result Saver::save(unique_ptr<Paint> paint, const char* filename, uint32_t quality) noexcept
{
    auto p = paint.release();
    if (!p) return Result::MemoryCorruption;

    //Already on saving another resource.
    if (pImpl->saveModule) {
        if (P(p)->refCnt == 0) delete(p);
        return Result::InsufficientCondition;
    }

    if (auto saveModule = _find(filename)) {
        if (saveModule->save(p, pImpl->bg, filename, quality)) {
            pImpl->saveModule = saveModule;
            return Result::Success;
        } else {
            if (P(p)->refCnt == 0) delete(p);
            delete(saveModule);
            return Result::Unknown;
        }
    }
    if (P(p)->refCnt == 0) delete(p);
    return Result::NonSupport;
}


Result Saver::background(unique_ptr<Paint> paint) noexcept
{
    delete(pImpl->bg);
    pImpl->bg = paint.release();

    return Result::Success;
}


Result Saver::save(unique_ptr<Animation> animation, const char* filename, uint32_t quality, uint32_t fps) noexcept
{
    auto a = animation.release();
    if (!a) return Result::MemoryCorruption;

    //animation holds the picture, it must be 1 at the bottom.
    auto remove = PP(a->picture())->refCnt <= 1 ? true : false;

    if (tvg::zero(a->totalFrame())) {
        if (remove) delete(a);
        return Result::InsufficientCondition;
    }

    //Already on saving another resource.
    if (pImpl->saveModule) {
        if (remove) delete(a);
        return Result::InsufficientCondition;
    }

    if (auto saveModule = _find(filename)) {
        if (saveModule->save(a, pImpl->bg, filename, quality, fps)) {
            pImpl->saveModule = saveModule;
            return Result::Success;
        } else {
            if (remove) delete(a);
            delete(saveModule);
            return Result::Unknown;
        }
    }
    if (remove) delete(a);
    return Result::NonSupport;
}


Result Saver::sync() noexcept
{
    if (!pImpl->saveModule) return Result::InsufficientCondition;
    pImpl->saveModule->close();
    delete(pImpl->saveModule);
    pImpl->saveModule = nullptr;

    return Result::Success;
}


unique_ptr<Saver> Saver::gen() noexcept
{
    return unique_ptr<Saver>(new Saver);
}
