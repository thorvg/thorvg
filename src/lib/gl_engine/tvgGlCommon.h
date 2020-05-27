#ifndef _TVG_GL_COMMON_H_
#define _TVG_GL_COMMON_H_

#include "tvgCommon.h"
#include "tvgGlProgram.h"
#include "tvgGlShader.h"
#include "tvgGlGeometry.h"


struct GlShape
{
  float             viewWd;
  float             viewHt;
  RenderUpdateFlag  updateFlag;
  unique_ptr<GlGeometry> geometry;
};


#endif /* _TVG_GL_COMMON_H_ */
