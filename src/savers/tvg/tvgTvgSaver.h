/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.

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
#ifndef _TVG_TVGSAVER_H_
#define _TVG_TVGSAVER_H_

#include "tvgArray.h"
#include "tvgBinaryDesc.h"
#include "tvgTaskScheduler.h"

namespace tvg
{

class TvgSaver : public SaveModule, public Task
{
private:
    Array<TvgBinByte> buffer;
    Paint* paint = nullptr;
    char *path = nullptr;
    float vsize[2] = {0.0f, 0.0f};

    bool flushTo(const std::string& path);
    void reserveCount();

    bool writeHeader();
    bool writeViewSize();
    void writeTag(TvgBinTag tag);
    void writeCount(TvgBinCounter cnt);
    void writeReservedCount(TvgBinCounter cnt);
    TvgBinCounter writeData(const void* data, TvgBinCounter cnt);
    TvgBinCounter writeTagProperty(TvgBinTag tag, TvgBinCounter cnt, const void* data);
    TvgBinCounter writeTransform(const Matrix* transform);

    TvgBinCounter serialize(const Paint* paint, const Matrix* transform);
    TvgBinCounter serializeScene(const Scene* scene, const Matrix* transform);
    TvgBinCounter serializeShape(const Shape* shape, const Matrix* transform);
    TvgBinCounter serializePicture(const Picture* picture, const Matrix* transform);
    TvgBinCounter serializePaint(const Paint* paint);
    TvgBinCounter serializeFill(const Fill* fill, TvgBinTag tag);
    TvgBinCounter serializeStroke(const Shape* shape);
    TvgBinCounter serializePath(const Shape* shape, const Matrix* transform);
    TvgBinCounter serializeComposite(const Paint* cmpTarget, CompositeMethod cmpMethod);
    TvgBinCounter serializeChildren(const Paint* paint, const Matrix* transform);

public:
    ~TvgSaver();

    bool save(Paint* paint, const string& path) override;
    bool close() override;
    void run(unsigned tid) override;
};

}

#endif  //_TVG_SAVE_MODULE_H_
