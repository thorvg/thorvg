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

#include "tvgPaint.h"
#include "tvgBinaryDesc.h"
#include <float.h>
#include <math.h>
#include <fstream>

struct Saver::Impl
{
    Saver* saver;
    Paint* paint = nullptr;        //TODO: replace with Array
    char* buffer = nullptr;
    char* pointer = nullptr;
    uint32_t size = 0;
    uint32_t reserved = 0;

    Impl(Saver* s) : saver(s)
    {
    }

    ~Impl()
    {
        sync();
    }

    bool sync()
    {
        if (paint) delete(paint);

        clearBuffer();

        return true;
    }

    void resizeBuffer(uint32_t newSize)
    {
        //OPTIMIZE ME: find more optimal alg ? "*2" is not opt when raw/png is used
        reserved += 100;
        if (newSize > reserved) reserved = newSize + 100;

        auto bufferOld = buffer;

        buffer = static_cast<char*>(realloc(buffer, reserved));

        if (buffer != bufferOld)
            pointer = buffer + (pointer - bufferOld);
    }

    void rewindBuffer(ByteCounter bytesNum)
    {
        if (pointer - bytesNum < buffer) return;

        pointer -= bytesNum;
        size -= bytesNum;
    }

    void clearBuffer()
    {
        if (buffer) free(buffer);
        buffer = nullptr;
        pointer = nullptr;
        size = 0;
        reserved = 0;
    }

    bool saveBufferToFile(const std::string& path)
    {
        ofstream outFile;
        outFile.open(path, ios::out | ios::trunc | ios::binary);
        if (!outFile.is_open()) return false;
        outFile.write(buffer, size);
        outFile.close();

        return true;
    }

    bool writeHeader()
    {
        reserved = TVG_BIN_HEADER_SIGNATURE_LENGTH + TVG_BIN_HEADER_VERSION_LENGTH;

        buffer = static_cast<char*>(malloc(reserved));
        if (!buffer) return false;

        pointer = buffer;

        memcpy(pointer, TVG_BIN_HEADER_SIGNATURE, TVG_BIN_HEADER_SIGNATURE_LENGTH);
        pointer += TVG_BIN_HEADER_SIGNATURE_LENGTH;
        memcpy(pointer, TVG_BIN_HEADER_VERSION, TVG_BIN_HEADER_VERSION_LENGTH);
        pointer += TVG_BIN_HEADER_VERSION_LENGTH;

        size += (TVG_BIN_HEADER_SIGNATURE_LENGTH + TVG_BIN_HEADER_VERSION_LENGTH);

        return true;
    }

    void writeMemberIndicator(TvgIndicator ind)
    {
        if (size + TVG_INDICATOR_SIZE > reserved) resizeBuffer(size + TVG_INDICATOR_SIZE);

        memcpy(pointer, &ind, TVG_INDICATOR_SIZE);
        pointer += TVG_INDICATOR_SIZE;
        size += TVG_INDICATOR_SIZE;
    }

    void writeMemberDataSize(ByteCounter byteCnt)
    {
        if (size + BYTE_COUNTER_SIZE > reserved) resizeBuffer(size + BYTE_COUNTER_SIZE);

        memcpy(pointer, &byteCnt, BYTE_COUNTER_SIZE);
        pointer += BYTE_COUNTER_SIZE;
        size += BYTE_COUNTER_SIZE;
    }

    void writeMemberDataSizeAt(ByteCounter byteCnt)
    {
        memcpy(pointer - byteCnt - BYTE_COUNTER_SIZE, &byteCnt, BYTE_COUNTER_SIZE);
    }

    void skipInBufferMemberDataSize()
    {
        if (size + BYTE_COUNTER_SIZE > reserved) resizeBuffer(size + BYTE_COUNTER_SIZE);
        pointer += BYTE_COUNTER_SIZE;
        size += BYTE_COUNTER_SIZE;
    }

    ByteCounter writeMemberData(const void* data, ByteCounter byteCnt)
    {
        if (size + byteCnt > reserved) resizeBuffer(size + byteCnt);

        memcpy(pointer, data, byteCnt);
        pointer += byteCnt;
        size += byteCnt;

        return byteCnt;
    }


    ByteCounter writeMember(TvgIndicator ind, ByteCounter byteCnt, const void* data)
    {
        ByteCounter blockByteCnt = TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE + byteCnt;

        if (size + blockByteCnt > reserved) resizeBuffer(size + blockByteCnt);

        memcpy(pointer, &ind, TVG_INDICATOR_SIZE);
        pointer += TVG_INDICATOR_SIZE;
        memcpy(pointer, &byteCnt, BYTE_COUNTER_SIZE);
        pointer += BYTE_COUNTER_SIZE;
        memcpy(pointer, data, byteCnt);
        pointer += byteCnt;

        size += blockByteCnt;

        return blockByteCnt;
    }


    ByteCounter serializePaint(const Paint* paint)
    {
        ByteCounter paintDataByteCnt = 0;

        uint8_t opacity = paint->opacity();
        if (opacity < 255) {
            paintDataByteCnt += writeMember(TVG_PAINT_OPACITY_INDICATOR, sizeof(opacity), &opacity);
        }

        Matrix m = const_cast<Paint*>(paint)->transform();
        if (fabs(m.e11 - 1) > FLT_EPSILON || fabs(m.e12) > FLT_EPSILON || fabs(m.e13) > FLT_EPSILON ||
            fabs(m.e21) > FLT_EPSILON || fabs(m.e22 - 1) > FLT_EPSILON || fabs(m.e23) > FLT_EPSILON ||
            fabs(m.e31) > FLT_EPSILON || fabs(m.e32) > FLT_EPSILON || fabs(m.e33 - 1) > FLT_EPSILON) {
            paintDataByteCnt += writeMember(TVG_PAINT_TRANSFORM_MATRIX_INDICATOR, sizeof(m), &m);
        }

        const Paint* cmpTarget = nullptr;
        auto cmpMethod = paint->composite(&cmpTarget);
        if (cmpMethod != CompositeMethod::None && cmpTarget) {
            paintDataByteCnt += serializeComposite(cmpTarget, cmpMethod);
        }

        return paintDataByteCnt;
    }


    ByteCounter serializeScene(const Paint* paint)
    {
        auto scene = static_cast<const Scene*>(paint);
        if (!scene) return 0;

        ByteCounter sceneDataByteCnt = 0;

        writeMemberIndicator(TVG_SCENE_BEGIN_INDICATOR);
        skipInBufferMemberDataSize();

        sceneDataByteCnt += serializeChildren(paint);
        sceneDataByteCnt += serializePaint(scene);

        writeMemberDataSizeAt(sceneDataByteCnt);

        return TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE + sceneDataByteCnt;
    }


    ByteCounter serializeShapeFill(const Fill* f, TvgIndicator fillTvgFlag)
    {
        ByteCounter fillDataByteCnt = 0;
        const Fill::ColorStop* stops = nullptr;
        auto stopsCnt = f->colorStops(&stops);
        if (!stops || stopsCnt == 0) return 0;

        writeMemberIndicator(fillTvgFlag);
        skipInBufferMemberDataSize();

        if (f->id() == TVG_CLASS_ID_RADIAL) {
            float argRadial[3];
            auto radGrad = static_cast<const RadialGradient*>(f);
            if (radGrad->radial(argRadial, argRadial + 1,argRadial + 2) != Result::Success) {
                rewindBuffer(TVG_FLAG_SIZE + BYTE_COUNTER_SIZE);
                return 0;
            }
            fillDataByteCnt += writeMember(TVG_FILL_RADIAL_GRADIENT_INDICATOR, sizeof(argRadial), argRadial);
        }
        else {
            float argLinear[4];
            auto linGrad = static_cast<const LinearGradient*>(f);
            if (linGrad->linear(argLinear, argLinear + 1, argLinear + 2, argLinear + 3) != Result::Success) {
                rewindBuffer(TVG_FLAG_SIZE + BYTE_COUNTER_SIZE);
                return 0;
            }
            fillDataByteCnt += writeMember(TVG_FILL_LINEAR_GRADIENT_INDICATOR, sizeof(argLinear), argLinear);
        }

        auto flag = static_cast<TvgFlag>(f->spread());
        fillDataByteCnt += writeMember(TVG_FILL_FILLSPREAD_INDICATOR, TVG_FLAG_SIZE, &flag);

        fillDataByteCnt += writeMember(TVG_FILL_COLORSTOPS_INDICATOR, stopsCnt * sizeof(stops), stops);

        writeMemberDataSizeAt(fillDataByteCnt);

        return TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE + fillDataByteCnt;
    }


    ByteCounter serializeShapeStroke(const Shape* shape)
    {
        ByteCounter strokeDataByteCnt = 0;
        TvgFlag flag;

        writeMemberIndicator(TVG_SHAPE_STROKE_INDICATOR);
        skipInBufferMemberDataSize();

        flag = static_cast<TvgFlag>(shape->strokeCap());
        strokeDataByteCnt += writeMember(TVG_SHAPE_STROKE_CAP_INDICATOR, TVG_FLAG_SIZE, &flag);

        flag = static_cast<TvgFlag>(shape->strokeJoin());
        strokeDataByteCnt += writeMember(TVG_SHAPE_STROKE_JOIN_INDICATOR, TVG_FLAG_SIZE, &flag);

        float width = shape->strokeWidth();
        strokeDataByteCnt += writeMember(TVG_SHAPE_STROKE_WIDTH_INDICATOR, sizeof(width), &width);

        if (auto fill = shape->strokeFill()) {
            strokeDataByteCnt += serializeShapeFill(fill, TVG_SHAPE_STROKE_FILL_INDICATOR);
        } else {
            uint8_t color[4] = {0, 0, 0, 0};
            shape->strokeColor(color, color + 1, color + 2, color + 3);
            strokeDataByteCnt += writeMember(TVG_SHAPE_STROKE_COLOR_INDICATOR, sizeof(color), &color);
        }

        const float* dashPattern = nullptr;
        uint32_t dashCnt = shape->strokeDash(&dashPattern);
        if (dashPattern && dashCnt > 0) {
            ByteCounter dashCntByteCnt = sizeof(dashCnt);
            ByteCounter dashPtrnByteCnt = dashCnt * sizeof(dashPattern[0]);

            writeMemberIndicator(TVG_SHAPE_STROKE_DASHPTRN_INDICATOR);
            writeMemberDataSize(dashCntByteCnt + dashPtrnByteCnt);
            strokeDataByteCnt += writeMemberData(&dashCnt, dashCntByteCnt);
            strokeDataByteCnt += writeMemberData(dashPattern, dashPtrnByteCnt);
            strokeDataByteCnt += TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE;
        }

        writeMemberDataSizeAt(strokeDataByteCnt);

        return TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE + strokeDataByteCnt;
    }


    ByteCounter serializeShapePath(const Shape* shape)
    {
        const PathCommand* cmds = nullptr;
        uint32_t cmdCnt = shape->pathCommands(&cmds);
        const Point* pts = nullptr;
        uint32_t ptsCnt = shape->pathCoords(&pts);

        if (!cmds || !pts || !cmdCnt || !ptsCnt) return 0;

        ByteCounter pathDataByteCnt = 0;

        writeMemberIndicator(TVG_SHAPE_PATH_INDICATOR);
        skipInBufferMemberDataSize();

        pathDataByteCnt += writeMemberData(&cmdCnt, sizeof(cmdCnt));
        pathDataByteCnt += writeMemberData(&ptsCnt, sizeof(ptsCnt));
        pathDataByteCnt += writeMemberData(cmds, cmdCnt * sizeof(cmds[0]));
        pathDataByteCnt += writeMemberData(pts, ptsCnt * sizeof(pts[0]));

        writeMemberDataSizeAt(pathDataByteCnt);

        return TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE + pathDataByteCnt;
    }


    ByteCounter serializeShape(const Paint* paint)
    {
        auto shape = static_cast<const Shape*>(paint);
        if (!shape) return 0;

        ByteCounter shapeDataByteCnt = 0;

        writeMemberIndicator(TVG_SHAPE_BEGIN_INDICATOR);
        skipInBufferMemberDataSize();

        TvgFlag ruleTvgFlag = (shape->fillRule() == FillRule::EvenOdd) ? TVG_SHAPE_FILLRULE_EVENODD_FLAG : TVG_SHAPE_FILLRULE_WINDING_FLAG;
        shapeDataByteCnt += writeMember(TVG_SHAPE_FILLRULE_INDICATOR, TVG_FLAG_SIZE, &ruleTvgFlag);

        if (shape->strokeWidth() > 0) {
            shapeDataByteCnt += serializeShapeStroke(shape);
        }

        if (auto fill = shape->fill()) {
            shapeDataByteCnt += serializeShapeFill(fill, TVG_SHAPE_FILL_INDICATOR);
        } else {
            uint8_t color[4] = {0, 0, 0, 0};
            shape->fillColor(color, color + 1, color + 2, color + 3);
            shapeDataByteCnt += writeMember(TVG_SHAPE_COLOR_INDICATOR, sizeof(color), color);
        }

        shapeDataByteCnt += serializeShapePath(shape);

        shapeDataByteCnt += serializeChildren(paint);
        shapeDataByteCnt += serializePaint(shape);

        writeMemberDataSizeAt(shapeDataByteCnt);

        return TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE + shapeDataByteCnt;
    }


    ByteCounter serializePicture(const Paint* paint)
    {
        auto picture = static_cast<const Picture*>(paint);
        if (!picture) return 0;
        auto pixels = picture->data();

        ByteCounter pictureDataByteCnt = 0;

        writeMemberIndicator(TVG_PICTURE_BEGIN_INDICATOR);
        skipInBufferMemberDataSize();

        if (pixels) {
            //TODO - loader expects uints
            float vw, vh;
            picture->viewbox(nullptr, nullptr, &vw, &vh);

            uint32_t w = static_cast<uint32_t>(vw);
            uint32_t h = static_cast<uint32_t>(vh);
            ByteCounter wByteCnt = sizeof(w); // same as h size
            ByteCounter pixelsByteCnt = w * h * sizeof(pixels[0]);

            writeMemberIndicator(TVG_RAW_IMAGE_BEGIN_INDICATOR);
            writeMemberDataSize(2 * wByteCnt + pixelsByteCnt);
            pictureDataByteCnt += writeMemberData(&w, wByteCnt);
            pictureDataByteCnt += writeMemberData(&h, wByteCnt);
            pictureDataByteCnt += writeMemberData(pixels, pixelsByteCnt);
            pictureDataByteCnt += TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE;
        } else {
            pictureDataByteCnt += serializeChildren(paint);
        }

        pictureDataByteCnt += serializePaint(picture);

        writeMemberDataSizeAt(pictureDataByteCnt);

        return TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE + pictureDataByteCnt;
    }


    ByteCounter serializeComposite(const Paint* cmpTarget, CompositeMethod cmpMethod)
    {
        ByteCounter cmpDataByteCnt = 0;

        writeMemberIndicator(TVG_PAINT_CMP_TARGET_INDICATOR);
        skipInBufferMemberDataSize();

        auto cmpMethodTvgFlag = static_cast<TvgFlag>(cmpMethod);
        cmpDataByteCnt += writeMember(TVG_PAINT_CMP_METHOD_INDICATOR, TVG_FLAG_SIZE, &cmpMethodTvgFlag);

        cmpDataByteCnt += serialize(cmpTarget);

        writeMemberDataSizeAt(cmpDataByteCnt);

        return TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE + cmpDataByteCnt;
    }


    ByteCounter serializeChildren(const Paint* paint)
    {
        if (!paint) return 0;
        ByteCounter dataByteCnt = 0;

        for (auto it = paint->begin(); it != paint->end(); ++it) {
            dataByteCnt += serialize(&(*it));
        }

        return dataByteCnt;
    }


    ByteCounter serialize(const Paint* paint)
    {
        if (!paint) return 0;
        ByteCounter dataByteCnt = 0;

        switch (paint->id()) {
            case TVG_CLASS_ID_SHAPE: {
                dataByteCnt += serializeShape(paint);
                break;
            }
            case TVG_CLASS_ID_SCENE: {
                dataByteCnt += serializeScene(paint);
                break;
            }
            case TVG_CLASS_ID_PICTURE: {
                dataByteCnt += serializePicture(paint);
                break;
            }
        }

        return dataByteCnt;
    }


    bool save(Paint* paint, const std::string& path)
    {
        //FIXME: use Array and remove sync() here
        sync();

        //TODO: Validate path

        this->paint = paint;

        if (!writeHeader()) return false;
        if (serialize(paint) == 0) return false;
        if (!saveBufferToFile(path)) return false;

        return true;
    }
};

#endif //_TVG_SAVER_IMPL_H_
