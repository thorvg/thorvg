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

#include "tvgGlCommon.h"
#include "tvgGlProgram.h"

#include <GLES2/gl2.h>


static std::vector<string> gStdAttributes = {
    "aLocation"
};

static std::vector<string> gStdUniforms = {
    "uColor"
};

uint32_t GlProgram::mCurrentProgram = 0;
map<string, int32_t> GlProgram::mAttributeBuffer;
map<string, int32_t> GlProgram::mUniformBuffer;


unique_ptr<GlProgram> GlProgram::gen(std::shared_ptr<GlShader> shader)
{
    return make_unique<GlProgram>(shader);
}


GlProgram::GlProgram(std::shared_ptr<GlShader> shader)
{
    linkProgram(shader);
    load();

    for (auto name : gStdAttributes)
    {
        getAttributeLocation(name.c_str());
    }
    for (auto name : gStdUniforms)
    {
        getUniformLocation(name.c_str());
    }

}


GlProgram::~GlProgram()
{
    if (mCurrentProgram == mProgramObj)
    {
        unload();
    }
    glDeleteProgram(mProgramObj);
}


void GlProgram::load()
{
    if (mCurrentProgram == mProgramObj)
    {
        return;
    }

    mCurrentProgram = mProgramObj;
    GL_CHECK(glUseProgram(mProgramObj));

}


void GlProgram::unload()
{
    mCurrentProgram = 0;
}


int32_t GlProgram::getAttributeLocation(const char* name)
{
    if (mAttributeBuffer.find(name) != mAttributeBuffer.end())
    {
        return mAttributeBuffer[name];
    }
    GL_CHECK(int32_t location = glGetAttribLocation(mCurrentProgram, name));
    if (location != -1)
    {
        mAttributeBuffer[name] = location;
    }
    return location;
}


int32_t GlProgram::getUniformLocation(const char* name)
{
    if (mUniformBuffer.find(name) != mUniformBuffer.end())
    {
        return mUniformBuffer[name];
    }
    GL_CHECK(int32_t location = glGetUniformLocation(mCurrentProgram, name));
    if (location != -1)
    {
        mUniformBuffer[name] = location;
    }
    return location;

}


void GlProgram::setUniformValue(int32_t location, float r, float g, float b, float a)
{
    glUniform4f(location, r, g, b, a);
}


void GlProgram::linkProgram(std::shared_ptr<GlShader> shader)
{
    GLint linked;

    // Create the program object
    uint32_t progObj = glCreateProgram();
    assert(progObj);

    glAttachShader(progObj, shader->getVertexShader());
    glAttachShader(progObj, shader->getFragmentShader());

    // Link the program
    glLinkProgram(progObj);

    // Check the link status
    glGetProgramiv(progObj, GL_LINK_STATUS, &linked);

    if (!linked)
    {
        GLint infoLen = 0;
        glGetProgramiv(progObj, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0)
        {
            char* infoLog = new char[infoLen];
            glGetProgramInfoLog(progObj, infoLen, NULL, infoLog);
            std::cout << "Error linking shader: " << infoLog << std::endl;
            delete[] infoLog;

        }
        glDeleteProgram(progObj);
        progObj = 0;
        assert(0);
    }
    mProgramObj = progObj;
}

