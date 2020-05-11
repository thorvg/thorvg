#ifndef _TVG_GL_SHADER_H_
#define _TVG_GL_SHADER_H_

class GlShader
{
public:
    static shared_ptr<GlShader> gen(const char * vertSrc, const char * fragSrc);

    uint32_t getVertexShader();
    uint32_t getFragmentShader();

private:
    void    createShader(const char* vertSrc, const char* fragSrc);
    uint32_t complileShader(uint32_t type, char* shaderSrc);

    uint32_t mVtShader;
    uint32_t mFrShader;
};

#endif /* _TVG_GL_SHADER_H_ */
