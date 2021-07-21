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
#ifndef _TVG_SAVER_IMPL_H_
#define _TVG_SAVER_IMPL_H_

#include <float.h>
#include <math.h>
#include <stdio.h>
#include "tvgPaint.h"
#include "tvgBinaryDesc.h"

#define SIZE(A) sizeof(A)


struct Saver::Impl
{
    Paint* paint = nullptr;        //TODO: replace with Array
    Array<TvgBinByte> buffer;

    ~Impl()
    {
        sync();
    }

    bool sync()
    {
        if (paint) delete(paint);

        buffer.reset();

        return true;
    }

    bool bufferToFile(const std::string& path)
    {
        FILE* fp = fopen(path.c_str(), "w+");
        if (!fp) return false;

        if (fwrite(buffer.data, sizeof(char), buffer.count, fp) == 0) return false;

        fclose(fp);

        return true;
    }

    bool writeHeader()
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

    void writeMemberIndicator(TvgBinTag ind)
    {
        buffer.grow(SIZE(TvgBinTag));
        memcpy(buffer.ptr(), &ind, SIZE(TvgBinTag));
        buffer.count += SIZE(TvgBinTag);
    }

    void writeMemberDataSize(TvgBinCounter byteCnt)
    {
        buffer.grow(SIZE(TvgBinCounter));
        memcpy(buffer.ptr(), &byteCnt, SIZE(TvgBinCounter));
        buffer.count += SIZE(TvgBinCounter);
    }

    void writeMemberDataSizeAt(TvgBinCounter byteCnt)
    {
        memcpy(buffer.ptr() - byteCnt - SIZE(TvgBinCounter), &byteCnt, SIZE(TvgBinCounter));
    }

    void skipInBufferMemberDataSize()
    {
        buffer.grow(SIZE(TvgBinCounter));
        buffer.count += SIZE(TvgBinCounter);
    }

    TvgBinCounter writeMemberData(const void* data, TvgBinCounter byteCnt)
    {
        buffer.grow(byteCnt);
        memcpy(buffer.ptr(), data, byteCnt);
        buffer.count += byteCnt;

        return byteCnt;
    }

    TvgBinCounter writeMember(TvgBinTag ind, TvgBinCounter byteCnt, const void* data)
    {
        TvgBinCounter blockByteCnt = SIZE(TvgBinTag) + SIZE(TvgBinCounter) + byteCnt;

        buffer.grow(blockByteCnt);

        auto ptr = buffer.ptr();

        memcpy(ptr, &ind, SIZE(TvgBinTag));
        ptr += SIZE(TvgBinTag);
        memcpy(ptr, &byteCnt, SIZE(TvgBinCounter));
        ptr += SIZE(TvgBinCounter);
        memcpy(ptr, data, byteCnt);
        ptr += byteCnt;

        buffer.count += blockByteCnt;

        return blockByteCnt;
    }

    TvgBinCounter serializePaint(const Paint* paint)
    {
        TvgBinCounter paintDataByteCnt = 0;

        auto opacity = paint->opacity();
        if (opacity < 255) {
            paintDataByteCnt += writeMember(TVG_TAG_PAINT_OPACITY, sizeof(opacity), &opacity);
        }

        auto m = const_cast<Paint*>(paint)->transform();
        if (fabs(m.e11 - 1) > FLT_EPSILON || fabs(m.e12) > FLT_EPSILON || fabs(m.e13) > FLT_EPSILON ||
            fabs(m.e21) > FLT_EPSILON || fabs(m.e22 - 1) > FLT_EPSILON || fabs(m.e23) > FLT_EPSILON ||
            fabs(m.e31) > FLT_EPSILON || fabs(m.e32) > FLT_EPSILON || fabs(m.e33 - 1) > FLT_EPSILON) {
            paintDataByteCnt += writeMember(TVG_TAG_PAINT_TRANSFORM, sizeof(m), &m);
        }

        const Paint* cmpTarget = nullptr;
        auto cmpMethod = paint->composite(&cmpTarget);
        if (cmpMethod != CompositeMethod::None && cmpTarget) {
            paintDataByteCnt += serializeComposite(cmpTarget, cmpMethod);
        }

        return paintDataByteCnt;
    }

    TvgBinCounter serialize(const Scene* scene)
    {
        writeMemberIndicator(TVG_TAG_CLASS_SCENE);
        skipInBufferMemberDataSize();

        auto sceneDataByteCnt = serializeChildren(scene);
        sceneDataByteCnt += serializePaint(scene);

        writeMemberDataSizeAt(sceneDataByteCnt);

        return SIZE(TvgBinTag) + SIZE(TvgBinCounter) + sceneDataByteCnt;
    }

    TvgBinCounter serializeShapeFill(const Fill* f, TvgBinTag fillTvgBinFlag)
    {
        TvgBinCounter fillDataByteCnt = 0;
        const Fill::ColorStop* stops = nullptr;
        auto stopsCnt = f->colorStops(&stops);
        if (!stops || stopsCnt == 0) return 0;

        writeMemberIndicator(fillTvgBinFlag);
        skipInBufferMemberDataSize();

        if (f->id() == TVG_CLASS_ID_RADIAL) {
            float argRadial[3];
            auto radGrad = static_cast<const RadialGradient*>(f);
            radGrad->radial(argRadial, argRadial + 1,argRadial + 2);
            fillDataByteCnt += writeMember(TVG_TAG_FILL_RADIAL_GRADIENT, sizeof(argRadial), argRadial);
        }
        else {
            float argLinear[4];
            auto linGrad = static_cast<const LinearGradient*>(f);
            linGrad->linear(argLinear, argLinear + 1, argLinear + 2, argLinear + 3);
            fillDataByteCnt += writeMember(TVG_TAG_FILL_LINEAR_GRADIENT, sizeof(argLinear), argLinear);
        }

        auto flag = static_cast<TvgBinFlag>(f->spread());
        fillDataByteCnt += writeMember(TVG_TAG_FILL_FILLSPREAD, SIZE(TvgBinFlag), &flag);
        fillDataByteCnt += writeMember(TVG_TAG_FILL_COLORSTOPS, stopsCnt * sizeof(stops), stops);

        writeMemberDataSizeAt(fillDataByteCnt);

        return SIZE(TvgBinTag) + SIZE(TvgBinCounter) + fillDataByteCnt;
    }

    TvgBinCounter serializeShapeStroke(const Shape* shape)
    {
        TvgBinCounter strokeDataByteCnt = 0;
        TvgBinFlag flag;

        writeMemberIndicator(TVG_TAG_SHAPE_STROKE);
        skipInBufferMemberDataSize();

        flag = static_cast<TvgBinFlag>(shape->strokeCap());
        strokeDataByteCnt += writeMember(TVG_TAG_SHAPE_STROKE_CAP, SIZE(TvgBinFlag), &flag);

        flag = static_cast<TvgBinFlag>(shape->strokeJoin());
        strokeDataByteCnt += writeMember(TVG_TAG_SHAPE_STROKE_JOIN, SIZE(TvgBinFlag), &flag);

        float width = shape->strokeWidth();
        strokeDataByteCnt += writeMember(TVG_TAG_SHAPE_STROKE_WIDTH, sizeof(width), &width);

        if (auto fill = shape->strokeFill()) {
            strokeDataByteCnt += serializeShapeFill(fill, TVG_TAG_SHAPE_STROKE_FILL);
        } else {
            uint8_t color[4] = {0, 0, 0, 0};
            shape->strokeColor(color, color + 1, color + 2, color + 3);
            strokeDataByteCnt += writeMember(TVG_TAG_SHAPE_STROKE_COLOR, sizeof(color), &color);
        }

        const float* dashPattern = nullptr;
        uint32_t dashCnt = shape->strokeDash(&dashPattern);
        if (dashPattern && dashCnt > 0) {
            TvgBinCounter dashCntByteCnt = sizeof(dashCnt);
            TvgBinCounter dashPtrnByteCnt = dashCnt * sizeof(dashPattern[0]);

            writeMemberIndicator(TVG_TAG_SHAPE_STROKE_DASHPTRN);
            writeMemberDataSize(dashCntByteCnt + dashPtrnByteCnt);
            strokeDataByteCnt += writeMemberData(&dashCnt, dashCntByteCnt);
            strokeDataByteCnt += writeMemberData(dashPattern, dashPtrnByteCnt);
            strokeDataByteCnt += SIZE(TvgBinTag) + SIZE(TvgBinCounter);
        }

        writeMemberDataSizeAt(strokeDataByteCnt);

        return SIZE(TvgBinTag) + SIZE(TvgBinCounter) + strokeDataByteCnt;
    }

    TvgBinCounter serializeShapePath(const Shape* shape)
    {
        const PathCommand* cmds = nullptr;
        uint32_t cmdCnt = shape->pathCommands(&cmds);
        const Point* pts = nullptr;
        uint32_t ptsCnt = shape->pathCoords(&pts);

        if (!cmds || !pts || !cmdCnt || !ptsCnt) return 0;

        TvgBinCounter pathDataByteCnt = 0;

        writeMemberIndicator(TVG_TAG_SHAPE_PATH);
        skipInBufferMemberDataSize();

        pathDataByteCnt += writeMemberData(&cmdCnt, sizeof(cmdCnt));
        pathDataByteCnt += writeMemberData(&ptsCnt, sizeof(ptsCnt));
        pathDataByteCnt += writeMemberData(cmds, cmdCnt * sizeof(cmds[0]));
        pathDataByteCnt += writeMemberData(pts, ptsCnt * sizeof(pts[0]));

        writeMemberDataSizeAt(pathDataByteCnt);

        return SIZE(TvgBinTag) + SIZE(TvgBinCounter) + pathDataByteCnt;
    }

    TvgBinCounter serialize(const Shape* shape)
    {
        writeMemberIndicator(TVG_TAG_CLASS_SHAPE);
        skipInBufferMemberDataSize();

        auto ruleTvgBinFlag = (shape->fillRule() == FillRule::EvenOdd) ? TVG_FLAG_SHAPE_FILLRULE_EVENODD : TVG_FLAG_SHAPE_FILLRULE_WINDING;
        auto shapeDataByteCnt = writeMember(TVG_TAG_SHAPE_FILLRULE, SIZE(TvgBinFlag), &ruleTvgBinFlag);

        if (shape->strokeWidth() > 0) shapeDataByteCnt += serializeShapeStroke(shape);

        if (auto fill = shape->fill()) {
            shapeDataByteCnt += serializeShapeFill(fill, TVG_TAG_SHAPE_FILL);
        } else {
            uint8_t color[4] = {0, 0, 0, 0};
            shape->fillColor(color, color + 1, color + 2, color + 3);
            shapeDataByteCnt += writeMember(TVG_TAG_SHAPE_COLOR, sizeof(color), color);
        }

        shapeDataByteCnt += serializeShapePath(shape);
        shapeDataByteCnt += serializePaint(shape);

        writeMemberDataSizeAt(shapeDataByteCnt);

        return SIZE(TvgBinTag) + SIZE(TvgBinCounter) + shapeDataByteCnt;
    }

    TvgBinCounter serialize(const Picture* picture)
    {
        auto pixels = picture->data();

        TvgBinCounter pictureDataByteCnt = 0;

        writeMemberIndicator(TVG_TAG_CLASS_PICTURE);
        skipInBufferMemberDataSize();

        if (pixels) {
            //TODO - loader expects uints
            float vw, vh;
            picture->viewbox(nullptr, nullptr, &vw, &vh);

            uint32_t w = static_cast<uint32_t>(vw);
            uint32_t h = static_cast<uint32_t>(vh);
            TvgBinCounter wByteCnt = sizeof(w); // same as h size
            TvgBinCounter pixelsByteCnt = w * h * sizeof(pixels[0]);

            writeMemberIndicator(TVG_TAG_PICTURE_RAW_IMAGE);
            writeMemberDataSize(2 * wByteCnt + pixelsByteCnt);
            pictureDataByteCnt += writeMemberData(&w, wByteCnt);
            pictureDataByteCnt += writeMemberData(&h, wByteCnt);
            pictureDataByteCnt += writeMemberData(pixels, pixelsByteCnt);
            pictureDataByteCnt += SIZE(TvgBinTag) + SIZE(TvgBinCounter);
        } else {
            pictureDataByteCnt += serializeChildren(picture);
        }

        pictureDataByteCnt += serializePaint(picture);

        writeMemberDataSizeAt(pictureDataByteCnt);

        return SIZE(TvgBinTag) + SIZE(TvgBinCounter) + pictureDataByteCnt;
    }

    TvgBinCounter serializeComposite(const Paint* cmpTarget, CompositeMethod cmpMethod)
    {
        TvgBinCounter cmpDataByteCnt = 0;

        writeMemberIndicator(TVG_TAG_PAINT_CMP_TARGET);
        skipInBufferMemberDataSize();

        auto cmpMethodTvgBinFlag = static_cast<TvgBinFlag>(cmpMethod);
        cmpDataByteCnt += writeMember(TVG_TAG_PAINT_CMP_METHOD, SIZE(TvgBinFlag), &cmpMethodTvgBinFlag);

        cmpDataByteCnt += serialize(cmpTarget);

        writeMemberDataSizeAt(cmpDataByteCnt);

        return SIZE(TvgBinTag) + SIZE(TvgBinCounter) + cmpDataByteCnt;
    }

    TvgBinCounter serializeChildren(const Paint* paint)
    {
        TvgBinCounter dataByteCnt = 0;

        auto it = paint->pImpl->iterator();

        while (auto p = it->next()) {
            dataByteCnt += serialize(p);
        }

        delete(it);

        return dataByteCnt;
    }

    TvgBinCounter serialize(const Paint* paint)
    {
        if (!paint) return 0;

        switch (paint->id()) {
            case TVG_CLASS_ID_SHAPE: return serialize(static_cast<const Shape*>(paint));
            case TVG_CLASS_ID_SCENE: return serialize(static_cast<const Scene*>(paint));
            case TVG_CLASS_ID_PICTURE: return serialize(static_cast<const Picture*>(paint));
        }

        return 0;
    }

    bool save(Paint* paint, const std::string& path)
    {
        //FIXME: use Array and remove sync() here
        sync();

        //TODO: Validate path

        this->paint = paint;

        if (!writeHeader()) return false;
        if (serialize(paint) == 0) return false;
        if (!bufferToFile(path)) return false;

        return true;
    }
};

#endif //_TVG_SAVER_IMPL_H_
