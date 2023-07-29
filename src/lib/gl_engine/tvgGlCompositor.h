#ifndef _TVG_GL_COMPOSITOR_H_
#define _TVG_GL_COMPOSITOR_H_

#include "thorvg.h"

#include "tvgGlCommon.h"
#include "tvgRender.h"

struct GlCompositor : public tvg::Compositor
{

    GlCompositor(uint32_t width, uint32_t height);
    ~GlCompositor();

    GLuint fboId;
    GLuint texId;

    GlCompositor* previous;

private:
    void init(uint32_t width, uint32_t height);
};


#endif  // _TVG_GL_COMPOSITOR_H_
