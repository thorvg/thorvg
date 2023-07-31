#ifndef _TVG_GL_COMPOSITOR_H_
#define _TVG_GL_COMPOSITOR_H_

#include "thorvg.h"

#include "tvgGlCommon.h"
#include "tvgRender.h"

struct GlCompositor : public tvg::Compositor
{

    GlCompositor(uint32_t width, uint32_t height);
    ~GlCompositor();

    GLuint fboId[2];

    GLuint texId[2];

    GLuint targetFbo() const;
    GLuint sourceFbo() const;
    GLuint targetTex() const;
    GLuint sourceTex() const;

private:
    void init(uint32_t width, uint32_t height);
};


#endif  // _TVG_GL_COMPOSITOR_H_
