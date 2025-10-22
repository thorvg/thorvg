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

bool GlShaderCache::support = false;

struct CacheHeader {
    uint32_t magic;
    uint16_t version;
    unsigned long hash; // the result of djb2Encode is unsigned long
    GLsizei length;
    GLenum binaryFormat;
};

static constexpr uint32_t SHADER_CACHE_MAGIC = 0x54484f52; // "THOR" in hex

static inline unsigned long  hashShader(const char* vertSrc, const char* fragSrc)
{
    auto vertHash = djb2Encode(vertSrc);
    auto fragHash = djb2Encode(fragSrc);
    return vertHash ^ (fragHash << 1);
}

bool GlShaderCache::path(const char* vertSrc, const char* fragSrc, char* outPath, size_t outPathSize)
{
#if !defined(THORVG_FILE_IO_SUPPORT) || defined(__EMSCRIPTEN__)
    return false;
#endif
    if (!vertSrc || !fragSrc) return false;
    auto combinedHash = hashShader(vertSrc, fragSrc);

    // Get system cache directory
    char cacheDir[PATH_MAX];
    if (!tvg::cachedir(cacheDir, PATH_MAX)) return false;

#ifdef _WIN32
    snprintf(outPath, outPathSize, "%s\\tvg_glshader_%08lx.bin", cacheDir, combinedHash);
#else
    auto needed = snprintf(outPath, outPathSize, "%s/tvg_glshader_%08lx.bin", cacheDir, combinedHash);
    if (needed < 0) return false;
    if (needed >= static_cast<int>(outPathSize)) return false;
#endif

    return true;
}


uint32_t GlShaderCache::read(const char* vertSrc, const char* fragSrc)
{

#if defined(THORVG_FILE_IO_SUPPORT) && !defined(__EMSCRIPTEN__)

    if (!vertSrc || !fragSrc) return 0;

    if (!support) return 0;

    char cachePath[PATH_MAX];
    if (!path(vertSrc, fragSrc, cachePath, PATH_MAX)) return 0;

    auto file = fopen(cachePath, "rb");
    if (!file) return 0;

    auto version = tvg::THORVG_VERSION_NUMBER();
    CacheHeader header;
    if ((fread(&header, sizeof(CacheHeader), 1, file) != 1) || header.magic != SHADER_CACHE_MAGIC || header.version != version || header.length < 1) {
        fclose(file);
        remove(cachePath);
        return 0;
    }

    auto binaryData = tvg::malloc<GLubyte*>(header.length);
    auto bytesRead = fread(binaryData, 1, header.length, file);
    fclose(file);

    auto computedHash = hashShader(vertSrc, fragSrc);
    if (bytesRead != static_cast<size_t>(header.length) || computedHash != header.hash) {
        tvg::free(binaryData);
        remove(cachePath);
        return 0;
    }

    uint32_t progObj = glCreateProgram();
    if (!progObj) {
        tvg::free(binaryData);
        return 0;
    }

    glProgramBinary(progObj, header.binaryFormat, binaryData, header.length);
    tvg::free(binaryData);

    GLint linked = 0;
    glGetProgramiv(progObj, GL_LINK_STATUS, &linked);

    if (!linked) {
        glDeleteProgram(progObj);
        remove(cachePath);
        return 0;
    }

    TVGLOG("GL_ENGINE", "Shader cache loaded: %s (%d bytes)", cachePath, header.length);
    return progObj;

#endif
}

void GlShaderCache::write(uint32_t progObj, const char* vertSrc, const char* fragSrc)
{

#if defined(THORVG_FILE_IO_SUPPORT) && !defined(__EMSCRIPTEN__)

    if (!progObj || !vertSrc || !fragSrc) return;

    if (!support) return;

    char cachePath[PATH_MAX];
    if (!path(vertSrc, fragSrc, cachePath, PATH_MAX)) return;

    GLint binaryLength = 0;
    glGetProgramiv(progObj, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
    if (binaryLength < 1) return;

    auto binaryData = tvg::malloc<GLubyte*>(binaryLength);

    GLenum binaryFormat = 0;
    GLsizei length = 0;
    glGetProgramBinary(progObj, binaryLength, &length, &binaryFormat, binaryData);

    if (length < 1) {
        tvg::free(binaryData);
        return;
    }

    CacheHeader header;
    header.magic = SHADER_CACHE_MAGIC;
    header.version = tvg::THORVG_VERSION_NUMBER();
    header.hash = hashShader(vertSrc, fragSrc);
    header.length = length;
    header.binaryFormat = binaryFormat;

    auto* file = fopen(cachePath, "wb");
    if (!file) {
        tvg::free(binaryData);
        return;
    }

    if (fwrite(&header, sizeof(CacheHeader), 1, file) != 1) {
        fclose(file);
        tvg::free(binaryData);
        remove(cachePath);
        return;
    }

    auto written = fwrite(binaryData, 1, length, file);
    fclose(file);

    if (written == static_cast<size_t>(length)) TVGLOG("GL_ENGINE", "Shader cache written: %s (%d bytes)", cachePath, length);

    tvg::free(binaryData);

#endif
}
