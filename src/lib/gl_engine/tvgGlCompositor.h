#ifndef _TVG_GL_COMPOSITOR_H_
#define _TVG_GL_COMPOSITOR_H_

#include "thorvg.h"

#include "tvgGlCommon.h"
#include "tvgRender.h"

struct GlCompositor : public tvg::Compositor
{

    GlCompositor(uint32_t width, uint32_t height);
    ~GlCompositor();

    GLuint fboId[3];

    GLuint texId[2];

    GLuint msaaTexId[2];

    GLuint targetFbo() const;
    GLuint sourceFbo() const;
    GLuint targetTex() const;
    GLuint sourceTex() const;

    void resolveMSAA();
private:
    void init(uint32_t width, uint32_t height);

    uint32_t mWidth;
    uint32_t mHeight;
};


#endif  // _TVG_GL_COMPOSITOR_H_
