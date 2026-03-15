#include "tvgLottieFlatBufferParser.h"
#include "tvgStr.h"
#include "thorvg.h"
#include "../../../../src/schema/zan_generated.h"
#include <unordered_map>
#include <cstdlib>

using namespace Zan::Data;

static unsigned long djb2(const char* str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

static RGB32 hexToRGB255(const flatbuffers::String* hex)
{
    if (!hex) return {255, 255, 255};
    auto str = hex->c_str();
    if (str[0] == '#') str++;
    uint32_t rgb = strtoul(str, nullptr, 16);
    return { (int32_t)((rgb >> 16) & 0xFF), (int32_t)((rgb >> 8) & 0xFF), (int32_t)(rgb & 0xFF) };
}

static RGB32 toRGB255(const Zan::Data::Color* c)
{
    if (!c) return {0, 0, 0};
    return {
        (int32_t)(c->r() * 255.0f),
        (int32_t)(c->g() * 255.0f),
        (int32_t)(c->b() * 255.0f)
    };
}

static char* dupString(const flatbuffers::String* str)
{
    if (!str) return nullptr;
    return tvg::duplicate(str->c_str());
}


static Point toPoint(const Vec2* v)
{
    if (!v) return {0, 0};
    return {v->x(), v->y()};
}

static Point toPoint(const ControlPoint* cp)
{
    if (!cp) return {0, 0};
    return {cp->x(), cp->y()};
}

static LottieInterpolator* getInterpolator(LottieComposition* comp, Point in, Point out)
{
    if (!comp) return nullptr;
    if (tvg::zero(in.x) && tvg::zero(in.y) && tvg::zero(out.x) && tvg::zero(out.y)) return nullptr;

    auto interpolator = new LottieInterpolator;
    interpolator->set(nullptr, in, out);
    comp->interpolators.push(interpolator);
    return interpolator;
}

// Generic Property Parsers

static void parseOpacity(LottieComposition* comp, LottieOpacity& prop, const FloatProperty* zProp)
{
    if (!zProp) return;
    prop.value = (uint8_t)(zProp->static_value() * 2.55f);
    prop.ix = 0;

    if (zProp->keyframes() && zProp->keyframes()->size() > 0) {
        auto count = zProp->keyframes()->size();
        if (prop.frames) {
            delete(prop.frames);
            prop.frames = nullptr;
        }
        prop.frames = new Array<LottieScalarFrame<uint8_t>>;
        prop.frames->reserve(count);
        
        for (uint32_t i = 0; i < count; ++i) {
            auto zKf = zProp->keyframes()->Get(i);
            LottieScalarFrame<uint8_t> tKf;
            
            tKf.no = zKf->start_frame();
            tKf.value = (uint8_t)(zKf->value() * 2.55f);
            
            auto in = toPoint(zKf->in_tangent());
            auto out = toPoint(zKf->out_tangent());
            tKf.interpolator = getInterpolator(comp, in, out);
            
            tKf.hold = zKf->hold();
            prop.frames->push(tKf);
        }
        prop.prepare();
    }
}

static void parseFloat(LottieComposition* comp, LottieFloat& prop, const FloatProperty* zProp)
{
    if (!zProp) return;
    prop.value = zProp->static_value();
    prop.ix = 0;

    if (zProp->keyframes() && zProp->keyframes()->size() > 0) {
        auto count = zProp->keyframes()->size();
        if (prop.frames) {
            delete(prop.frames);
            prop.frames = nullptr;
        }
        prop.frames = new Array<LottieScalarFrame<float>>;
        prop.frames->reserve(count);
        
        for (uint32_t i = 0; i < count; ++i) {
            auto zKf = zProp->keyframes()->Get(i);
            LottieScalarFrame<float> tKf;
            
            tKf.no = zKf->start_frame();
            tKf.value = zKf->value();
            
            auto in = toPoint(zKf->in_tangent());
            auto out = toPoint(zKf->out_tangent());
            tKf.interpolator = getInterpolator(comp, in, out);
            
            tKf.hold = zKf->hold();
            prop.frames->push(tKf);
        }
        prop.prepare();
    }
}

static void parsePoint(LottieComposition* comp, LottieVector& prop, const PointProperty* zProp)
{
    if (!zProp) return;
    prop.value = toPoint(zProp->static_value());
    prop.ix = 0;

    if (zProp->keyframes() && zProp->keyframes()->size() > 0) {
        auto count = zProp->keyframes()->size();
        if (prop.frames) {
            delete(prop.frames);
            prop.frames = nullptr;
        }
        prop.frames = new Array<LottieVectorFrame<Point>>;
        prop.frames->reserve(count);
        
        for (uint32_t i = 0; i < count; ++i) {
            auto zKf = zProp->keyframes()->Get(i);
            LottieVectorFrame<Point> tKf;
            
            tKf.no = zKf->start_frame();
            tKf.value = toPoint(zKf->value());
            
            auto in = toPoint(zKf->in_tangent());
            auto out = toPoint(zKf->out_tangent());
            tKf.interpolator = getInterpolator(comp, in, out);
            
            if (zKf->spatial_in()) tKf.inTangent = toPoint(zKf->spatial_in());
            if (zKf->spatial_out()) tKf.outTangent = toPoint(zKf->spatial_out());
            tKf.hasTangent = true;
            
            tKf.hold = zKf->hold();
            prop.frames->push(tKf);
        }
        prop.prepare();
    }
}

static void parsePointScalar(LottieComposition* comp, LottieScalar& prop, const PointProperty* zProp)
{
    if (!zProp) return;
    prop.value = toPoint(zProp->static_value());
    prop.ix = 0;

    if (zProp->keyframes() && zProp->keyframes()->size() > 0) {
        auto count = zProp->keyframes()->size();
        if (prop.frames) {
            delete(prop.frames);
            prop.frames = nullptr;
        }
        prop.frames = new Array<LottieScalarFrame<Point>>;
        prop.frames->reserve(count);
        
        for (uint32_t i = 0; i < count; ++i) {
            auto zKf = zProp->keyframes()->Get(i);
            LottieScalarFrame<Point> tKf;
            
            tKf.no = zKf->start_frame();
            tKf.value = toPoint(zKf->value());
            
            auto in = toPoint(zKf->in_tangent());
            auto out = toPoint(zKf->out_tangent());
            tKf.interpolator = getInterpolator(comp, in, out);
            
            tKf.hold = zKf->hold();
            prop.frames->push(tKf);
        }
        prop.prepare();
    }
}

static void parseColor(LottieComposition* comp, LottieColor& prop, const ColorProperty* zProp)
{
    if (!zProp) return;
    prop.value = toRGB255(zProp->static_value());
    prop.ix = 0;

    if (zProp->keyframes() && zProp->keyframes()->size() > 0) {
        auto count = zProp->keyframes()->size();
        if (prop.frames) {
            delete(prop.frames);
            prop.frames = nullptr;
        }
        prop.frames = new Array<LottieScalarFrame<RGB32>>;
        prop.frames->reserve(count);
        
        for (uint32_t i = 0; i < count; ++i) {
            auto zKf = zProp->keyframes()->Get(i);
            LottieScalarFrame<RGB32> tKf;
            
            tKf.no = zKf->start_frame();
            tKf.value = toRGB255(zKf->value());
            
            auto in = toPoint(zKf->in_tangent());
            auto out = toPoint(zKf->out_tangent());
            tKf.interpolator = getInterpolator(comp, in, out);
            
            tKf.hold = zKf->hold();
            prop.frames->push(tKf);
        }
        prop.prepare();
    }
}

static void parseTransform(LottieComposition* comp, LottieTransform* tTrans, const Zan::Data::Transform* zTrans)
{
    if (!zTrans || !tTrans) return;
    
    parsePoint(comp, tTrans->position, zTrans->position());
    parsePointScalar(comp, tTrans->anchor, zTrans->anchor_point());
    parsePointScalar(comp, tTrans->scale, zTrans->scale());
    parseFloat(comp, tTrans->rotation, zTrans->rotation());
    parseOpacity(comp, tTrans->opacity, zTrans->opacity());
}

static void parsePathData(PathSet& out, const PathData* zPath)
{
    if (!zPath) return;
    
    if (!zPath->points() || zPath->points()->size() == 0) return;

    auto pointCount = zPath->points()->size();
    bool closed = zPath->closed();
    
    int totalSegments = pointCount - 1 + (closed ? 1 : 0);

    int totalPts = 1 + (totalSegments * 3);
    
    out.ptsCnt = totalPts;
    out.cmdsCnt = 1 + totalSegments + (closed ? 1 : 0);
    
    out.pts = tvg::malloc<Point>(sizeof(Point) * out.ptsCnt);
    out.cmds = tvg::malloc<PathCommand>(sizeof(PathCommand) * out.cmdsCnt);
    
    int pIdx = 0;
    int cIdx = 0;
    
    auto pt0 = zPath->points()->Get(0);
    out.pts[pIdx++] = {pt0->x(), pt0->y()};
    out.cmds[cIdx++] = PathCommand::MoveTo;
    
    for (uint16_t i = 1; i < pointCount; ++i) {
        auto prevPt = zPath->points()->Get(i-1);
        auto currPt = zPath->points()->Get(i);
        
        float prevOutX = 0, prevOutY = 0;
        if (zPath->out_tangents() && i-1 < zPath->out_tangents()->size()) {
            auto t = zPath->out_tangents()->Get(i-1);
            prevOutX = t->x();
            prevOutY = t->y();
        }
        
        float currInX = 0, currInY = 0;
        if (zPath->in_tangents() && i < zPath->in_tangents()->size()) {
            auto t = zPath->in_tangents()->Get(i);
            currInX = t->x();
            currInY = t->y();
        }
        
        out.pts[pIdx++] = {prevPt->x() + prevOutX, prevPt->y() + prevOutY};
        out.pts[pIdx++] = {currPt->x() + currInX, currPt->y() + currInY};
        out.pts[pIdx++] = {currPt->x(), currPt->y()};
        
        out.cmds[cIdx++] = PathCommand::CubicTo;
    }
    
    if (closed) {
        auto prevPt = zPath->points()->Get(pointCount - 1);
        auto currPt = zPath->points()->Get(0);
        
        float prevOutX = 0, prevOutY = 0;
        if (zPath->out_tangents() && pointCount - 1 < zPath->out_tangents()->size()) {
            auto t = zPath->out_tangents()->Get(pointCount - 1);
            prevOutX = t->x();
            prevOutY = t->y();
        }
        
        float currInX = 0, currInY = 0;
        if (zPath->in_tangents() && 0 < zPath->in_tangents()->size()) {
            auto t = zPath->in_tangents()->Get(0);
            currInX = t->x();
            currInY = t->y();
        }
        
        out.pts[pIdx++] = {prevPt->x() + prevOutX, prevPt->y() + prevOutY};
        out.pts[pIdx++] = {currPt->x() + currInX, currPt->y() + currInY};
        out.pts[pIdx++] = {currPt->x(), currPt->y()};
        
        out.cmds[cIdx++] = PathCommand::CubicTo;
        out.cmds[cIdx++] = PathCommand::Close;
    }
}

static void parsePathProperty(LottieComposition* comp, LottiePathSet& pathSet, const PathProperty* zProp)
{
    if (!zProp) return;
    
    parsePathData(pathSet.value, zProp->static_value());

    if (zProp->keyframes() && zProp->keyframes()->size() > 0) {
        auto count = zProp->keyframes()->size();
        if (pathSet.frames) {
             delete(pathSet.frames);
             pathSet.frames = nullptr;
        }
        pathSet.frames = new Array<LottieScalarFrame<PathSet>>;
        pathSet.frames->reserve(count);
        
        for (uint32_t i = 0; i < count; ++i) {
            auto zKf = zProp->keyframes()->Get(i);
            LottieScalarFrame<PathSet> tKf;
            
            tKf.no = zKf->start_frame();
            parsePathData(tKf.value, zKf->value());
            
            auto in = toPoint(zKf->in_tangent());
            auto out = toPoint(zKf->out_tangent());
            tKf.interpolator = getInterpolator(comp, in, out);
            
            tKf.hold = zKf->hold();
            pathSet.frames->push(tKf);
        }
    }
}

static void parseGradientStops(LottieGradient* grad, const flatbuffers::Vector<flatbuffers::Offset<GradientColor>>* stops)
{
    if (!stops) return;
    
    auto count = stops->size();
    grad->colorStops.count = count;
    grad->colorStops.value.data = tvg::malloc<Fill::ColorStop>(sizeof(Fill::ColorStop) * count);
    
    for (uint32_t i = 0; i < count; ++i) {
        auto stop = stops->Get(i);
        grad->colorStops.value.data[i].offset = stop->offset();
        grad->colorStops.value.data[i].r = (uint8_t)(stop->color()->r() * 255);
        grad->colorStops.value.data[i].g = (uint8_t)(stop->color()->g() * 255);
        grad->colorStops.value.data[i].b = (uint8_t)(stop->color()->b() * 255);
        grad->colorStops.value.data[i].a = (uint8_t)stop->alpha();
    }
}

static void parseTextDocument(tvg::TextDocument& doc, const Zan::Data::TextDocument* zDoc)
{
    if (!zDoc) return;
    doc.text = dupString(zDoc->text());
    doc.name = dupString(zDoc->name());
    doc.height = zDoc->height();
    doc.shift = zDoc->shift();
    doc.color = toRGB255(zDoc->color());
    doc.bbox.pos = toPoint(zDoc->bbox_pos());
    doc.bbox.size = toPoint(zDoc->bbox_size());
    doc.stroke.color = toRGB255(zDoc->stroke_color());
    doc.stroke.width = zDoc->stroke_width();
    doc.stroke.below = zDoc->stroke_below();
    doc.size = zDoc->size();
    doc.tracking = zDoc->tracking();
    doc.justify = zDoc->justify();
    doc.caps = zDoc->caps();
}

static void parseTextProperty(LottieComposition* comp, LottieTextDoc& prop, const TextProperty* zProp)
{
    if (!zProp) return;
    prop.release();
    prop.ix = 0;
    if (zProp->static_value()) parseTextDocument(prop.value, zProp->static_value());
    if (zProp->keyframes() && zProp->keyframes()->size() > 0) {
        auto count = zProp->keyframes()->size();
        prop.frames = new Array<LottieScalarFrame<tvg::TextDocument>>;
        prop.frames->reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            auto zKf = zProp->keyframes()->Get(i);
            LottieScalarFrame<tvg::TextDocument> tKf{};
            tKf.no = zKf->start_frame();
            if (zKf->value()) parseTextDocument(tKf.value, zKf->value());
            auto in = toPoint(zKf->in_tangent());
            auto out = toPoint(zKf->out_tangent());
            tKf.interpolator = getInterpolator(comp, in, out);
            tKf.hold = zKf->hold();
            prop.frames->push(tKf);
        }
    }
}

static void parseTextAlign(LottieComposition* comp, LottieText* text, const TextAlignOption* zAlign)
{
    if (!zAlign || !text) return;
    text->alignOp.group = (LottieText::AlignOption::Group)zAlign->group();
    if (zAlign->anchor()) parsePointScalar(comp, text->alignOp.anchor, zAlign->anchor());
}

static LottieText* parseText(LottieComposition* comp, const Zan::Data::Text* zText)
{
    if (!zText) return nullptr;
    auto text = new LottieText;
    if (zText->doc()) parseTextProperty(comp, text->doc, zText->doc());
    if (zText->align()) parseTextAlign(comp, text, zText->align());
    return text;
}

static LottieObject* parseShape(LottieComposition* comp, const ShapeItem* item);

static LottieGlyph* parseGlyph(LottieComposition* comp, const Glyph* zGlyph)
{
    if (!zGlyph) return nullptr;
    auto glyph = new LottieGlyph;
    glyph->code = dupString(zGlyph->code());
    glyph->family = dupString(zGlyph->family());
    glyph->style = dupString(zGlyph->style());
    glyph->size = zGlyph->size();
    glyph->width = zGlyph->width();
    if (zGlyph->shapes()) {
        for (auto item : *zGlyph->shapes()) {
            auto shape = parseShape(comp, item);
            if (shape) glyph->children.push(shape);
        }
    }
    glyph->prepare();
    return glyph;
}

static LottieFont* parseFont(LottieComposition* comp, const Font* zFont)
{
    if (!zFont) return nullptr;
    auto font = new LottieFont;
    font->name = dupString(zFont->name());
    font->family = dupString(zFont->family());
    font->style = dupString(zFont->style());
    font->ascent = zFont->ascent();
    font->origin = (LottieFont::Origin)zFont->origin();
    if (zFont->b64src() && zFont->b64src()->size() > 0) {
        font->b64src = (char*)zFont->b64src()->Data();
        font->size = zFont->b64src()->size();
    } else if (zFont->path()) {
        font->path = (char*)zFont->path()->c_str();
    }
    if (zFont->chars()) {
        for (auto zGlyph : *zFont->chars()) {
            auto glyph = parseGlyph(comp, zGlyph);
            if (glyph) font->chars.push(glyph);
        }
    }
    font->prepare();
    return font;
}

static LottieGroup* parseGroup(LottieComposition* comp, const ShapeGroup* zGroup)
{
    auto group = new LottieGroup;

    if (zGroup->items()) {
        for (auto item : *zGroup->items()) {
            auto child = parseShape(comp, item);
            if (child) group->children.push(child);
        }
    }
    group->prepare();
    return group;
}

static LottieObject* parseShape(LottieComposition* comp, const ShapeItem* item)
{
    if (!item) return nullptr;
    
    LottieObject* obj = nullptr;
    
    switch (item->content_type()) {
        case ShapeContent_ShapeGroup: {
            obj = parseGroup(comp, item->content_as_ShapeGroup());
            break;
        }
        case ShapeContent_ShapeRect: {
            auto zRect = item->content_as_ShapeRect();
            auto rect = new LottieRect;
            parsePoint(comp, rect->position, zRect->position());
            parsePointScalar(comp, rect->size, zRect->size());
            parseFloat(comp, rect->radius, zRect->roundness());
            obj = rect;
            break;
        }
        case ShapeContent_ShapeEllipse: {
            auto zEllipse = item->content_as_ShapeEllipse();
            auto ellipse = new LottieEllipse;
            parsePoint(comp, ellipse->position, zEllipse->position());
            parsePointScalar(comp, ellipse->size, zEllipse->size());
            obj = ellipse;
            break;
        }
        case ShapeContent_ShapeFill: {
            auto zFill = item->content_as_ShapeFill();
            auto fill = new LottieSolidFill;
            parseColor(comp, fill->color, zFill->color());
            parseOpacity(comp, fill->opacity, zFill->opacity());
            fill->rule = (FillRule)zFill->rule(); 
            obj = fill;
            break;
        }
        case ShapeContent_ShapeStroke: {
            auto zStroke = item->content_as_ShapeStroke();
            auto stroke = new LottieSolidStroke;
            parseColor(comp, stroke->color, zStroke->color());
            parseOpacity(comp, stroke->opacity, zStroke->opacity());
            parseFloat(comp, stroke->width, zStroke->width());
            stroke->cap = (StrokeCap)zStroke->cap();
            stroke->join = (StrokeJoin)zStroke->join();
            stroke->miterLimit = zStroke->miter_limit();

            if (zStroke->dash_array()) {
                for (uint32_t i = 0; i < zStroke->dash_array()->size(); ++i) {
                    parseFloat(comp, stroke->dashValue(), zStroke->dash_array()->Get(i));
                }
            }
            parseFloat(comp, stroke->dashOffset(), zStroke->dash_offset());

            obj = stroke;
            break;
        }
        case ShapeContent_ShapeGradientFill: {
            auto zGrad = item->content_as_ShapeGradientFill();
            auto fill = new LottieGradientFill;
            parsePointScalar(comp, fill->start, zGrad->start_point());
            parsePointScalar(comp, fill->end, zGrad->end_point());
            parseOpacity(comp, fill->opacity, zGrad->opacity());
            fill->id = (zGrad->type() == GradientType_Linear) ? 1 : 2;
            fill->rule = (FillRule)zGrad->rule();
            parseGradientStops(fill, zGrad->stops());
            obj = fill;
            break;
        }
        case ShapeContent_ShapeGradientStroke: {
            auto zGrad = item->content_as_ShapeGradientStroke();
            auto stroke = new LottieGradientStroke;
            parsePointScalar(comp, stroke->start, zGrad->start_point());
            parsePointScalar(comp, stroke->end, zGrad->end_point());
            parseOpacity(comp, stroke->opacity, zGrad->opacity());
            parseFloat(comp, stroke->width, zGrad->width());
            stroke->id = (zGrad->type() == GradientType_Linear) ? 1 : 2;
            stroke->cap = (StrokeCap)zGrad->cap();
            stroke->join = (StrokeJoin)zGrad->join();
            stroke->miterLimit = zGrad->miter_limit();
            
            if (zGrad->dash_array()) {
                for (uint32_t i = 0; i < zGrad->dash_array()->size(); ++i) {
                    parseFloat(comp, stroke->dashValue(), zGrad->dash_array()->Get(i));
                }
            }
            parseFloat(comp, stroke->dashOffset(), zGrad->dash_offset());

            stroke->colorStops.populated = false;
            parseGradientStops(stroke, zGrad->stops());
            obj = stroke;
            break;
        }
        case ShapeContent_ShapeTrim: {
            auto zTrim = item->content_as_ShapeTrim();
            auto trim = new LottieTrimpath;
            parseFloat(comp, trim->start, zTrim->start());
            parseFloat(comp, trim->end, zTrim->end());
            parseFloat(comp, trim->offset, zTrim->offset());
            trim->type = (LottieTrimpath::Type)zTrim->mode();
            obj = trim;
            break;
        }
        case ShapeContent_ShapeOffsetPath: {
            auto zOffset = item->content_as_ShapeOffsetPath();
            auto offset = new LottieOffsetPath;
            parseFloat(comp, offset->offset, zOffset->offset());
            parseFloat(comp, offset->miterLimit, zOffset->miter_limit());
            offset->join = (StrokeJoin)zOffset->join();
            obj = offset;
            break;
        }
        case ShapeContent_ShapeMerge: {
            TVGLOG("LOTTIE", "MergePath(mm) is not supported yet");
            break;
        }
        case ShapeContent_ShapePath: {
            auto zPath = item->content_as_ShapePath();
            auto path = new LottiePath;
            parsePathProperty(comp, path->pathset, zPath->path());
            obj = path;
            break;
        }
        case ShapeContent_Transform: {
            auto zTrans = item->content_as_Transform();
            auto trans = new LottieTransform;
            parseTransform(comp, trans, zTrans);
            obj = trans;
            break;
        }
        default:
            break;
    }
    
    if (obj) {
        if (item->name()) obj->id = djb2(item->name()->c_str());
        obj->hidden = item->hidden();
        if (obj->hidden) {
            delete(obj);
            obj = nullptr;
        }
    }
    
    return obj;
}

static MaskMethod getMaskMethod(MaskMode mode, bool inv)
{
    switch (mode) {
        case MaskMode_Add: {
            if (inv) return MaskMethod::InvAlpha;
            return MaskMethod::Add;
        }
        case MaskMode_Subtract: {
            if (inv) return MaskMethod::Intersect;
            return MaskMethod::Subtract;
        }
        case MaskMode_Intersect: {
            if (inv) return MaskMethod::Difference;
            return MaskMethod::Intersect;
        }
        case MaskMode_Difference: {
            if (inv) return MaskMethod::Intersect;
            return MaskMethod::Difference;
        }
        default: return MaskMethod::None;
    }
}

static LottieMask* parseMask(LottieComposition* comp, const Zan::Data::Mask* zMask)
{
    if (!zMask) return nullptr;
    auto mask = new LottieMask;
    mask->inverse = zMask->inv();
    mask->method = getMaskMethod(zMask->mode(), mask->inverse);
    parseOpacity(comp, mask->opacity, zMask->opacity());
    parsePathProperty(comp, mask->pathset, zMask->path());
    parseFloat(comp, mask->expand, zMask->expand());
    return mask;
}

static void resolveParents(LottieLayer* parentLayer)
{
    if (!parentLayer || parentLayer->children.count == 0) return;

    std::unordered_map<int16_t, LottieLayer*> map;
    
    // First pass: populate map
    ARRAY_FOREACH(p, parentLayer->children) {
        if ((*p)->type == LottieObject::Layer) {
            auto layer = static_cast<LottieLayer*>(*p);
            if (layer->ix >= 0) map[layer->ix] = layer;
        }
    }
    
    // Second pass: link parents and recurse
    ARRAY_FOREACH(p, parentLayer->children) {
        if ((*p)->type == LottieObject::Layer) {
            auto layer = static_cast<LottieLayer*>(*p);
            
            // Link Parent
            if (layer->pix != -1) {
                auto it = map.find(layer->pix);
                if (it != map.end()) {
                    layer->parent = it->second;
                }
            }
            
            // Recurse if Precomp (has children)
            if (layer->children.count > 0) {
                resolveParents(layer);
            }
        }
    }
}

static LottieEffect* parseEffect(LottieComposition* comp, const Effect* zEffect) {
    if (!zEffect) return nullptr;
    
    LottieEffect* effect = nullptr;
    auto type = (LottieEffect::Type)zEffect->type();
    
    switch (type) {
        case LottieEffect::Tint: {
            auto fx = new LottieFxTint;
            effect = fx;
            if (zEffect->values()) {
                for (auto val : *zEffect->values()) {
                    auto name = val->name()->c_str();
                    if (strcmp(name, "Map Black To") == 0) parseColor(comp, fx->black, val->v_color());
                    else if (strcmp(name, "Map White To") == 0) parseColor(comp, fx->white, val->v_color());
                    else if (strcmp(name, "Amount to Tint") == 0) parseFloat(comp, fx->intensity, val->v_float());
                }
            }
            break;
        }
        case LottieEffect::Fill: {
            auto fx = new LottieFxFill;
            effect = fx;
            if (zEffect->values()) {
                for (auto val : *zEffect->values()) {
                    auto name = val->name()->c_str();
                    if (strcmp(name, "Color") == 0) parseColor(comp, fx->color, val->v_color());
                    else if (strcmp(name, "Opacity") == 0) parseFloat(comp, fx->opacity, val->v_float());
                }
            }
            break;
        }
        case LottieEffect::Tritone: {
            auto fx = new LottieFxTritone;
            effect = fx;
            if (zEffect->values()) {
                for (auto val : *zEffect->values()) {
                    auto name = val->name()->c_str();
                    if (strcmp(name, "Highlights") == 0) parseColor(comp, fx->bright, val->v_color());
                    else if (strcmp(name, "Midtones") == 0) parseColor(comp, fx->midtone, val->v_color());
                    else if (strcmp(name, "Shadows") == 0) parseColor(comp, fx->dark, val->v_color());
                }
            }
            break;
        }
         case LottieEffect::GaussianBlur: {
            auto fx = new LottieFxGaussianBlur;
            effect = fx;
             if (zEffect->values()) {
                for (auto val : *zEffect->values()) {
                    auto name = val->name()->c_str();
                    if (strcmp(name, "Blurriness") == 0) parseFloat(comp, fx->blurness, val->v_float());
                    else if (strcmp(name, "Blur Dimensions") == 0 && val->v_float()) fx->direction = (int)val->v_float()->static_value();
                }
            }
            break;
        }
        case LottieEffect::DropShadow: {
             auto fx = new LottieFxDropShadow;
             effect = fx;
             if (zEffect->values()) {
                 for (auto val : *zEffect->values()) {
                     auto name = val->name()->c_str();
                     if (strcmp(name, "Shadow Color") == 0) parseColor(comp, fx->color, val->v_color());
                     else if (strcmp(name, "Opacity") == 0) parseFloat(comp, fx->opacity, val->v_float());
                     else if (strcmp(name, "Direction") == 0) parseFloat(comp, fx->angle, val->v_float());
                     else if (strcmp(name, "Distance") == 0) parseFloat(comp, fx->distance, val->v_float());
                     else if (strcmp(name, "Softness") == 0) parseFloat(comp, fx->blurness, val->v_float());
                 }
             }
             break;
        }
        default:
            break;
    }
    
    if (effect) {
        if (zEffect->name()) {
             auto nameStr = zEffect->name()->c_str();
             effect->nm = djb2(nameStr);
        }
        if (zEffect->match_name()) {
             auto matchNameStr = zEffect->match_name()->c_str();
             effect->mn = djb2(matchNameStr);
        } else {
             effect->mn = effect->nm;
        }
        effect->type = type;
        effect->enable = zEffect->is_enabled();
    }
    return effect;
}

static MaskMethod getMatteType(int type)
{
    switch (type) {
        case 1: return MaskMethod::Alpha;
        case 2: return MaskMethod::InvAlpha;
        case 3: return MaskMethod::Luma;
        case 4: return MaskMethod::InvLuma;
        default: return MaskMethod::None;
    }
}

static LottieLayer* parseLayer(LottieComposition* comp, LottieLayer* parent, const Layer* zLayer, const std::unordered_map<unsigned long, const Asset*>& assetMap)
{
    auto layer = new LottieLayer;
    static_cast<LottieObject*>(layer)->type = LottieObject::Layer;
    RGB32 color = {255, 255, 255};
    layer->comp = parent;
    
    if (zLayer->name()) {
        layer->name = dupString(zLayer->name());
        layer->id = djb2(zLayer->name()->c_str());
    }
    layer->ix = zLayer->index();
    if (layer->ix == 0 && zLayer->id() != 0) layer->ix = static_cast<int16_t>(zLayer->id());
    layer->pix = zLayer->parent_id();
    
    // LottieLayer: inFrame, outFrame, startTime
    layer->inFrame = zLayer->in_frame();
    layer->outFrame = zLayer->out_frame();
    layer->startFrame = zLayer->start_frame(); 
    layer->timeStretch = zLayer->time_stretch();
    
    // Transform
    if (zLayer->transform()) {
        layer->transform = new LottieTransform;
        parseTransform(comp, layer->transform, zLayer->transform());
    }

    if (zLayer->width() > 0) layer->w = (float)zLayer->width();
    if (zLayer->height() > 0) layer->h = (float)zLayer->height();
    
    // Masks
    if (zLayer->masks()) {
        for (auto zMask : *zLayer->masks()) {
            auto mask = parseMask(comp, zMask);
            if (mask) layer->masks.push(mask);
        }
    }
    
    // Matte
    layer->matteType = getMatteType(zLayer->matte());
    layer->mix = zLayer->matte_parent();
    
    layer->hidden = zLayer->hidden();
    layer->matteSrc = zLayer->matte_src();
    layer->autoOrient = zLayer->auto_orient();
    parseFloat(comp, layer->timeRemap, zLayer->time_remap());

    // Effects
    if (zLayer->effects()) {
        for (auto zEffect : *zLayer->effects()) {
            auto effect = parseEffect(comp, zEffect);
            if (effect) layer->effects.push(effect);
        }
    }

    // Type specific
    switch (zLayer->type()) {
        case Zan::Data::LayerType_Precomp: {
            layer->type = LottieLayer::Precomp;
            if (zLayer->ref_id()) {
                auto refIdHash = djb2(zLayer->ref_id()->c_str());
                layer->rid = refIdHash;

                // Only set dimensions from asset; children are populated via _buildReference
                auto it = assetMap.find(refIdHash);
                if (it != assetMap.end()) {
                    const Asset* asset = it->second;
                    if (layer->w <= 0) layer->w = asset->width();
                    if (layer->h <= 0) layer->h = asset->height();
                }
            }
            break;
        }
        case Zan::Data::LayerType_Solid: {
            layer->type = LottieLayer::Solid;
            // Prefer specific solid dimensions if available, otherwise fallback to layer w/h
            float w = (float)zLayer->solid_width();
            float h = (float)zLayer->solid_height();
            if (w > 0) layer->w = w;
            if (h > 0) layer->h = h;
            color = hexToRGB255(zLayer->solid_color());
            break;
        }
        case Zan::Data::LayerType_Shape: {
            layer->type = LottieLayer::Shape;
            if (zLayer->shapes()) {
                for (auto item : *zLayer->shapes()) {
                    auto shape = parseShape(comp, item);
                    if (shape) layer->children.push(shape);
                }
            }
            break;
        }
        case Zan::Data::LayerType_Null: {
            layer->type = LottieLayer::Null;
            break;
        }
        case Zan::Data::LayerType_Image: {
            bool valid = false;
            if (zLayer->ref_id()) {
                auto refIdHash = djb2(zLayer->ref_id()->c_str());
                layer->rid = refIdHash;

                auto it = assetMap.find(refIdHash);
                if (it != assetMap.end()) {
                    layer->type = LottieLayer::Image;
                    const Asset* asset = it->second;
                    layer->w = (float)asset->width();
                    layer->h = (float)asset->height();
                    
                    auto image = new LottieImage;
                    image->bitmap.width = (float)asset->width();
                    image->bitmap.height = (float)asset->height();
                    image->bitmap.picture = tvg::Picture::gen();
                    
                    if (asset->buffer() && asset->buffer()->size() > 0) {
                        auto data = (const char*)asset->buffer()->Data();
                        auto size = asset->buffer()->size();
                        if (image->bitmap.picture->load(data, size, "", "", false) == tvg::Result::Success) {
                             image->resolved = true;
                        }
                    } else if (asset->path()) {
                        if (asset->folder() && asset->folder()->size() > 0) {
                             std::string p = asset->folder()->str() + asset->path()->str();
                             image->bitmap.path = tvg::duplicate(p.c_str());
                        } else {
                             image->bitmap.path = dupString(asset->path());
                        }
                    }
                    
                    layer->children.push(image);
                    valid = true;
                }
            }
            if (!valid) layer->type = LottieLayer::Null;
            break;
        }
        case Zan::Data::LayerType_Text: {
            layer->type = LottieLayer::Text;
            if (zLayer->text()) {
                auto text = parseText(comp, zLayer->text());
                if (text) layer->children.push(text);
            }
            break;
        }
        default: break;
    }
    
    layer->prepare(&color);
    layer->blendMethod = static_cast<tvg::BlendMethod>(zLayer->blend_mode());
    return layer;
}

bool LottieFlatBufferParser::parse()
{
    auto verifier = flatbuffers::Verifier((const uint8_t*)data, size);
    if (!VerifyMovieBuffer(verifier)) return false;

    auto movie = GetMovie(data);
    
    comp = new LottieComposition;
    comp->w = movie->width();
    comp->h = movie->height();
    comp->frameRate = movie->frame_rate();
    comp->root = new LottieLayer; 
    static_cast<LottieObject*>(comp->root)->type = LottieObject::Layer;
    comp->root->type = LottieLayer::Precomp;
    comp->root->inFrame = movie->in_point();
    comp->root->outFrame = movie->out_point();
    comp->root->w = comp->w;
    comp->root->h = comp->h;
    
    // Index Assets by ID for fast lookup
    std::unordered_map<unsigned long, const Asset*> assetMap;
    if (movie->assets()) {
        for (auto zAsset : *movie->assets()) {
            if (zAsset->id()) {
                assetMap[djb2(zAsset->id()->c_str())] = zAsset;
            }
        }
    }

    // Build comp->assets: for each precomp asset, create a LottieLayer with its children.
    // This mirrors how tvgLottieParser.cpp populates comp->assets so that _buildReference
    // and _buildComposition can correctly process matte/hierarchy relationships inside precomps.
    if (movie->assets()) {
        for (auto zAsset : *movie->assets()) {
            if (!zAsset->id() || !zAsset->layers()) continue;
            auto assetLayer = new LottieLayer;
            static_cast<LottieObject*>(assetLayer)->type = LottieObject::Layer;
            assetLayer->type = LottieLayer::Precomp;
            assetLayer->id = djb2(zAsset->id()->c_str());
            assetLayer->comp = comp->root;
            assetLayer->w = (float)zAsset->width();
            assetLayer->h = (float)zAsset->height();
            for (auto zChild : *zAsset->layers()) {
                auto child = parseLayer(comp, assetLayer, zChild, assetMap);
                assetLayer->children.push(child);
            }
            assetLayer->prepare();
            comp->assets.push(assetLayer);
        }
    }

    // Parse Root Layers
    if (movie->layers()) {
        for (auto zLayer : *movie->layers()) {
            auto layer = parseLayer(comp, comp->root, zLayer, assetMap);
            comp->root->children.push(layer);
        }
    }

    if (movie->fonts()) {
        for (auto zFont : *movie->fonts()) {
            auto font = parseFont(comp, zFont);
            if (font) comp->fonts.push(font);
        }
    }
    
    // Parse Markers if present
    if (movie->markers()) {
        for (auto zMarker : *movie->markers()) {
            auto marker = new LottieMarker;
            if (zMarker->name()) marker->name = dupString(zMarker->name());
            marker->time = zMarker->time();
            marker->duration = zMarker->duration();
            comp->markers.push(marker);
        }
    }

    resolveParents(comp->root);
    comp->root->prepare();
    return true;
}

