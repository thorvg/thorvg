#ifndef _TVG_GL_COMMON_H_
#define _TVG_GL_COMMON_H_

#include "tvgCommon.h"


#define GL_CHECK(x) \
        x; \
        do { \
          GLenum glError = glGetError(); \
          if(glError != GL_NO_ERROR) { \
            printf("glGetError() = %i (0x%.8x) at line %s : %i\n", glError, glError, __FILE__, __LINE__); \
            assert(0); \
          } \
        } while(0)

#define EGL_CHECK(x) \
    x; \
    do { \
        EGLint eglError = eglGetError(); \
        if(eglError != EGL_SUCCESS) { \
            printf("eglGetError() = %i (0x%.8x) at line %s : %i\n", eglError, eglError, __FILE__, __LINE__); \
            assert(0); \
        } \
    } while(0)


class GlGeometry;

struct GlShape
{
  float             viewWd;
  float             viewHt;
  RenderUpdateFlag  updateFlag;
  unique_ptr<GlGeometry> geometry;
};


#endif /* _TVG_GL_COMMON_H_ */
