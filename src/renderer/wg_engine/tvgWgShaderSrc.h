/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

#ifndef _TVG_WG_SHADER_SRC_H_
#define _TVG_WG_SHADER_SRC_H_

// graphics shader sources
extern const char* cShaderSrc_Stencil;
extern const char* cShaderSrc_Solid;
extern const char* cShaderSrc_Linear;
extern const char* cShaderSrc_Radial;
extern const char* cShaderSrc_Image;

// compute shader sources: blend, compose and merge path
extern const char* cShaderSrc_MergeMasks;
extern const char* cShaderSrc_Blend_Solid[14];
extern const char* cShaderSrc_Blend_Solid[14];
extern const char* cShaderSrc_Blend_Gradient[14];
extern const char* cShaderSrc_Blend_Image[14];
extern const char* cShaderSrc_Compose[10];
extern const char* cShaderSrc_Copy;

#endif // _TVG_WG_SHEDER_SRC_H_
