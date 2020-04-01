/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#ifndef _TVG_GL_RASTER_H_
#define _TVG_GL_RASTER_H_

namespace tvg
{

class GlRaster : public RasterMethod
{
public:
    int prepare(ShapeNode *shape) override;
    static GlRaster* inst();
    static int init();
    static int term();

private:
    GlRaster(){};
    ~GlRaster(){};
};

}

#endif /* _TVG_GL_RASTER_H_ */
