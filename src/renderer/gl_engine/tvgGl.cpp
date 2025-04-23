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

#include "tvgGl.h"
#include "tvgCommon.h"

#ifndef __EMSCRIPTEN__
// GL_VERSION_1_0
PFNGLCULLFACEPROC               glCullFace = nullptr;
PFNGLFRONTFACEPROC              glFrontFace = nullptr;
// PFNGLHINTPROC                   glHint = nullptr;
// PFNGLLINEWIDTHPROC              glLineWidth = nullptr;
// PFNGLPOINTSIZEPROC              glPointSize = nullptr;
// PFNGLPOLYGONMODEPROC            glPolygonMode = nullptr;
PFNGLSCISSORPROC                glScissor = nullptr;
// PFNGLTEXPARAMETERFPROC          glTexParameterf = nullptr;
// PFNGLTEXPARAMETERFVPROC         glTexParameterfv = nullptr;
PFNGLTEXPARAMETERIPROC          glTexParameteri = nullptr;
// PFNGLTEXPARAMETERIVPROC         glTexParameteriv = nullptr;
// PFNGLTEXIMAGE1DPROC             glTexImage1D = nullptr;
PFNGLTEXIMAGE2DPROC             glTexImage2D = nullptr;
PFNGLDRAWBUFFERPROC             glDrawBuffer = nullptr;
PFNGLCLEARPROC                  glClear = nullptr;
PFNGLCLEARCOLORPROC             glClearColor = nullptr;
PFNGLCLEARSTENCILPROC           glClearStencil = nullptr;
PFNGLCLEARDEPTHPROC             glClearDepth = nullptr;
PFNGLCLEARDEPTHFPROC            glClearDepthf = nullptr; // GLES
// PFNGLSTENCILMASKPROC            glStencilMask = nullptr;
PFNGLCOLORMASKPROC              glColorMask = nullptr;
PFNGLDEPTHMASKPROC              glDepthMask = nullptr;
PFNGLDISABLEPROC                glDisable = nullptr;
PFNGLENABLEPROC                 glEnable = nullptr;
// PFNGLFINISHPROC                 glFinish = nullptr;
// PFNGLFLUSHPROC                  glFlush = nullptr;
PFNGLBLENDFUNCPROC              glBlendFunc = nullptr;
// PFNGLLOGICOPPROC                glLogicOp = nullptr;
PFNGLSTENCILFUNCPROC            glStencilFunc = nullptr;
PFNGLSTENCILOPPROC              glStencilOp = nullptr;
PFNGLDEPTHFUNCPROC              glDepthFunc = nullptr;
// PFNGLPIXELSTOREFPROC            glPixelStoref = nullptr;
// PFNGLPIXELSTOREIPROC            glPixelStorei = nullptr;
// PFNGLREADBUFFERPROC             glReadBuffer = nullptr;
// PFNGLREADPIXELSPROC             glReadPixels = nullptr;
// PFNGLGETBOOLEANVPROC            glGetBooleanv = nullptr;
// PFNGLGETDOUBLEVPROC             glGetDoublev = nullptr;
PFNGLGETERRORPROC               glGetError = nullptr;
// PFNGLGETFLOATVPROC              glGetFloatv = nullptr;
PFNGLGETINTEGERVPROC            glGetIntegerv = nullptr;
PFNGLGETSTRINGPROC              glGetString = nullptr;
// PFNGLGETTEXIMAGEPROC            glGetTexImage = nullptr;
// PFNGLGETTEXPARAMETERFVPROC      glGetTexParameterfv = nullptr;
// PFNGLGETTEXPARAMETERIVPROC      glGetTexParameteriv = nullptr;
// PFNGLGETTEXLEVELPARAMETERFVPROC glGetTexLevelParameterfv = nullptr;
// PFNGLGETTEXLEVELPARAMETERIVPROC glGetTexLevelParameteriv = nullptr;
// PFNGLISENABLEDPROC              glIsEnabled = nullptr;
// PFNGLDEPTHRANGEPROC             glDepthRange = nullptr;
PFNGLVIEWPORTPROC               glViewport = nullptr;

// GL_VERSION_1_1
// PFNGLDRAWARRAYSPROC        glDrawArrays = nullptr;
PFNGLDRAWELEMENTSPROC      glDrawElements = nullptr;
// PFNGLGETPOINTERVPROC       glGetPointerv = nullptr;
// PFNGLPOLYGONOFFSETPROC     glPolygonOffset = nullptr;
// PFNGLCOPYTEXIMAGE1DPROC    glCopyTexImage1D = nullptr;
// PFNGLCOPYTEXIMAGE2DPROC    glCopyTexImage2D = nullptr;
// PFNGLCOPYTEXSUBIMAGE1DPROC glCopyTexSubImage1D = nullptr;
// PFNGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D = nullptr;
// PFNGLTEXSUBIMAGE1DPROC     glTexSubImage1D = nullptr;
// PFNGLTEXSUBIMAGE2DPROC     glTexSubImage2D = nullptr;
PFNGLBINDTEXTUREPROC       glBindTexture = nullptr;
PFNGLDELETETEXTURESPROC    glDeleteTextures = nullptr;
PFNGLGENTEXTURESPROC       glGenTextures = nullptr;
// PFNGLISTEXTUREPROC         glIsTexture = nullptr;

// GL_VERSION_1_2
// PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements = nullptr;
// PFNGLTEXIMAGE3DPROC        glTexImage3D = nullptr;
// PFNGLTEXSUBIMAGE3DPROC     glTexSubImage3D = nullptr;
// PFNGLCOPYTEXSUBIMAGE3DPROC glCopyTexSubImage3D = nullptr;

// GL_VERSION_1_3
PFNGLACTIVETEXTUREPROC           glActiveTexture = nullptr;
// PFNGLSAMPLECOVERAGEPROC          glSampleCoverage = nullptr;
// PFNGLCOMPRESSEDTEXIMAGE3DPROC    glCompressedTexImage3D = nullptr;
// PFNGLCOMPRESSEDTEXIMAGE2DPROC    glCompressedTexImage2D = nullptr;
// PFNGLCOMPRESSEDTEXIMAGE1DPROC    glCompressedTexImage1D = nullptr;
// PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D = nullptr;
// PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D = nullptr;
// PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glCompressedTexSubImage1D = nullptr;
// PFNGLGETCOMPRESSEDTEXIMAGEPROC   glGetCompressedTexImage = nullptr;

// GL_VERSION_1_4
// PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate = nullptr;
// PFNGLMULTIDRAWARRAYSPROC   glMultiDrawArrays = nullptr;
// PFNGLMULTIDRAWELEMENTSPROC glMultiDrawElements = nullptr;
// PFNGLPOINTPARAMETERFPROC   glPointParameterf = nullptr;
// PFNGLPOINTPARAMETERFVPROC  glPointParameterfv = nullptr;
// PFNGLPOINTPARAMETERIPROC   glPointParameteri = nullptr;
// PFNGLPOINTPARAMETERIVPROC  glPointParameteriv = nullptr;
// PFNGLBLENDCOLORPROC        glBlendColor = nullptr;
PFNGLBLENDEQUATIONPROC     glBlendEquation = nullptr;

// GL_VERSION_1_5
// PFNGLGENQUERIESPROC           glGenQueries = nullptr;
// PFNGLDELETEQUERIESPROC        glDeleteQueries = nullptr;
// PFNGLISQUERYPROC              glIsQuery = nullptr;
// PFNGLBEGINQUERYPROC           glBeginQuery = nullptr;
// PFNGLENDQUERYPROC             glEndQuery = nullptr;
// PFNGLGETQUERYIVPROC           glGetQueryiv = nullptr;
// PFNGLGETQUERYOBJECTIVPROC     glGetQueryObjectiv = nullptr;
// PFNGLGETQUERYOBJECTUIVPROC    glGetQueryObjectuiv = nullptr;
PFNGLBINDBUFFERPROC           glBindBuffer = nullptr;
PFNGLDELETEBUFFERSPROC        glDeleteBuffers = nullptr;
PFNGLGENBUFFERSPROC           glGenBuffers = nullptr;
// PFNGLISBUFFERPROC             glIsBuffer = nullptr;
PFNGLBUFFERDATAPROC           glBufferData = nullptr;
// PFNGLBUFFERSUBDATAPROC        glBufferSubData = nullptr;
// PFNGLGETBUFFERSUBDATAPROC     glGetBufferSubData = nullptr;
// PFNGLMAPBUFFERPROC            glMapBuffer = nullptr;
// PFNGLUNMAPBUFFERPROC          glUnmapBuffer = nullptr;
// PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv = nullptr;
// PFNGLGETBUFFERPOINTERVPROC    glGetBufferPointerv = nullptr;

// GL_VERSION_2_0
// PFNGLBLENDEQUATIONSEPARATEPROC    glBlendEquationSeparate = nullptr;
PFNGLDRAWBUFFERSPROC              glDrawBuffers = nullptr;
PFNGLSTENCILOPSEPARATEPROC        glStencilOpSeparate = nullptr;
PFNGLSTENCILFUNCSEPARATEPROC      glStencilFuncSeparate = nullptr;
// PFNGLSTENCILMASKSEPARATEPROC      glStencilMaskSeparate = nullptr;
PFNGLATTACHSHADERPROC             glAttachShader = nullptr;
// PFNGLBINDATTRIBLOCATIONPROC       glBindAttribLocation = nullptr;
PFNGLCOMPILESHADERPROC            glCompileShader = nullptr;
PFNGLCREATEPROGRAMPROC            glCreateProgram = nullptr;
PFNGLCREATESHADERPROC             glCreateShader = nullptr;
PFNGLDELETEPROGRAMPROC            glDeleteProgram = nullptr;
PFNGLDELETESHADERPROC             glDeleteShader = nullptr;
// PFNGLDETACHSHADERPROC             glDetachShader = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray = nullptr;
// PFNGLGETACTIVEATTRIBPROC          glGetActiveAttrib = nullptr;
// PFNGLGETACTIVEUNIFORMPROC         glGetActiveUniform = nullptr;
// PFNGLGETATTACHEDSHADERSPROC       glGetAttachedShaders = nullptr;
PFNGLGETATTRIBLOCATIONPROC        glGetAttribLocation = nullptr;
PFNGLGETPROGRAMIVPROC             glGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC        glGetProgramInfoLog = nullptr;
PFNGLGETSHADERIVPROC              glGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog = nullptr;
// PFNGLGETSHADERSOURCEPROC          glGetShaderSource = nullptr;
PFNGLGETUNIFORMLOCATIONPROC       glGetUniformLocation = nullptr;
// PFNGLGETUNIFORMFVPROC             glGetUniformfv = nullptr;
// PFNGLGETUNIFORMIVPROC             glGetUniformiv = nullptr;
// PFNGLGETVERTEXATTRIBDVPROC        glGetVertexAttribdv = nullptr;
// PFNGLGETVERTEXATTRIBFVPROC        glGetVertexAttribfv = nullptr;
// PFNGLGETVERTEXATTRIBIVPROC        glGetVertexAttribiv = nullptr;
// PFNGLGETVERTEXATTRIBPOINTERVPROC  glGetVertexAttribPointerv = nullptr;
// PFNGLISPROGRAMPROC                glIsProgram = nullptr;
// PFNGLISSHADERPROC                 glIsShader = nullptr;
PFNGLLINKPROGRAMPROC              glLinkProgram = nullptr;
PFNGLSHADERSOURCEPROC             glShaderSource = nullptr;
PFNGLUSEPROGRAMPROC               glUseProgram = nullptr;
PFNGLUNIFORM1FPROC                glUniform1f = nullptr;
// PFNGLUNIFORM2FPROC                glUniform2f = nullptr;
// PFNGLUNIFORM3FPROC                glUniform3f = nullptr;
// PFNGLUNIFORM4FPROC                glUniform4f = nullptr;
// PFNGLUNIFORM1IPROC                glUniform1i = nullptr;
// PFNGLUNIFORM2IPROC                glUniform2i = nullptr;
// PFNGLUNIFORM3IPROC                glUniform3i = nullptr;
// PFNGLUNIFORM4IPROC                glUniform4i = nullptr;
PFNGLUNIFORM1FVPROC               glUniform1fv = nullptr;
PFNGLUNIFORM2FVPROC               glUniform2fv = nullptr;
PFNGLUNIFORM3FVPROC               glUniform3fv = nullptr;
PFNGLUNIFORM4FVPROC               glUniform4fv = nullptr;
PFNGLUNIFORM1IVPROC               glUniform1iv = nullptr;
PFNGLUNIFORM2IVPROC               glUniform2iv = nullptr;
PFNGLUNIFORM3IVPROC               glUniform3iv = nullptr;
PFNGLUNIFORM4IVPROC               glUniform4iv = nullptr;
// PFNGLUNIFORMMATRIX2FVPROC         glUniformMatrix2fv = nullptr;
// PFNGLUNIFORMMATRIX3FVPROC         glUniformMatrix3fv = nullptr;
PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv = nullptr;
// PFNGLVALIDATEPROGRAMPROC          glValidateProgram = nullptr;
// PFNGLVERTEXATTRIB1DPROC           glVertexAttrib1d = nullptr;
// PFNGLVERTEXATTRIB1DVPROC          glVertexAttrib1dv = nullptr;
// PFNGLVERTEXATTRIB1FPROC           glVertexAttrib1f = nullptr;
// PFNGLVERTEXATTRIB1FVPROC          glVertexAttrib1fv = nullptr;
// PFNGLVERTEXATTRIB1SPROC           glVertexAttrib1s = nullptr;
// PFNGLVERTEXATTRIB1SVPROC          glVertexAttrib1sv = nullptr;
// PFNGLVERTEXATTRIB2DPROC           glVertexAttrib2d = nullptr;
// PFNGLVERTEXATTRIB2DVPROC          glVertexAttrib2dv = nullptr;
// PFNGLVERTEXATTRIB2FPROC           glVertexAttrib2f = nullptr;
// PFNGLVERTEXATTRIB2FVPROC          glVertexAttrib2fv = nullptr;
// PFNGLVERTEXATTRIB2SPROC           glVertexAttrib2s = nullptr;
// PFNGLVERTEXATTRIB2SVPROC          glVertexAttrib2sv = nullptr;
// PFNGLVERTEXATTRIB3DPROC           glVertexAttrib3d = nullptr;
// PFNGLVERTEXATTRIB3DVPROC          glVertexAttrib3dv = nullptr;
// PFNGLVERTEXATTRIB3FPROC           glVertexAttrib3f = nullptr;
// PFNGLVERTEXATTRIB3FVPROC          glVertexAttrib3fv = nullptr;
// PFNGLVERTEXATTRIB3SPROC           glVertexAttrib3s = nullptr;
// PFNGLVERTEXATTRIB3SVPROC          glVertexAttrib3sv = nullptr;
// PFNGLVERTEXATTRIB4NBVPROC         glVertexAttrib4Nbv = nullptr;
// PFNGLVERTEXATTRIB4NIVPROC         glVertexAttrib4Niv = nullptr;
// PFNGLVERTEXATTRIB4NSVPROC         glVertexAttrib4Nsv = nullptr;
// PFNGLVERTEXATTRIB4NUBPROC         glVertexAttrib4Nub = nullptr;
// PFNGLVERTEXATTRIB4NUBVPROC        glVertexAttrib4Nubv = nullptr;
// PFNGLVERTEXATTRIB4NUIVPROC        glVertexAttrib4Nuiv = nullptr;
// PFNGLVERTEXATTRIB4NUSVPROC        glVertexAttrib4Nusv = nullptr;
// PFNGLVERTEXATTRIB4BVPROC          glVertexAttrib4bv = nullptr;
// PFNGLVERTEXATTRIB4DPROC           glVertexAttrib4d = nullptr;
// PFNGLVERTEXATTRIB4DVPROC          glVertexAttrib4dv = nullptr;
// PFNGLVERTEXATTRIB4FPROC           glVertexAttrib4f = nullptr;
// PFNGLVERTEXATTRIB4FVPROC          glVertexAttrib4fv = nullptr;
// PFNGLVERTEXATTRIB4IVPROC          glVertexAttrib4iv = nullptr;
// PFNGLVERTEXATTRIB4SPROC           glVertexAttrib4s = nullptr;
// PFNGLVERTEXATTRIB4SVPROC          glVertexAttrib4sv = nullptr;
// PFNGLVERTEXATTRIB4UBVPROC         glVertexAttrib4ubv = nullptr;
// PFNGLVERTEXATTRIB4UIVPROC         glVertexAttrib4uiv = nullptr;
// PFNGLVERTEXATTRIB4USVPROC         glVertexAttrib4usv = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer = nullptr;

// GL_VERSION_2_1
// PFNGLUNIFORMMATRIX2X3FVPROC glUniformMatrix2x3fv = nullptr;
// PFNGLUNIFORMMATRIX3X2FVPROC glUniformMatrix3x2fv = nullptr;
// PFNGLUNIFORMMATRIX2X4FVPROC glUniformMatrix2x4fv = nullptr;
// PFNGLUNIFORMMATRIX4X2FVPROC glUniformMatrix4x2fv = nullptr;
// PFNGLUNIFORMMATRIX3X4FVPROC glUniformMatrix3x4fv = nullptr;
// PFNGLUNIFORMMATRIX4X3FVPROC glUniformMatrix4x3fv = nullptr;

// GL_VERSION_3_0
// PFNGLCOLORMASKIPROC                          glColorMaski = nullptr;
// PFNGLGETBOOLEANI_VPROC                       glGetBooleani_v = nullptr;
// PFNGLGETINTEGERI_VPROC                       glGetIntegeri_v = nullptr;
// PFNGLENABLEIPROC                             glEnablei = nullptr;
// PFNGLDISABLEIPROC                            glDisablei = nullptr;
// PFNGLISENABLEDIPROC                          glIsEnabledi = nullptr;
// PFNGLBEGINTRANSFORMFEEDBACKPROC              glBeginTransformFeedback = nullptr;
// PFNGLENDTRANSFORMFEEDBACKPROC                glEndTransformFeedback = nullptr;
PFNGLBINDBUFFERRANGEPROC                     glBindBufferRange = nullptr;
// PFNGLBINDBUFFERBASEPROC                      glBindBufferBase = nullptr;
// PFNGLTRANSFORMFEEDBACKVARYINGSPROC           glTransformFeedbackVaryings = nullptr;
// PFNGLGETTRANSFORMFEEDBACKVARYINGPROC         glGetTransformFeedbackVarying = nullptr;
// PFNGLCLAMPCOLORPROC                          glClampColor = nullptr;
// PFNGLBEGINCONDITIONALRENDERPROC              glBeginConditionalRender = nullptr;
// PFNGLENDCONDITIONALRENDERPROC                glEndConditionalRender = nullptr;
// PFNGLVERTEXATTRIBIPOINTERPROC                glVertexAttribIPointer = nullptr;
// PFNGLGETVERTEXATTRIBIIVPROC                  glGetVertexAttribIiv = nullptr;
// PFNGLGETVERTEXATTRIBIUIVPROC                 glGetVertexAttribIuiv = nullptr;
// PFNGLVERTEXATTRIBI1IPROC                     glVertexAttribI1i = nullptr;
// PFNGLVERTEXATTRIBI2IPROC                     glVertexAttribI2i = nullptr;
// PFNGLVERTEXATTRIBI3IPROC                     glVertexAttribI3i = nullptr;
// PFNGLVERTEXATTRIBI4IPROC                     glVertexAttribI4i = nullptr;
// PFNGLVERTEXATTRIBI1UIPROC                    glVertexAttribI1ui = nullptr;
// PFNGLVERTEXATTRIBI2UIPROC                    glVertexAttribI2ui = nullptr;
// PFNGLVERTEXATTRIBI3UIPROC                    glVertexAttribI3ui = nullptr;
// PFNGLVERTEXATTRIBI4UIPROC                    glVertexAttribI4ui = nullptr;
// PFNGLVERTEXATTRIBI1IVPROC                    glVertexAttribI1iv = nullptr;
// PFNGLVERTEXATTRIBI2IVPROC                    glVertexAttribI2iv = nullptr;
// PFNGLVERTEXATTRIBI3IVPROC                    glVertexAttribI3iv = nullptr;
// PFNGLVERTEXATTRIBI4IVPROC                    glVertexAttribI4iv = nullptr;
// PFNGLVERTEXATTRIBI1UIVPROC                   glVertexAttribI1uiv = nullptr;
// PFNGLVERTEXATTRIBI2UIVPROC                   glVertexAttribI2uiv = nullptr;
// PFNGLVERTEXATTRIBI3UIVPROC                   glVertexAttribI3uiv = nullptr;
// PFNGLVERTEXATTRIBI4UIVPROC                   glVertexAttribI4uiv = nullptr;
// PFNGLVERTEXATTRIBI4BVPROC                    glVertexAttribI4bv = nullptr;
// PFNGLVERTEXATTRIBI4SVPROC                    glVertexAttribI4sv = nullptr;
// PFNGLVERTEXATTRIBI4UBVPROC                   glVertexAttribI4ubv = nullptr;
// PFNGLVERTEXATTRIBI4USVPROC                   glVertexAttribI4usv = nullptr;
// PFNGLGETUNIFORMUIVPROC                       glGetUniformuiv = nullptr;
// PFNGLBINDFRAGDATALOCATIONPROC                glBindFragDataLocation = nullptr;
// PFNGLGETFRAGDATALOCATIONPROC                 glGetFragDataLocation = nullptr;
// PFNGLUNIFORM1UIPROC                          glUniform1ui = nullptr;
// PFNGLUNIFORM2UIPROC                          glUniform2ui = nullptr;
// PFNGLUNIFORM3UIPROC                          glUniform3ui = nullptr;
// PFNGLUNIFORM4UIPROC                          glUniform4ui = nullptr;
// PFNGLUNIFORM1UIVPROC                         glUniform1uiv = nullptr;
// PFNGLUNIFORM2UIVPROC                         glUniform2uiv = nullptr;
// PFNGLUNIFORM3UIVPROC                         glUniform3uiv = nullptr;
// PFNGLUNIFORM4UIVPROC                         glUniform4uiv = nullptr;
// PFNGLTEXPARAMETERIIVPROC                     glTexParameterIiv = nullptr;
// PFNGLTEXPARAMETERIUIVPROC                    glTexParameterIuiv = nullptr;
// PFNGLGETTEXPARAMETERIIVPROC                  glGetTexParameterIiv = nullptr;
// PFNGLGETTEXPARAMETERIUIVPROC                 glGetTexParameterIuiv = nullptr;
// PFNGLCLEARBUFFERIVPROC                       glClearBufferiv = nullptr;
// PFNGLCLEARBUFFERUIVPROC                      glClearBufferuiv = nullptr;
// PFNGLCLEARBUFFERFVPROC                       glClearBufferfv = nullptr;
// PFNGLCLEARBUFFERFIPROC                       glClearBufferfi = nullptr;
// PFNGLGETSTRINGIPROC                          glGetStringi = nullptr;
// PFNGLISRENDERBUFFERPROC                      glIsRenderbuffer = nullptr;
PFNGLBINDRENDERBUFFERPROC                    glBindRenderbuffer = nullptr;
PFNGLDELETERENDERBUFFERSPROC                 glDeleteRenderbuffers = nullptr;
PFNGLGENRENDERBUFFERSPROC                    glGenRenderbuffers = nullptr;
// PFNGLRENDERBUFFERSTORAGEPROC                 glRenderbufferStorage = nullptr;
// PFNGLGETRENDERBUFFERPARAMETERIVPROC          glGetRenderbufferParameteriv = nullptr;
// PFNGLISFRAMEBUFFERPROC                       glIsFramebuffer = nullptr;
PFNGLINVALIDATEFRAMEBUFFERPROC               glInvalidateFramebuffer = nullptr;
PFNGLBINDFRAMEBUFFERPROC                     glBindFramebuffer = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC                  glDeleteFramebuffers = nullptr;
PFNGLGENFRAMEBUFFERSPROC                     glGenFramebuffers = nullptr;
// PFNGLCHECKFRAMEBUFFERSTATUSPROC              glCheckFramebufferStatus = nullptr;
// PFNGLFRAMEBUFFERTEXTURE1DPROC                glFramebufferTexture1D = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC                glFramebufferTexture2D = nullptr;
// PFNGLFRAMEBUFFERTEXTURE3DPROC                glFramebufferTexture3D = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC             glFramebufferRenderbuffer = nullptr;
// PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameteriv = nullptr;
// PFNGLGENERATEMIPMAPPROC                      glGenerateMipmap = nullptr;
PFNGLBLITFRAMEBUFFERPROC                     glBlitFramebuffer = nullptr;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC      glRenderbufferStorageMultisample = nullptr;
// PFNGLFRAMEBUFFERTEXTURELAYERPROC             glFramebufferTextureLayer = nullptr;
// PFNGLMAPBUFFERRANGEPROC                      glMapBufferRange = nullptr;
// PFNGLFLUSHMAPPEDBUFFERRANGEPROC              glFlushMappedBufferRange = nullptr;
PFNGLBINDVERTEXARRAYPROC                     glBindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC                  glDeleteVertexArrays = nullptr;
PFNGLGENVERTEXARRAYSPROC                     glGenVertexArrays = nullptr;
// PFNGLISVERTEXARRAYPROC                       glIsVertexArray = nullptr;

// GL_VERSION_3_1
// PFNGLDRAWARRAYSINSTANCEDPROC       glDrawArraysInstanced = nullptr;
// PFNGLDRAWELEMENTSINSTANCEDPROC     glDrawElementsInstanced = nullptr;
// PFNGLTEXBUFFERPROC                 glTexBuffer = nullptr;
// PFNGLPRIMITIVERESTARTINDEXPROC     glPrimitiveRestartIndex = nullptr;
// PFNGLCOPYBUFFERSUBDATAPROC         glCopyBufferSubData = nullptr;
// PFNGLGETUNIFORMINDICESPROC         glGetUniformIndices = nullptr;
// PFNGLGETACTIVEUNIFORMSIVPROC       glGetActiveUniformsiv = nullptr;
// PFNGLGETACTIVEUNIFORMNAMEPROC      glGetActiveUniformName = nullptr;
PFNGLGETUNIFORMBLOCKINDEXPROC      glGetUniformBlockIndex = nullptr;
// PFNGLGETACTIVEUNIFORMBLOCKIVPROC   glGetActiveUniformBlockiv = nullptr;
// PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glGetActiveUniformBlockName = nullptr;
PFNGLUNIFORMBLOCKBINDINGPROC       glUniformBlockBinding = nullptr;

// windows
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__)
HMODULE libOpenGL = NULL;
typedef PROC(WINAPI *PFNGLGETPROCADDRESSPROC)(LPCSTR);
PFNGLGETPROCADDRESSPROC glGetProcAddress = NULL;
// load opengl libraries
bool openglInitLibs() {
    #ifndef THORVG_GL_TARGET_GLES
    if (!libOpenGL) libOpenGL = LoadLibraryW(L"opengl32.dll");
    if (!libOpenGL) return false;
    if (!glGetProcAddress) glGetProcAddress = (PFNGLGETPROCADDRESSPROC)GetProcAddress(libOpenGL, "wglGetProcAddress");
    if (!glGetProcAddress) glGetProcAddress = (PFNGLGETPROCADDRESSPROC)GetProcAddress(libOpenGL, "wglGetProcAddressARB");
    if (!glGetProcAddress) return false;
    #else
    if (!libOpenGL) libOpenGL = LoadLibraryW(L"libGLESv2.dll");
    if (!libOpenGL) return false;
    #endif
    return true;
}
// load opengl proc address from dll or from wglGetProcAddress
PROC openglGetProcAddress(const char* procName) {
    PROC procHandle = GetProcAddress(libOpenGL, procName);
    #ifndef THORVG_GL_TARGET_GLES
    if (!procHandle) procHandle = glGetProcAddress(procName);
    #endif
    return procHandle;
}
// linux
#elif defined(__linux__)
#include <dlfcn.h>
void* libOpenGL = nullptr;
typedef void* (APIENTRYP PFNGLGETPROCADDRESSPROC)(const char*);
PFNGLGETPROCADDRESSPROC glGetProcAddress = nullptr;
// load opengl libraries
bool openglInitLibs() {
    #ifndef THORVG_GL_TARGET_GLES
    if (!libOpenGL) libOpenGL = dlopen("libGL.so", RTLD_NOW);
    if (!libOpenGL) libOpenGL = dlopen("libGL.so.0", RTLD_NOW);
    if (!libOpenGL) libOpenGL = dlopen("libGL.so.1", RTLD_NOW);
    if (!libOpenGL) libOpenGL = dlopen("libGL.so.2", RTLD_NOW);
    if (!libOpenGL) libOpenGL = dlopen("libGL.so.3", RTLD_NOW);
    if (!libOpenGL) return false;
    if (!glGetProcAddress) glGetProcAddress = (PFNGLGETPROCADDRESSPROC)dlsym(libOpenGL, "glXGetProcAddress");
    if (!glGetProcAddress) glGetProcAddress = (PFNGLGETPROCADDRESSPROC)dlsym(libOpenGL, "glXGetProcAddressARB");
    if (!glGetProcAddress) return false;
    #else
    if (!libOpenGL) libOpenGL = dlopen("libGLESv2.so", RTLD_NOW);
    if (!libOpenGL) libOpenGL = dlopen("libGLESv2.so.0", RTLD_NOW);
    if (!libOpenGL) libOpenGL = dlopen("libGLESv2.so.1", RTLD_NOW);
    if (!libOpenGL) libOpenGL = dlopen("libGLESv2.so.2", RTLD_NOW);
    if (!libOpenGL) libOpenGL = dlopen("libGLESv2.so.2.0", RTLD_NOW);
    if (!libOpenGL) return false;
    #endif
    return true;
}
// load opengl proc address from glXGetProcAddress
void* openglGetProcAddress(const char* procName) {
    #ifndef THORVG_GL_TARGET_GLES
    return glGetProcAddress(procName);
    #else
    return dlsym(libOpenGL, procName);
    #endif
}
// macos
#elif defined(__APPLE__) || defined(__MACH__)
#include <dlfcn.h>
void* libOpenGL = nullptr;
// load opengl libraries
bool openglInitLibs() {
    if (!libOpenGL) libOpenGL = dlopen("/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_NOW);
    if (!libOpenGL) libOpenGL = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_NOW);
    if (!libOpenGL) libOpenGL = dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", RTLD_NOW);
    if (!libOpenGL) return false;
    return true;
}
// load opengl proc address from dylib
void* openglGetProcAddress(const char* procName) {
    return dlsym(libOpenGL, procName);
}
#endif

#define GL_FUNCTION_FETCH(procName, procType)                   \
    procName = (procType)openglGetProcAddress(#procName);       \
    if (!procName) {                                            \
        TVGLOG("GL_ENGINE", "%s is not supported.", #procName); \
        return false;                                           \
    }

bool initOpenGL() {
    if (!openglInitLibs()) return false;

    // GL_VERSION_1_0
    GL_FUNCTION_FETCH(glCullFace, PFNGLCULLFACEPROC);
    GL_FUNCTION_FETCH(glFrontFace, PFNGLFRONTFACEPROC);
    // GL_FUNCTION_FETCH(glHint, PFNGLHINTPROC);
    // GL_FUNCTION_FETCH(glLineWidth, PFNGLLINEWIDTHPROC);
    // GL_FUNCTION_FETCH(glPointSize, PFNGLPOINTSIZEPROC);
    // GL_FUNCTION_FETCH(glPolygonMode, PFNGLPOLYGONMODEPROC);
    GL_FUNCTION_FETCH(glScissor, PFNGLSCISSORPROC);
    // GL_FUNCTION_FETCH(glTexParameterf, PFNGLTEXPARAMETERFPROC);
    // GL_FUNCTION_FETCH(glTexParameterfv, PFNGLTEXPARAMETERFVPROC);
    GL_FUNCTION_FETCH(glTexParameteri, PFNGLTEXPARAMETERIPROC);
    // GL_FUNCTION_FETCH(glTexParameteriv, PFNGLTEXPARAMETERIVPROC);
    // GL_FUNCTION_FETCH(glTexImage1D, PFNGLTEXIMAGE1DPROC);
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
    // GL_FUNCTION_FETCH(glStencilMask, PFNGLSTENCILMASKPROC);
    GL_FUNCTION_FETCH(glColorMask, PFNGLCOLORMASKPROC);
    GL_FUNCTION_FETCH(glDepthMask, PFNGLDEPTHMASKPROC);
    GL_FUNCTION_FETCH(glDisable, PFNGLDISABLEPROC);
    GL_FUNCTION_FETCH(glEnable, PFNGLENABLEPROC);
    // GL_FUNCTION_FETCH(glFinish, PFNGLFINISHPROC);
    // GL_FUNCTION_FETCH(glFlush, PFNGLFLUSHPROC);
    GL_FUNCTION_FETCH(glBlendFunc, PFNGLBLENDFUNCPROC);
    // GL_FUNCTION_FETCH(glLogicOp, PFNGLLOGICOPPROC);
    GL_FUNCTION_FETCH(glStencilFunc, PFNGLSTENCILFUNCPROC);
    GL_FUNCTION_FETCH(glStencilOp, PFNGLSTENCILOPPROC);
    GL_FUNCTION_FETCH(glDepthFunc, PFNGLDEPTHFUNCPROC);
    // GL_FUNCTION_FETCH(glPixelStoref, PFNGLPIXELSTOREFPROC);
    // GL_FUNCTION_FETCH(glPixelStorei, PFNGLPIXELSTOREIPROC);
    // GL_FUNCTION_FETCH(glReadBuffer, PFNGLREADBUFFERPROC);
    // GL_FUNCTION_FETCH(glReadPixels, PFNGLREADPIXELSPROC);
    // GL_FUNCTION_FETCH(glGetBooleanv, PFNGLGETBOOLEANVPROC);
    // GL_FUNCTION_FETCH(glGetDoublev, PFNGLGETDOUBLEVPROC);
    GL_FUNCTION_FETCH(glGetError, PFNGLGETERRORPROC);
    // GL_FUNCTION_FETCH(glGetFloatv, PFNGLGETFLOATVPROC);
    GL_FUNCTION_FETCH(glGetIntegerv, PFNGLGETINTEGERVPROC);
    GL_FUNCTION_FETCH(glGetString, PFNGLGETSTRINGPROC);
    // GL_FUNCTION_FETCH(glGetTexImage, PFNGLGETTEXIMAGEPROC);
    // GL_FUNCTION_FETCH(glGetTexParameterfv, PFNGLGETTEXPARAMETERFVPROC);
    // GL_FUNCTION_FETCH(glGetTexParameteriv, PFNGLGETTEXPARAMETERIVPROC);
    // GL_FUNCTION_FETCH(glGetTexLevelParameterfv, PFNGLGETTEXLEVELPARAMETERFVPROC);
    // GL_FUNCTION_FETCH(glGetTexLevelParameteriv, PFNGLGETTEXLEVELPARAMETERIVPROC);
    // GL_FUNCTION_FETCH(glIsEnabled, PFNGLISENABLEDPROC);
    // GL_FUNCTION_FETCH(glDepthRange, PFNGLDEPTHRANGEPROC);
    GL_FUNCTION_FETCH(glViewport, PFNGLVIEWPORTPROC);

    // GL_VERSION_1_1
    // GL_FUNCTION_FETCH(glDrawArrays, PFNGLDRAWARRAYSPROC);
    GL_FUNCTION_FETCH(glDrawElements, PFNGLDRAWELEMENTSPROC);
    // GL_FUNCTION_FETCH(glGetPointerv, PFNGLGETPOINTERVPROC);
    // GL_FUNCTION_FETCH(glPolygonOffset, PFNGLPOLYGONOFFSETPROC);
    // GL_FUNCTION_FETCH(glCopyTexImage1D, PFNGLCOPYTEXIMAGE1DPROC);
    // GL_FUNCTION_FETCH(glCopyTexImage2D, PFNGLCOPYTEXIMAGE2DPROC);
    // GL_FUNCTION_FETCH(glCopyTexSubImage1D, PFNGLCOPYTEXSUBIMAGE1DPROC);
    // GL_FUNCTION_FETCH(glCopyTexSubImage2D, PFNGLCOPYTEXSUBIMAGE2DPROC);
    // GL_FUNCTION_FETCH(glTexSubImage1D, PFNGLTEXSUBIMAGE1DPROC);
    // GL_FUNCTION_FETCH(glTexSubImage2D, PFNGLTEXSUBIMAGE2DPROC);
    GL_FUNCTION_FETCH(glBindTexture, PFNGLBINDTEXTUREPROC);
    GL_FUNCTION_FETCH(glDeleteTextures, PFNGLDELETETEXTURESPROC);
    GL_FUNCTION_FETCH(glGenTextures, PFNGLGENTEXTURESPROC);
    // GL_FUNCTION_FETCH(glIsTexture, PFNGLISTEXTUREPROC);

    // // GL_VERSION_1_2
    // GL_FUNCTION_FETCH(glDrawRangeElements, PFNGLDRAWRANGEELEMENTSPROC);
    // GL_FUNCTION_FETCH(glTexImage3D, PFNGLTEXIMAGE3DPROC);
    // GL_FUNCTION_FETCH(glTexSubImage3D, PFNGLTEXSUBIMAGE3DPROC);
    // GL_FUNCTION_FETCH(glCopyTexSubImage3D, PFNGLCOPYTEXSUBIMAGE3DPROC);
    
    // // GL_VERSION_1_3
    GL_FUNCTION_FETCH(glActiveTexture, PFNGLACTIVETEXTUREPROC);
    // GL_FUNCTION_FETCH(glSampleCoverage, PFNGLSAMPLECOVERAGEPROC);
    // GL_FUNCTION_FETCH(glCompressedTexImage3D, PFNGLCOMPRESSEDTEXIMAGE3DPROC);
    // GL_FUNCTION_FETCH(glCompressedTexImage2D, PFNGLCOMPRESSEDTEXIMAGE2DPROC);
    // GL_FUNCTION_FETCH(glCompressedTexImage1D, PFNGLCOMPRESSEDTEXIMAGE1DPROC);
    // GL_FUNCTION_FETCH(glCompressedTexSubImage3D, PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC);
    // GL_FUNCTION_FETCH(glCompressedTexSubImage2D, PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC);
    // GL_FUNCTION_FETCH(glCompressedTexSubImage1D, PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC);
    // GL_FUNCTION_FETCH(glGetCompressedTexImage, PFNGLGETCOMPRESSEDTEXIMAGEPROC);
    
    // // GL_VERSION_1_4
    // GL_FUNCTION_FETCH(glBlendFuncSeparate, PFNGLBLENDFUNCSEPARATEPROC);
    // GL_FUNCTION_FETCH(glMultiDrawArrays, PFNGLMULTIDRAWARRAYSPROC);
    // GL_FUNCTION_FETCH(glMultiDrawElements, PFNGLMULTIDRAWELEMENTSPROC);
    // GL_FUNCTION_FETCH(glPointParameterf, PFNGLPOINTPARAMETERFPROC);
    // GL_FUNCTION_FETCH(glPointParameterfv, PFNGLPOINTPARAMETERFVPROC);
    // GL_FUNCTION_FETCH(glPointParameteri, PFNGLPOINTPARAMETERIPROC);
    // GL_FUNCTION_FETCH(glPointParameteriv, PFNGLPOINTPARAMETERIVPROC);
    // GL_FUNCTION_FETCH(glBlendColor, PFNGLBLENDCOLORPROC);
    GL_FUNCTION_FETCH(glBlendEquation, PFNGLBLENDEQUATIONPROC);

    // GL_VERSION_1_5
    // GL_FUNCTION_FETCH(glGenQueries, PFNGLGENQUERIESPROC);
    // GL_FUNCTION_FETCH(glDeleteQueries, PFNGLDELETEQUERIESPROC);
    // GL_FUNCTION_FETCH(glIsQuery, PFNGLISQUERYPROC);
    // GL_FUNCTION_FETCH(glBeginQuery, PFNGLBEGINQUERYPROC);
    // GL_FUNCTION_FETCH(glEndQuery, PFNGLENDQUERYPROC);
    // GL_FUNCTION_FETCH(glGetQueryiv, PFNGLGETQUERYIVPROC);
    // GL_FUNCTION_FETCH(glGetQueryObjectiv, PFNGLGETQUERYOBJECTIVPROC);
    // GL_FUNCTION_FETCH(glGetQueryObjectuiv, PFNGLGETQUERYOBJECTUIVPROC);
    GL_FUNCTION_FETCH(glBindBuffer, PFNGLBINDBUFFERPROC);
    GL_FUNCTION_FETCH(glDeleteBuffers, PFNGLDELETEBUFFERSPROC);
    GL_FUNCTION_FETCH(glGenBuffers, PFNGLGENBUFFERSPROC);
    // GL_FUNCTION_FETCH(glIsBuffer, PFNGLISBUFFERPROC);
    GL_FUNCTION_FETCH(glBufferData, PFNGLBUFFERDATAPROC);
    // GL_FUNCTION_FETCH(glBufferSubData, PFNGLBUFFERSUBDATAPROC);
    // GL_FUNCTION_FETCH(glGetBufferSubData, PFNGLGETBUFFERSUBDATAPROC);
    // GL_FUNCTION_FETCH(glMapBuffer, PFNGLMAPBUFFERPROC);
    // GL_FUNCTION_FETCH(glUnmapBuffer, PFNGLUNMAPBUFFERPROC);
    // GL_FUNCTION_FETCH(glGetBufferParameteriv, PFNGLGETBUFFERPARAMETERIVPROC);
    // GL_FUNCTION_FETCH(glGetBufferPointerv, PFNGLGETBUFFERPOINTERVPROC);

    // GL_VERSION_2_0
    // GL_FUNCTION_FETCH(glBlendEquationSeparate, PFNGLBLENDEQUATIONSEPARATEPROC);
    #if defined(THORVG_GL_TARGET_GLES)
    GL_FUNCTION_FETCH(glDrawBuffers, PFNGLDRAWBUFFERSPROC);
    #endif
    GL_FUNCTION_FETCH(glStencilOpSeparate, PFNGLSTENCILOPSEPARATEPROC);
    GL_FUNCTION_FETCH(glStencilFuncSeparate, PFNGLSTENCILFUNCSEPARATEPROC);
    // GL_FUNCTION_FETCH(glStencilMaskSeparate, PFNGLSTENCILMASKSEPARATEPROC);
    GL_FUNCTION_FETCH(glAttachShader, PFNGLATTACHSHADERPROC);
    // GL_FUNCTION_FETCH(glBindAttribLocation, PFNGLBINDATTRIBLOCATIONPROC);
    GL_FUNCTION_FETCH(glCompileShader, PFNGLCOMPILESHADERPROC);
    GL_FUNCTION_FETCH(glCreateProgram, PFNGLCREATEPROGRAMPROC);
    GL_FUNCTION_FETCH(glCreateShader, PFNGLCREATESHADERPROC);
    GL_FUNCTION_FETCH(glDeleteProgram, PFNGLDELETEPROGRAMPROC);
    GL_FUNCTION_FETCH(glDeleteShader, PFNGLDELETESHADERPROC);
    // GL_FUNCTION_FETCH(glDetachShader, PFNGLDETACHSHADERPROC);
    GL_FUNCTION_FETCH(glDisableVertexAttribArray, PFNGLDISABLEVERTEXATTRIBARRAYPROC);
    GL_FUNCTION_FETCH(glEnableVertexAttribArray, PFNGLENABLEVERTEXATTRIBARRAYPROC);
    // GL_FUNCTION_FETCH(glGetActiveAttrib, PFNGLGETACTIVEATTRIBPROC);
    // GL_FUNCTION_FETCH(glGetActiveUniform, PFNGLGETACTIVEUNIFORMPROC);
    // GL_FUNCTION_FETCH(glGetAttachedShaders, PFNGLGETATTACHEDSHADERSPROC);
    GL_FUNCTION_FETCH(glGetAttribLocation, PFNGLGETATTRIBLOCATIONPROC);
    GL_FUNCTION_FETCH(glGetProgramiv, PFNGLGETPROGRAMIVPROC);
    GL_FUNCTION_FETCH(glGetProgramInfoLog, PFNGLGETPROGRAMINFOLOGPROC);
    GL_FUNCTION_FETCH(glGetShaderiv, PFNGLGETSHADERIVPROC);
    GL_FUNCTION_FETCH(glGetShaderInfoLog, PFNGLGETSHADERINFOLOGPROC);
    // GL_FUNCTION_FETCH(glGetShaderSource, PFNGLGETSHADERSOURCEPROC);
    GL_FUNCTION_FETCH(glGetUniformLocation, PFNGLGETUNIFORMLOCATIONPROC);
    // GL_FUNCTION_FETCH(glGetUniformfv, PFNGLGETUNIFORMFVPROC);
    // GL_FUNCTION_FETCH(glGetUniformiv, PFNGLGETUNIFORMIVPROC);
    // GL_FUNCTION_FETCH(glGetVertexAttribdv, PFNGLGETVERTEXATTRIBDVPROC);
    // GL_FUNCTION_FETCH(glGetVertexAttribfv, PFNGLGETVERTEXATTRIBFVPROC);
    // GL_FUNCTION_FETCH(glGetVertexAttribiv, PFNGLGETVERTEXATTRIBIVPROC);
    // GL_FUNCTION_FETCH(glGetVertexAttribPointerv, PFNGLGETVERTEXATTRIBPOINTERVPROC);
    // GL_FUNCTION_FETCH(glIsProgram, PFNGLISPROGRAMPROC);
    // GL_FUNCTION_FETCH(glIsShader, PFNGLISSHADERPROC);
    GL_FUNCTION_FETCH(glLinkProgram, PFNGLLINKPROGRAMPROC);
    GL_FUNCTION_FETCH(glShaderSource, PFNGLSHADERSOURCEPROC);
    GL_FUNCTION_FETCH(glUseProgram, PFNGLUSEPROGRAMPROC);
    GL_FUNCTION_FETCH(glUniform1f, PFNGLUNIFORM1FPROC);
    // GL_FUNCTION_FETCH(glUniform2f, PFNGLUNIFORM2FPROC);
    // GL_FUNCTION_FETCH(glUniform3f, PFNGLUNIFORM3FPROC);
    // GL_FUNCTION_FETCH(glUniform4f, PFNGLUNIFORM4FPROC);
    // GL_FUNCTION_FETCH(glUniform1i, PFNGLUNIFORM1IPROC);
    // GL_FUNCTION_FETCH(glUniform2i, PFNGLUNIFORM2IPROC);
    // GL_FUNCTION_FETCH(glUniform3i, PFNGLUNIFORM3IPROC);
    // GL_FUNCTION_FETCH(glUniform4i, PFNGLUNIFORM4IPROC);
    GL_FUNCTION_FETCH(glUniform1fv, PFNGLUNIFORM1FVPROC);
    GL_FUNCTION_FETCH(glUniform2fv, PFNGLUNIFORM2FVPROC);
    GL_FUNCTION_FETCH(glUniform3fv, PFNGLUNIFORM3FVPROC);
    GL_FUNCTION_FETCH(glUniform4fv, PFNGLUNIFORM4FVPROC);
    GL_FUNCTION_FETCH(glUniform1iv, PFNGLUNIFORM1IVPROC);
    GL_FUNCTION_FETCH(glUniform2iv, PFNGLUNIFORM2IVPROC);
    GL_FUNCTION_FETCH(glUniform3iv, PFNGLUNIFORM3IVPROC);
    GL_FUNCTION_FETCH(glUniform4iv, PFNGLUNIFORM4IVPROC);
    // GL_FUNCTION_FETCH(glUniformMatrix2fv, PFNGLUNIFORMMATRIX2FVPROC);
    // GL_FUNCTION_FETCH(glUniformMatrix3fv, PFNGLUNIFORMMATRIX3FVPROC);
    GL_FUNCTION_FETCH(glUniformMatrix4fv, PFNGLUNIFORMMATRIX4FVPROC);
    // GL_FUNCTION_FETCH(glValidateProgram, PFNGLVALIDATEPROGRAMPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib1d, PFNGLVERTEXATTRIB1DPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib1dv, PFNGLVERTEXATTRIB1DVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib1f, PFNGLVERTEXATTRIB1FPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib1fv, PFNGLVERTEXATTRIB1FVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib1s, PFNGLVERTEXATTRIB1SPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib1sv, PFNGLVERTEXATTRIB1SVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib2d, PFNGLVERTEXATTRIB2DPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib2dv, PFNGLVERTEXATTRIB2DVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib2f, PFNGLVERTEXATTRIB2FPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib2fv, PFNGLVERTEXATTRIB2FVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib2s, PFNGLVERTEXATTRIB2SPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib2sv, PFNGLVERTEXATTRIB2SVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib3d, PFNGLVERTEXATTRIB3DPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib3dv, PFNGLVERTEXATTRIB3DVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib3f, PFNGLVERTEXATTRIB3FPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib3fv, PFNGLVERTEXATTRIB3FVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib3s, PFNGLVERTEXATTRIB3SPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib3sv, PFNGLVERTEXATTRIB3SVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4Nbv, PFNGLVERTEXATTRIB4NBVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4Niv, PFNGLVERTEXATTRIB4NIVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4Nsv, PFNGLVERTEXATTRIB4NSVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4Nub, PFNGLVERTEXATTRIB4NUBPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4Nubv, PFNGLVERTEXATTRIB4NUBVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4Nuiv, PFNGLVERTEXATTRIB4NUIVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4Nusv, PFNGLVERTEXATTRIB4NUSVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4bv, PFNGLVERTEXATTRIB4BVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4d, PFNGLVERTEXATTRIB4DPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4dv, PFNGLVERTEXATTRIB4DVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4f, PFNGLVERTEXATTRIB4FPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4fv, PFNGLVERTEXATTRIB4FVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4iv, PFNGLVERTEXATTRIB4IVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4s, PFNGLVERTEXATTRIB4SPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4sv, PFNGLVERTEXATTRIB4SVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4ubv, PFNGLVERTEXATTRIB4UBVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4uiv, PFNGLVERTEXATTRIB4UIVPROC);
    // GL_FUNCTION_FETCH(glVertexAttrib4usv, PFNGLVERTEXATTRIB4USVPROC);
    GL_FUNCTION_FETCH(glVertexAttribPointer, PFNGLVERTEXATTRIBPOINTERPROC);
    
    // // GL_VERSION_2_1
    // GL_FUNCTION_FETCH(glUniformMatrix2x3fv, PFNGLUNIFORMMATRIX2X3FVPROC);
    // GL_FUNCTION_FETCH(glUniformMatrix3x2fv, PFNGLUNIFORMMATRIX3X2FVPROC);
    // GL_FUNCTION_FETCH(glUniformMatrix2x4fv, PFNGLUNIFORMMATRIX2X4FVPROC);
    // GL_FUNCTION_FETCH(glUniformMatrix4x2fv, PFNGLUNIFORMMATRIX4X2FVPROC);
    // GL_FUNCTION_FETCH(glUniformMatrix3x4fv, PFNGLUNIFORMMATRIX3X4FVPROC);
    // GL_FUNCTION_FETCH(glUniformMatrix4x3fv, PFNGLUNIFORMMATRIX4X3FVPROC);
    
    // GL_VERSION_3_0
    // GL_FUNCTION_FETCH(glColorMaski, PFNGLCOLORMASKIPROC);
    // GL_FUNCTION_FETCH(glGetBooleani_v, PFNGLGETBOOLEANI_VPROC);
    // GL_FUNCTION_FETCH(glGetIntegeri_v, PFNGLGETINTEGERI_VPROC);
    // GL_FUNCTION_FETCH(glEnablei, PFNGLENABLEIPROC);
    // GL_FUNCTION_FETCH(glDisablei, PFNGLDISABLEIPROC);
    // GL_FUNCTION_FETCH(glIsEnabledi, PFNGLISENABLEDIPROC);
    // GL_FUNCTION_FETCH(glBeginTransformFeedback, PFNGLBEGINTRANSFORMFEEDBACKPROC);
    // GL_FUNCTION_FETCH(glEndTransformFeedback, PFNGLENDTRANSFORMFEEDBACKPROC);
    GL_FUNCTION_FETCH(glBindBufferRange, PFNGLBINDBUFFERRANGEPROC);
    // GL_FUNCTION_FETCH(glBindBufferBase, PFNGLBINDBUFFERBASEPROC);
    // GL_FUNCTION_FETCH(glTransformFeedbackVaryings, PFNGLTRANSFORMFEEDBACKVARYINGSPROC);
    // GL_FUNCTION_FETCH(glGetTransformFeedbackVarying, PFNGLGETTRANSFORMFEEDBACKVARYINGPROC);
    // GL_FUNCTION_FETCH(glClampColor, PFNGLCLAMPCOLORPROC);
    // GL_FUNCTION_FETCH(glBeginConditionalRender, PFNGLBEGINCONDITIONALRENDERPROC);
    // GL_FUNCTION_FETCH(glEndConditionalRender, PFNGLENDCONDITIONALRENDERPROC);
    // GL_FUNCTION_FETCH(glVertexAttribIPointer, PFNGLVERTEXATTRIBIPOINTERPROC);
    // GL_FUNCTION_FETCH(glGetVertexAttribIiv, PFNGLGETVERTEXATTRIBIIVPROC);
    // GL_FUNCTION_FETCH(glGetVertexAttribIuiv, PFNGLGETVERTEXATTRIBIUIVPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI1i, PFNGLVERTEXATTRIBI1IPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI2i, PFNGLVERTEXATTRIBI2IPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI3i, PFNGLVERTEXATTRIBI3IPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI4i, PFNGLVERTEXATTRIBI4IPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI1ui, PFNGLVERTEXATTRIBI1UIPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI2ui, PFNGLVERTEXATTRIBI2UIPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI3ui, PFNGLVERTEXATTRIBI3UIPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI4ui, PFNGLVERTEXATTRIBI4UIPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI1iv, PFNGLVERTEXATTRIBI1IVPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI2iv, PFNGLVERTEXATTRIBI2IVPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI3iv, PFNGLVERTEXATTRIBI3IVPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI4iv, PFNGLVERTEXATTRIBI4IVPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI1uiv, PFNGLVERTEXATTRIBI1UIVPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI2uiv, PFNGLVERTEXATTRIBI2UIVPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI3uiv, PFNGLVERTEXATTRIBI3UIVPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI4uiv, PFNGLVERTEXATTRIBI4UIVPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI4bv, PFNGLVERTEXATTRIBI4BVPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI4sv, PFNGLVERTEXATTRIBI4SVPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI4ubv, PFNGLVERTEXATTRIBI4UBVPROC);
    // GL_FUNCTION_FETCH(glVertexAttribI4usv, PFNGLVERTEXATTRIBI4USVPROC);
    // GL_FUNCTION_FETCH(glGetUniformuiv, PFNGLGETUNIFORMUIVPROC);
    // GL_FUNCTION_FETCH(glBindFragDataLocation, PFNGLBINDFRAGDATALOCATIONPROC);
    // GL_FUNCTION_FETCH(glGetFragDataLocation, PFNGLGETFRAGDATALOCATIONPROC);
    // GL_FUNCTION_FETCH(glUniform1ui, PFNGLUNIFORM1UIPROC);
    // GL_FUNCTION_FETCH(glUniform2ui, PFNGLUNIFORM2UIPROC);
    // GL_FUNCTION_FETCH(glUniform3ui, PFNGLUNIFORM3UIPROC);
    // GL_FUNCTION_FETCH(glUniform4ui, PFNGLUNIFORM4UIPROC);
    // GL_FUNCTION_FETCH(glUniform1uiv, PFNGLUNIFORM1UIVPROC);
    // GL_FUNCTION_FETCH(glUniform2uiv, PFNGLUNIFORM2UIVPROC);
    // GL_FUNCTION_FETCH(glUniform3uiv, PFNGLUNIFORM3UIVPROC);
    // GL_FUNCTION_FETCH(glUniform4uiv, PFNGLUNIFORM4UIVPROC);
    // GL_FUNCTION_FETCH(glTexParameterIiv, PFNGLTEXPARAMETERIIVPROC);
    // GL_FUNCTION_FETCH(glTexParameterIuiv, PFNGLTEXPARAMETERIUIVPROC);
    // GL_FUNCTION_FETCH(glGetTexParameterIiv, PFNGLGETTEXPARAMETERIIVPROC);
    // GL_FUNCTION_FETCH(glGetTexParameterIuiv, PFNGLGETTEXPARAMETERIUIVPROC);
    // GL_FUNCTION_FETCH(glClearBufferiv, PFNGLCLEARBUFFERIVPROC);
    // GL_FUNCTION_FETCH(glClearBufferuiv, PFNGLCLEARBUFFERUIVPROC);
    // GL_FUNCTION_FETCH(glClearBufferfv, PFNGLCLEARBUFFERFVPROC);
    // GL_FUNCTION_FETCH(glClearBufferfi, PFNGLCLEARBUFFERFIPROC);
    // GL_FUNCTION_FETCH(glGetStringi, PFNGLGETSTRINGIPROC);
    // GL_FUNCTION_FETCH(glIsRenderbuffer, PFNGLISRENDERBUFFERPROC);
    GL_FUNCTION_FETCH(glBindRenderbuffer, PFNGLBINDRENDERBUFFERPROC);
    GL_FUNCTION_FETCH(glDeleteRenderbuffers, PFNGLDELETERENDERBUFFERSPROC);
    GL_FUNCTION_FETCH(glGenRenderbuffers, PFNGLGENRENDERBUFFERSPROC);
    // GL_FUNCTION_FETCH(glRenderbufferStorage, PFNGLRENDERBUFFERSTORAGEPROC);
    // GL_FUNCTION_FETCH(glGetRenderbufferParameteriv, PFNGLGETRENDERBUFFERPARAMETERIVPROC);
    // GL_FUNCTION_FETCH(glIsFramebuffer, PFNGLISFRAMEBUFFERPROC);
    #if defined(THORVG_GL_TARGET_GLES)
    GL_FUNCTION_FETCH(glInvalidateFramebuffer, PFNGLINVALIDATEFRAMEBUFFERPROC);
    #endif
    GL_FUNCTION_FETCH(glBindFramebuffer, PFNGLBINDFRAMEBUFFERPROC);
    GL_FUNCTION_FETCH(glDeleteFramebuffers, PFNGLDELETEFRAMEBUFFERSPROC);
    GL_FUNCTION_FETCH(glGenFramebuffers, PFNGLGENFRAMEBUFFERSPROC);
    // GL_FUNCTION_FETCH(glCheckFramebufferStatus, PFNGLCHECKFRAMEBUFFERSTATUSPROC);
    // GL_FUNCTION_FETCH(glFramebufferTexture1D, PFNGLFRAMEBUFFERTEXTURE1DPROC);
    GL_FUNCTION_FETCH(glFramebufferTexture2D, PFNGLFRAMEBUFFERTEXTURE2DPROC);
    // GL_FUNCTION_FETCH(glFramebufferTexture3D, PFNGLFRAMEBUFFERTEXTURE3DPROC);
    GL_FUNCTION_FETCH(glFramebufferRenderbuffer, PFNGLFRAMEBUFFERRENDERBUFFERPROC);
    // GL_FUNCTION_FETCH(glGetFramebufferAttachmentParameteriv, PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC);
    // GL_FUNCTION_FETCH(glGenerateMipmap, PFNGLGENERATEMIPMAPPROC);
    GL_FUNCTION_FETCH(glBlitFramebuffer, PFNGLBLITFRAMEBUFFERPROC);
    GL_FUNCTION_FETCH(glRenderbufferStorageMultisample, PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC);
    // GL_FUNCTION_FETCH(glFramebufferTextureLayer, PFNGLFRAMEBUFFERTEXTURELAYERPROC);
    // GL_FUNCTION_FETCH(glMapBufferRange, PFNGLMAPBUFFERRANGEPROC);
    // GL_FUNCTION_FETCH(glFlushMappedBufferRange, PFNGLFLUSHMAPPEDBUFFERRANGEPROC);
    GL_FUNCTION_FETCH(glBindVertexArray, PFNGLBINDVERTEXARRAYPROC);
    GL_FUNCTION_FETCH(glDeleteVertexArrays, PFNGLDELETEVERTEXARRAYSPROC);
    GL_FUNCTION_FETCH(glGenVertexArrays, PFNGLGENVERTEXARRAYSPROC);
    // GL_FUNCTION_FETCH(glIsVertexArray, PFNGLISVERTEXARRAYPROC);
    
    // GL_VERSION_3_1
    // GL_FUNCTION_FETCH(glDrawArraysInstanced, PFNGLDRAWARRAYSINSTANCEDPROC);
    // GL_FUNCTION_FETCH(glDrawElementsInstanced, PFNGLDRAWELEMENTSINSTANCEDPROC);
    // GL_FUNCTION_FETCH(glTexBuffer, PFNGLTEXBUFFERPROC);
    // GL_FUNCTION_FETCH(glPrimitiveRestartIndex, PFNGLPRIMITIVERESTARTINDEXPROC);
    // GL_FUNCTION_FETCH(glCopyBufferSubData, PFNGLCOPYBUFFERSUBDATAPROC);
    // GL_FUNCTION_FETCH(glGetUniformIndices, PFNGLGETUNIFORMINDICESPROC);
    // GL_FUNCTION_FETCH(glGetActiveUniformsiv, PFNGLGETACTIVEUNIFORMSIVPROC);
    // GL_FUNCTION_FETCH(glGetActiveUniformName, PFNGLGETACTIVEUNIFORMNAMEPROC);
    GL_FUNCTION_FETCH(glGetUniformBlockIndex, PFNGLGETUNIFORMBLOCKINDEXPROC);
    // GL_FUNCTION_FETCH(glGetActiveUniformBlockiv, PFNGLGETACTIVEUNIFORMBLOCKIVPROC);
    // GL_FUNCTION_FETCH(glGetActiveUniformBlockName, PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC);
    GL_FUNCTION_FETCH(glUniformBlockBinding, PFNGLUNIFORMBLOCKBINDINGPROC);

    return true;
};
#endif // __EMSCRIPTEN__