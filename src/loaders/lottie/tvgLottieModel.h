/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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

#ifndef _TVG_LOTTIE_MODEL_H_
#define _TVG_LOTTIE_MODEL_H_

#include <cstring>

#include "tvgCommon.h"
#include "tvgRender.h"
#include "tvgLottieProperty.h"
#include "tvgLottieRenderPooler.h"


struct LottieComposition;

struct LottieStroke
{
    struct DashAttr
    {
        LottieFloat offset = 0.0f;
        LottieFloat* values = nullptr;
        uint8_t size = 0;
        uint8_t allocated = 0;
    };

    virtual ~LottieStroke()
    {
        if (dashattr) delete[] dashattr->values;
        delete(dashattr);
    }


    LottieFloat& dashValue()
    {
        if (!dashattr) dashattr = new DashAttr;

        if (dashattr->size + 1 > dashattr->allocated) {
            dashattr->allocated = dashattr->size + 2;
            auto newValues = new LottieFloat[dashattr->allocated];
            for (uint8_t i = 0; i < dashattr->size; ++i) newValues[i] = LottieFloat(dashattr->values[i]);
            delete[] dashattr->values;
            dashattr->values = newValues;
        }

        return dashattr->values[dashattr->size++];
    }


    LottieFloat& dashOffset()
    {
        if (!dashattr) dashattr = new DashAttr;
        return dashattr->offset;
    }

    LottieFloat width = 0.0f;
    DashAttr* dashattr = nullptr;
    float miterLimit = 0;
    StrokeCap cap = StrokeCap::Round;
    StrokeJoin join = StrokeJoin::Round;
};

struct LottieEffect
{
    enum Type : uint8_t {Tint = 20, Fill, Stroke, Tritone, DropShadow = 25, GaussianBlur = 29};

    virtual ~LottieEffect() {}
    Type type;
    bool enable = false;
};

struct LottieFxFill : LottieEffect
{
    //LottieInteger mask;
    //LottieInteger allMask;
    LottieColor color;
    //LottieInteger invert;
    //LottieSlider hFeather;
    //LottieSlider vFeather;
    LottieFloat opacity;

    LottieFxFill()
    {
        type = LottieEffect::Fill;
    }
};

struct LottieFxStroke : LottieEffect
{
    LottieInteger mask;
    LottieInteger allMask;
    //LottieInteger sequential;
    LottieColor color;
    LottieFloat size;
    //LottieFloat hardness;    //should support with the blurness?
    LottieFloat opacity;
    LottieFloat begin;
    LottieFloat end;
    //LottieFloat space;
    //LottieInteger style;

    LottieFxStroke()
    {
        type = LottieEffect::Stroke;
    }
};

struct LottieFxTint : LottieEffect
{
    LottieColor black;
    LottieColor white;
    LottieFloat intensity;

    LottieFxTint()
    {
        type = LottieEffect::Tint;
    }
};

struct LottieFxTritone : LottieEffect
{
    LottieColor bright;
    LottieColor midtone;
    LottieColor dark;

    LottieFxTritone()
    {
        type = LottieEffect::Tritone;
    }
};

struct LottieFxDropShadow : LottieEffect
{
    LottieColor color;
    LottieFloat opacity = 0;
    LottieFloat angle = 0.0f;
    LottieFloat distance = 0.0f;
    LottieFloat blurness = 0.0f;

    LottieFxDropShadow()
    {
        type = LottieEffect::DropShadow;
    }
};

struct LottieFxGaussianBlur : LottieEffect
{
    LottieFloat blurness = 0.0f;
    LottieInteger direction = 0;
    LottieInteger wrap = 0;

    LottieFxGaussianBlur()
    {
        type = LottieEffect::GaussianBlur;
    }
};


struct LottieMask
{
    LottiePathSet pathset;
    LottieFloat expand = 0.0f;
    LottieOpacity opacity = 255;
    MaskMethod method;
    bool inverse = false;
};


struct LottieObject
{
    enum Type : uint8_t
    {
        Composition = 0,
        Layer,
        Group,
        Transform,
        SolidFill,
        SolidStroke,
        GradientFill,
        GradientStroke,
        Rect,
        Ellipse,
        Path,
        Polystar,
        Image,
        Trimpath,
        Text,
        Repeater,
        RoundedCorner,
        OffsetPath
    };

    virtual ~LottieObject()
    {
    }

    virtual void override(LottieProperty* prop, bool shallow, bool byDefault)
    {
        TVGERR("LOTTIE", "Unsupported slot type");
    }

    virtual bool mergeable() { return false; }
    virtual LottieProperty* property(uint16_t ix) { return nullptr; }

    unsigned long id = 0;
    Type type;
    bool hidden = false;       //remove?
};


struct LottieGlyph
{
    Array<LottieObject*> children;   //glyph shapes.
    float width;
    char* code;
    char* family = nullptr;
    char* style = nullptr;
    uint16_t size;
    uint8_t len;

    void prepare()
    {
        len = strlen(code);
    }

    ~LottieGlyph()
    {
        ARRAY_FOREACH(p, children) delete(*p);
        free(code);
    }
};


struct LottieTextRange
{
    enum Based : uint8_t { Chars = 1, CharsExcludingSpaces, Words, Lines };
    enum Shape : uint8_t { Square = 1, RampUp, RampDown, Triangle, Round, Smooth };
    enum Unit : uint8_t { Percent = 1, Index };

    ~LottieTextRange()
    {
        free(interpolator);
    }

    struct {
        LottieColor fillColor = RGB24{255, 255, 255};
        LottieColor strokeColor = RGB24{255, 255, 255};
        LottieVector position = Point{0, 0};
        LottieScalar scale = Point{100, 100};
        LottieFloat letterSpacing = 0.0f;
        LottieFloat lineSpacing = 0.0f;
        LottieFloat strokeWidth = 0.0f;
        LottieFloat rotation = 0.0f;
        LottieOpacity fillOpacity = 255;
        LottieOpacity strokeOpacity = 255;
        LottieOpacity opacity = 255;
    } style;

    LottieFloat offset = 0.0f;
    LottieFloat maxEase = 0.0f;
    LottieFloat minEase = 0.0f;
    LottieFloat maxAmount = 0.0f;
    LottieFloat smoothness = 0.0f;
    LottieFloat start = 0.0f;
    LottieFloat end = FLT_MAX;
    LottieInterpolator* interpolator = nullptr;
    Based based = Chars;
    Shape shape = Square;
    Unit rangeUnit = Percent;
    uint8_t random = 0;
    bool expressible = false;

    float factor(float frameNo, float totalLen, float idx);
};


struct LottieFont
{
    enum Origin : uint8_t { Local = 0, CssURL, ScriptURL, FontURL, Embedded };

    ~LottieFont()
    {
        ARRAY_FOREACH(p, chars) delete(*p);
        free(style);
        free(family);
        free(name);
        free(data.b64src);
    }

    struct {
        char* b64src = nullptr;
        uint32_t size = 0;
    } data;

    Array<LottieGlyph*> chars;
    char* name = nullptr;
    char* family = nullptr;
    char* style = nullptr;
    size_t dataSize = 0;
    float ascent = 0.0f;
    Origin origin = Embedded;

    void prepare();
};

struct LottieMarker
{
    char* name = nullptr;
    float time = 0.0f;
    float duration = 0.0f;
    
    ~LottieMarker()
    {
        free(name);
    }
};

struct LottieText : LottieObject, LottieRenderPooler<tvg::Shape>
{
    struct AlignOption
    {
        enum Group : uint8_t { Chars = 1, Word = 2, Line = 3, All = 4 };
        Group grouping = Chars;
        LottieScalar anchor{};
    } alignOption;

    LottieText()
    {
        LottieObject::type = LottieObject::Text;
    }

    void override(LottieProperty* prop, bool shallow, bool byDefault = false) override
    {
        if (byDefault) doc.release();
        doc.copy(*static_cast<LottieTextDoc*>(prop), shallow);
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (doc.ix == ix) return &doc;
        return nullptr;
    }

    LottieTextDoc doc;
    LottieFont* font;
    Array<LottieTextRange*> ranges;

    ~LottieText()
    {
        ARRAY_FOREACH(p, ranges) delete(*p);
    }
};


struct LottieTrimpath : LottieObject
{
    enum Type : uint8_t { Simultaneous = 1, Individual = 2 };

    LottieTrimpath()
    {
        LottieObject::type = LottieObject::Trimpath;
    }

    bool mergeable() override
    {
        if (!start.frames && start.value == 0.0f && !end.frames && end.value == 100.0f && !offset.frames && offset.value == 0.0f) return true;
        return false;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (start.ix == ix) return &start;
        if (end.ix == ix) return &end;
        if (offset.ix == ix) return &offset;
        return nullptr;
    }

    void segment(float frameNo, float& start, float& end, Tween& tween, LottieExpressions* exps);

    LottieFloat start = 0.0f;
    LottieFloat end = 100.0f;
    LottieFloat offset = 0.0f;
    Type type = Simultaneous;
};


struct LottieShape : LottieObject, LottieRenderPooler<tvg::Shape>
{
    bool clockwise = true;   //clockwise or counter-clockwise

    virtual ~LottieShape() {}

    bool mergeable() override
    {
        return true;
    }

    LottieShape(LottieObject::Type type)
    {
        LottieObject::type = type;
    }
};


struct LottieRoundedCorner : LottieObject
{
    LottieRoundedCorner()
    {
        LottieObject::type = LottieObject::RoundedCorner;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (radius.ix == ix) return &radius;
        return nullptr;
    }

    LottieFloat radius = 0.0f;
};


struct LottiePath : LottieShape
{
    LottiePath() : LottieShape(LottieObject::Path) {}

    LottieProperty* property(uint16_t ix) override
    {
        if (pathset.ix == ix) return &pathset;
        return nullptr;
    }

    LottiePathSet pathset;
};


struct LottieRect : LottieShape
{
    LottieRect() : LottieShape(LottieObject::Rect) {}

    LottieProperty* property(uint16_t ix) override
    {
        if (position.ix == ix) return &position;
        if (size.ix == ix) return &size;
        if (radius.ix == ix) return &radius;
        return nullptr;
    }

    LottieVector position = Point{0.0f, 0.0f};
    LottieScalar size = Point{0.0f, 0.0f};
    LottieFloat radius = 0.0f;       //rounded corner radius
};


struct LottiePolyStar : LottieShape
{
    enum Type : uint8_t {Star = 1, Polygon};

    LottiePolyStar() : LottieShape(LottieObject::Polystar) {}

    LottieProperty* property(uint16_t ix) override
    {
        if (position.ix == ix) return &position;
        if (innerRadius.ix == ix) return &innerRadius;
        if (outerRadius.ix == ix) return &outerRadius;
        if (innerRoundness.ix == ix) return &innerRoundness;
        if (outerRoundness.ix == ix) return &outerRoundness;
        if (rotation.ix == ix) return &rotation;
        if (ptsCnt.ix == ix) return &ptsCnt;
        return nullptr;
    }

    LottieVector position = Point{0.0f, 0.0f};
    LottieFloat innerRadius = 0.0f;
    LottieFloat outerRadius = 0.0f;
    LottieFloat innerRoundness = 0.0f;
    LottieFloat outerRoundness = 0.0f;
    LottieFloat rotation = 0.0f;
    LottieFloat ptsCnt = 0.0f;
    Type type = Polygon;
};


struct LottieEllipse : LottieShape
{
    LottieEllipse() : LottieShape(LottieObject::Ellipse) {}

    LottieProperty* property(uint16_t ix) override
    {
        if (position.ix == ix) return &position;
        if (size.ix == ix) return &size;
        return nullptr;
    }

    LottieVector position = Point{0.0f, 0.0f};
    LottieScalar size = Point{0.0f, 0.0f};
};


struct LottieTransform : LottieObject
{
    struct SeparateCoord
    {
        LottieFloat x = 0.0f;
        LottieFloat y = 0.0f;
    };

    struct RotationEx
    {
        LottieFloat x = 0.0f;
        LottieFloat y = 0.0f;
    };

    ~LottieTransform()
    {
        delete(coords);
        delete(rotationEx);
    }

    LottieTransform()
    {
        LottieObject::type = LottieObject::Transform;
    }

    bool mergeable() override
    {
        if (!opacity.frames && opacity.value == 255) return true;
        return false;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (position.ix == ix) return &position;
        if (rotation.ix == ix) return &rotation;
        if (scale.ix == ix) return &scale;
        if (anchor.ix == ix) return &anchor;
        if (opacity.ix == ix) return &opacity;
        if (skewAngle.ix == ix) return &skewAngle;
        if (skewAxis.ix == ix) return &skewAxis;
        if (coords) {
            if (coords->x.ix == ix) return &coords->x;
            if (coords->y.ix == ix) return &coords->y;
        }
        return nullptr;
    }

    void override(LottieProperty* prop, bool shallow, bool byDefault) override
    {
        switch (prop->type) {
            case LottieProperty::Type::Position: {
                if (byDefault) position.release();
                position.copy(*static_cast<LottieVector*>(prop), shallow);
                break;
            }
            case LottieProperty::Type::Float: {
                if (byDefault) rotation.release();
                rotation.copy(*static_cast<LottieFloat*>(prop), shallow);
                break;
            }
            case LottieProperty::Type::Point: {
                if (byDefault) scale.release();
                scale.copy(*static_cast<LottieScalar*>(prop), shallow);
                break;
            }
            case LottieProperty::Type::Opacity: {
                if (byDefault) opacity.release();
                opacity.copy(*static_cast<LottieOpacity*>(prop), shallow);
                break;
            }
            default: break;
        }
    }

    LottieVector position = Point{0.0f, 0.0f};
    LottieFloat rotation = 0.0f;           //z rotation
    LottieScalar scale = Point{100.0f, 100.0f};
    LottieScalar anchor = Point{0.0f, 0.0f};
    LottieOpacity opacity = 255;
    LottieFloat skewAngle = 0.0f;
    LottieFloat skewAxis = 0.0f;

    SeparateCoord* coords = nullptr;       //either a position or separate coordinates
    RotationEx* rotationEx = nullptr;      //extension for 3d rotation
};


struct LottieSolid : LottieObject 
{
    LottieColor color = RGB24{255, 255, 255};
    LottieOpacity opacity = 255;

    LottieProperty* property(uint16_t ix) override
    {
        if (color.ix == ix) return &color;
        if (opacity.ix == ix) return &opacity;
        return nullptr;
    }
};


struct LottieSolidStroke : LottieSolid, LottieStroke
{
    LottieSolidStroke()
    {
        LottieObject::type = LottieObject::SolidStroke;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (width.ix == ix) return &width;
        if (dashattr) {
            for (uint8_t i = 0; i < dashattr->size ; ++i)
                if (dashattr->values[i].ix == ix) return &dashattr->values[i];
        }
        return LottieSolid::property(ix);
    }

    void override(LottieProperty* prop, bool shallow, bool byDefault) override
    {
        if (byDefault) color.release();
        color.copy(*static_cast<LottieColor*>(prop), shallow);
    }
};


struct LottieSolidFill : LottieSolid
{
    LottieSolidFill()
    {
        LottieObject::type = LottieObject::SolidFill;
    }

    void override(LottieProperty* prop, bool shallow, bool byDefault) override
    {
        if (prop->type == LottieProperty::Type::Opacity) {
            if (byDefault) opacity.release();
            opacity.copy(*static_cast<LottieOpacity*>(prop), shallow);
        } else if (prop->type == LottieProperty::Type::Color) {
            if (byDefault) color.release();
            color.copy(*static_cast<LottieColor*>(prop), shallow);
        }
    }

    FillRule rule = FillRule::NonZero;
};


struct LottieGradient : LottieObject
{
    bool prepare()
    {
        if (!colorStops.populated) {
            auto count = colorStops.count;  //colorstop count can be modified after population
            if (colorStops.frames) {
                ARRAY_FOREACH(v, *colorStops.frames) {
                    colorStops.count = populate(v->value, count);
                }
            } else {
                colorStops.count = populate(colorStops.value, count);
            }
            colorStops.populated = true;
        }
        if (start.frames || end.frames || height.frames || angle.frames || opacity.frames || colorStops.frames) return true;
        return false;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (start.ix == ix) return &start;
        if (end.ix == ix) return &end;
        if (height.ix == ix) return &height;
        if (angle.ix == ix) return &angle;
        if (opacity.ix == ix) return &opacity;
        if (colorStops.ix == ix) return &colorStops;
        return nullptr;
    }

    void override(LottieProperty* prop, bool shallow, bool byDefault = false) override
    {
        if (byDefault) colorStops.release();
        colorStops.copy(*static_cast<LottieColorStop*>(prop), shallow);
        prepare();
    }

    uint32_t populate(ColorStop& color, size_t count);
    Fill* fill(float frameNo, Tween& tween, LottieExpressions* exps);

    LottieScalar start = Point{0.0f, 0.0f};
    LottieScalar end = Point{0.0f, 0.0f};
    LottieFloat height = 0.0f;
    LottieFloat angle = 0.0f;
    LottieOpacity opacity = 255;
    LottieColorStop colorStops;
    uint8_t id = 0;    //1: linear, 2: radial
};


struct LottieGradientFill : LottieGradient
{
    LottieGradientFill()
    {
        LottieObject::type = LottieObject::GradientFill;
    }

    FillRule rule = FillRule::NonZero;
};


struct LottieGradientStroke : LottieGradient, LottieStroke
{
    LottieGradientStroke()
    {
        LottieObject::type = LottieObject::GradientStroke;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (width.ix == ix) return &width;
        if (dashattr) {
            for (uint8_t i = 0; i < dashattr->size ; ++i)
                if (dashattr->values[i].ix == ix) return &dashattr->values[i];
        }
        return LottieGradient::property(ix);
    }
};


struct LottieImage : LottieObject, LottieRenderPooler<tvg::Picture>
{
    LottieBitmap data;

    void override(LottieProperty* prop, bool shallow, bool byDefault = false) override
    {
        if (byDefault) data.release();
        data.copy(*static_cast<LottieBitmap*>(prop), shallow);
        update();
    }

    void prepare();
    void update();
};


struct LottieRepeater : LottieObject
{
    LottieRepeater()
    {
        LottieObject::type = LottieObject::Repeater;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (copies.ix == ix) return &copies;
        if (offset.ix == ix) return &offset;
        if (position.ix == ix) return &position;
        if (rotation.ix == ix) return &rotation;
        if (scale.ix == ix) return &scale;
        if (anchor.ix == ix) return &anchor;
        if (startOpacity.ix == ix) return &startOpacity;
        if (endOpacity.ix == ix) return &endOpacity;
        return nullptr;
    }

    LottieFloat copies = 0.0f;
    LottieFloat offset = 0.0f;

    //Transform
    LottieVector position = Point{0.0f, 0.0f};
    LottieFloat rotation = 0.0f;
    LottieScalar scale = Point{100.0f, 100.0f};
    LottieScalar anchor = Point{0.0f, 0.0f};
    LottieOpacity startOpacity = 255;
    LottieOpacity endOpacity = 255;
    bool inorder = true;        //true: higher,  false: lower
};


struct LottieOffsetPath : LottieObject
{
    LottieOffsetPath()
    {
        LottieObject::type = LottieObject::OffsetPath;
    }

    LottieFloat offset = 0.0f;
    LottieFloat miterLimit = 4.0f;
    StrokeJoin join = StrokeJoin::Miter;
};


struct LottieGroup : LottieObject, LottieRenderPooler<tvg::Shape>
{
    LottieGroup();

    virtual ~LottieGroup()
    {
        ARRAY_FOREACH(p, children) delete(*p);
    }

    void prepare(LottieObject::Type type = LottieObject::Group);
    bool mergeable() override { return allowMerge; }

    LottieObject* content(unsigned long id)
    {
        if (this->id == id) return this;

        //source has children, find recursively.
        ARRAY_FOREACH(p, children) {
            auto child = *p;
            if (child->type == LottieObject::Type::Group || child->type == LottieObject::Type::Layer) {
                if (auto ret = static_cast<LottieGroup*>(child)->content(id)) return ret;
            } else if (child->id == id) return child;
        }
        return nullptr;
    }

    Scene* scene = nullptr;
    Array<LottieObject*> children;

    bool reqFragment : 1;   //requirement to fragment the render context
    bool buildDone : 1;     //completed in building the composition.
    bool trimpath : 1;      //this group has a trimpath.
    bool visible : 1;       //this group has visible contents.
    bool allowMerge : 1;    //if this group is consisted of simple (transformed) shapes.
};


struct LottieLayer : LottieGroup
{
    enum Type : uint8_t {Precomp = 0, Solid, Image, Null, Shape, Text};

    ~LottieLayer();

    bool mergeable() override { return false; }
    void prepare(RGB24* color = nullptr);
    float remap(LottieComposition* comp, float frameNo, LottieExpressions* exp);

    char* name = nullptr;
    LottieLayer* parent = nullptr;
    LottieFloat timeRemap = 0.0f;
    LottieLayer* comp = nullptr;  //Precompositor, current layer is belonges.
    LottieTransform* transform = nullptr;
    Array<LottieMask*> masks;
    Array<LottieEffect*> effects;
    LottieLayer* matteTarget = nullptr;

    LottieRenderPooler<tvg::Shape> statical;  //static pooler for solid fill and clipper

    float timeStretch = 1.0f;
    float w = 0.0f, h = 0.0f;
    float inFrame = 0.0f;
    float outFrame = 0.0f;
    float startFrame = 0.0f;
    unsigned long rid = 0;      //pre-composition reference id.
    int16_t mid = -1;           //id of the matte layer.
    int16_t pidx = -1;          //index of the parent layer.
    int16_t idx = -1;           //index of the current layer.

    struct {
        float frameNo = -1.0f;
        Matrix matrix;
        uint8_t opacity;
    } cache;

    MaskMethod matteType = MaskMethod::None;
    BlendMethod blendMethod = BlendMethod::Normal;
    Type type = Null;
    bool autoOrient = false;
    bool matteSrc = false;

    LottieLayer* layerById(unsigned long id)
    {
        ARRAY_FOREACH(p, children) {
            if ((*p)->type != LottieObject::Type::Layer) continue;
            auto layer = static_cast<LottieLayer*>(*p);
            if (layer->id == id) return layer;
        }
        return nullptr;
    }

    LottieLayer* layerByIdx(int16_t idx)
    {
        ARRAY_FOREACH(p, children) {
            if ((*p)->type != LottieObject::Type::Layer) continue;
            auto layer = static_cast<LottieLayer*>(*p);
            if (layer->idx == idx) return layer;
        }
        return nullptr;
    }
};


struct LottieSlot
{
    struct Pair {
        LottieObject* obj;
        LottieProperty* prop;
    };

    void assign(LottieObject* target, bool byDefault);
    void reset();

    LottieSlot(char* sid, LottieObject* obj, LottieProperty::Type type) : sid(sid), type(type)
    {
        pairs.push({obj});
    }

    ~LottieSlot()
    {
        free(sid);
        if (!overridden) return;
        ARRAY_FOREACH(pair, pairs) delete(pair->prop);
    }

    char* sid;
    Array<Pair> pairs;
    LottieProperty::Type type;
    bool overridden = false;
};


struct LottieComposition
{
    ~LottieComposition();

    float duration() const
    {
        return frameCnt() / frameRate;  // in second
    }

    float frameAtTime(float timeInSec) const
    {
        auto p = timeInSec / duration();
        if (p < 0.0f) p = 0.0f;
        return p * frameCnt();
    }

    float timeAtFrame(float frameNo)
    {
        return (frameNo - root->inFrame) / frameRate;
    }

    float frameCnt() const
    {
        return root->outFrame - root->inFrame;
    }

    LottieLayer* asset(unsigned long id)
    {
        ARRAY_FOREACH(p, assets) {
            auto layer = static_cast<LottieLayer*>(*p);
            if (layer->id == id) return layer;
        }
        return nullptr;
    }

    void clamp(float& frameNo)
    {
        frameNo += root->inFrame;
        if (frameNo < root->inFrame) frameNo = root->inFrame;
        if (frameNo >= root->outFrame) frameNo = root->outFrame - 1;
    }

    LottieLayer* root = nullptr;
    char* version = nullptr;
    char* name = nullptr;
    float w, h;
    float frameRate;
    Array<LottieObject*> assets;
    Array<LottieInterpolator*> interpolators;
    Array<LottieFont*> fonts;
    Array<LottieSlot*> slots;
    Array<LottieMarker*> markers;
    bool expressions = false;
    bool initiated = false;
};

#endif //_TVG_LOTTIE_MODEL_H_
