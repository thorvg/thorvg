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

#include <memory.h>
#include "tvgTvgLoadParser.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#define SIZE(A) sizeof(A)


struct TvgBinBlock
{
    TvgBinTag type;
    TvgBinCounter length;
    const char* data;
    const char* end;
};

#define _read_tvg_ui32(dst, src) memcpy(dst, (src), sizeof(uint32_t))
#define _read_tvg_float(dst, src) memcpy(dst, (src), sizeof(float))

static Paint* _parsePaint(TvgBinBlock block);

static bool _paintProperty(TvgBinBlock block)
{
    switch (block.type) {
        case TVG_TAG_PAINT_OPACITY:
        case TVG_TAG_PAINT_TRANSFORM:
        case TVG_TAG_PAINT_CMP_TARGET:
        return true;
    }
    return false;
}


static TvgBinBlock _readBlock(const char *ptr)
{
    TvgBinBlock block;
    block.type = *ptr;
    _read_tvg_ui32(&block.length, ptr + SIZE(TvgBinTag));
    block.data = ptr + SIZE(TvgBinTag) + SIZE(TvgBinCounter);
    block.end = block.data + block.length;
    return block;
}

static bool _readTvgHeader(const char **ptr)
{
    if (!*ptr) return false;

    //Sign phase, always TVG_HEADER_SIGNATURE is declared
    if (memcmp(*ptr, TVG_HEADER_SIGNATURE, TVG_HEADER_SIGNATURE_LENGTH)) return false;
    *ptr += TVG_HEADER_SIGNATURE_LENGTH;

    //Version number, declared in TVG_HEADER_VERSION
    if (memcmp(*ptr, TVG_HEADER_VERSION, TVG_HEADER_VERSION_LENGTH)) return false;
    *ptr += TVG_HEADER_VERSION_LENGTH;

    return true;
}


static bool _parseCmpTarget(const char *ptr, const char *end, Paint *paint)
{
    auto block = _readBlock(ptr);
    if (block.end > end) return false;

    if (block.type != TVG_TAG_PAINT_CMP_METHOD) return false;
    if (block.length != sizeof(TvgBinFlag)) return false;

    CompositeMethod cmpMethod;

    switch (*block.data) {
        case TVG_FLAG_PAINT_CMP_METHOD_CLIPPATH: {
            cmpMethod = CompositeMethod::ClipPath;
            break;
        }
        case TVG_FLAG_PAINT_CMP_METHOD_ALPHAMASK: {
            cmpMethod = CompositeMethod::AlphaMask;
            break;
        }
        case TVG_FLAG_PAINT_CMP_METHOD_IALPHAMASK: {
            cmpMethod = CompositeMethod::InvAlphaMask;
            break;
        }
        default: return false;
    }

    ptr = block.end;

    auto cmpBlock = _readBlock(ptr);
    if (cmpBlock.end > end) return false;

    paint->composite(unique_ptr<Paint>(_parsePaint(cmpBlock)), cmpMethod);

    return true;
}


static bool _parsePaintProperty(TvgBinBlock block, Paint *paint)
{
    switch (block.type) {
        case TVG_TAG_PAINT_OPACITY: {
            if (block.length != sizeof(uint8_t)) return false;
            paint->opacity(*block.data);
            return true;
        }
        case TVG_TAG_PAINT_TRANSFORM: {
            if (block.length != sizeof(Matrix)) return false;
            Matrix matrix;
            memcpy(&matrix, block.data, sizeof(Matrix));
            if (paint->transform(matrix) != Result::Success) return false;
            return true;
        }
        case TVG_TAG_PAINT_CMP_TARGET: {
            if (block.length < SIZE(TvgBinTag) + SIZE(TvgBinCounter)) return false;
            return _parseCmpTarget(block.data, block.end, paint);
        }
    }
    return false;
}


static bool _parseScene(TvgBinBlock block, Paint *paint)
{
    auto scene = static_cast<Scene*>(paint);

    switch (block.type) {
        case TVG_FLAG_SCENE_RESERVEDCNT: {
            if (block.length != sizeof(uint32_t)) return false;
            uint32_t reservedCnt;
            _read_tvg_ui32(&reservedCnt, block.data);
            scene->reserve(reservedCnt);
            return true;
        }
    }

    if (_paintProperty(block)) return _parsePaintProperty(block, scene);

    if (auto paint = _parsePaint(block)) {
        scene->push(unique_ptr<Paint>(paint));
        return true;
    }

    return false;
}


static bool _parseShapePath(const char *ptr, const char *end, Shape *shape)
{
    //Shape Path
    uint32_t cmdCnt, ptsCnt;
    _read_tvg_ui32(&cmdCnt, ptr);
    ptr += sizeof(uint32_t);
    _read_tvg_ui32(&ptsCnt, ptr);
    ptr += sizeof(uint32_t);

    const PathCommand* cmds = (PathCommand*) ptr;
    ptr += sizeof(PathCommand) * cmdCnt;
    const Point* pts = (Point*) ptr;
    ptr += sizeof(Point) * ptsCnt;

    if (ptr > end) return false;

    shape->appendPath(cmds, cmdCnt, pts, ptsCnt);
    return true;
}


static bool _parseShapeFill(const char *ptr, const char *end, Fill **fillOutside)
{
    unique_ptr<Fill> fillGrad;

    while (ptr < end) {
        auto block = _readBlock(ptr);
        if (block.end > end) return false;

        switch (block.type) {
            case TVG_TAG_FILL_RADIAL_GRADIENT: {
                if (block.length != 3 * sizeof(float)) return false;

                auto ptr = block.data;
                float x, y, radius;

                _read_tvg_float(&x, ptr);
                ptr += sizeof(float);
                _read_tvg_float(&y, ptr);
                ptr += sizeof(float);
                _read_tvg_float(&radius, ptr);

                auto fillGradRadial = RadialGradient::gen();
                fillGradRadial->radial(x, y, radius);
                fillGrad = move(fillGradRadial);
                break;
            }
            case TVG_TAG_FILL_LINEAR_GRADIENT: {
                if (block.length != 4 * sizeof(float)) return false;

                auto ptr = block.data;
                float x1, y1, x2, y2;

                _read_tvg_float(&x1, ptr);
                ptr += sizeof(float);
                _read_tvg_float(&y1, ptr);
                ptr += sizeof(float);
                _read_tvg_float(&x2, ptr);
                ptr += sizeof(float);
                _read_tvg_float(&y2, ptr);

                auto fillGradLinear = LinearGradient::gen();
                fillGradLinear->linear(x1, y1, x2, y2);
                fillGrad = move(fillGradLinear);
                break;
            }
            case TVG_TAG_FILL_FILLSPREAD: {
                if (!fillGrad) return false;
                if (block.length != sizeof(TvgBinFlag)) return false;
                switch (*block.data) {
                    case TVG_FLAG_FILL_FILLSPREAD_PAD: {
                        fillGrad->spread(FillSpread::Pad);
                        break;
                    }
                    case TVG_FLAG_FILL_FILLSPREAD_REFLECT: {
                        fillGrad->spread(FillSpread::Reflect);
                        break;
                    }
                    case TVG_FLAG_FILL_FILLSPREAD_REPEAT: {
                        fillGrad->spread(FillSpread::Repeat);
                        break;
                    }
                }
                break;
            }
            case TVG_TAG_FILL_COLORSTOPS: {
                if (!fillGrad) return false;
                if (block.length == 0 || block.length & 0x07) return false;
                uint32_t stopsCnt = block.length >> 3; // 8 bytes per ColorStop
                if (stopsCnt > 1023) return false;
                Fill::ColorStop stops[stopsCnt];
                auto p = block.data;
                for (uint32_t i = 0; i < stopsCnt; i++, p += 8) {
                    _read_tvg_float(&stops[i].offset, p);
                    stops[i].r = p[4];
                    stops[i].g = p[5];
                    stops[i].b = p[6];
                    stops[i].a = p[7];
                }
                fillGrad->colorStops(stops, stopsCnt);
                break;
            }
        }
        ptr = block.end;
    }
    *fillOutside = fillGrad.release();
    return true;
}


static bool _parseShapeStrokeDashPattern(const char *ptr, const char *end, Shape *shape)
{
    uint32_t dashPatternCnt;
    _read_tvg_ui32(&dashPatternCnt, ptr);
    ptr += sizeof(uint32_t);
    const float* dashPattern = (float*) ptr;
    ptr += sizeof(float) * dashPatternCnt;

    if (ptr > end) return false;

    shape->stroke(dashPattern, dashPatternCnt);
    return true;
}


static bool _parseShapeStroke(const char *ptr, const char *end, Shape *shape)
{
    while (ptr < end) {
        auto block = _readBlock(ptr);
        if (block.end > end) return false;

        switch (block.type) {
            case TVG_TAG_SHAPE_STROKE_CAP: {
                if (block.length != sizeof(TvgBinFlag)) return false;
                switch (*block.data) {
                    case TVG_FLAG_SHAPE_STROKE_CAP_SQUARE:
                        shape->stroke(StrokeCap::Square);
                        break;
                    case TVG_FLAG_SHAPE_STROKE_CAP_ROUND:
                        shape->stroke(StrokeCap::Round);
                        break;
                    case TVG_FLAG_SHAPE_STROKE_CAP_BUTT:
                        shape->stroke(StrokeCap::Butt);
                        break;
                }
                break;
            }
            case TVG_TAG_SHAPE_STROKE_JOIN: {
                if (block.length != sizeof(TvgBinFlag)) return false;
                switch (*block.data) {
                    case TVG_FLAG_SHAPE_STROKE_JOIN_BEVEL:
                        shape->stroke(StrokeJoin::Bevel);
                        break;
                    case TVG_FLAG_SHAPE_STROKE_JOIN_ROUND:
                        shape->stroke(StrokeJoin::Round);
                        break;
                    case TVG_FLAG_SHAPE_STROKE_JOIN_MITER:
                        shape->stroke(StrokeJoin::Miter);
                        break;
                }
                break;
            }
            case TVG_TAG_SHAPE_STROKE_WIDTH: {
                if (block.length != sizeof(float)) return false;
                float width;
                _read_tvg_float(&width, block.data);
                shape->stroke(width);
                break;
            }
            case TVG_TAG_SHAPE_STROKE_COLOR: {
                if (block.length != 4) return false;
                shape->stroke(block.data[0], block.data[1], block.data[2], block.data[3]);
                break;
            }
            case TVG_TAG_SHAPE_STROKE_FILL: {
                Fill* fill;
                if (!_parseShapeFill(block.data, block.end, &fill)) return false;
                shape->stroke(unique_ptr < Fill > (fill));
                break;
            }
            case TVG_TAG_SHAPE_STROKE_DASHPTRN: {
                if (!_parseShapeStrokeDashPattern(block.data, block.end, shape)) return false;
                break;
            }
        }
        ptr = block.end;
    }
    return true;
}


static bool _parseShape(TvgBinBlock block, Paint* paint)
{
    auto shape = static_cast<Shape*>(paint);

    switch (block.type) {
        case TVG_TAG_SHAPE_PATH: {
            if (!_parseShapePath(block.data, block.end, shape)) return false;
            break;
        }
        case TVG_TAG_SHAPE_STROKE: {
            if (!_parseShapeStroke(block.data, block.end, shape)) return false;
            break;
        }
        case TVG_TAG_SHAPE_FILL: {
            Fill* fill;
            if (!_parseShapeFill(block.data, block.end, &fill)) return false;
            shape->fill(unique_ptr < Fill > (fill));
            break;
        }
        case TVG_TAG_SHAPE_COLOR: {
            if (block.length != 4) return false;
            shape->fill(block.data[0], block.data[1], block.data[2], block.data[3]);
            break;
        }
        case TVG_TAG_SHAPE_FILLRULE: {
            if (block.length != sizeof(TvgBinFlag)) return false;
            switch (*block.data) {
                case TVG_FLAG_SHAPE_FILLRULE_WINDING:
                    shape->fill(FillRule::Winding);
                    break;
                case TVG_FLAG_SHAPE_FILLRULE_EVENODD:
                    shape->fill(FillRule::EvenOdd);
                    break;
            }
            break;
        }
        default: return _parsePaintProperty(block, shape);
    }
    return true;
}


static bool _parsePicture(TvgBinBlock block, Paint* paint)
{
    auto picture = static_cast<Picture*>(paint);

    switch (block.type) {
        case TVG_TAG_PICTURE_RAW_IMAGE: {
            if (block.length < 2 * sizeof(uint32_t)) return false;

            auto ptr = block.data;
            uint32_t w, h;

            _read_tvg_ui32(&w, ptr);
            ptr += sizeof(uint32_t);
            _read_tvg_ui32(&h, ptr);
            ptr += sizeof(uint32_t);

            auto size = w * h * sizeof(uint32_t);
            if (block.length != 2 * sizeof(uint32_t) + size) return false;

            picture->load((uint32_t*) ptr, w, h, true);
            return true;
        }
    }

    if (_paintProperty(block)) return _parsePaintProperty(block, picture);

    if (auto paint = _parsePaint(block)) {
        picture->paint(unique_ptr<Paint>(paint));
        return true;
    }

    return false;
}


static Paint* _parsePaint(TvgBinBlock baseBlock)
{
    bool (*parser)(TvgBinBlock, Paint*);
    Paint *paint = nullptr;

    switch (baseBlock.type) {
        case TVG_TAG_CLASS_SCENE: {
            paint = Scene::gen().release();
            parser = _parseScene;
            break;
        }
        case TVG_TAG_CLASS_SHAPE: {
            paint = Shape::gen().release();
            parser = _parseShape;
            break;
        }
        case TVG_TAG_CLASS_PICTURE: {
            paint = Picture::gen().release();
            parser = _parsePicture;
            break;
        }
        default: return nullptr;
    }

    auto ptr = baseBlock.data;

    while (ptr < baseBlock.end) {
        auto block = _readBlock(ptr);
        if (block.end > baseBlock.end) return paint;
        //TODO: Do we need to clarify an invalid type case from the parsing failure?
        if (!parser(block, paint)) return paint;
        ptr = block.end;
    }
    return paint;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool tvgValidateData(const char *ptr, uint32_t size)
{
    auto end = ptr + size;
    if (!_readTvgHeader(&ptr) || ptr >= end) return false;
    return true;
}

unique_ptr<Scene> tvgLoadData(const char *ptr, uint32_t size)
{
    auto end = ptr + size;

    if (!_readTvgHeader(&ptr) || ptr >= end) {
        TVGLOG("TVG: Invalid TVG Data!");
        return nullptr;
    }

    auto scene = Scene::gen();
    if (!scene) return nullptr;

    while (ptr < end) {
        auto block = _readBlock(ptr);
        if (block.end > end) return nullptr;
        scene->push(unique_ptr<Paint>(_parsePaint(block)));
        ptr = block.end;
    }

    return move(scene);
}
