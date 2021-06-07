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

#include <string.h>
#include "tvgTvgLoadParser.h"

// Enumeration specifying the loading result
enum class LoaderResult { InvalidType, Success, SizeCorruption, MemoryCorruption, LogicalCorruption };

// Load paint from .tvg binary file.
static LoaderResult _parsePaint(tvgBlock block, Paint ** paint);

// Read tvgBlock from pointer location.
static tvgBlock _readTvgBlock(const char *pointer)
{
    tvgBlock block;
    block.type = *pointer;
    _read_tvg_ui32(&block.length, pointer + TVG_INDICATOR_SIZE);
    block.data = pointer + TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE;
    block.blockEnd = block.data + block.length;
    return block;
}

/*
 * Read header of the .tvg binary file
 * Returns true on success, false otherwise.
 * Details:
 * Each file starts with "TVG" phrase and three chars of version number.
 * Two next bytes are the metadata length (amount of bytes to skip if the meta is not read).
 * Metadata may contain file generation time and date or other information about device and file.
 * ["TVG"][version eg. "01a"][uint16 metadata length eg. 0x0000][metadata]
 *
 * Starting phase together with metadata are called header.
 * After header, binary section starts.
 * NOTE: In whole file little endian is used.
 */
static bool _readTvgHeader(const char **pointer)
{
    // Sign phase, always "TVG" declared in TVG_HEADER_TVG_SIGN_CODE
    if (memcmp(*pointer, TVG_HEADER_TVG_SIGN_CODE, TVG_HEADER_TVG_SIGN_CODE_LENGTH)) return false;
    *pointer += TVG_HEADER_TVG_SIGN_CODE_LENGTH; // move after sing code

    // Standard version number, declared in TVG_HEADER_TVG_VERSION_CODE
    if (memcmp(*pointer, TVG_HEADER_TVG_VERSION_CODE, TVG_HEADER_TVG_VERSION_CODE_LENGTH)) return false;
    *pointer += TVG_HEADER_TVG_VERSION_CODE_LENGTH; // move after version code

    // Matadata phase
    uint16_t meta_length; // Matadata phase length
    _read_tvg_ui16(&meta_length, *pointer);
    *pointer += 2; // move after length

#ifdef THORVG_LOG_ENABLED
    char metadata[meta_length + 1];
    memcpy(metadata, *pointer, meta_length);
    metadata[meta_length] = '\0';
    printf("TVG_LOADER: Header is valid, metadata[%d]: %s.\n", meta_length, metadata);
#endif

    *pointer += meta_length;
    return true;
}

// Paint
/*
 * Load paint's cmpMethod and cmpTarget from .tvg binary file.
 * Returns LoaderResult::Success on success or LoaderResult::SizeCorruption / MemoryCorruption if corrupted.
 */
static LoaderResult _parseCmpTarget(const char *pointer, const char *end, Paint *paint)
{
    tvgBlock block = _readTvgBlock(pointer);
    if (block.blockEnd > end) return LoaderResult::SizeCorruption;

    CompositeMethod cmpMethod;
    if (block.type != TVG_PAINT_CMP_METHOD_INDICATOR) return LoaderResult::LogicalCorruption;
    if (block.length != sizeof(TvgFlag)) return LoaderResult::SizeCorruption;
    switch (*block.data)
    {
        case TVG_PAINT_CMP_METHOD_CLIPPATH_FLAG:
            cmpMethod = CompositeMethod::ClipPath;
            break;
        case TVG_PAINT_CMP_METHOD_ALPHAMASK_FLAG:
            cmpMethod = CompositeMethod::AlphaMask;
            break;
        case TVG_PAINT_CMP_METHOD_INV_ALPHAMASK_FLAG:
            cmpMethod = CompositeMethod::InvAlphaMask;
            break;
        default:
            return LoaderResult::LogicalCorruption;
    }

    pointer = block.blockEnd;
    tvgBlock block_paint = _readTvgBlock(pointer);
    if (block_paint.blockEnd > end) return LoaderResult::SizeCorruption;

    Paint* cmpTarget;
    LoaderResult result = _parsePaint(block_paint, &cmpTarget);
    if (result != LoaderResult::Success) return result;

    if (paint->composite(unique_ptr<Paint>(cmpTarget), cmpMethod) != Result::Success) return LoaderResult::MemoryCorruption;
    return LoaderResult::Success;
}

/*
 * Load paint from .tvg binary file.
 * Returns LoaderResult::Success on success, LoaderResult::InvalidType or other if corrupted.
 *
 * Details:
 * [TVG_PAINT_OPACITY_INDICATOR][1][uint8_t opacity]
 * [TVG_PAINT_TRANSFORM_MATRIX_INDICATOR][36][Matrix]
 * [TVG_PAINT_CMP_TARGET_INDICATOR] - see _parseCmpTarget()
 */
static LoaderResult _parsePaint(tvgBlock block, Paint *paint)
{
    switch (block.type)
    {
        case TVG_PAINT_OPACITY_INDICATOR:
        { // opacity
            if (block.length != sizeof(uint8_t)) return LoaderResult::SizeCorruption;
            paint->opacity(*block.data);
            return LoaderResult::Success;
        }
        case TVG_PAINT_TRANSFORM_MATRIX_INDICATOR:
        { // transform matrix
            if (block.length != sizeof(Matrix)) return LoaderResult::SizeCorruption;
            Matrix matrix;
            memcpy(&matrix, block.data, sizeof(Matrix));
            if (paint->transform(matrix) != Result::Success) return LoaderResult::MemoryCorruption;
            return LoaderResult::Success;
        }
        case TVG_PAINT_CMP_TARGET_INDICATOR:
        { // cmp target
            if (block.length < TVG_INDICATOR_SIZE + BYTE_COUNTER_SIZE) return LoaderResult::SizeCorruption;
            return _parseCmpTarget(block.data, block.blockEnd, paint);
        }
    }
    return LoaderResult::InvalidType;
}

// Scene
/*
 * Load scene from .tvg binary file.
 * Returns LoaderResult::Success on success, LoaderResult::InvalidType or other if corrupted.
 *
 * Details:
 * [TVG_SCENE_FLAG_RESERVEDCNT][4][uint32_t reservedCnt]
 * other paints - see _parsePaint()
 */
static LoaderResult _parseScene(tvgBlock block, Scene *scene)
{
    switch (block.type)
    {
        case TVG_SCENE_FLAG_RESERVEDCNT:
        {
            if (block.length != sizeof(uint32_t)) return LoaderResult::SizeCorruption;
            uint32_t reservedCnt;
            _read_tvg_ui32(&reservedCnt, block.data);
            scene->reserve(reservedCnt);
            return LoaderResult::Success;
        }
    }

    Paint* paint;
    LoaderResult result = _parsePaint(block, &paint);
    if (result == LoaderResult::Success) {
        if (scene->push(unique_ptr<Paint>(paint)) != Result::Success) {
            return LoaderResult::MemoryCorruption;
        }
    }
    return result;
}

// Shape
/*
 * Load shape path from .tvg binary file.
 * Returns LoaderResult::Success on success or LoaderResult::SizeCorruption if size corrupted.
 *
 * Details:
 * [uint32_t cmdCnt][uint32_t ptsCnt][cmdCnt * PathCommand cmds][ptsCnt * Point pts]
 * Updates flag with RenderUpdateFlag::Path bit.
 */
static LoaderResult _parseShapePath(const char *pointer, const char *end, Shape *shape)
{
    // ShapePath
    uint32_t cmdCnt, ptsCnt;
    _read_tvg_ui32(&cmdCnt, pointer);
    pointer += sizeof(uint32_t);
    _read_tvg_ui32(&ptsCnt, pointer);
    pointer += sizeof(uint32_t);

    const PathCommand* cmds = (PathCommand*) pointer;
    pointer += sizeof(PathCommand) * cmdCnt;
    const Point* pts = (Point*) pointer;
    pointer += sizeof(Point) * ptsCnt;

    if (pointer > end) return LoaderResult::SizeCorruption;

    shape->appendPath(cmds, cmdCnt, pts, ptsCnt);
    return LoaderResult::Success;
}

/*
 * Load fill (gradient) for shape or shape stroke from .tvg binary file.
 * Returns LoaderResult::Success on success or LoaderResult::SizeCorruption if size corrupted,
 * LoaderResult::LogicalCorruption if logically corrupted (gradient parameters before gradient type).
 *
 * Details:
 * Must start with gradient type:
 * [TVG_GRADIENT_FLAG_TYPE_RADIAL][12][float x][float y][float radius]
 * or
 * [TVG_GRADIENT_FLAG_TYPE_LINEAR][16][float x1][float y1][float x2][float y2]
 * Next are gradient parameters:
 * [TVG_FILL_FLAG_FILLSPREAD][1][FILLSPREAD_PAD/FILLSPREAD_REFLECT/FILLSPREAD_REPEAT]
 * [TVG_FILL_FLAG_COLORSTOPS][8*stopsCnt][stopsCnt * [float offset][uint8_t r][uint8_t g][uint8_t b][uint8_t a]]
 */
static LoaderResult _parseShapeFill(const char *pointer, const char *end, Fill **fillOutside)
{
    unique_ptr<Fill> fillGrad;

    while (pointer < end)
    {
        tvgBlock block = _readTvgBlock(pointer);
        if (block.blockEnd > end) return LoaderResult::SizeCorruption;

        switch (block.type)
        {
            case TVG_FILL_RADIAL_GRADIENT_INDICATOR:
            { // radial gradient
                if (block.length != 3 * sizeof(float)) return LoaderResult::SizeCorruption;

                const char* pointer = block.data;
                float x, y, radius;

                _read_tvg_float(&x, pointer);
                pointer += sizeof(float);
                _read_tvg_float(&y, pointer);
                pointer += sizeof(float);
                _read_tvg_float(&radius, pointer);

                auto fillGradRadial = RadialGradient::gen();
                fillGradRadial->radial(x, y, radius);
                fillGrad = move(fillGradRadial);
                break;
            }
            case TVG_FILL_LINEAR_GRADIENT_INDICATOR:
            { // linear gradient
                if (block.length != 4 * sizeof(float)) return LoaderResult::SizeCorruption;

                const char* pointer = block.data;
                float x1, y1, x2, y2;

                _read_tvg_float(&x1, pointer);
                pointer += sizeof(float);
                _read_tvg_float(&y1, pointer);
                pointer += sizeof(float);
                _read_tvg_float(&x2, pointer);
                pointer += sizeof(float);
                _read_tvg_float(&y2, pointer);

                auto fillGradLinear = LinearGradient::gen();
                fillGradLinear->linear(x1, y1, x2, y2);
                fillGrad = move(fillGradLinear);
                break;
            }
            case TVG_FILL_FILLSPREAD_INDICATOR:
            { // fill spread
                if (!fillGrad) return LoaderResult::LogicalCorruption;
                if (block.length != sizeof(TvgFlag)) return LoaderResult::SizeCorruption;
                switch (*block.data)
                {
                    case TVG_FILL_FILLSPREAD_PAD_FLAG:
                        fillGrad->spread(FillSpread::Pad);
                        break;
                    case TVG_FILL_FILLSPREAD_REFLECT_FLAG:
                        fillGrad->spread(FillSpread::Reflect);
                        break;
                    case TVG_FILL_FILLSPREAD_REPEAT_FLAG:
                        fillGrad->spread(FillSpread::Repeat);
                        break;
                }
                break;
            }
            case TVG_FILL_COLORSTOPS_INDICATOR:
            { // color stops
                if (!fillGrad) return LoaderResult::LogicalCorruption;
                if (block.length == 0 || block.length & 0x07) return LoaderResult::SizeCorruption;
                uint32_t stopsCnt = block.length >> 3; // 8 bytes per ColorStop
                if (stopsCnt > 1023) return LoaderResult::SizeCorruption;
                Fill::ColorStop stops[stopsCnt];
                const char* p = block.data;
                for (uint32_t i = 0; i < stopsCnt; i++, p += 8)
                {
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

        pointer = block.blockEnd;
    }

    *fillOutside = fillGrad.release();
    return LoaderResult::Success;
}

/*
 * Load stroke dash pattern from .tvg binary file.
 * Returns LoaderResult::Success on success or LoaderResult::SizeCorruption if size corrupted,
 * LoaderResult::MemoryCorruption if memory corruption.
 *
 * Details:
 * [uint32_t dashPatternCnt][dashPatternCnt * float dashPattern]
 */
static LoaderResult _parseShapeStrokeDashPattern(const char *pointer, const char *end, Shape *shape)
{
    uint32_t dashPatternCnt;
    _read_tvg_ui32(&dashPatternCnt, pointer);
    pointer += sizeof(uint32_t);
    const float* dashPattern = (float*) pointer;
    pointer += sizeof(float) * dashPatternCnt;

    if (pointer > end) return LoaderResult::SizeCorruption;

    shape->stroke(dashPattern, dashPatternCnt);
    return LoaderResult::Success;
}

/*
 * Load stroke from .tvg binary file.
 * Returns LoaderResult:: Success on success and moves pointer to next position,
 * LoaderResult::SizeCorruption if corrupted or LoaderResult::InvalidType if not applicable for paint.
 *
 * Details:
 * [TVG_SHAPE_STROKE_FLAG_CAP][1][CAP_SQUARE/CAP_ROUND/CAP_BUTT]
 * [TVG_SHAPE_STROKE_FLAG_JOIN][1][JOIN_BEVEL/JOIN_ROUND/JOIN_MITER]
 * [TVG_SHAPE_STROKE_FLAG_WIDTH][4][float width]
 * [TVG_SHAPE_STROKE_FLAG_COLOR][4][color color]
 * [TVG_SHAPE_STROKE_FLAG_HAS_FILL] - see _parseShapeFill()
 * [TVG_SHAPE_STROKE_FLAG_HAS_DASHPTRN] - see _parseShapeStrokeDashPattern()
 */
static LoaderResult _parseShapeStroke(const char *pointer, const char *end, Shape *shape)
{
    while (pointer < end)
    {
        tvgBlock block = _readTvgBlock(pointer);
        if (block.blockEnd > end) return LoaderResult::SizeCorruption;

        switch (block.type)
        {
            case TVG_SHAPE_STROKE_CAP_INDICATOR:
            { // stroke cap
                if (block.length != sizeof(TvgFlag)) return LoaderResult::SizeCorruption;
                switch (*block.data)
                {
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
            case TVG_SHAPE_STROKE_JOIN_INDICATOR:
            { // stroke join
                if (block.length != sizeof(TvgFlag)) return LoaderResult::SizeCorruption;
                switch (*block.data)
                {
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
            case TVG_SHAPE_STROKE_WIDTH_INDICATOR:
            { // stroke width
                if (block.length != sizeof(float)) return LoaderResult::SizeCorruption;
                float width;
                _read_tvg_float(&width, block.data);
                shape->stroke(width);
                break;
            }
            case TVG_SHAPE_STROKE_COLOR_INDICATOR:
            { // stroke color
                if (block.length != 4) return LoaderResult::SizeCorruption;
                shape->stroke(block.data[0], block.data[1], block.data[2], block.data[3]);
                break;
            }
            case TVG_SHAPE_STROKE_FILL_INDICATOR:
            { // stroke fill
                Fill* fill;
                LoaderResult result = _parseShapeFill(block.data, block.blockEnd, &fill);
                if (result != LoaderResult::Success) return result;
                shape->stroke(unique_ptr < Fill > (fill));
                break;
            }
            case TVG_SHAPE_STROKE_DASHPTRN_INDICATOR:
            { // dashed stroke
                LoaderResult result = _parseShapeStrokeDashPattern(block.data, block.blockEnd, shape);
                if (result != LoaderResult::Success) return result;
                break;
            }
        }

        pointer = block.blockEnd;
    }

    return LoaderResult::Success;
}

/*
 * Load shape from .tvg binary file.
 * Returns LoaderResult::Success on success, LoaderResult::InvalidType or other if corrupted.
 *
 * Details:
 * [TVG_SHAPE_FLAG_HAS_PATH] - see _parseShapePath()
 * [TVG_SHAPE_FLAG_HAS_STROKE] - see _parseShapeStroke()
 * [TVG_SHAPE_FLAG_HAS_FILL] - see _parseShapeFill()
 * [TVG_SHAPE_FLAG_COLOR][4][color color]
 * [TVG_SHAPE_FLAG_FILLRULE][1][FILLRULE_WINDING/FILLRULE_EVENODD]
 */
static LoaderResult _parseShape(tvgBlock block, Shape *shape)
{
    switch (block.type)
    {
        case TVG_SHAPE_PATH_INDICATOR:
        { // path
            LoaderResult result = _parseShapePath(block.data, block.blockEnd, shape);
            if (result != LoaderResult::Success) return result;
            break;
        }
        case TVG_SHAPE_STROKE_INDICATOR:
        { // stroke section
            LoaderResult result = _parseShapeStroke(block.data, block.blockEnd, shape);
            if (result != LoaderResult::Success) return result;
            break;
        }
        case TVG_SHAPE_FILL_INDICATOR:
        { // fill (gradient)
            Fill* fill;
            LoaderResult result = _parseShapeFill(block.data, block.blockEnd, &fill);
            if (result != LoaderResult::Success) return result;
            shape->fill(unique_ptr < Fill > (fill));
            break;
        }
        case TVG_SHAPE_COLOR_INDICATOR:
        { // color
            if (block.length != 4) return LoaderResult::SizeCorruption;
            shape->fill(block.data[0], block.data[1], block.data[2], block.data[3]);
            break;
        }
        case TVG_SHAPE_FILLRULE_INDICATOR:
        { // fill rule
            if (block.length != sizeof(TvgFlag)) return LoaderResult::SizeCorruption;
            switch (*block.data)
            {
                case TVG_SHAPE_FILLRULE_WINDING_FLAG:
                    shape->fill(FillRule::Winding);
                    break;
                case TVG_SHAPE_FILLRULE_EVENODD_FLAG:
                    shape->fill(FillRule::EvenOdd);
                    break;
            }
            break;
        }
        default:
        {
            return LoaderResult::InvalidType;
        }
    }

    return LoaderResult::Success;
}

// Picture
/*
 * Load picture from .tvg binary file.
 * Returns LoaderResult::Success on success, LoaderResult::InvalidType or other if corrupted.
 *
 * Details:
 * [TVG_RAW_IMAGE_BEGIN_INDICATOR][8+size][uint32_t width][uint32_t height][pixels]
 * other single paint - see _parsePaint()
 */
static LoaderResult _parsePicture(tvgBlock block, Picture *picture)
{
    switch (block.type)
    {
        case TVG_RAW_IMAGE_BEGIN_INDICATOR:
        {
            if (block.length < 2*sizeof(uint32_t)) return LoaderResult::SizeCorruption;

            const char* pointer = block.data;
            uint32_t w, h;

            _read_tvg_ui32(&w, pointer);
            pointer += sizeof(uint32_t);
            _read_tvg_ui32(&h, pointer);
            pointer += sizeof(uint32_t);

            uint32_t size = w * h * sizeof(uint32_t);
            if (block.length != 2*sizeof(uint32_t) + size) return LoaderResult::SizeCorruption;

            uint32_t* pixels = (uint32_t*) pointer;
            picture->load(pixels, w, h, true);
            return LoaderResult::Success;
        }
    }

    Paint* paint;
    LoaderResult result = _parsePaint(block, &paint);
    if (result == LoaderResult::Success) {
        if (picture->paint(unique_ptr<Paint>(paint)) != Result::Success) {
            return LoaderResult::LogicalCorruption;
        }
    }
    return result;
}

/*
 * Read general paint section of the .tvg binary file
 * base_block should point scene / shape / picture begin indicator.
 * Returns LoaderResult::Success on success, LoaderResult::InvalidType or other if corrupted.
 */
static LoaderResult _parsePaint(tvgBlock base_block, Paint **paint)
{
    switch (base_block.type)
    {
        case TVG_SCENE_BEGIN_INDICATOR:
        {
            auto s = Scene::gen();
            const char* pointer = base_block.data;
            while (pointer < base_block.blockEnd)
            {
                tvgBlock block = _readTvgBlock(pointer);
                if (block.blockEnd > base_block.blockEnd) return LoaderResult::SizeCorruption;

                LoaderResult result = _parseScene(block, s.get());
                if (result == LoaderResult::InvalidType) result = _parsePaint(block, s.get());

                if (result > LoaderResult::Success)
                {
                    // LOG: tvg parsing error
#ifdef THORVG_LOG_ENABLED
                    printf("TVG_LOADER: Loading scene error[type: 0x%02x]: %d\n", (int) block.type, (int) result);
#endif
                    return result;
                }
                pointer = block.blockEnd;
            }
            *paint = s.release();
            break;
        }
        case TVG_SHAPE_BEGIN_INDICATOR:
        {
            auto s = Shape::gen();
            const char* pointer = base_block.data;
            while (pointer < base_block.blockEnd)
            {
                tvgBlock block = _readTvgBlock(pointer);
                if (block.blockEnd > base_block.blockEnd) return LoaderResult::SizeCorruption;

                LoaderResult result = _parseShape(block, s.get());
                if (result == LoaderResult::InvalidType) result = _parsePaint(block, s.get());

                if (result > LoaderResult::Success)
                {
                    // LOG: tvg parsing error
#ifdef THORVG_LOG_ENABLED
                    printf("TVG_LOADER: Loading shape error[type: 0x%02x]: %d\n", (int) block.type, (int) result);
#endif
                    return result;
                }
                pointer = block.blockEnd;
            }
            *paint = s.release();
            break;
        }
        case TVG_PICTURE_BEGIN_INDICATOR:
        {
            auto s = Picture::gen();
            const char* pointer = base_block.data;
            while (pointer < base_block.blockEnd)
            {
                tvgBlock block = _readTvgBlock(pointer);
                if (block.blockEnd > base_block.blockEnd) return LoaderResult::SizeCorruption;

                LoaderResult result = _parsePicture(block, s.get());
                if (result == LoaderResult::InvalidType) result = _parsePaint(block, s.get());

                if (result > LoaderResult::Success)
                {
                    // LOG: tvg parsing error
#ifdef THORVG_LOG_ENABLED
                    printf("TVG_LOADER: Loading picture error[type: 0x%02x]: %d\n", (int) block.type, (int) result);
#endif
                    return result;
                }
                pointer = block.blockEnd;
            }
            *paint = s.release();
            break;
        }
        default:
            return LoaderResult::InvalidType;
    }
    return LoaderResult::Success;
}

// Load .tvg file to pointed scene
bool tvgParseTvgFile(const char *pointer, uint32_t size, Scene *scene)
{
    const char* end = pointer + size;
    if (!_readTvgHeader(&pointer) || pointer >= end)
    {
        // LOG: Header is improper
#ifdef THORVG_LOG_ENABLED
        printf("TVG_LOADER: Header is improper.\n");
#endif
        return false;
    }

    Paint* paint;
    while (pointer < end)
    {
        tvgBlock block = _readTvgBlock(pointer);
        if (block.blockEnd > end) return false;

        LoaderResult result = _parsePaint(block, &paint);
        if (result > LoaderResult::Success) return false;
        if (result == LoaderResult::Success) {
            if (scene->push(unique_ptr<Paint>(paint)) != Result::Success) {
                return false;
            }
        }

        pointer = block.blockEnd;
    }

    // LOG: File parsed correctly
#ifdef THORVG_LOG_ENABLED
    printf("TVG_LOADER: File parsed correctly.\n");
#endif
    return true;
}
