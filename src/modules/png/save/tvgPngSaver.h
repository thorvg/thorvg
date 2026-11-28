/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_PNGSAVER_H_
#define _TVG_PNGSAVER_H_

#include "tvgSaveModule.h"
#include "tvgTaskScheduler.h"

namespace tvg
{

struct PngSaver : SaveModule, Task
{
    ~PngSaver();

    bool save(Paint* paint, Paint* bg, const char* filename, uint32_t quality) override;
    bool save(Animation* animation, Paint* bg, const char* filename, uint32_t quality, uint32_t fps) override;
    bool close() override;

private:
    uint32_t* buffer = nullptr;
    Scene* scene = nullptr;
    Paint* bg = nullptr;
    char* path = nullptr;
    uint32_t size[2] = {0, 0};
    uint32_t quality = 100;

    void run(unsigned tid) override;
};

}

#endif  //_TVG_PNGSAVER_H_
