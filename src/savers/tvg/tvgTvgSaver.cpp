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
#include <float.h>
#include <math.h>
#include <stdio.h>
#include "tvgSaveModule.h"
#include "tvgTvgSaver.h"

#define SIZE(A) sizeof(A)

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static inline TvgBinCounter SERIAL_DONE(TvgBinCounter cnt)
{
    return SIZE(TvgBinTag) + SIZE(TvgBinCounter) + cnt;
}


bool TvgSaver::flushTo(const std::string& path)
{
    FILE* fp = fopen(path.c_str(), "w+");
    if (!fp) return false;

    if (fwrite(buffer.data, sizeof(char), buffer.count, fp) == 0) {
        fclose(fp);
        return false;
    }

    fclose(fp);

    return true;
}


bool TvgSaver::writeHeader()
{
    buffer.grow(TVG_HEADER_SIGNATURE_LENGTH + TVG_HEADER_VERSION_LENGTH);

    auto ptr = buffer.ptr();
    memcpy(ptr, TVG_HEADER_SIGNATURE, TVG_HEADER_SIGNATURE_LENGTH);
    ptr += TVG_HEADER_SIGNATURE_LENGTH;
    memcpy(ptr, TVG_HEADER_VERSION, TVG_HEADER_VERSION_LENGTH);
    ptr += TVG_HEADER_VERSION_LENGTH;

    buffer.count += (TVG_HEADER_SIGNATURE_LENGTH + TVG_HEADER_VERSION_LENGTH);

    return true;
}


void TvgSaver::writeTag(TvgBinTag tag)
{
    buffer.grow(SIZE(TvgBinTag));
    memcpy(buffer.ptr(), &tag, SIZE(TvgBinTag));
    buffer.count += SIZE(TvgBinTag);
}


void TvgSaver::writeCount(TvgBinCounter cnt)
{
    buffer.grow(SIZE(TvgBinCounter));
    memcpy(buffer.ptr(), &cnt, SIZE(TvgBinCounter));
    buffer.count += SIZE(TvgBinCounter);
}


void TvgSaver::writeReservedCount(TvgBinCounter cnt)
{
    memcpy(buffer.ptr() - cnt - SIZE(TvgBinCounter), &cnt, SIZE(TvgBinCounter));
}


void TvgSaver::reserveCount()
{
    buffer.grow(SIZE(TvgBinCounter));
    buffer.count += SIZE(TvgBinCounter);
}


TvgBinCounter TvgSaver::writeData(const void* data, TvgBinCounter cnt)
{
    buffer.grow(cnt);
    memcpy(buffer.ptr(), data, cnt);
    buffer.count += cnt;

    return cnt;
}


TvgBinCounter TvgSaver::writeTagProperty(TvgBinTag tag, TvgBinCounter cnt, const void* data)
{
    auto growCnt = SERIAL_DONE(cnt);

    buffer.grow(growCnt);

    auto ptr = buffer.ptr();

    *ptr = tag;
    ++ptr;

    memcpy(ptr, &cnt, SIZE(TvgBinCounter));
    ptr += SIZE(TvgBinCounter);

    memcpy(ptr, data, cnt);
    ptr += cnt;

    buffer.count += growCnt;

    return growCnt;
}


TvgBinCounter TvgSaver::serializePaint(const Paint* paint)
{
    TvgBinCounter cnt = 0;

    //opacity
    auto opacity = paint->opacity();
    if (opacity < 255) {
        cnt += writeTagProperty(TVG_TAG_PAINT_OPACITY, sizeof(opacity), &opacity);
    }

    //transform
    auto m = const_cast<Paint*>(paint)->transform();
    if (fabs(m.e11 - 1) > FLT_EPSILON || fabs(m.e12) > FLT_EPSILON || fabs(m.e13) > FLT_EPSILON ||
        fabs(m.e21) > FLT_EPSILON || fabs(m.e22 - 1) > FLT_EPSILON || fabs(m.e23) > FLT_EPSILON ||
        fabs(m.e31) > FLT_EPSILON || fabs(m.e32) > FLT_EPSILON || fabs(m.e33 - 1) > FLT_EPSILON) {
        cnt += writeTagProperty(TVG_TAG_PAINT_TRANSFORM, sizeof(m), &m);
    }

    //composite
    const Paint* cmpTarget = nullptr;
    auto cmpMethod = paint->composite(&cmpTarget);
    if (cmpMethod != CompositeMethod::None && cmpTarget) {
        cnt += serializeComposite(cmpTarget, cmpMethod);
    }

    return cnt;
}


TvgBinCounter TvgSaver::serializeScene(const Scene* scene)
{
    writeTag(TVG_TAG_CLASS_SCENE);
    reserveCount();

    auto cnt = serializeChildren(scene) + serializePaint(scene);

    writeReservedCount(cnt);

    return SERIAL_DONE(cnt);
}


TvgBinCounter TvgSaver::serializeFill(const Fill* fill, TvgBinTag tag)
{
    const Fill::ColorStop* stops = nullptr;
    auto stopsCnt = fill->colorStops(&stops);
    if (!stops || stopsCnt == 0) return 0;

    writeTag(tag);
    reserveCount();

    TvgBinCounter cnt = 0;

    //radial fill
    if (fill->id() == TVG_CLASS_ID_RADIAL) {
        float args[3];
        static_cast<const RadialGradient*>(fill)->radial(args, args + 1,args + 2);
        cnt += writeTagProperty(TVG_TAG_FILL_RADIAL_GRADIENT, sizeof(args), args);
    //linear fill
    } else {
        float args[4];
        static_cast<const LinearGradient*>(fill)->linear(args, args + 1, args + 2, args + 3);
        cnt += writeTagProperty(TVG_TAG_FILL_LINEAR_GRADIENT, sizeof(args), args);
    }

    if (auto flag = static_cast<TvgBinFlag>(fill->spread()))
        cnt += writeTagProperty(TVG_TAG_FILL_FILLSPREAD, SIZE(TvgBinFlag), &flag);
    cnt += writeTagProperty(TVG_TAG_FILL_COLORSTOPS, stopsCnt * sizeof(Fill::ColorStop), stops);

    writeReservedCount(cnt);

    return SERIAL_DONE(cnt);
}


TvgBinCounter TvgSaver::serializeStroke(const Shape* shape)
{
    writeTag(TVG_TAG_SHAPE_STROKE);
    reserveCount();

    //width
    auto width = shape->strokeWidth();
    auto cnt = writeTagProperty(TVG_TAG_SHAPE_STROKE_WIDTH, sizeof(width), &width);

    //cap
    if (auto flag = static_cast<TvgBinFlag>(shape->strokeCap()))
        cnt += writeTagProperty(TVG_TAG_SHAPE_STROKE_CAP, SIZE(TvgBinFlag), &flag);

    //join
    if (auto flag = static_cast<TvgBinFlag>(shape->strokeJoin()))
        cnt += writeTagProperty(TVG_TAG_SHAPE_STROKE_JOIN, SIZE(TvgBinFlag), &flag);

    //fill
    if (auto fill = shape->strokeFill()) {
        cnt += serializeFill(fill, TVG_TAG_SHAPE_STROKE_FILL);
    } else {
        uint8_t color[4] = {0, 0, 0, 0};
        shape->strokeColor(color, color + 1, color + 2, color + 3);
        cnt += writeTagProperty(TVG_TAG_SHAPE_STROKE_COLOR, sizeof(color), &color);
    }

    //dash
    const float* dashPattern = nullptr;
    auto dashCnt = shape->strokeDash(&dashPattern);
    if (dashPattern && dashCnt > 0) {
        TvgBinCounter dashCntSize = sizeof(dashCnt);
        TvgBinCounter dashPtrnSize = dashCnt * sizeof(dashPattern[0]);

        writeTag(TVG_TAG_SHAPE_STROKE_DASHPTRN);
        writeCount(dashCntSize + dashPtrnSize);
        cnt += writeData(&dashCnt, dashCntSize);
        cnt += writeData(dashPattern, dashPtrnSize);
        cnt += SIZE(TvgBinTag) + SIZE(TvgBinCounter);
    }

    writeReservedCount(cnt);

    return SERIAL_DONE(cnt);
}


TvgBinCounter TvgSaver::serializePath(const Shape* shape)
{
    const PathCommand* cmds = nullptr;
    auto cmdCnt = shape->pathCommands(&cmds);
    const Point* pts = nullptr;
    auto ptsCnt = shape->pathCoords(&pts);

    if (!cmds || !pts || cmdCnt == 0 || ptsCnt == 0) return 0;

    writeTag(TVG_TAG_SHAPE_PATH);
    reserveCount();

    auto cnt = writeData(&cmdCnt, sizeof(cmdCnt));
    cnt += writeData(&ptsCnt, sizeof(ptsCnt));
    cnt += writeData(cmds, cmdCnt * sizeof(cmds[0]));
    cnt += writeData(pts, ptsCnt * sizeof(pts[0]));

    writeReservedCount(cnt);

    return SERIAL_DONE(cnt);
}


TvgBinCounter TvgSaver::serializeShape(const Shape* shape)
{
    writeTag(TVG_TAG_CLASS_SHAPE);
    reserveCount();
    TvgBinCounter cnt = 0;

    //fill rule
    if (auto flag = static_cast<TvgBinFlag>(shape->fillRule()))
        cnt = writeTagProperty(TVG_TAG_SHAPE_FILLRULE, SIZE(TvgBinFlag), &flag);

    //stroke
    if (shape->strokeWidth() > 0) {
        uint8_t color[4] = {0, 0, 0, 0};
        shape->strokeColor(color, color + 1, color + 2, color + 3);
        auto fill = shape->strokeFill();
        if (fill || color[3] > 0) cnt += serializeStroke(shape);
    }

    //fill
    if (auto fill = shape->fill()) {
        cnt += serializeFill(fill, TVG_TAG_SHAPE_FILL);
    } else {
        uint8_t color[4] = {0, 0, 0, 0};
        shape->fillColor(color, color + 1, color + 2, color + 3);
        if (color[3] > 0) cnt += writeTagProperty(TVG_TAG_SHAPE_COLOR, sizeof(color), color);
    }

    cnt += serializePath(shape);
    cnt += serializePaint(shape);

    writeReservedCount(cnt);

    return SERIAL_DONE(cnt);
}


TvgBinCounter TvgSaver::serializePicture(const Picture* picture)
{
    writeTag(TVG_TAG_CLASS_PICTURE);
    reserveCount();

    TvgBinCounter cnt = 0;

    //Bitmap Image
    if (auto pixels = picture->data()) {
        //TODO: Loader expects uints
        float fw, fh;
        picture->size(&fw, &fh);

        auto w = static_cast<uint32_t>(fw);
        auto h = static_cast<uint32_t>(fh);
        TvgBinCounter sizeCnt = sizeof(w);
        TvgBinCounter imgSize = w * h * sizeof(pixels[0]);

        writeTag(TVG_TAG_PICTURE_RAW_IMAGE);
        writeCount(2 * sizeCnt + imgSize);

        cnt += writeData(&w, sizeCnt);
        cnt += writeData(&h, sizeCnt);
        cnt += writeData(pixels, imgSize);
        cnt += SIZE(TvgBinTag) + SIZE(TvgBinCounter);
    //Vector Image
    } else {
        cnt += serializeChildren(picture);
    }

    cnt += serializePaint(picture);

    writeReservedCount(cnt);

    return SERIAL_DONE(cnt);
}


TvgBinCounter TvgSaver::serializeComposite(const Paint* cmpTarget, CompositeMethod cmpMethod)
{
    writeTag(TVG_TAG_PAINT_CMP_TARGET);
    reserveCount();

    auto flag = static_cast<TvgBinFlag>(cmpMethod);
    auto cnt = writeTagProperty(TVG_TAG_PAINT_CMP_METHOD, SIZE(TvgBinFlag), &flag);

    cnt += serialize(cmpTarget);

    writeReservedCount(cnt);

    return SERIAL_DONE(cnt);
}


TvgBinCounter TvgSaver::serializeChildren(const Paint* paint)
{
    auto it = this->iterator(paint);
    if (!it) return 0;

    TvgBinCounter cnt = 0;

    while (auto p = it->next())
        cnt += serialize(p);

    delete(it);

    return cnt;
}


TvgBinCounter TvgSaver::serialize(const Paint* paint)
{
    if (!paint) return 0;

    switch (paint->id()) {
        case TVG_CLASS_ID_SHAPE: return serializeShape(static_cast<const Shape*>(paint));
        case TVG_CLASS_ID_SCENE: return serializeScene(static_cast<const Scene*>(paint));
        case TVG_CLASS_ID_PICTURE: return serializePicture(static_cast<const Picture*>(paint));
    }

    return 0;
}

void TvgSaver::run(unsigned tid)
{
    if (!writeHeader()) return;
    if (serialize(paint) == 0) return;
    if (!flushTo(path)) return;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

TvgSaver::~TvgSaver()
{
    close();
}

bool TvgSaver::close()
{
    this->done();

    if (paint) {
        delete(paint);
        paint = nullptr;
    }
    if (path) {
        free(path);
        path = nullptr;
    }
    buffer.reset();
    return true;
}


bool TvgSaver::save(Paint* paint, const string& path)
{
    close();

    this->path = strdup(path.c_str());
    if (!this->path) return false;

    this->paint = paint;

    TaskScheduler::request(this);

    return true;
}
