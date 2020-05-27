#ifndef _TVG_GL_PROGRAM_H_
#define _TVG_GL_PROGRAM_H_

#include "tvgGlShader.h"

#include <map>


class GlProgram
{
public:
    GlProgram(shared_ptr<GlShader> shader);
    void create();
    void load();
    int32_t getAttributeLocation(const char* name);
    int32_t getUniformLocation(const char* name);
    void setUniformValue(int32_t location, float r, float g, float b, float a);

private:
    void linkProgram();
    std::shared_ptr<GlShader> mShader;
    uint32_t mProgramObj;
    static uint32_t mCurrentProgram;

    static std::map<string, int32_t> mAttributeBuffer;
    static std::map<string, int32_t> mUniformBuffer;
};

#endif /* _TVG_GL_PROGRAM_H_ */
