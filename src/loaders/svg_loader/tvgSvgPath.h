#ifndef _TVG_SVG_PATH_H_
#define _TVG_SVG_PATH_H_

#include "tvgCommon.h"

tuple<vector<tvg::PathCommand>, vector<tvg::Point>> svgPathToTvgPath(const char* svg_path_data);

#endif //_TVG_SVG_PATH_H_
