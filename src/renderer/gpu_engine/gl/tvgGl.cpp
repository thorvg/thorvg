/*
 * Copyright (c) 2025 - 2026 ThorVG project. All rights reserved.

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

#include "tvgGl.h"
#include "tvgCommon.h"

#ifdef __EMSCRIPTEN__

bool glInit()
{
    return true;
}


bool glTerm()
{
    return true;
}

#else //__EMSCRIPTEN__

#if defined(_WIN32) && !defined(__CYGWIN__)

#ifdef THORVG_GL_TARGET_GL
    typedef PROC(WINAPI *PFNGLGETPROCADDRESSPROC)(LPCSTR);
    static PFNGLGETPROCADDRESSPROC glGetProcAddress = NULL;
#endif

static HMODULE _libGL = NULL;
#ifdef THORVG_GL_TARGET_GLES
    static HMODULE _libEGL = NULL;
#endif

static bool _glLoad()
{
#ifdef THORVG_GL_TARGET_GL
    _libGL = LoadLibraryW(L"opengl32.dll");
    if (!_libGL) {
        TVGERR("GL_ENGINE", "Cannot find the gl library.");
        return false;
    }
    if (!glGetProcAddress) glGetProcAddress = (PFNGLGETPROCADDRESSPROC)GetProcAddress(_libGL, "wglGetProcAddress");
    if (!glGetProcAddress) glGetProcAddress = (PFNGLGETPROCADDRESSPROC)GetProcAddress(_libGL, "wglGetProcAddressARB");
    if (!glGetProcAddress) return false;
#else
    if (!_libGL) _libGL = LoadLibraryW(L"GLESv2.dll");
    if (!_libGL) _libGL = LoadLibraryW(L"libGLESv2.dll");
    if (!_libGL) {
        TVGERR("GL_ENGINE", "Cannot find gles library.");
        return false;
    }
#endif
    return true;
}

// load opengl proc address from dll or from wglGetProcAddress
static PROC _getProcAddress(const char* procName)
{
    auto procHandle = GetProcAddress(_libGL, procName);
#ifdef THORVG_GL_TARGET_GL
    if (!procHandle) procHandle = glGetProcAddress(procName);
#endif
    return procHandle;
}

#elif defined(__linux__)

#include <dlfcn.h>

#ifdef THORVG_GL_TARGET_GL
    typedef void* (*PFNGLGETPROCADDRESSPROC)(const char*);
    static PFNGLGETPROCADDRESSPROC glGetProcAddress = nullptr;
#endif

static void* _libGL = nullptr;
#ifdef THORVG_GL_TARGET_GLES
    static void* _libEGL = nullptr;
#endif

static bool _glLoad()
{
#ifdef THORVG_GL_TARGET_GL
    _libGL = dlopen("libGL.so", RTLD_LAZY);
    if (!_libGL) _libGL = dlopen("libGL.so.4", RTLD_LAZY);
    if (!_libGL) _libGL = dlopen("libGL.so.3", RTLD_LAZY);
    if (!_libGL) {
        TVGERR("GL_ENGINE", "Cannot find the gl library.");
        return false;
    }
    if (!glGetProcAddress) glGetProcAddress = (PFNGLGETPROCADDRESSPROC)dlsym(_libGL, "glXGetProcAddress");
    if (!glGetProcAddress) glGetProcAddress = (PFNGLGETPROCADDRESSPROC)dlsym(_libGL, "glXGetProcAddressARB");
    if (!glGetProcAddress) return false;
#else
    if (!_libGL) _libGL = dlopen("libGLESv2.so", RTLD_LAZY);
    if (!_libGL) _libGL = dlopen("libGLESv2.so.2.0", RTLD_LAZY);  // sometimes used in Mesa
    if (!_libGL) _libGL = dlopen("libGLESv2.so.2", RTLD_LAZY);
    if (!_libGL) {
        TVGERR("GL_ENGINE", "Cannot find the gles library.");
        return false;
    }
#endif
    return true;
}


static void* _getProcAddress(const char* procName)
{
#ifdef THORVG_GL_TARGET_GL
    return glGetProcAddress(procName);
#else
    return dlsym(_libGL, procName);
#endif
}

#elif defined(__APPLE__) || defined(__MACH__)

#include <dlfcn.h>

static void* _libGL = nullptr;
#ifdef THORVG_GL_TARGET_GLES
    static void* _libEGL = nullptr;
#endif

static bool _glLoad()
{
    if (!_libGL) _libGL = dlopen("/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
    if (!_libGL) _libGL = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
    if (_libGL) return true;
    TVGERR("GL_ENGINE", "Cannot find gl library.");
    return false;
}


static void* _getProcAddress(const char* procName)
{
    return dlsym(_libGL, procName);
}

#endif

#define GL_FUNCTION_FETCH(procName, procType)                   \
    procName = (procType)_getProcAddress(#procName);            \
    if (!procName) {                                            \
        TVGERR("GL_ENGINE", "%s is not supported.", #procName); \
        return false;                                           \
    }

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

//GL_VERSION_1_0
PFNGLCULLFACEPROC               glCullFace;
PFNGLFRONTFACEPROC              glFrontFace;
PFNGLSCISSORPROC                glScissor;
PFNGLTEXPARAMETERIPROC          glTexParameteri;
PFNGLTEXIMAGE2DPROC             glTexImage2D;
PFNGLDRAWBUFFERPROC             glDrawBuffer;
PFNGLCLEARPROC                  glClear;
PFNGLCLEARCOLORPROC             glClearColor;
PFNGLCLEARSTENCILPROC           glClearStencil;
PFNGLCLEARDEPTHPROC             glClearDepth;
PFNGLCLEARDEPTHFPROC            glClearDepthf; // GLES
PFNGLCOLORMASKPROC              glColorMask;
PFNGLDEPTHMASKPROC              glDepthMask;
PFNGLDISABLEPROC                glDisable;
PFNGLENABLEPROC                 glEnable;
PFNGLSTENCILFUNCPROC            glStencilFunc;
PFNGLSTENCILOPPROC              glStencilOp;
PFNGLDEPTHFUNCPROC              glDepthFunc;
PFNGLGETERRORPROC               glGetError;
PFNGLGETINTEGERVPROC            glGetIntegerv;
PFNGLGETSTRINGPROC              glGetString;
PFNGLVIEWPORTPROC               glViewport;

//GL_VERSION_1_1
PFNGLDRAWELEMENTSPROC      glDrawElements;
PFNGLBINDTEXTUREPROC       glBindTexture;
PFNGLDELETETEXTURESPROC    glDeleteTextures;
PFNGLGENTEXTURESPROC       glGenTextures;
PFNGLDRAWARRAYSPROC        glDrawArrays;

//GL_VERSION_1_3
PFNGLACTIVETEXTUREPROC           glActiveTexture;

//GL_VERSION_1_4
PFNGLBLENDEQUATIONPROC     glBlendEquation;
PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;

//GL_VERSION_1_5
PFNGLBINDBUFFERPROC           glBindBuffer;
PFNGLDELETEBUFFERSPROC        glDeleteBuffers;
PFNGLGENBUFFERSPROC           glGenBuffers;
PFNGLBUFFERDATAPROC           glBufferData;

//GL_VERSION_2_0
PFNGLDRAWBUFFERSPROC              glDrawBuffers;
PFNGLSTENCILOPSEPARATEPROC        glStencilOpSeparate;
PFNGLSTENCILFUNCSEPARATEPROC      glStencilFuncSeparate;
PFNGLATTACHSHADERPROC             glAttachShader;
PFNGLCOMPILESHADERPROC            glCompileShader;
PFNGLCREATEPROGRAMPROC            glCreateProgram;
PFNGLCREATESHADERPROC             glCreateShader;
PFNGLDELETEPROGRAMPROC            glDeleteProgram;
PFNGLDELETESHADERPROC             glDeleteShader;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray;
PFNGLGETATTRIBLOCATIONPROC        glGetAttribLocation;
PFNGLGETPROGRAMIVPROC             glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC        glGetProgramInfoLog;
PFNGLGETSHADERIVPROC              glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog;
PFNGLGETUNIFORMLOCATIONPROC       glGetUniformLocation;
PFNGLLINKPROGRAMPROC              glLinkProgram;
PFNGLSHADERSOURCEPROC             glShaderSource;
PFNGLUSEPROGRAMPROC               glUseProgram;
PFNGLUNIFORM1FPROC                glUniform1f;
PFNGLUNIFORM1FVPROC               glUniform1fv;
PFNGLUNIFORM2FVPROC               glUniform2fv;
PFNGLUNIFORM3FVPROC               glUniform3fv;
PFNGLUNIFORM4FVPROC               glUniform4fv;
PFNGLUNIFORM1IVPROC               glUniform1iv;
PFNGLUNIFORM2IVPROC               glUniform2iv;
PFNGLUNIFORM3IVPROC               glUniform3iv;
PFNGLUNIFORM4IVPROC               glUniform4iv;
PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer;
PFNGLUNIFORMMATRIX3FVPROC          glUniformMatrix3fv;
PFNGLVERTEXATTRIB4FPROC             glVertexAttrib4f;

//GL_VERSION_2_1

//GL_VERSION_3_0
PFNGLBINDBUFFERRANGEPROC                     glBindBufferRange;
PFNGLBINDRENDERBUFFERPROC                    glBindRenderbuffer;
PFNGLDELETERENDERBUFFERSPROC                 glDeleteRenderbuffers;
PFNGLGENRENDERBUFFERSPROC                    glGenRenderbuffers;
PFNGLINVALIDATEFRAMEBUFFERPROC               glInvalidateFramebuffer;
PFNGLBINDFRAMEBUFFERPROC                     glBindFramebuffer;
PFNGLDELETEFRAMEBUFFERSPROC                  glDeleteFramebuffers;
PFNGLGENFRAMEBUFFERSPROC                     glGenFramebuffers;
PFNGLFRAMEBUFFERTEXTURE2DPROC                glFramebufferTexture2D;
PFNGLFRAMEBUFFERRENDERBUFFERPROC             glFramebufferRenderbuffer;
PFNGLBLITFRAMEBUFFERPROC                     glBlitFramebuffer;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC      glRenderbufferStorageMultisample;
PFNGLBINDVERTEXARRAYPROC                     glBindVertexArray;
PFNGLDELETEVERTEXARRAYSPROC                  glDeleteVertexArrays;
PFNGLGENVERTEXARRAYSPROC                     glGenVertexArrays;

//GL_VERSION_3_1
PFNGLGETUNIFORMBLOCKINDEXPROC      glGetUniformBlockIndex;
PFNGLUNIFORMBLOCKBINDINGPROC       glUniformBlockBinding;

#if defined(_WIN32) && !defined(__CYGWIN__) && defined(THORVG_GL_TARGET_GL)
    PFNWGLGETCURRENTCONTEXTPROC  tvgWglGetCurrentContext;
    PFNWGLMAKECURRENTPROC        tvgWglMakeCurrent;
#endif

#if defined(THORVG_GL_TARGET_GLES)
    PFNEGLGETCURRENTCONTEXTPROC  tvgEglGetCurrentContext;
    PFNEGLMAKECURRENTPROC        tvgEglMakeCurrent;
#endif

bool glInit()
{
    if (!_glLoad()) return false;

    // GL_VERSION_1_0
    GL_FUNCTION_FETCH(glCullFace, PFNGLCULLFACEPROC);
    GL_FUNCTION_FETCH(glFrontFace, PFNGLFRONTFACEPROC);
    GL_FUNCTION_FETCH(glScissor, PFNGLSCISSORPROC);
    GL_FUNCTION_FETCH(glTexParameteri, PFNGLTEXPARAMETERIPROC);
    GL_FUNCTION_FETCH(glTexImage2D, PFNGLTEXIMAGE2DPROC);
#if !defined(THORVG_GL_TARGET_GLES)
    GL_FUNCTION_FETCH(glDrawBuffer, PFNGLDRAWBUFFERPROC);
#endif
    GL_FUNCTION_FETCH(glClear, PFNGLCLEARPROC);
    GL_FUNCTION_FETCH(glClearColor, PFNGLCLEARCOLORPROC);
    GL_FUNCTION_FETCH(glClearStencil, PFNGLCLEARSTENCILPROC);
#if defined(THORVG_GL_TARGET_GLES)
    GL_FUNCTION_FETCH(glClearDepthf, PFNGLCLEARDEPTHFPROC);
#else
    GL_FUNCTION_FETCH(glClearDepth, PFNGLCLEARDEPTHPROC);
#endif
    GL_FUNCTION_FETCH(glColorMask, PFNGLCOLORMASKPROC);
    GL_FUNCTION_FETCH(glDepthMask, PFNGLDEPTHMASKPROC);
    GL_FUNCTION_FETCH(glDisable, PFNGLDISABLEPROC);
    GL_FUNCTION_FETCH(glEnable, PFNGLENABLEPROC);
    GL_FUNCTION_FETCH(glStencilFunc, PFNGLSTENCILFUNCPROC);
    GL_FUNCTION_FETCH(glStencilOp, PFNGLSTENCILOPPROC);
    GL_FUNCTION_FETCH(glDepthFunc, PFNGLDEPTHFUNCPROC);
    GL_FUNCTION_FETCH(glGetError, PFNGLGETERRORPROC);
    GL_FUNCTION_FETCH(glGetIntegerv, PFNGLGETINTEGERVPROC);
    GL_FUNCTION_FETCH(glGetString, PFNGLGETSTRINGPROC);
    GL_FUNCTION_FETCH(glViewport, PFNGLVIEWPORTPROC);

    // GL_VERSION_1_1
    GL_FUNCTION_FETCH(glDrawArrays, PFNGLDRAWARRAYSPROC);
    GL_FUNCTION_FETCH(glDrawElements, PFNGLDRAWELEMENTSPROC);
    GL_FUNCTION_FETCH(glBindTexture, PFNGLBINDTEXTUREPROC);
    GL_FUNCTION_FETCH(glDeleteTextures, PFNGLDELETETEXTURESPROC);
    GL_FUNCTION_FETCH(glGenTextures, PFNGLGENTEXTURESPROC);

    // GL_VERSION_1_2

    // GL_VERSION_1_3
    GL_FUNCTION_FETCH(glActiveTexture, PFNGLACTIVETEXTUREPROC);

    // GL_VERSION_1_4
    GL_FUNCTION_FETCH(glBlendFuncSeparate, PFNGLBLENDFUNCSEPARATEPROC);
    GL_FUNCTION_FETCH(glBlendEquation, PFNGLBLENDEQUATIONPROC);

    // GL_VERSION_1_5
    GL_FUNCTION_FETCH(glBindBuffer, PFNGLBINDBUFFERPROC);
    GL_FUNCTION_FETCH(glDeleteBuffers, PFNGLDELETEBUFFERSPROC);
    GL_FUNCTION_FETCH(glGenBuffers, PFNGLGENBUFFERSPROC);
    GL_FUNCTION_FETCH(glBufferData, PFNGLBUFFERDATAPROC);

    // GL_VERSION_2_0
#if defined(THORVG_GL_TARGET_GLES)
    GL_FUNCTION_FETCH(glDrawBuffers, PFNGLDRAWBUFFERSPROC);
#endif
    GL_FUNCTION_FETCH(glStencilOpSeparate, PFNGLSTENCILOPSEPARATEPROC);
    GL_FUNCTION_FETCH(glStencilFuncSeparate, PFNGLSTENCILFUNCSEPARATEPROC);
    GL_FUNCTION_FETCH(glAttachShader, PFNGLATTACHSHADERPROC);
    GL_FUNCTION_FETCH(glCompileShader, PFNGLCOMPILESHADERPROC);
    GL_FUNCTION_FETCH(glCreateProgram, PFNGLCREATEPROGRAMPROC);
    GL_FUNCTION_FETCH(glCreateShader, PFNGLCREATESHADERPROC);
    GL_FUNCTION_FETCH(glDeleteProgram, PFNGLDELETEPROGRAMPROC);
    GL_FUNCTION_FETCH(glDeleteShader, PFNGLDELETESHADERPROC);
    GL_FUNCTION_FETCH(glDisableVertexAttribArray, PFNGLDISABLEVERTEXATTRIBARRAYPROC);
    GL_FUNCTION_FETCH(glEnableVertexAttribArray, PFNGLENABLEVERTEXATTRIBARRAYPROC);
    GL_FUNCTION_FETCH(glGetAttribLocation, PFNGLGETATTRIBLOCATIONPROC);
    GL_FUNCTION_FETCH(glGetProgramiv, PFNGLGETPROGRAMIVPROC);
    GL_FUNCTION_FETCH(glGetProgramInfoLog, PFNGLGETPROGRAMINFOLOGPROC);
    GL_FUNCTION_FETCH(glGetShaderiv, PFNGLGETSHADERIVPROC);
    GL_FUNCTION_FETCH(glGetShaderInfoLog, PFNGLGETSHADERINFOLOGPROC);
    GL_FUNCTION_FETCH(glGetUniformLocation, PFNGLGETUNIFORMLOCATIONPROC);
    GL_FUNCTION_FETCH(glLinkProgram, PFNGLLINKPROGRAMPROC);
    GL_FUNCTION_FETCH(glShaderSource, PFNGLSHADERSOURCEPROC);
    GL_FUNCTION_FETCH(glUseProgram, PFNGLUSEPROGRAMPROC);
    GL_FUNCTION_FETCH(glUniform1f, PFNGLUNIFORM1FPROC);
    GL_FUNCTION_FETCH(glUniform1fv, PFNGLUNIFORM1FVPROC);
    GL_FUNCTION_FETCH(glUniform2fv, PFNGLUNIFORM2FVPROC);
    GL_FUNCTION_FETCH(glUniform3fv, PFNGLUNIFORM3FVPROC);
    GL_FUNCTION_FETCH(glUniform4fv, PFNGLUNIFORM4FVPROC);
    GL_FUNCTION_FETCH(glUniform1iv, PFNGLUNIFORM1IVPROC);
    GL_FUNCTION_FETCH(glUniform2iv, PFNGLUNIFORM2IVPROC);
    GL_FUNCTION_FETCH(glUniform3iv, PFNGLUNIFORM3IVPROC);
    GL_FUNCTION_FETCH(glUniform4iv, PFNGLUNIFORM4IVPROC);
    GL_FUNCTION_FETCH(glUniformMatrix3fv, PFNGLUNIFORMMATRIX3FVPROC);
    GL_FUNCTION_FETCH(glVertexAttrib4f, PFNGLVERTEXATTRIB4FPROC);
    GL_FUNCTION_FETCH(glVertexAttribPointer, PFNGLVERTEXATTRIBPOINTERPROC);

    // GL_VERSION_2_1

    // GL_VERSION_3_0
    GL_FUNCTION_FETCH(glBindBufferRange, PFNGLBINDBUFFERRANGEPROC);
    GL_FUNCTION_FETCH(glBindRenderbuffer, PFNGLBINDRENDERBUFFERPROC);
    GL_FUNCTION_FETCH(glDeleteRenderbuffers, PFNGLDELETERENDERBUFFERSPROC);
    GL_FUNCTION_FETCH(glGenRenderbuffers, PFNGLGENRENDERBUFFERSPROC);
#if defined(THORVG_GL_TARGET_GLES)
    GL_FUNCTION_FETCH(glInvalidateFramebuffer, PFNGLINVALIDATEFRAMEBUFFERPROC);
#endif
    GL_FUNCTION_FETCH(glBindFramebuffer, PFNGLBINDFRAMEBUFFERPROC);
    GL_FUNCTION_FETCH(glDeleteFramebuffers, PFNGLDELETEFRAMEBUFFERSPROC);
    GL_FUNCTION_FETCH(glGenFramebuffers, PFNGLGENFRAMEBUFFERSPROC);
    GL_FUNCTION_FETCH(glFramebufferTexture2D, PFNGLFRAMEBUFFERTEXTURE2DPROC);
    GL_FUNCTION_FETCH(glFramebufferRenderbuffer, PFNGLFRAMEBUFFERRENDERBUFFERPROC);
    GL_FUNCTION_FETCH(glBlitFramebuffer, PFNGLBLITFRAMEBUFFERPROC);
    GL_FUNCTION_FETCH(glRenderbufferStorageMultisample, PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC);
    GL_FUNCTION_FETCH(glBindVertexArray, PFNGLBINDVERTEXARRAYPROC);
    GL_FUNCTION_FETCH(glDeleteVertexArrays, PFNGLDELETEVERTEXARRAYSPROC);
    GL_FUNCTION_FETCH(glGenVertexArrays, PFNGLGENVERTEXARRAYSPROC);
    
    // GL_VERSION_3_1
    GL_FUNCTION_FETCH(glGetUniformBlockIndex, PFNGLGETUNIFORMBLOCKINDEXPROC);
    GL_FUNCTION_FETCH(glUniformBlockBinding, PFNGLUNIFORMBLOCKBINDINGPROC);

#if defined(_WIN32) && !defined(__CYGWIN__) && defined(THORVG_GL_TARGET_GL)
    tvgWglGetCurrentContext = (PFNWGLGETCURRENTCONTEXTPROC)GetProcAddress(_libGL, "wglGetCurrentContext");
    tvgWglMakeCurrent = (PFNWGLMAKECURRENTPROC)GetProcAddress(_libGL, "wglMakeCurrent");
    if (!tvgWglGetCurrentContext || !tvgWglMakeCurrent) {
        TVGERR("GL_ENGINE", "Failed to load WGL context management functions.");
        return false;
    }
#endif

#if defined(THORVG_GL_TARGET_GLES)
    #if defined(_WIN32) && !defined(__CYGWIN__)
        if (!_libEGL) _libEGL = LoadLibraryW(L"libEGL.dll");
        if (!_libEGL) _libEGL = LoadLibraryW(L"EGL.dll");
        if (!_libEGL) {
            TVGERR("GL_ENGINE", "Cannot find EGL library.");
            return false;
        }
        tvgEglGetCurrentContext = (PFNEGLGETCURRENTCONTEXTPROC)GetProcAddress(_libEGL, "eglGetCurrentContext");
        tvgEglMakeCurrent = (PFNEGLMAKECURRENTPROC)GetProcAddress(_libEGL, "eglMakeCurrent");
    #else
        if (!_libEGL) _libEGL = dlopen("libEGL.so.1", RTLD_LAZY);
        if (!_libEGL) _libEGL = dlopen("libEGL.so", RTLD_LAZY);
        if (!_libEGL) {
            TVGERR("GL_ENGINE", "Cannot find EGL library.");
            return false;
        }
        tvgEglGetCurrentContext = (PFNEGLGETCURRENTCONTEXTPROC)dlsym(_libEGL, "eglGetCurrentContext");
        tvgEglMakeCurrent = (PFNEGLMAKECURRENTPROC)dlsym(_libEGL, "eglMakeCurrent");
    #endif
    if (!tvgEglGetCurrentContext || !tvgEglMakeCurrent) {
        TVGERR("GL_ENGINE", "Failed to load EGL context management functions.");
        return false;
    }
#endif

    //Confirm the version
    GLint vMajor, vMinor;
    glGetIntegerv(GL_MAJOR_VERSION, &vMajor);
    glGetIntegerv(GL_MINOR_VERSION, &vMinor);
    if (vMajor < TVG_REQUIRE_GL_MAJOR_VER || (vMajor ==  TVG_REQUIRE_GL_MAJOR_VER && vMinor <  TVG_REQUIRE_GL_MINOR_VER)) {
        TVGERR("GL_ENGINE", "OpenGL/ES version is not satisfied. Current: v%d.%d, Required: v%d.%d", vMajor, vMinor, TVG_REQUIRE_GL_MAJOR_VER, TVG_REQUIRE_GL_MINOR_VER);
        return false;
    }

    TVGLOG("GL_ENGINE", "OpenGL/ES version = v%d.%d", vMajor, vMinor);

    return true;
};


bool glTerm()
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    if (_libGL) { 
        FreeLibrary(_libGL);
        _libGL = nullptr;
    }
    #if defined(THORVG_GL_TARGET_GLES)
        if (_libEGL) { 
            FreeLibrary(_libEGL);
            _libEGL = nullptr;
        }
    #endif
#else
    if (_libGL) { 
        dlclose(_libGL);
        _libGL = nullptr;
    }
    #if defined(THORVG_GL_TARGET_GLES)
        if (_libEGL) { 
            dlclose(_libEGL);
            _libEGL = nullptr;
        }
    #endif
#endif

#if defined(THORVG_GL_TARGET_GL) && ((defined(_WIN32) && !defined(__CYGWIN__)) || defined(__linux__))
    if (glGetProcAddress) glGetProcAddress = nullptr;
#endif

    return true;
}

#endif // __EMSCRIPTEN__
