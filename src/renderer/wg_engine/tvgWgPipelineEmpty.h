/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _TVG_WG_PIPELINE_EMPTY_H_
#define _TVG_WG_PIPELINE_EMPTY_H_

#include "tvgWgPipelineBase.h"

class WgPipelineEmpty;

struct WgPipelineDataEmpty: WgPipelineData {};

class WgPipelineBindGroupEmpty: public WgPipelineBindGroup {
public:
    void initialize(WGPUDevice device, WgPipelineEmpty& pipelinePipelineEmpty);
    void release();

    void update(WGPUQueue mQueue, WgPipelineDataEmpty& pipelineDataSolid);
};

/*
* This pipeline is used for drawing filled, concave polygons using the stencil buffer
* This can be done using the stencil buffer, with a two-pass algorithm.
*
* First, clear the stencil buffer and disable writing into the color buffer. Next, draw each of the triangles in turn, using the INVERT function in the stencil buffer. (For best performance, use triangle fans.)
* This flips the value between zero and a nonzero value every time a triangle is drawn that covers a pixel. 
*
* After all the triangles are drawn, if a pixel is covered an even number of times, the value in the stencil buffers is zero; otherwise, it's nonzero.
* Finally, draw a large polygon over the whole region (or redraw the triangles), but allow drawing only where the stencil buffer is nonzero.
*
* There is a link to the solution, how to draw filled, concave polygons using the stencil buffer:
* https://www.glprogramming.com/red/chapter14.html#name13
*
* The benefit of this solution is to don`t use complex tesselation to fill self intersected or concave poligons.
* 
* This pipeline implements the first pass of this solution. It`s did not write anything into color buffer but fills the stencil buffer using invert strategy
*/
class WgPipelineEmpty: public WgPipelineBase {
public:
    void initialize(WGPUDevice device) override;
    void release() override;
};

#endif // _TVG_WG_PIPELINE_EMPTY_H_
