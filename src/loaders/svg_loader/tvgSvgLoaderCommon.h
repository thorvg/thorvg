/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#ifndef _TVG_SVG_LOADER_COMMON_H_
#define _TVG_SVG_LOADER_COMMON_H_

#include "tvgCommon.h"
#include "tvgSimpleXmlParser.h"

enum class SvgNodeType
{
    Doc,
    G,
    Defs,
    //Switch,  //Not support
    Animation,
    Arc,
    Circle,
    Ellipse,
    Image,
    Line,
    Path,
    Polygon,
    Polyline,
    Rect,
    Text,
    TextArea,
    Tspan,
    Use,
    Video,
    //Custome_command,   //Not support
    Unknown
};

enum class SvgLengthType
{
    Percent,
    Px,
    Pc,
    Pt,
    Mm,
    Cm,
    In,
};

enum class SvgFillFlags
{
    Paint = 0x1,
    Opacity = 0x2,
    Gradient = 0x4,
    FillRule = 0x8
};

enum class SvgStrokeFlags
{
    Paint = 0x1,
    Opacity = 0x2,
    Gradient = 0x4,
    Scale = 0x8,
    Width = 0x10,
    Cap = 0x20,
    Join = 0x40,
    Dash = 0x80,
};

enum class SvgGradientType
{
    Linear,
    Radial
};

enum class SvgStyleType
{
    Quality,
    Fill,
    ViewportFill,
    Font,
    Stroke,
    SolidColor,
    Gradient,
    Transform,
    Opacity,
    CompOp
};

enum class SvgFillRule
{
    Winding = 0,
    OddEven = 1
};

//Length type to recalculate %, pt, pc, mm, cm etc
enum class SvgParserLengthType
{
    Vertical,
    Horizontal,
    //In case of, for example, radius of radial gradient
    Other
};

typedef struct _SvgNode SvgNode;
typedef struct _SvgStyleGradient SvgStyleGradient;

struct SvgDocNode
{
    float w;
    float h;
    float vx;
    float vy;
    float vw;
    float vh;
    SvgNode* defs;
    bool preserveAspect;
};

struct SvgGNode
{
};

struct SvgDefsNode
{
    vector<SvgStyleGradient *> gradients;
};

struct SvgArcNode
{
};

struct SvgEllipseNode
{
    float cx;
    float cy;
    float rx;
    float ry;
};

struct SvgCircleNode
{
    float cx;
    float cy;
    float r;
};

struct SvgRectNode
{
    float x;
    float y;
    float w;
    float h;
    float rx;
    float ry;
};

struct SvgLineNode
{
    float x1;
    float y1;
    float x2;
    float y2;
};

struct SvgPathNode
{
    string* path;
};

struct SvgPolygonNode
{
    int pointsCount;
    float* points;
};

struct SvgLinearGradient
{
    float x1;
    float y1;
    float x2;
    float y2;
};

struct SvgRadialGradient
{
    float cx;
    float cy;
    float fx;
    float fy;
    float r;
};

struct SvgGradientStop
{
    float offset;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct SvgPaint
{
    SvgStyleGradient* gradient;
    string *url;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    bool none;
    bool curColor;
};

struct SvgDash
{
    float length;
    float gap;
};

struct _SvgStyleGradient
{
    SvgGradientType type;
    string *id;
    string *ref;
    FillSpread spread;
    SvgRadialGradient* radial;
    SvgLinearGradient* linear;
    Matrix* transform;
    vector<Fill::ColorStop *> stops;
    bool userSpace;
    bool usePercentage;
};

struct SvgStyleFill
{
    SvgFillFlags flags;
    SvgPaint paint;
    int opacity;
    SvgFillRule fillRule;
};

struct SvgStyleStroke
{
    SvgStrokeFlags flags;
    SvgPaint paint;
    int opacity;
    float scale;
    float width;
    float centered;
    StrokeCap cap;
    StrokeJoin join;
    SvgDash* dash;
    int dashCount;
};

struct SvgStyleProperty
{
    SvgStyleFill fill;
    SvgStyleStroke stroke;
    int opacity;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct _SvgNode
{
    SvgNodeType type;
    SvgNode* parent;
    vector<SvgNode*> child;
    string *id;
    SvgStyleProperty *style;
    Matrix* transform;
    union {
        SvgGNode g;
        SvgDocNode doc;
        SvgDefsNode defs;
        SvgArcNode arc;
        SvgCircleNode circle;
        SvgEllipseNode ellipse;
        SvgPolygonNode polygon;
        SvgPolygonNode polyline;
        SvgRectNode rect;
        SvgPathNode path;
        SvgLineNode line;
    } node;
    bool display;
};

struct SvgParser
{
    SvgNode* node;
    SvgStyleGradient* styleGrad;
    Fill::ColorStop* gradStop;
    struct
    {
        int x, y;
        uint32_t w, h;
    } global;
    struct
    {
        bool parsedFx;
        bool parsedFy;
    } gradient;
};

struct SvgLoaderData
{
    vector<SvgNode *> stack;
    SvgNode* doc = nullptr;
    SvgNode* def = nullptr;
    vector<SvgStyleGradient*> gradients;
    SvgStyleGradient* latestGradient = nullptr; //For stops
    SvgParser* svgParse = nullptr;
    int level = 0;
    bool result = false;
};

#endif
