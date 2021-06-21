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

enum class LoaderResult { InvalidType, Success, SizeCorruption, MemoryCorruption, LogicalCorruption };

static LoaderResult _parsePaint(tvgBlock block, Paint ** paint);

static tvgBlock _readTvgBlock(const char *ptr)
{
    tvgBlock block;
    block.type = *ptr;
    _read_tvg_ui32(&block.length, ptr + TVG_INDICATOR_SIZE);
    block.data = ptr + TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE;
    block.end = block.data + block.length;
    return block;
}

static bool _readTvgHeader(const char **ptr)
{
    if (!*ptr) return false;

    //Sign phase, always TVG_BIN_HEADER_SIGNATURE is declared
    if (memcmp(*ptr, TVG_BIN_HEADER_SIGNATURE, TVG_BIN_HEADER_SIGNATURE_LENGTH)) return false;
    *ptr += TVG_BIN_HEADER_SIGNATURE_LENGTH;

    //Version number, declared in TVG_BIN_HEADER_VERSION
    if (memcmp(*ptr, TVG_BIN_HEADER_VERSION, TVG_BIN_HEADER_VERSION_LENGTH)) return false;
    *ptr += TVG_BIN_HEADER_VERSION_LENGTH;

    //Meta data for proof?
    uint16_t metaLen;
    _read_tvg_ui16(&metaLen, *ptr);
    *ptr += 2;


//Meta data... Necessary?
#ifdef THORVG_LOG_ENABLED
    char metadata[metaLen + 1];
    memcpy(metadata, *ptr, metaLen);
    metadata[metaLen] = '\0';
    printf("TVG_LOADER: Header is valid, metadata[%d]: %s.\n", metaLen, metadata);
#endif

    *ptr += metaLen;

    return true;
}


static LoaderResult _parseCmpTarget(const char *ptr, const char *end, Paint *paint)
{
    auto block = _readTvgBlock(ptr);
    if (block.end > end) return LoaderResult::SizeCorruption;

    CompositeMethod cmpMethod;
    if (block.type != TVG_PAINT_CMP_METHOD_INDICATOR) return LoaderResult::LogicalCorruption;
    if (block.length != sizeof(TvgFlag)) return LoaderResult::SizeCorruption;
    switch (*block.data) {
        case TVG_PAINT_CMP_METHOD_CLIPPATH_FLAG: {
            cmpMethod = CompositeMethod::ClipPath;
            break;
        }
        case TVG_PAINT_CMP_METHOD_ALPHAMASK_FLAG: {
            cmpMethod = CompositeMethod::AlphaMask;
            break;
        }
        case TVG_PAINT_CMP_METHOD_INV_ALPHAMASK_FLAG: {
            cmpMethod = CompositeMethod::InvAlphaMask;
            break;
        }
        default: return LoaderResult::LogicalCorruption;
    }

    ptr = block.end;
    auto block_paint = _readTvgBlock(ptr);
    if (block_paint.end > end) return LoaderResult::SizeCorruption;

    Paint* cmpTarget;
    auto result = _parsePaint(block_paint, &cmpTarget);
    if (result != LoaderResult::Success) return result;

    if (paint->composite(unique_ptr<Paint>(cmpTarget), cmpMethod) != Result::Success) return LoaderResult::MemoryCorruption;
    return LoaderResult::Success;
}


static LoaderResult _parsePaintProperty(tvgBlock block, Paint *paint)
{
    switch (block.type) {
        case TVG_PAINT_OPACITY_INDICATOR: {
            if (block.length != sizeof(uint8_t)) return LoaderResult::SizeCorruption;
            paint->opacity(*block.data);
            return LoaderResult::Success;
        }
        case TVG_PAINT_TRANSFORM_MATRIX_INDICATOR: {
            if (block.length != sizeof(Matrix)) return LoaderResult::SizeCorruption;
            Matrix matrix;
            memcpy(&matrix, block.data, sizeof(Matrix));
            if (paint->transform(matrix) != Result::Success) return LoaderResult::MemoryCorruption;
            return LoaderResult::Success;
        }
        case TVG_PAINT_CMP_TARGET_INDICATOR: {
            if (block.length < TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE) return LoaderResult::SizeCorruption;
            return _parseCmpTarget(block.data, block.end, paint);
        }
    }
    return LoaderResult::InvalidType;
}


static LoaderResult _parseSceneProperty(tvgBlock block, Scene *scene)
{
    switch (block.type) {
        case TVG_SCENE_FLAG_RESERVEDCNT: {
            if (block.length != sizeof(uint32_t)) return LoaderResult::SizeCorruption;
            uint32_t reservedCnt;
            _read_tvg_ui32(&reservedCnt, block.data);
            scene->reserve(reservedCnt);
            return LoaderResult::Success;
        }
    }

    Paint* paint;
    auto result = _parsePaint(block, &paint);
    if (result == LoaderResult::Success) {
        if (scene->push(unique_ptr<Paint>(paint)) != Result::Success) {
            return LoaderResult::MemoryCorruption;
        }
    }
    return result;
}


static LoaderResult _parseShapePath(const char *ptr, const char *end, Shape *shape)
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

    if (ptr > end) return LoaderResult::SizeCorruption;

    shape->appendPath(cmds, cmdCnt, pts, ptsCnt);
    return LoaderResult::Success;
}


static LoaderResult _parseShapeFill(const char *ptr, const char *end, Fill **fillOutside)
{
    unique_ptr<Fill> fillGrad;

    while (ptr < end) {
        auto block = _readTvgBlock(ptr);
        if (block.end > end) return LoaderResult::SizeCorruption;

        switch (block.type) {
            case TVG_FILL_RADIAL_GRADIENT_INDICATOR: {
                if (block.length != 3 * sizeof(float)) return LoaderResult::SizeCorruption;

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
            case TVG_FILL_LINEAR_GRADIENT_INDICATOR: {
                if (block.length != 4 * sizeof(float)) return LoaderResult::SizeCorruption;

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
            case TVG_FILL_FILLSPREAD_INDICATOR: {
                if (!fillGrad) return LoaderResult::LogicalCorruption;
                if (block.length != sizeof(TvgFlag)) return LoaderResult::SizeCorruption;
                switch (*block.data) {
                    case TVG_FILL_FILLSPREAD_PAD_FLAG: {
                        fillGrad->spread(FillSpread::Pad);
                        break;
                    }
                    case TVG_FILL_FILLSPREAD_REFLECT_FLAG: {
                        fillGrad->spread(FillSpread::Reflect);
                        break;
                    }
                    case TVG_FILL_FILLSPREAD_REPEAT_FLAG: {
                        fillGrad->spread(FillSpread::Repeat);
                        break;
                    }
                }
                break;
            }
            case TVG_FILL_COLORSTOPS_INDICATOR: {
                if (!fillGrad) return LoaderResult::LogicalCorruption;
                if (block.length == 0 || block.length & 0x07) return LoaderResult::SizeCorruption;
                uint32_t stopsCnt = block.length >> 3; // 8 bytes per ColorStop
                if (stopsCnt > 1023) return LoaderResult::SizeCorruption;
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
    return LoaderResult::Success;
}


static LoaderResult _parseShapeStrokeDashPattern(const char *ptr, const char *end, Shape *shape)
{
    uint32_t dashPatternCnt;
    _read_tvg_ui32(&dashPatternCnt, ptr);
    ptr += sizeof(uint32_t);
    const float* dashPattern = (float*) ptr;
    ptr += sizeof(float) * dashPatternCnt;

    if (ptr > end) return LoaderResult::SizeCorruption;

    shape->stroke(dashPattern, dashPatternCnt);
    return LoaderResult::Success;
}


static LoaderResult _parseShapeStroke(const char *ptr, const char *end, Shape *shape)
{
    while (ptr < end) {
        auto block = _readTvgBlock(ptr);
        if (block.end > end) return LoaderResult::SizeCorruption;

        switch (block.type) {
            case TVG_SHAPE_STROKE_CAP_INDICATOR: {
                if (block.length != sizeof(TvgFlag)) return LoaderResult::SizeCorruption;
                switch (*block.data) {
                    case TVG_SHAPE_STROKE_CAP_SQUARE_FLAG:
                        shape->stroke(StrokeCap::Square);
                        break;
                    case TVG_SHAPE_STROKE_CAP_ROUND_FLAG:
                        shape->stroke(StrokeCap::Round);
                        break;
                    case TVG_SHAPE_STROKE_CAP_BUTT_FLAG:
                        shape->stroke(StrokeCap::Butt);
                        break;
                }
                break;
            }
            case TVG_SHAPE_STROKE_JOIN_INDICATOR: {
                if (block.length != sizeof(TvgFlag)) return LoaderResult::SizeCorruption;
                switch (*block.data) {
                    case TVG_SHAPE_STROKE_JOIN_BEVEL_FLAG:
                        shape->stroke(StrokeJoin::Bevel);
                        break;
                    case TVG_SHAPE_STROKE_JOIN_ROUND_FLAG:
                        shape->stroke(StrokeJoin::Round);
                        break;
                    case TVG_SHAPE_STROKE_JOIN_MITER_FLAG:
                        shape->stroke(StrokeJoin::Miter);
                        break;
                }
                break;
            }
            case TVG_SHAPE_STROKE_WIDTH_INDICATOR: {
                if (block.length != sizeof(float)) return LoaderResult::SizeCorruption;
                float width;
                _read_tvg_float(&width, block.data);
                shape->stroke(width);
                break;
            }
            case TVG_SHAPE_STROKE_COLOR_INDICATOR: {
                if (block.length != 4) return LoaderResult::SizeCorruption;
                shape->stroke(block.data[0], block.data[1], block.data[2], block.data[3]);
                break;
            }
            case TVG_SHAPE_STROKE_FILL_INDICATOR: {
                Fill* fill;
                auto result = _parseShapeFill(block.data, block.end, &fill);
                if (result != LoaderResult::Success) return result;
                shape->stroke(unique_ptr < Fill > (fill));
                break;
            }
            case TVG_SHAPE_STROKE_DASHPTRN_INDICATOR: {
                auto result = _parseShapeStrokeDashPattern(block.data, block.end, shape);
                if (result != LoaderResult::Success) return result;
                break;
            }
        }
        ptr = block.end;
    }
    return LoaderResult::Success;
}


static LoaderResult _parseShapeProperty(tvgBlock block, Shape *shape)
{
    switch (block.type) {
        case TVG_SHAPE_PATH_INDICATOR: {
            auto result = _parseShapePath(block.data, block.end, shape);
            if (result != LoaderResult::Success) return result;
            break;
        }
        case TVG_SHAPE_STROKE_INDICATOR: {
            auto result = _parseShapeStroke(block.data, block.end, shape);
            if (result != LoaderResult::Success) return result;
            break;
        }
        case TVG_SHAPE_FILL_INDICATOR: {
            Fill* fill;
            auto result = _parseShapeFill(block.data, block.end, &fill);
            if (result != LoaderResult::Success) return result;
            shape->fill(unique_ptr < Fill > (fill));
            break;
        }
        case TVG_SHAPE_COLOR_INDICATOR: {
            if (block.length != 4) return LoaderResult::SizeCorruption;
            shape->fill(block.data[0], block.data[1], block.data[2], block.data[3]);
            break;
        }
        case TVG_SHAPE_FILLRULE_INDICATOR: {
            if (block.length != sizeof(TvgFlag)) return LoaderResult::SizeCorruption;
            switch (*block.data) {
                case TVG_SHAPE_FILLRULE_WINDING_FLAG:
                    shape->fill(FillRule::Winding);
                    break;
                case TVG_SHAPE_FILLRULE_EVENODD_FLAG:
                    shape->fill(FillRule::EvenOdd);
                    break;
            }
            break;
        }
        default: return LoaderResult::InvalidType;
    }
    return LoaderResult::Success;
}


static LoaderResult _parsePicture(tvgBlock block, Picture *picture)
{
    switch (block.type) {
        case TVG_RAW_IMAGE_BEGIN_INDICATOR: {
            if (block.length < 2*sizeof(uint32_t)) return LoaderResult::SizeCorruption;

            const char* ptr = block.data;
            uint32_t w, h;

            _read_tvg_ui32(&w, ptr);
            ptr += sizeof(uint32_t);
            _read_tvg_ui32(&h, ptr);
            ptr += sizeof(uint32_t);

            uint32_t size = w * h * sizeof(uint32_t);
            if (block.length != 2*sizeof(uint32_t) + size) return LoaderResult::SizeCorruption;

            uint32_t* pixels = (uint32_t*) ptr;
            picture->load(pixels, w, h, true);
            return LoaderResult::Success;
        }
    }

    Paint* paint;
    auto result = _parsePaint(block, &paint);
    if (result == LoaderResult::Success) {
        if (picture->paint(unique_ptr<Paint>(paint)) != Result::Success) {
            return LoaderResult::LogicalCorruption;
        }
    }
    return result;
}


static LoaderResult _parsePaint(tvgBlock base_block, Paint **paint)
{
    switch (base_block.type) {
        case TVG_SCENE_BEGIN_INDICATOR: {
            auto s = Scene::gen();
            auto ptr = base_block.data;
            while (ptr < base_block.end) {
                auto block = _readTvgBlock(ptr);
                if (block.end > base_block.end) return LoaderResult::SizeCorruption;

                auto result = _parseSceneProperty(block, s.get());
                if (result == LoaderResult::InvalidType) result = _parsePaintProperty(block, s.get());

                if (result > LoaderResult::Success) {
#ifdef THORVG_LOG_ENABLED
                    printf("TVG_LOADER: Loading scene error[type: 0x%02x]: %d\n", (int) block.type, (int) result);
#endif
                    return result;
                }
                ptr = block.end;
            }
            *paint = s.release();
            break;
        }
        case TVG_SHAPE_BEGIN_INDICATOR: {
            auto s = Shape::gen();
            auto ptr = base_block.data;
            while (ptr < base_block.end) {
                auto block = _readTvgBlock(ptr);
                if (block.end > base_block.end) return LoaderResult::SizeCorruption;

                auto result = _parseShapeProperty(block, s.get());
                if (result == LoaderResult::InvalidType) result = _parsePaintProperty(block, s.get());

                if (result > LoaderResult::Success) {
#ifdef THORVG_LOG_ENABLED
                    printf("TVG_LOADER: Loading shape error[type: 0x%02x]: %d\n", (int) block.type, (int) result);
#endif
                    return result;
                }
                ptr = block.end;
            }
            *paint = s.release();
            break;
        }
        case TVG_PICTURE_BEGIN_INDICATOR: {
            auto s = Picture::gen();
            auto ptr = base_block.data;
            while (ptr < base_block.end) {
                auto block = _readTvgBlock(ptr);
                if (block.end > base_block.end) return LoaderResult::SizeCorruption;

                auto result = _parsePicture(block, s.get());
                if (result == LoaderResult::InvalidType) result = _parsePaintProperty(block, s.get());

                if (result > LoaderResult::Success) {
#ifdef THORVG_LOG_ENABLED
                    printf("TVG_LOADER: Loading picture error[type: 0x%02x]: %d\n", (int) block.type, (int) result);
#endif
                    return result;
                }
                ptr = block.end;
            }
            *paint = s.release();
            break;
        }
        default: return LoaderResult::InvalidType;
    }
    return LoaderResult::Success;
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
#ifdef THORVG_LOG_ENABLED
        printf("TVG_LOADER: Invalid TVG Data!\n");
#endif
        return nullptr;
    }

    auto scene = Scene::gen();
    if (!scene) return nullptr;

    Paint* paint;

    while (ptr < end) {
        auto block = _readTvgBlock(ptr);
        if (block.end > end) return nullptr;

        auto result = _parsePaint(block, &paint);
        if (result > LoaderResult::Success) return nullptr;
        if (result == LoaderResult::Success) {
            if (scene->push(unique_ptr<Paint>(paint)) != Result::Success) {
                return nullptr;
            }
        }
        ptr = block.end;
    }

    return move(scene);
}