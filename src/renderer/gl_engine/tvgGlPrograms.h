/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#ifndef _TVG_GL_PROGRAMS_H_
#define _TVG_GL_PROGRAMS_H_

#include "tvgGlCommon.h"

class GlProgram
{
public:
    GLuint vert{};
    GLuint frag{};
    GLuint prog{};

    GlProgram(){};
    GlProgram(const char* vsrc, const char* fsrc);
    ~GlProgram();

    void load();

    int32_t getUniformLocation(const char* name);
    int32_t getUniformBlockIndex(const char* name);
private:
    static GLuint shaderCreate(GLenum type, const char* src);
    static bool shaderStatusCheck(GLuint shader);
    static bool programStatusCheck(GLuint program);
};

class GlPrograms {
public:
    bool inited{};
    // service programs
    GlProgram* stencil{};
    GlProgram* blit{};
    // normal blend programs
    GlProgram* color{};
    GlProgram* linear{};
    GlProgram* radial{};
    GlProgram* image{};
private:
    // compose programs
    GlProgram* compose[11]{};
    // blend programs
    GlProgram* color_blend[17]{};
    GlProgram* grad_blend[17]{};
    GlProgram* image_blend[17]{};
    GlProgram* scene_blend[17]{};
    // shader helper functions
    const char* getBlendHelpers(BlendMethod method);
    // shared shader sources
    static const char* composeFragShaders[11];
    static const char* blendFuncs[17];
public:
    GlPrograms() {};
    ~GlPrograms(){ term(); };
    void init();
    void term();
    // compose programs
    GlProgram* getCompose(MaskMethod method);
    // blend programs
    GlProgram* getBlendColor(BlendMethod method);
    GlProgram* getBlendGrad(BlendMethod method);
    GlProgram* getBlendImage(BlendMethod method);
    GlProgram* getBlendScene(BlendMethod method);
};

#endif /* _TVG_GL_PROGRAMS_H_ */
