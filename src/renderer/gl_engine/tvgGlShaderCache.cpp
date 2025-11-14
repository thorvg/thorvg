/*
 * Copyright (c) 2025 the ThorVG project. All rights reserved.

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

#include "tvgGlShaderCache.h"

#include "tvgCompressor.h"
#include "tvgEnv.h"

#ifdef _WIN32
    #ifndef PATH_MAX
        #define PATH_MAX MAX_PATH
    #endif
#else
    #include <limits.h>
#endif


struct FileHeader
{
    uint32_t magic;
    uint16_t version;
    uint16_t entryCount;
};


struct EntryHeader
{
    GLsizei length;
    GLenum binaryFormat;
    uint8_t renderType;
};


struct CachedBinary {
    GLubyte* data;
    GLsizei length;
    GLenum format;
};


std::unordered_map<uint8_t, CachedBinary> cache;
static constexpr uint32_t SHADER_CACHE_MAGIC = 0x54484f52; // "THOR" in hex
inline static void _load();
static bool _path(char* outPath, size_t outPathSize);
bool triedLoad = false;
bool GlShaderCache::support = false;


bool _path(char* outPath, size_t outPathSize)
{
#if !defined(THORVG_FILE_IO_SUPPORT) || defined(__EMSCRIPTEN__)
    return false;
#endif

    // Get system cache directory
    char cacheDir[PATH_MAX];
    if (!tvg::cachedir(cacheDir, PATH_MAX)) return false;

#ifdef _WIN32
    snprintf(outPath, outPathSize, "%s\\tvg_glshader.bin", cacheDir);
#else
    auto needed = snprintf(outPath, outPathSize, "%s/tvg_glshader.bin", cacheDir);
    if (needed < 0) return false;
    if (needed >= static_cast<int>(outPathSize)) return false;
#endif

    return true;
}


uint32_t GlShaderCache::read(uint8_t renderType)
{
#if defined(THORVG_FILE_IO_SUPPORT) && !defined(__EMSCRIPTEN__)

    if (!support) return 0;

    if (!triedLoad) _load();

    auto it = cache.find(renderType);
    if (it != cache.end()) {
        const CachedBinary& cached = it->second;

        uint32_t progObj = glCreateProgram();
        if (!progObj) return 0;

        glProgramBinary(progObj, cached.format, cached.data, cached.length);

        GLint linked = 0;
        glGetProgramiv(progObj, GL_LINK_STATUS, &linked);

        if (linked) return progObj;
        else glDeleteProgram(progObj);
    }

    return 0;

#else
    return 0;
#endif
}

void GlShaderCache::write(uint32_t progObj, uint8_t renderType)
{

#if defined(THORVG_FILE_IO_SUPPORT) && !defined(__EMSCRIPTEN__)

    if (!progObj) return;

    if (!support) return;

    char cachePath[PATH_MAX];
    if (!_path(cachePath, PATH_MAX)) return;

    GLint binaryLength = 0;
    glGetProgramiv(progObj, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
    if (binaryLength < 1) return;

    auto binaryData = tvg::malloc<GLubyte>(binaryLength);

    GLenum binaryFormat = 0;
    GLsizei length = 0;
    glGetProgramBinary(progObj, binaryLength, &length, &binaryFormat, binaryData);

    if (length < 1) {
        tvg::free(binaryData);
        return;
    }

    auto version = tvg::THORVG_VERSION_NUMBER();
    bool valid = false;
    FileHeader fileHeader;

    auto* file = fopen(cachePath, "rb");
    if (file) {
        valid = true;
        if (fread(&fileHeader, sizeof(FileHeader), 1, file) != 1 || fileHeader.magic != SHADER_CACHE_MAGIC || fileHeader.version != version) {
            valid = false;
            fileHeader.entryCount = 0;
        }
        fclose(file);
    }

    if (valid) {
        file = fopen(cachePath, "r+b");
        if (!file) {
            tvg::free(binaryData);
            return;
        }

        fileHeader.entryCount++;
        fseek(file, 0, SEEK_SET);
        if (fwrite(&fileHeader, sizeof(FileHeader), 1, file) != 1) {
            fclose(file);
            tvg::free(binaryData);
            return;
        }
        fseek(file, 0, SEEK_END);
    } else {
        file = fopen(cachePath, "wb");
        if (!file) {
            tvg::free(binaryData);
            return;
        }
        fileHeader.magic = SHADER_CACHE_MAGIC;
        fileHeader.version = version;
        fileHeader.entryCount = 1;

        if (fwrite(&fileHeader, sizeof(FileHeader), 1, file) != 1) {
            fclose(file);
            tvg::free(binaryData);
            remove(cachePath);
            return;
        }
    }

    EntryHeader entryHeader;
    entryHeader.renderType = renderType;
    entryHeader.length = length;
    entryHeader.binaryFormat = binaryFormat;

    if (fwrite(&entryHeader, sizeof(EntryHeader), 1, file) != 1) {
        fclose(file);
        tvg::free(binaryData);
        if (!valid) remove(cachePath);
        return;
    }

    auto written = fwrite(binaryData, 1, length, file);
    fclose(file);
    tvg::free(binaryData);

    if (written == static_cast<size_t>(length)) TVGLOG("GL_ENGINE", "Shader cache written: renderType=%d (%d bytes, total entries=%d)", renderType, length, fileHeader.entryCount);
    else {
        if (!valid) remove(cachePath);
    }

#endif
}


void GlShaderCache::cleanup()
{
    for (auto& entry : cache) tvg::free(entry.second.data);
    cache.clear();
    triedLoad = false;
}


inline static void _load()
{
#if defined(THORVG_FILE_IO_SUPPORT) && !defined(__EMSCRIPTEN__)

    if (triedLoad) return;
    triedLoad = true;

    char cachePath[PATH_MAX];
    if (!_path(cachePath, PATH_MAX)) return;

    auto file = fopen(cachePath, "rb");
    if (!file) return;

    auto version = tvg::THORVG_VERSION_NUMBER();
    FileHeader fileHeader;

    if (fread(&fileHeader, sizeof(FileHeader), 1, file) != 1 ||
        fileHeader.magic != SHADER_CACHE_MAGIC ||
        fileHeader.version != version) {
        fclose(file);
        return;
    }

    TVGLOG("GL_ENGINE", "Loading shader cache with %d entries", fileHeader.entryCount);

    for (uint16_t i = 0; i < fileHeader.entryCount; i++) {
        EntryHeader entryHeader;
        if (fread(&entryHeader, sizeof(EntryHeader), 1, file) != 1 || entryHeader.length <= 0) {
            fclose(file);
            for (auto& entry : cache) tvg::free(entry.second.data);
            cache.clear();
            return;
        }

        auto binaryData = tvg::malloc<GLubyte>(entryHeader.length);
        auto bytesRead = fread(binaryData, 1, entryHeader.length, file);

        if (bytesRead != static_cast<size_t>(entryHeader.length)) {
            tvg::free(binaryData);
            fclose(file);
            for (auto& entry : cache) tvg::free(entry.second.data);
            cache.clear();
            return;
        }

        CachedBinary cached;
        cached.data = binaryData;
        cached.length = entryHeader.length;
        cached.format = entryHeader.binaryFormat;
        cache[entryHeader.renderType] = cached; // The last entry will overwrite the previous one with the same renderType
    }

    fclose(file);
    TVGLOG("GL_ENGINE", "Shader cache loaded successfully with %zu entries", cache.size());
#endif
}
