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


static Matrix _multiply(const Matrix* lhs, const Matrix* rhs)
{
    Matrix m;

    m.e11 = lhs->e11 * rhs->e11 + lhs->e12 * rhs->e21 + lhs->e13 * rhs->e31;
    m.e12 = lhs->e11 * rhs->e12 + lhs->e12 * rhs->e22 + lhs->e13 * rhs->e32;
    m.e13 = lhs->e11 * rhs->e13 + lhs->e12 * rhs->e23 + lhs->e13 * rhs->e33;

    m.e21 = lhs->e21 * rhs->e11 + lhs->e22 * rhs->e21 + lhs->e23 * rhs->e31;
    m.e22 = lhs->e21 * rhs->e12 + lhs->e22 * rhs->e22 + lhs->e23 * rhs->e32;
    m.e23 = lhs->e21 * rhs->e13 + lhs->e22 * rhs->e23 + lhs->e23 * rhs->e33;

    m.e31 = lhs->e31 * rhs->e11 + lhs->e32 * rhs->e21 + lhs->e33 * rhs->e31;
    m.e32 = lhs->e31 * rhs->e12 + lhs->e32 * rhs->e22 + lhs->e33 * rhs->e32;
    m.e33 = lhs->e31 * rhs->e13 + lhs->e32 * rhs->e23 + lhs->e33 * rhs->e33;

    return m;
}


static void _multiply(Point* pt, const Matrix* transform)
{
    auto tx = pt->x * transform->e11 + pt->y * transform->e12 + transform->e13;
    auto ty = pt->x * transform->e21 + pt->y * transform->e22 + transform->e23;
    pt->x = tx;
    pt->y = ty;
}


bool TvgSaver::flushTo(const std::string& path)
{
    FILE* fp = fopen(path.c_str(), "w+");
    if (!fp) return false;

    if (fwrite(buffer.data, SIZE(char), buffer.count, fp) == 0) {
        fclose(fp);
        return false;
    }
    fclose(fp);

    return true;
}


/* WARNING: Header format shall not changed! */
bool TvgSaver::writeHeader()
{
    auto headerSize = TVG_HEADER_SIGNATURE_LENGTH + TVG_HEADER_VERSION_LENGTH + TVG_HEADER_RESERVED_LENGTH + SIZE(vsize);
    buffer.grow(headerSize);

    //1. Signature
    auto ptr = buffer.ptr();
    memcpy(ptr, TVG_HEADER_SIGNATURE, TVG_HEADER_SIGNATURE_LENGTH);
    ptr += TVG_HEADER_SIGNATURE_LENGTH;

    //2. Version
    memcpy(ptr, TVG_HEADER_VERSION, TVG_HEADER_VERSION_LENGTH);
    ptr += TVG_HEADER_VERSION_LENGTH;

    buffer.count += (TVG_HEADER_SIGNATURE_LENGTH + TVG_HEADER_VERSION_LENGTH + TVG_HEADER_RESERVED_LENGTH);

    //3. View Size
    writeData(vsize, SIZE(vsize));

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


TvgBinCounter TvgSaver::writeTransform(const Matrix* transform)
{
    if (!transform) return 0;

    if (fabs(transform->e11 - 1) > FLT_EPSILON || fabs(transform->e12) > FLT_EPSILON || fabs(transform->e13) > FLT_EPSILON ||
        fabs(transform->e21) > FLT_EPSILON || fabs(transform->e22 - 1) > FLT_EPSILON || fabs(transform->e23) > FLT_EPSILON ||
        fabs(transform->e31) > FLT_EPSILON || fabs(transform->e32) > FLT_EPSILON || fabs(transform->e33 - 1) > FLT_EPSILON) {
        return writeTagProperty(TVG_TAG_PAINT_TRANSFORM, SIZE(Matrix), transform);
    }
    return 0;
}


TvgBinCounter TvgSaver::serializePaint(const Paint* paint)
{
    TvgBinCounter cnt = 0;

    //opacity
    auto opacity = paint->opacity();
    if (opacity < 255) {
        cnt += writeTagProperty(TVG_TAG_PAINT_OPACITY, SIZE(opacity), &opacity);
    }

    //composite
    const Paint* cmpTarget = nullptr;
    auto cmpMethod = paint->composite(&cmpTarget);
    if (cmpMethod != CompositeMethod::None && cmpTarget) {
        cnt += serializeComposite(cmpTarget, cmpMethod);
    }

    return cnt;
}


TvgBinCounter TvgSaver::serializeScene(const Scene* scene, const Matrix* transform)
{
    writeTag(TVG_TAG_CLASS_SCENE);
    reserveCount();

    auto cnt = serializeChildren(scene, transform) + serializePaint(scene);

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
        cnt += writeTagProperty(TVG_TAG_FILL_RADIAL_GRADIENT, SIZE(args), args);
    //linear fill
    } else {
        float args[4];
        static_cast<const LinearGradient*>(fill)->linear(args, args + 1, args + 2, args + 3);
        cnt += writeTagProperty(TVG_TAG_FILL_LINEAR_GRADIENT, SIZE(args), args);
    }

    if (auto flag = static_cast<TvgBinFlag>(fill->spread()))
        cnt += writeTagProperty(TVG_TAG_FILL_FILLSPREAD, SIZE(TvgBinFlag), &flag);
    cnt += writeTagProperty(TVG_TAG_FILL_COLORSTOPS, stopsCnt * SIZE(Fill::ColorStop), stops);

    writeReservedCount(cnt);

    return SERIAL_DONE(cnt);
}


TvgBinCounter TvgSaver::serializeStroke(const Shape* shape)
{
    writeTag(TVG_TAG_SHAPE_STROKE);
    reserveCount();

    //width
    auto width = shape->strokeWidth();
    auto cnt = writeTagProperty(TVG_TAG_SHAPE_STROKE_WIDTH, SIZE(width), &width);

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
        cnt += writeTagProperty(TVG_TAG_SHAPE_STROKE_COLOR, SIZE(color), &color);
    }

    //dash
    const float* dashPattern = nullptr;
    auto dashCnt = shape->strokeDash(&dashPattern);
    if (dashPattern && dashCnt > 0) {
        TvgBinCounter dashCntSize = SIZE(dashCnt);
        TvgBinCounter dashPtrnSize = dashCnt * SIZE(dashPattern[0]);

        writeTag(TVG_TAG_SHAPE_STROKE_DASHPTRN);
        writeCount(dashCntSize + dashPtrnSize);
        cnt += writeData(&dashCnt, dashCntSize);
        cnt += writeData(dashPattern, dashPtrnSize);
        cnt += SIZE(TvgBinTag) + SIZE(TvgBinCounter);
    }

    writeReservedCount(cnt);

    return SERIAL_DONE(cnt);
}


TvgBinCounter TvgSaver::serializePath(const Shape* shape, const Matrix* transform)
{
    const PathCommand* cmds = nullptr;
    auto cmdCnt = shape->pathCommands(&cmds);
    const Point* pts = nullptr;
    auto ptsCnt = shape->pathCoords(&pts);

    if (!cmds || !pts || cmdCnt == 0 || ptsCnt == 0) return 0;

    writeTag(TVG_TAG_SHAPE_PATH);
    reserveCount();

    /* Reduce the binary size.
       Convert PathCommand(4 bytes) to TvgBinFlag(1 byte) */
    TvgBinFlag outCmds[cmdCnt];
    for (uint32_t i = 0; i < cmdCnt; ++i) {
        outCmds[i] = static_cast<TvgBinFlag>(cmds[i]);
    }

    auto cnt = writeData(&cmdCnt, SIZE(cmdCnt));
    cnt += writeData(&ptsCnt, SIZE(ptsCnt));
    cnt += writeData(outCmds, SIZE(outCmds));

    //transform?
    if (fabs(transform->e11 - 1) > FLT_EPSILON || fabs(transform->e12) > FLT_EPSILON || fabs(transform->e13) > FLT_EPSILON ||
        fabs(transform->e21) > FLT_EPSILON || fabs(transform->e22 - 1) > FLT_EPSILON || fabs(transform->e23) > FLT_EPSILON ||
        fabs(transform->e31) > FLT_EPSILON || fabs(transform->e32) > FLT_EPSILON || fabs(transform->e33 - 1) > FLT_EPSILON) {
        auto p = const_cast<Point*>(pts);
        for (uint32_t i = 0; i < ptsCnt; ++i) _multiply(p++, transform);
    }

    cnt += writeData(pts, ptsCnt * SIZE(pts[0]));

    writeReservedCount(cnt);

    return SERIAL_DONE(cnt);
}


TvgBinCounter TvgSaver::serializeShape(const Shape* shape, const Matrix* transform)
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
        if (color[3] > 0) cnt += writeTagProperty(TVG_TAG_SHAPE_COLOR, SIZE(color), color);
    }

    cnt += serializePath(shape, transform);
    cnt += serializePaint(shape);

    writeReservedCount(cnt);

    return SERIAL_DONE(cnt);
}


TvgBinCounter TvgSaver::serializePicture(const Picture* picture, const Matrix* transform)
{
    writeTag(TVG_TAG_CLASS_PICTURE);
    reserveCount();

    TvgBinCounter cnt = 0;

    //Bitmap Image
    uint32_t w, h;

    if (auto pixels = picture->data(&w, &h)) {
        TvgBinCounter sizeCnt = SIZE(w);
        TvgBinCounter imgSize = w * h * SIZE(pixels[0]);

        writeTag(TVG_TAG_PICTURE_RAW_IMAGE);
        writeCount(2 * sizeCnt + imgSize);

        cnt += writeData(&w, sizeCnt);
        cnt += writeData(&h, sizeCnt);
        cnt += writeData(pixels, imgSize);
        cnt += SIZE(TvgBinTag) + SIZE(TvgBinCounter);

        //Only Bitmap picture needs the transform info.
        cnt += writeTransform(transform);
    //Vector Image
    } else {
        cnt += serializeChildren(picture, transform);
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

    cnt += serialize(cmpTarget, nullptr);

    writeReservedCount(cnt);

    return SERIAL_DONE(cnt);
}


TvgBinCounter TvgSaver::serializeChildren(const Paint* paint, const Matrix* transform)
{
    auto it = this->iterator(paint);
    if (!it) return 0;

    TvgBinCounter cnt = 0;

    while (auto p = it->next())
        cnt += serialize(p, transform);

    delete(it);

    return cnt;
}


TvgBinCounter TvgSaver::serialize(const Paint* paint, const Matrix* transform)
{
    if (!paint) return 0;

    auto m = const_cast<Paint*>(paint)->transform();
    if (transform) m = _multiply(transform, &m);

    switch (paint->id()) {
        case TVG_CLASS_ID_SHAPE: return serializeShape(static_cast<const Shape*>(paint), &m);
        case TVG_CLASS_ID_SCENE: return serializeScene(static_cast<const Scene*>(paint), &m);
        case TVG_CLASS_ID_PICTURE: return serializePicture(static_cast<const Picture*>(paint), &m);
    }

    return 0;
}


void TvgSaver::run(unsigned tid)
{
    if (!writeHeader()) return;
    if (serialize(paint, nullptr) == 0) return;
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

    paint->bounds(nullptr, nullptr, &vsize[0], &vsize[1]);
    if (vsize[0] <= FLT_EPSILON || vsize[1] <= FLT_EPSILON) {
        TVGLOG("TVG_SAVER", "Saving paint(%p) has zero view size.", paint);
        return false;
    }

    this->paint = paint;

    TaskScheduler::request(this);

    return true;
}
