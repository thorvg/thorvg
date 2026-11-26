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

#ifndef _TVG_TEXT_PARSER_H_
#define _TVG_TEXT_PARSER_H_

#include "tvgCommon.h"
#include "tvgColor.h"
#include "tvgRender.h"
#include "tvgModel.h"
#include "tvgResource.h"

struct TvgTextParser
{
    TvgTextParser(const char* content, uint32_t size);
    ~TvgTextParser() { delete (resource); }
    TvgScene* parse(TvgResource*& out);
    Result header(uint16_t& ver, float& w, float& h, float& dur);

private:
    // resuable buffers
    Array<TvgPoint::Keyframe> pointFrame;
    Array<TvgFloat::Keyframe> floatFrame;
    Array<TvgFloat4::Keyframe> float4Frame;
    Array<TvgFloat3::Keyframe> float3Frame;
    Array<TvgOpacity::Keyframe> opacityFrame;
    Array<TvgSolid::Keyframe> colorFrame;
    Array<TvgPoints::Keyframe> pathFrame;
    Array<TvgColorStop::Keyframe> stopFrame;
    Array<Fill::ColorStop> stops;

    TvgResource* resource = nullptr;

    string buffer;           // current key and optional ID or value, separated by a null terminator.
    const char* cursor;      // current position in content where parsing resumes.
    const char* end;         // tvg data end position
    uint32_t idOffset = 0;   // byte offset of the ID in buffer, or zero when no ID is present.
    size_t valueOffset = 0;  // byte offset of the property value, or zero for a structure.
    bool pending = false;    // next() skips the current structure unless enter() is called.
    bool malformed = false;  // true if this parsing data is malformed

    // helpers
    void enter();
    void trim(const char*& p, const char* end);
    const char* next();
    bool next(const char* expected, bool optional = false);
    void error(const char* msg);
    bool read(const char*& p, const char* end, float& out, bool separator);

    // parsers
    void parseStyle();
    void parseInterp();
    bool parseTransform(TvgPaint* paint, TvgTransform*& t, const char* key);
    bool parseGradient(TvgShape* shape, TvgGradient* g, const char* key);
    bool parsePaint(TvgPaint* paint, const char* key);
    TvgScene* parseScene(TvgScene* parent);
    void parseShape(TvgScene* parent);
    void parseClip(TvgScene* scene);
    void parseBlur(TvgScene* scene);
    void parseShadow(TvgScene* scene);
    void parseMotion(TvgScene* scene);
    void parseFill(TvgScene* scene);
    void parseTritone(TvgScene* scene);
    void parseTint(TvgScene* scene);
    void parseEffect(TvgScene* scene);
    void parseRect(TvgPaint* paint, Array<TvgPath*>& list);
    void parseCircle(TvgPaint* paint, Array<TvgPath*>& list);
    void parsePath(TvgPaint* paint, Array<TvgPath*>& list);
    bool parsePath(TvgPaint* paint, Array<TvgPath*>& list, const char* key);
    void parseStroke(TvgShape* shape);
    void parseTrim(TvgShape* shape);
    void parseLinear(TvgShape* shape, TvgGradient*& gradient);
    void parseRadial(TvgShape* shape, TvgGradient*& gradient);
    void parseConic(TvgShape* shape, TvgGradient*& gradient);

    // primitive property
    const char* getId();
    bool getFloat(float* out, uint32_t cnt);
    float getFloat();
    Point getPoint();
    int getInteger();
    RGBA getFill();
    bool getBool();

    // animatable property
    Interpolator* getInterpolator();
    void getValue(Point& value) { value = getPoint(); }
    void getValue(float& value) { value = getFloat(); }
    void getValue(Float4& value) { getFloat(value.v, 4); }
    void getValue(Float3& value) { getFloat(value.v, 3); }
    void getValue(uint8_t& value) { value = getInteger(); }
    void getValue(RGBA& value) { value = getFill(); }
    void getValue(ColorStops& value);
    Array<TvgPoint::Keyframe>& getFrames(Point*) { return pointFrame; }
    Array<TvgFloat::Keyframe>& getFrames(float*) { return floatFrame; }
    Array<TvgFloat3::Keyframe>& getFrames(Float3*) { return float3Frame; }
    Array<TvgFloat4::Keyframe>& getFrames(Float4*) { return float4Frame; }
    Array<TvgOpacity::Keyframe>& getFrames(uint8_t*) { return opacityFrame; }
    Array<TvgSolid::Keyframe>& getFrames(RGBA*) { return colorFrame; }
    Array<TvgColorStop::Keyframe>& getFrames(ColorStops*) { return stopFrame; }

    template<typename T> void getProperty(TvgPaint* obj, TvgProperty<T>& out);
};

#endif  // _TVG_TEXT_PARSER_H_
