/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

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

#include "tvgCommon.h"
#include "tvgStr.h"
#include "tvgText.h"
#include "tvgCompressor.h"
#include "tvgInlist.h"
#include "tvgRender.h"
#include "tvgLottieProperty.h"
#include "tvgLottieRenderPooler.h"
#include "tvgLottieTween.h"

#ifdef THORVG_MEDIA_LOADER_SUPPORT
    #include "thorvg_media.h"
#endif

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
            for (uint8_t i = 0; i < dashattr->size; ++i) {
                newValues[i].copy(dashattr->values[i]);
            }
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
    enum Type : uint8_t
    {
        Custom = 5,
        Tint = 20,
        Fill,
        Stroke,
        Tritone,
        DropShadow = 25,
        SetMatte = 28,
        GaussianBlur = 29
    };

    LottieEffect(Type type) : type(type) {}
    virtual ~LottieEffect() {}

    unsigned long nm;  //encoded by djb2
    unsigned long mn;  //encoded by djb2
    int16_t ix;
    Type type;
    bool enable = false;
};


struct LottieFxCustom : LottieEffect
{
    struct Property
    {
        LottieProperty* property;
        unsigned long nm = 0;  //encoded by djb2
        unsigned long mn = 0;  //encoded by djb2
    };

    char* name = nullptr;
    Array<Property> props;

    LottieFxCustom() : LottieEffect(LottieEffect::Custom) {}

    ~LottieFxCustom()
    {
        ARRAY_FOREACH(p, props) delete(p->property);
    }

    Property* property(int type)
    {
        LottieProperty* prop = nullptr;

        switch (type) {
            case 0: //slider
            case 1: prop = new LottieFloat; break; //angle
            case 2: prop = new LottieColor; break; //color
            case 3: prop = new LottieVector; break; //point
            case 4: //checkbox
            case 7: //dropdown
            case 10: prop = new LottieInteger; break;  //effect layer
            case 6: TVGLOG("LOTTIE", "custom ignored?"); break;
            default:
                TVGLOG("LOTTIE", "missing custom property = %d\n", type);
                return nullptr;
        }
        if (prop) {
            props.push({prop, });
            return &props.last();
        }
        return nullptr;
    }

    LottieProperty* property(const char* name)
    {
        auto id = djb2Encode(name);
        ARRAY_FOREACH(p, props) {
            if (p->mn == id || p->nm == id) return p->property;
        }
        return nullptr;
    }
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

    LottieFxFill() : LottieEffect(LottieEffect::Fill) {}
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

    LottieFxStroke() : LottieEffect(LottieEffect::Stroke) {}
};

struct LottieFxTint : LottieEffect
{
    LottieColor black;
    LottieColor white;
    LottieFloat intensity;

    LottieFxTint() : LottieEffect(LottieEffect::Tint) {}
};

struct LottieFxTritone : LottieEffect
{
    LottieColor bright;
    LottieColor midtone;
    LottieColor dark;
    LottieOpacity blend;

    LottieFxTritone() : LottieEffect(LottieEffect::Tritone) {}
};

struct LottieFxDropShadow : LottieEffect
{
    LottieColor color;
    LottieFloat opacity = 0;
    LottieFloat angle = 0.0f;
    LottieFloat distance = 0.0f;
    LottieFloat blurness = 0.0f;

    LottieFxDropShadow() : LottieEffect(LottieEffect::DropShadow) {}
};

struct LottieFxGaussianBlur : LottieEffect
{
    LottieFloat blurness = 0.0f;
    LottieInteger direction = 0;
    LottieInteger wrap = 0;

    LottieFxGaussianBlur() : LottieEffect(LottieEffect::GaussianBlur) {}
};

struct LottieFxSetMatte : LottieEffect
{
    LottieInteger matteLayer = -1;
    LottieInteger useForMatte = 1;
    LottieInteger invert = 0;
    //LottieInteger layerSizeDiff;
    LottieInteger composite = 1;
    //LottieInteger premultiply;

    LottieFxSetMatte()
    {
        type = LottieEffect::SetMatte;
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

// Property (Slot) Override Helper
struct LottieOverride
{
    // Applies a slot property override to the first target with a matching sid.
    // The original property is either saved as a backup or released before the
    // override is copied into the target.
    template<typename T>
    static bool apply(T& target, LottieProperty* prop, LottieProperty*& backup, bool release)
    {
        if (target.sid != prop->sid) return false;
        if (release) target.release();
        else backup = new T(target);
        target.copy(*static_cast<T*>(prop), false);
        return true;
    }

#define OVERRIDE(target) LottieOverride::apply(target, prop, backup, release)
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
        OffsetPath,
        PuckerBloat,
        ZigZag,
        TextRange,
        Audio
    };

    LottieObject(Type type) : type(type) {}
    virtual ~LottieObject() {}

    virtual LottieProperty* override(LottieProperty* prop, bool release)
    {
        TVGERR("LOTTIE", "Unsupported slot type");
        return nullptr;
    }

    virtual bool mergeable() { return false; }
    virtual LottieProperty* property(uint16_t ix) { return nullptr; }

    unsigned long id = 0;      //unique id by name generated by djb2 encoding
    Type type;
    bool hidden = false;       //remove?
};


struct LottieGlyph
{
    Array<LottieObject*> children;   //glyph shapes.
    float width;
    char* code = nullptr;
    char* family = nullptr;
    char* style = nullptr;
    uint16_t size;
    uint8_t len;

    bool prepare()
    {
        len = code ? strlen(code) : 0;
        return len > 0;
    }

    ~LottieGlyph()
    {
        ARRAY_FOREACH(p, children) delete(*p);
        tvg::free(code);
        tvg::free(family);
        tvg::free(style);
    }
};


struct LottieTextRange : LottieObject
{
    enum Based : uint8_t { Chars = 1, CharsExcludingSpaces, Words, Lines };
    enum Shape : uint8_t { Square = 1, RampUp, RampDown, Triangle, Round, Smooth };
    enum Unit : uint8_t { Percent = 1, Index };

    LottieTextRange() : LottieObject(LottieObject::TextRange)
    {
        style.flags.fillColor = 0;
        style.flags.strokeColor = 0;
        style.flags.strokeWidth = 0;
    }

    ~LottieTextRange()
    {
        tvg::free(interpolator);
    }

    struct {
        LottieColor fillColor = RGB32{255, 255, 255};
        LottieColor strokeColor = RGB32{255, 255, 255};
        LottieVector position = Point{0, 0};
        LottieScalar scale = Point{100, 100};
        LottieFloat letterSpace = 0.0f;
        LottieFloat lineSpace = 0.0f;
        LottieFloat strokeWidth = 0.0f;
        LottieFloat rotation = 0.0f;
        LottieOpacity fillOpacity = 255;
        LottieOpacity strokeOpacity = 255;
        LottieOpacity opacity = 255;
        struct {
            bool fillColor : 1;
            bool strokeColor : 1;
            bool strokeWidth : 1;
        } flags;
    } style;

    LottieFloat offset = 0.0f;
    LottieFloat maxEase = 0.0f;
    LottieFloat minEase = 0.0f;
    LottieFloat maxAmount = 0.0f;
    LottieFloat smoothness = 100.0f;
    LottieFloat start = 0.0f;
    LottieFloat end = 100.0f;
    LottieInterpolator* interpolator = nullptr;
    Based based = Chars;
    Shape shape = Square;
    Unit rangeUnit = Percent;
    uint8_t random = 0;
    bool expressible = false;

    float factor(float frameNo, float totalLen, float idx);

    void color(float frameNo, RGB32& fillColor, RGB32& strokeColor, float factor, LottieTween& tween, LottieExpressions* exps)
    {
        if (style.flags.fillColor) {
            fillColor = tvg::lerp(fillColor, style.fillColor(frameNo, tween, exps), factor);
        }
        if (style.flags.strokeColor) {
            strokeColor = tvg::lerp(strokeColor, style.strokeColor(frameNo, tween, exps), factor);
        }
    }

    LottieProperty* override(LottieProperty* prop, bool release) override
    {
        LottieProperty* backup = nullptr;
        OVERRIDE(style.fillColor) || OVERRIDE(style.strokeColor) || OVERRIDE(style.position) || OVERRIDE(style.scale) || OVERRIDE(style.rotation) || OVERRIDE(style.letterSpace) ||
        OVERRIDE(style.lineSpace) || OVERRIDE(style.strokeWidth) || OVERRIDE(style.fillOpacity) || OVERRIDE(style.strokeOpacity) || OVERRIDE(style.opacity);
        return backup;
    }
};


struct LottieFont
{
    enum Origin : uint8_t {Local = 0, CssURL, ScriptURL, FontURL};

    ~LottieFont()
    {
        if (path) {
            Text::unload(name);
            tvg::free(b64src);
        } else {
            Text::load(name, nullptr, size, mime, false);
        }

        ARRAY_FOREACH(p, chars) delete(*p);
        tvg::free(style);
        tvg::free(family);
        tvg::free(name);
        tvg::free(mime);
    }

    char* b64src = nullptr;
    Array<LottieGlyph*> chars;
    char* name = nullptr;
    char* family = nullptr;
    char* style = nullptr;
    char* mime = nullptr;
    uint32_t size = 0;
    float ascent = 0.0f;
    Origin origin = Local;
    bool path = false;  // true if the b64src is path

    void prepare()
    {
        if (path) Text::load(b64src);
        else TextImpl::load(name, b64src, size, mime, Ownership::Transfer);
    }
};

struct LottieMarker
{
    char* name = nullptr;
    float time = 0.0f;
    float duration = 0.0f;
    
    ~LottieMarker()
    {
        tvg::free(name);
    }
};


struct LottieTextFollowPath
{
private:
    RenderPath path;
    PathCommand* cmds;
    uint32_t cmdsCnt;
    Point* pts;
    Point* start;
    float totalLen;
    float currentLen;
    Point split(float dLen, float lenSearched, float& angle);
    void rewind();

public:
    LottieFloat firstMargin = 0.0f;
    LottieMask* mask;
    int8_t maskIdx = -1;

    Point position(float lenSearched, float& angle);
    float prepare(LottieMask* mask, float frameNo, float scale, LottieTween& tween, LottieExpressions* exps);
};


struct LottieText : LottieObject, LottieRenderPooler<tvg::Shape>
{
    struct AlignOption
    {
        enum Group : uint8_t { Chars = 1, Word = 2, Line = 3, All = 4 };
        Group group = Chars;
        LottieScalar anchor{};
    } alignOp;

    LottieText() : LottieObject(LottieObject::Text) {}

    LottieProperty* override(LottieProperty* prop, bool release) override
    {
        LottieProperty* backup = nullptr;
        OVERRIDE(doc);
        return backup;
    }

    LottieProperty* property(uint16_t ix) override
    {
        if (doc.ix == ix) return &doc;
        return nullptr;
    }

    LottieTextDoc doc;
    LottieFont* font = nullptr;
    LottieTextFollowPath* follow = nullptr;
    Array<LottieTextRange*> ranges;

    ~LottieText()
    {
        ARRAY_FOREACH(p, ranges) delete(*p);
        delete(follow);
    }
};


struct LottieTrimpath : LottieObject
{
    enum Type : uint8_t { Simultaneous = 1, Individual = 2 };

    LottieTrimpath() : LottieObject(LottieObject::Trimpath) {}

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

    void segment(float frameNo, float& start, float& end, LottieTween& tween, LottieExpressions* exps);

    LottieFloat start = 0.0f;
    LottieFloat end = 100.0f;
    LottieFloat offset = 0.0f;
    Type type = Simultaneous;
};


struct LottieShape : LottieObject, LottieRenderPooler<Shape>
{
    bool clockwise = true;   //clockwise or counter-clockwise

    virtual ~LottieShape() {}

    bool mergeable() override
    {
        return true;
    }

    LottieShape(LottieObject::Type type) : LottieObject(type) {}
};


struct LottieRoundedCorner : LottieObject
{
    LottieRoundedCorner() : LottieObject(LottieObject::RoundedCorner) {}

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

    SeparateCoord* separateCoord()
    {
        if (!coords) coords = new SeparateCoord;
        return coords;
    }

    ~LottieTransform()
    {
        delete(coords);
        delete (ddd);
    }

    LottieTransform() : LottieObject(LottieObject::Transform) {}

    bool mergeable() override
    {
        return true;
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

    LottieProperty* override(LottieProperty* prop, bool release) override
    {
        LottieProperty* backup = nullptr;
        OVERRIDE(rotation) || OVERRIDE(scale) || OVERRIDE(position) || OVERRIDE(opacity) || OVERRIDE(skewAngle) || OVERRIDE(skewAxis);
        return backup;
    }

    LottieVector position = Point{0.0f, 0.0f};
    LottieFloat rotation = 0.0f;           //z rotation
    LottieScalar scale = Point{100.0f, 100.0f};
    LottieScalar anchor = Point{0.0f, 0.0f};
    LottieOpacity opacity = 255;
    LottieFloat skewAngle = 0.0f;
    LottieFloat skewAxis = 0.0f;

    SeparateCoord* coords = nullptr;       //either a position or separate coordinates

    struct Dimension3
    {
        LottieFloat rx = 0.0f, ry = 0.0f;  // use the rotation for z rotation
        LottieScalar3 orient = {};
    }* ddd = nullptr;
};


struct LottieSolid : LottieObject 
{
    LottieSolid(LottieObject::Type type) : LottieObject(type) {}

    LottieColor color = RGB32{255, 255, 255};
    LottieOpacity opacity = 255;

    LottieProperty* property(uint16_t ix) override
    {
        if (color.ix == ix) return &color;
        if (opacity.ix == ix) return &opacity;
        return nullptr;
    }

    LottieProperty* override(LottieProperty* prop, bool release) override
    {
        LottieProperty* backup = nullptr;
        OVERRIDE(color) || OVERRIDE(opacity);
        return backup;
    }
};


struct LottieSolidStroke : LottieSolid, LottieStroke
{
    LottieSolidStroke() : LottieSolid(LottieObject::SolidStroke) {}

    LottieProperty* property(uint16_t ix) override
    {
        if (width.ix == ix) return &width;
        if (dashattr) {
            for (uint8_t i = 0; i < dashattr->size ; ++i)
                if (dashattr->values[i].ix == ix) return &dashattr->values[i];
        }
        return LottieSolid::property(ix);
    }
};


struct LottieSolidFill : LottieSolid
{
    LottieSolidFill() : LottieSolid(LottieObject::SolidFill) {}

    FillRule rule = FillRule::NonZero;
};


struct LottieGradient : LottieObject
{
    LottieGradient(LottieObject::Type type) : LottieObject(type) {}

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

    LottieProperty* override(LottieProperty* prop, bool release) override
    {
        LottieProperty* backup = nullptr;
        if (OVERRIDE(colorStops)) prepare();
        else OVERRIDE(opacity);
        return backup;
    }

    uint32_t populate(ColorStop& color, size_t count);
    Fill* fill(float frameNo, uint8_t opacity, LottieTween& tween, LottieExpressions* exps);

    LottieScalar start = Point{0.0f, 0.0f};
    LottieScalar end = Point{0.0f, 0.0f};
    LottieFloat height = 0.0f;
    LottieFloat angle = 0.0f;
    LottieOpacity opacity = 255;
    LottieColorStop colorStops;
    uint8_t id = 0; //1: linear, 2: radial
    bool opaque = true; //fully opaque or not
};


struct LottieGradientFill : LottieGradient
{
    LottieGradientFill() : LottieGradient(LottieObject::GradientFill) {}

    FillRule rule = FillRule::NonZero;
};


struct LottieGradientStroke : LottieGradient, LottieStroke
{
    LottieGradientStroke() : LottieGradient(LottieObject::GradientStroke) {}

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

struct LottieImage : LottieObject
{
#ifdef THORVG_MEDIA_LOADER_SUPPORT
    Video* video = nullptr;
#endif
    Picture* picture = nullptr;
    LottieAsset asset;
    bool valid = false;
    bool playing = false;

    LottieImage() : LottieObject(LottieObject::Image) {}

    ~LottieImage()
    {
        release();
    }

    void release()
    {
#ifdef THORVG_MEDIA_LOADER_SUPPORT
        delete (video);
        video = nullptr;
        picture = nullptr;
#else
        if (picture) {
            picture->unref();
            picture = nullptr;
        }
#endif
    }

    LottieProperty* override(LottieProperty* prop, bool release) override
    {
        LottieImage::release();

        LottieProperty* backup = nullptr;
        OVERRIDE(asset);
        valid = playing = false;
        return backup;
    }

    void stop()
    {
#ifdef THORVG_MEDIA_LOADER_SUPPORT
        if (playing) {
            video->stop();
            playing = false;
        }
#endif
    }

    void play(TVG_UNUSED float progress)
    {
#ifdef THORVG_MEDIA_LOADER_SUPPORT
        // tolerance for seeking, to avoid unnecessary seek calls
        auto time = video->time();
        auto seek = progress * video->duration();
        if (fabsf(time - seek) > 1.0f) video->seek(seek);
        if (!playing) {
            video->play();
            playing = true;
        }
#endif
    }

    Picture* get();
};

struct LottieAudio : LottieObject
{
    AssetSrc src;

    LottieAudio() : LottieObject(LottieObject::Audio) {}

    // TODO: override?
};

struct LottieRepeater : LottieObject
{
    LottieRepeater() : LottieObject(LottieObject::Repeater) {}

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
    LottieOffsetPath() : LottieObject(LottieObject::OffsetPath) {}

    LottieFloat offset = 0.0f;
    LottieFloat miterLimit = 4.0f;
    StrokeJoin join = StrokeJoin::Miter;
};

struct LottiePuckerBloat : LottieObject
{
    LottiePuckerBloat() : LottieObject(LottieObject::PuckerBloat) {}

    LottieFloat amount = 0.0f;
};

struct LottieZigZag : LottieObject
{
    LottieZigZag() : LottieObject(LottieObject::ZigZag) {}

    LottieFloat amplitude = 0.0f;
    LottieInteger frequency = 0;
    LottieInteger point = 1; //1: corner, 2: smooth
};

struct LottieGroup : LottieObject, LottieRenderPooler<tvg::Shape>
{
    LottieGroup(LottieObject::Type type = LottieObject::Group);

    virtual ~LottieGroup()
    {
        clear();
    }

    void clear();
    void prepare();
    bool mergeable() override { return allowMerge; }
    LottieProperty* property(uint16_t ix) override;

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
    BlendMethod blendMethod = BlendMethod::Normal;

    bool reqFragment : 1;   //requirement to fragment the render context
    bool buildDone : 1;     //completed in building the composition.
    bool trimpath : 1;      //this group has a trimpath.
    bool visible : 1;       //this group has visible contents.
    bool allowMerge : 1;    //if this group is consisted of simple (transformed) shapes.
};

struct LottieRootLayer : LottieGroup
{
    LottieFloat timeRemap = -1.0f;

    float timeStretch = 1.0f;
    float w = 0.0f, h = 0.0f;
    float inFrame = 0.0f;
    float outFrame = 0.0f;
    float startFrame = 0.0f;

    bool effect = false;  // true if any effect is activated in its tree

    LottieRootLayer(LottieObject::Type type = LottieObject::Composition) : LottieGroup(type) {}

    float remap(LottieComposition* comp, float frameNo, LottieExpressions* exp);
    LottieLayer* layerById(unsigned long id);
    LottieLayer* layerByIdx(int16_t ix);

    LottieProperty* override(LottieProperty* prop, bool release) override
    {
        LottieProperty* backup = nullptr;
        OVERRIDE(timeRemap);
        return backup;
    }
};

struct LottieLayer : LottieRootLayer
{
    enum Type : uint8_t {Precomp = 0, Solid, Image, Null, Shape, Text, Audio};

    LottieLayer();
    ~LottieLayer();

    bool mergeable() override { return false; }
    void prepare(RGB32* color = nullptr);
    void invalidate();
    LottieProperty* property(uint16_t ix) override;

    char* name = nullptr;
    LottieLayer* parent = nullptr;
    LottieRootLayer* precomp = nullptr;  // precompositor, the current layer is belonges.
    LottieTransform* transform = nullptr;
    Array<LottieMask*> masks;
    Array<LottieEffect*> effects;
    LottieLayer* matteTarget = nullptr;

    LottieRenderPooler<tvg::Shape> statical;  //static pooler for solid fill and clipper

    struct AudioControl {
        LottieFloat volume = 100.0f;
        float prevVolume = -1.0f;
        bool prevActive = false;
    } *audioCtrl = nullptr;

    unsigned long rid = 0;      //pre-composition reference id.
    int16_t mix = -1;           //index of the matte layer.
    int16_t pix = -1;           //index of the parent layer.
    int16_t ix = -1;            //index of the current layer.

    struct {
        float frameNo = -1.0f;
        Matrix matrix;
        uint8_t opacity;
    } cache;

    MaskMethod matteType = MaskMethod::None;
    Type type = Null;
    bool autoOrient : 1;
    bool matteSrc : 1;

    AudioControl* audio()
    {
        if (!audioCtrl) audioCtrl = new AudioControl;
        return audioCtrl;
    }

    LottieEffect* effectById(unsigned long id)
    {
        ARRAY_FOREACH(p, effects) {
            if (id == (*p)->nm || id == (*p)->mn) return *p;
        }
        return nullptr;
    }

    LottieEffect* effectByIdx(int16_t ix)
    {
        ARRAY_FOREACH(p, effects) {
            if (ix == (*p)->ix) return *p;
        }
        return nullptr;
    }
};

#undef OVERRIDE

struct LottieSlot
{
    struct Pair
    {
        LottieObject* obj;
        LottieProperty* prop;
    };

    void add(uint32_t slotcode, LottieProperty* prop);
    void apply(LottieProperty* prop, bool byDefault = false);
    void reset();

    LottieSlot(LottieLayer* layer, LottieObject* parent, unsigned long sid, LottieObject* obj, LottieProperty::Type type) : context{layer, parent}, sid(sid), type(type)
    {
        pairs.push({obj, nullptr});
    }

    ~LottieSlot()
    {
        if (!overridden) return;
        ARRAY_FOREACH(pair, pairs) delete(pair->prop);
    }

    struct {
        LottieLayer* layer;
        LottieObject* parent;
    } context;

    unsigned long sid;    // djb2 encoded
    Array<Pair> pairs;    // object-property pairs that can be overridden by this slot
    LottieProperty::Type type;

    bool overridden = false;
};


struct LottieComposition
{
    ~LottieComposition();

    void clear()
    {
        if (root && root->scene) root->scene->remove();
    }

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
            auto obj = *p;
            if (obj->id == id && obj->type == LottieObject::Layer) return static_cast<LottieLayer*>(obj);
        }
        return nullptr;
    }

    void clamp(float& frameNo)
    {
        frameNo += root->inFrame;
        if (frameNo < root->inFrame) frameNo = root->inFrame;
        if (frameNo >= root->outFrame) frameNo = root->outFrame - 1;
    }

    LottieRootLayer* root = nullptr;
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
    uint8_t quality = 50;
};

#endif //_TVG_LOTTIE_MODEL_H_
