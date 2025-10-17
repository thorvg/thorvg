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

#ifdef __APPLE__
    #include <sys/stat.h>
    #include <unistd.h>
#endif

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

uint32_t GlShaderCache::hashString(const char* str)
{
    // Simple FNV-1a hash
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= static_cast<uint32_t>(*str++);
        hash *= 16777619u;
    }
    return hash;
}


char* GlShaderCache::getCachePath(const char* vertSrc, const char* fragSrc)
{
#ifndef __EMSCRIPTEN__
    // Get user's cache directory on macOS
    const char* home = getenv("HOME");
    if (!home) return nullptr;

    // Compute hash from shader sources
    uint32_t vertHash = hashString(vertSrc);
    uint32_t fragHash = hashString(fragSrc);
    uint32_t combinedHash = vertHash ^ (fragHash << 1);

    // Build cache directory path: ~/Library/Caches/thorvg/shaders
    char* cacheDir = tvg::malloc<char*>(512);
    if (!cacheDir) return nullptr;

    snprintf(cacheDir, 512, "%s/Library/Caches/thorvg/shaders", home);

    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(cacheDir, &st) == -1) {
        // Create thorvg directory first
        char* thorvgDir = tvg::malloc<char*>(512);
        if (!thorvgDir) {
            tvg::free(cacheDir);
            return nullptr;
        }
        snprintf(thorvgDir, 512, "%s/Library/Caches/thorvg", home);
        mkdir(thorvgDir, 0755);
        tvg::free(thorvgDir);

        // Create shaders subdirectory
        mkdir(cacheDir, 0755);
    }

    // Build full cache file path
    char* cachePath = tvg::malloc<char*>(512);
    if (!cachePath) {
        tvg::free(cacheDir);
        return nullptr;
    }

    snprintf(cachePath, 512, "%s/shader_%08x.bin", cacheDir, combinedHash);
    tvg::free(cacheDir);

    return cachePath;
#else
    return nullptr;
#endif
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

uint32_t GlShaderCache::read(const char* vertSrc, const char* fragSrc)
{
#ifndef __EMSCRIPTEN__
    if (!vertSrc || !fragSrc) return 0;

    // Get cache file path
    auto cachePath = getCachePath(vertSrc, fragSrc);
    if (!cachePath) return 0;

    // Check if cache file exists
    FILE* file = fopen(cachePath, "rb");
    if (!file) {
        tvg::free(cachePath);
        return 0;
    }

    // Read format and length
    GLenum binaryFormat = 0;
    GLsizei length = 0;

    if (fread(&binaryFormat, sizeof(GLenum), 1, file) != 1 ||
        fread(&length, sizeof(GLsizei), 1, file) != 1 || length < 1) {
        TVGLOG("GL_ENGINE", "Failed to read shader cache header: %s", cachePath);
        fclose(file);
        tvg::free(cachePath);
        return 0;
    }

    // Allocate buffer for binary data
    auto binaryData = tvg::malloc<GLubyte*>(length);
    if (!binaryData) {
        fclose(file);
        tvg::free(cachePath);
        return 0;
    }

    // Read binary data
    size_t bytesRead = fread(binaryData, 1, length, file);
    fclose(file);

    if (bytesRead != static_cast<size_t>(length)) {
        TVGLOG("GL_ENGINE", "Failed to read shader cache data: %s", cachePath);
        tvg::free(binaryData);
        tvg::free(cachePath);
        return 0;
    }

    // Create program object
    uint32_t progObj = glCreateProgram();
    if (!progObj) {
        TVGLOG("GL_ENGINE", "Failed to create program object");
        tvg::free(binaryData);
        tvg::free(cachePath);
        return 0;
    }

    // Load program binary
    glProgramBinary(progObj, binaryFormat, binaryData, length);
    tvg::free(binaryData);

    // Check link status
    GLint linked = 0;
    glGetProgramiv(progObj, GL_LINK_STATUS, &linked);

    if (!linked) {
        TVGLOG("GL_ENGINE", "Failed to link cached shader program: %s", cachePath);
        glDeleteProgram(progObj);
        tvg::free(cachePath);
        return 0;
    }

    TVGLOG("GL_ENGINE", "Shader cache loaded: %s (%d bytes)", cachePath, length);
    tvg::free(cachePath);
    return progObj;
#else
    return 0;
#endif
}


bool GlShaderCache::write(uint32_t programId, const char* vertSrc, const char* fragSrc)
{
#ifndef __EMSCRIPTEN__
    if (!programId || !vertSrc || !fragSrc) return false;

    // Get cache file path
    auto cachePath = getCachePath(vertSrc, fragSrc);
    if (!cachePath) return false;

    // Check if GL_ARB_get_program_binary is supported
    GLint numFormats = 0;
    glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &numFormats);
    if (1 > numFormats) {
        TVGLOG("GL_ENGINE", "Program binary not supported");
        tvg::free(cachePath);
        return false;
    }

    // Get program binary length
    GLint binaryLength = 0;
    glGetProgramiv(programId, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
    if (1 > binaryLength) {
        TVGLOG("GL_ENGINE", "Failed to get program binary length");
        tvg::free(cachePath);
        return false;
    }

    // Allocate buffer for binary data
    auto binaryData = tvg::malloc<GLubyte*>(binaryLength);
    if (!binaryData) {
        tvg::free(cachePath);
        return false;
    }

    // Get program binary
    GLenum binaryFormat = 0;
    GLsizei length = 0;
    glGetProgramBinary(programId, binaryLength, &length, &binaryFormat, binaryData);

    if (1 > length) {
        TVGLOG("GL_ENGINE", "Failed to retrieve program binary");
        tvg::free(binaryData);
        tvg::free(cachePath);
        return false;
    }

    // Write to file
    FILE* file = fopen(cachePath, "wb");
    if (!file) {
        TVGLOG("GL_ENGINE", "Failed to open cache file for writing: %s", cachePath);
        tvg::free(binaryData);
        tvg::free(cachePath);
        return false;
    }

    // Write format and length first
    fwrite(&binaryFormat, sizeof(GLenum), 1, file);
    fwrite(&length, sizeof(GLsizei), 1, file);

    // Write binary data
    size_t written = fwrite(binaryData, 1, length, file);
    fclose(file);

    bool success = (written == static_cast<size_t>(length));

    if (success) {
        TVGLOG("GL_ENGINE", "Shader cache written: %s (%d bytes)", cachePath, length);
    } else {
        TVGLOG("GL_ENGINE", "Failed to write shader cache: %s", cachePath);
    }

    tvg::free(binaryData);
    tvg::free(cachePath);

    return success;
#else
    return false;
#endif
}

