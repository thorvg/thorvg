/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_GL_GRADIENT_H_
#define _TVG_GL_GRADIENT_H_

#include "tvgArray.h"
#include "tvgGl.h"
#include "tvgRender.h"

#define GL_TEXTURE_GRADIENT_SIZE 512

struct GlGradientAtlas
{
    GLuint texId = 0;

    void update(uint32_t& slot, const Fill* fill);
    void flush();
    void invalidate();
    void release(uint32_t& slot);
    void clear();
    float rowCenter(uint32_t slot) const;

    struct Slot {
        bool used = false;
        uint8_t data[GL_TEXTURE_GRADIENT_SIZE * 4] = {};
    };

    uint32_t height = 0;
    Array<Slot> slots;
    Array<uint32_t> freeSlots;
    uint32_t dirtyMin = UINT32_MAX;
    uint32_t dirtyMax = 0;
    bool fullUpload = false;
};

#endif /* _TVG_GL_GRADIENT_H_ */
