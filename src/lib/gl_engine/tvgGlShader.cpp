#include "tvgGlCommon.h"
#include "tvgGlShader.h"

#include <GLES2/gl2.h>


shared_ptr<GlShader> GlShader::gen(const char * vertSrc, const char * fragSrc)
{
    shared_ptr<GlShader> shader = make_shared<GlShader>();
    shader->createShader(vertSrc, fragSrc);
    return shader;
}


GlShader::~GlShader()
{
    glDeleteShader(mVtShader);
    glDeleteShader(mFrShader);
}

uint32_t GlShader::getVertexShader()
{
    return mVtShader;
}


uint32_t GlShader::getFragmentShader()
{
    return mFrShader;
}


void GlShader::createShader(const char* vertSrc, const char* fragSrc)
{
    mVtShader = complileShader(GL_VERTEX_SHADER, const_cast<char*>(vertSrc));
    mFrShader = complileShader(GL_FRAGMENT_SHADER, const_cast<char*>(fragSrc));
}


uint32_t GlShader::complileShader(uint32_t type, char* shaderSrc)
{
    GLuint shader;
    GLint compiled;

    // Create the shader object
    shader = glCreateShader(type);
    assert(shader);

    // Load the shader source
    glShaderSource(shader, 1, &shaderSrc, NULL);

    // Compile the shader
    glCompileShader(shader);

    // Check the compile status
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled)
    {
        GLint infoLen = 0;

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

        if (infoLen > 0)
        {
            char* infoLog = new char[infoLen];
            glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
            std::cout << "Error compiling shader: " << infoLog << std::endl;
            delete[] infoLog;
        }
        glDeleteShader(shader);
        assert(0);
    }

    return shader;
}

