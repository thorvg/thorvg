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

#ifndef _TVG_SVG_SAVER_H_
#define _TVG_SVG_SAVER_H_

#include "tvgSaveModule.h"
#include "tvgTaskScheduler.h"
#include "tvgRender.h"
#include "tvgSvgWriter.h"

namespace tvg
{

struct SvgSaver : SaveModule, Task
{
    ~SvgSaver();

    bool save(Paint* paint, Paint* bg, const char* filename, uint32_t quality) override;
    bool save(Animation* animation, Paint* bg, const char* filename, uint32_t quality, uint32_t fps) override;
    bool close() override;

private:
    SvgWriter writer;
    char* path = nullptr;
    float vsize[2] = {0.0f, 0.0f};

    void region(const Matrix& ctm, Point& min, Point& max);
    uint32_t writeGradient(const Fill* fill, float scale);
    uint32_t writeEffects(const Array<RenderEffect*>& effects, const Matrix& ctm);
    string writeClip(const Shape* clipper, const Matrix& ctm);
    uint32_t writeMask(const Paint* target, MaskMethod method, const Matrix& ctm, bool masking = true);
    void writeCompound(string& out, const Paint* paint, const Paint* target, MaskMethod method, const Matrix& ctm);
    void writeShape(string& out, const Shape* shape, const string& extra, float gradScale = 1.0f, float strokeWidth = -1.0f);
    void writeScene(string& out, const Scene* scene, const Matrix& ctm, const string& extra, uint32_t filter, bool alpha);
    void writePicture(string& out, const Picture* picture, const Matrix& ctm, const string& extra, bool alpha);
    void writeText(string& out, const Text* text, const string& extra);
    void writePaint(string& out, const Paint* paint, const Matrix& ctm, bool alpha, bool blending = true, bool masking = true);

    void run(unsigned tid) override;
};

}  // namespace tvg

#endif  //_TVG_SVG_SAVER_H_
