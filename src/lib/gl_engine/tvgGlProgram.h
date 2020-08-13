/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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

#ifndef _TVG_GL_PROGRAM_H_
#define _TVG_GL_PROGRAM_H_

#include "tvgGlShader.h"

#include <memory>
#include <map>

class GlProgram
{
public:
    static std::unique_ptr<GlProgram> gen(std::shared_ptr<GlShader> shader);
    GlProgram(std::shared_ptr<GlShader> shader);
    ~GlProgram();

    void load();
    void unload();
    int32_t getAttributeLocation(const char* name);
    int32_t getUniformLocation(const char* name);
    void setUniformValue(int32_t location, float r, float g, float b, float a);

private:

    void linkProgram(std::shared_ptr<GlShader> shader);
    uint32_t mProgramObj;
    static uint32_t mCurrentProgram;

    static std::map<string, int32_t> mAttributeBuffer;
    static std::map<string, int32_t> mUniformBuffer;
};

#endif /* _TVG_GL_PROGRAM_H_ */
