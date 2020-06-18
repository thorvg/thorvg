#include "tvgGlShaderSrc.h"

const char* COLOR_VERT_SHADER =
"attribute highp vec4 aLocation;                \n"
"uniform highp vec4 uColor;                     \n"
"varying highp vec4 vcolor;                     \n"
"varying highp float vOpacity;                  \n"
"void main()                                    \n"
"{                                              \n"
"   gl_Position = vec4(aLocation.xy, 0.0, 1.0); \n"
"   vcolor = uColor;                            \n"
"   vOpacity = aLocation.z;                     \n"
"}                                              \n";

const char* COLOR_FRAG_SHADER =
"varying highp vec4 vcolor;                             \n"
"varying highp float vOpacity;                          \n"
"void main()                                            \n"
"{                                                      \n"
"  gl_FragColor = vec4(vcolor.xyz, vcolor.w*vOpacity);  \n"
"}                                                      \n";

