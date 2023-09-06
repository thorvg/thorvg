/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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

#ifndef _TVG_LOTTIE_PARSER_H_
#define _TVG_LOTTIE_PARSER_H_

#include "tvgCommon.h"
#include "tvgLottieParserHandler.h"

struct LottieParser : LookaheadParserHandler
{
public:
    LottieParser(const char *str, const char* dirName) : LookaheadParserHandler(str)
    {
        this->dirName = dirName;
    }

    bool parse();

    LottieComposition* comp = nullptr;
    const char* dirName = nullptr;       //base resource directory

private:
    BlendMethod getBlendMethod();
    RGB24 getColor(const char *str);
    CompositeMethod getMatteType();
    FillRule getFillRule();
    StrokeCap getStrokeCap();
    StrokeJoin getStrokeJoin();
    CompositeMethod getMaskMethod(bool inversed);
    LottieInterpolator* getInterpolator(const char* key, Point& in, Point& out);

    void getInperpolatorPoint(Point& pt);
    void getPathSet(LottiePathSet& path);
    void getValue(PathSet& path);
    void getValue(Array<Point>& pts);
    void getValue(ColorStop& color);
    void getValue(float& val);
    void getValue(uint8_t& val);
    void getValue(Point& pt);
    void getValue(RGB24& color);
    void getLayerSize(uint32_t& val);

    template<typename T> bool parseTangent(const char *key, LottieVectorFrame<T>& value);
    template<typename T> bool parseTangent(const char *key, LottieScalarFrame<T>& value);
    template<typename T> void parseKeyFrame(T& prop);
    template<typename T> void parsePropertyInternal(T& prop);
    template<typename T> void parseProperty(T& prop);

    LottieObject* parseObject();
    LottieObject* parseAsset();
    LottieImage* parseImage(const char* key);
    LottieLayer* parseLayer();
    LottieObject* parseGroup();
    LottieRect* parseRect();
    LottieEllipse* parseEllipse();
    LottieSolidFill* parseSolidFill();
    LottieTransform* parseTransform(bool ddd = false);
    LottieSolidStroke* parseSolidStroke();
    LottieGradientStroke* parseGradientStroke();
    LottiePath* parsePath();
    LottiePolyStar* parsePolyStar();
    LottieRoundedCorner* parseRoundedCorner();
    LottieGradientFill* parseGradientFill();
    LottieLayer* parseLayers();
    LottieMask* parseMask();
    LottieTrimpath* parseTrimpath();
    LottieRepeater* parseRepeater();

    void parseObject(LottieGroup* parent);
    void parseShapes(LottieLayer* layer);
    void parseMasks(LottieLayer* layer);
    void parseTimeRemap(LottieLayer* layer);
    void parseStrokeDash(LottieStroke* stroke);
    void parseGradient(LottieGradient* gradient, const char* key);
    void parseAssets();

    //Current parsing context
    struct Context {
        LottieLayer* layer = nullptr;
        LottieGradient* gradient = nullptr;
    } *context;
};

#endif //_TVG_LOTTIE_PARSER_H_