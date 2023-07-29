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

#include "tvgLottieModel.h"
#include "tvgLottieParser.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static char* _int2str(int num)
{
    char str[20];
    snprintf(str, 20, "%d", num);
    return strdup(str);
}


BlendMethod LottieParser::getBlendMethod()
{
    switch (getInt()) {
        case 1: return BlendMethod::Multiply;
        case 2: return BlendMethod::Screen;
        case 3: return BlendMethod::Overlay;
        default: return BlendMethod::Normal;
    }
}


RGB24 LottieParser::getColor(const char *str)
{
    RGB24 color;

    if (!str) return color;

    auto len = strlen(str);

    // some resource has empty color string, return a default color for those cases.
    if (len != 7 || str[0] != '#') return color;

    char tmp[3] = {'\0', '\0', '\0'};
    tmp[0] = str[1];
    tmp[1] = str[2];
    color.rgb[0] = uint8_t(strtol(tmp, nullptr, 16));

    tmp[0] = str[3];
    tmp[1] = str[4];
    color.rgb[1] = uint8_t(strtol(tmp, nullptr, 16));

    tmp[0] = str[5];
    tmp[1] = str[6];
    color.rgb[2] = uint8_t(strtol(tmp, nullptr, 16));

    return color;
}


FillRule LottieParser::getFillRule()
{
    switch (getInt()) {
        case 2: return FillRule::EvenOdd;
        default: return FillRule::Winding;
    }
}


CompositeMethod LottieParser::getMatteType()
{
    switch (getInt()) {
        case 1: return CompositeMethod::AlphaMask;
        case 2: return CompositeMethod::InvAlphaMask;
        case 3: return CompositeMethod::LumaMask;
        case 4: return CompositeMethod::InvLumaMask;
        default: return CompositeMethod::None;
    }
}


StrokeCap LottieParser::getStrokeCap()
{
    switch (getInt()) {
        case 1: return StrokeCap::Butt;
        case 2: return StrokeCap::Round;
        default: return StrokeCap::Square;
    }
}


StrokeJoin LottieParser::getStrokeJoin()
{
    switch (getInt()) {
        case 1: return StrokeJoin::Miter;
        case 2: return StrokeJoin::Round;
        default: return StrokeJoin::Bevel;
    }
}


void LottieParser::getValue(PathSet& path)
{
    Array<Point> outs, ins, pts;
    bool closed = false;

    /* The shape object could be wrapped by a array
       if its part of the keyframe object */
    auto arrayWrapper = (peekType() == kArrayType) ? true : false;
    if (arrayWrapper) enterArray();

    enterObject();
    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "i")) {
            getValue(ins);
        } else if (!strcmp(key, "o")) {
            getValue(outs);
        } else if (!strcmp(key, "v")) {
            getValue(pts);
        } else if (!strcmp(key, "c")) {
            closed = getBool();
        } else {
            Error();
            skip(key);
        }
    }

    //exit properly from the array
    if (arrayWrapper) nextArrayValue();

    //valid path data?
    if (ins.empty() || outs.empty() || pts.empty()) return;
    if (ins.count != outs.count || outs.count != pts.count) return;

    //convert path
    auto out = outs.data;
    auto in = ins.data;
    auto pt = pts.data;

    //Store manipulated results
    Array<Point> outPts;
    Array<PathCommand> outCmds;

    //Resuse the buffers
    outPts.data = path.pts;
    outPts.reserved = path.ptsCnt;
    outCmds.data = path.cmds;
    outCmds.reserved = path.cmdsCnt;

    outPts.reserve(pts.count * 3 + 1);
    outCmds.reserve(pts.count + 2);

    outCmds.push(PathCommand::MoveTo);
    outPts.push(*pt);

    for (++pt, ++out, ++in; pt < pts.end(); ++pt, ++out, ++in) {
        outCmds.push(PathCommand::CubicTo);
        outPts.push(*(pt - 1) + *(out - 1));
        outPts.push(*pt + *in);
        outPts.push(*pt);
    }

    if (closed) {
        outPts.push(pts.last() + outs.last());
        outPts.push(pts.first() + ins.first());
        outPts.push(pts.first());
        outCmds.push(PathCommand::CubicTo);
        outCmds.push(PathCommand::Close);
    }

    path.pts = outPts.data;
    path.cmds = outCmds.data;
    path.ptsCnt = outPts.count;
    path.cmdsCnt = outCmds.count;

    outPts.data = nullptr;
    outCmds.data = nullptr;
}


void LottieParser::getValue(ColorStop& color)
{
    if (peekType() == kArrayType) enterArray();

    int idx = 0;
    auto count = context->gradient->colorStops.count;
    if (!color.data) color.data = static_cast<Fill::ColorStop*>(malloc(sizeof(Fill::ColorStop) * count));

    while (nextArrayValue()) {
        auto remains = (idx % 4);
        if (remains == 0) {
            color.data[idx / 4].offset = getFloat();
            color.data[idx / 4].a = 255; //Not used.
        } else if (remains == 1) {
            color.data[idx / 4].r = lroundf(getFloat() * 255.0f);
        } else if (remains == 2) {
            color.data[idx / 4].g = lroundf(getFloat() * 255.0f);
        } else if (remains == 3) {
            color.data[idx / 4].b = lroundf(getFloat() * 255.0f);
        }
        ++idx;
    }
}


void LottieParser::getValue(Array<Point>& pts)
{
    enterArray();
    while (nextArrayValue()) {
        enterArray();
        Point pt;
        getValue(pt);
        pts.push(pt);
    }
}


void LottieParser::getValue(uint8_t& val)
{
    if (peekType() == kArrayType) {
        enterArray();
        if (nextArrayValue()) val = (uint8_t)(getFloat() * 2.55f);
        //discard rest
        while (nextArrayValue()) getFloat();
    } else if (peekType() == kNumberType) {
        val = (uint8_t)(getFloat() * 2.55f);
    } else {
        Error();
    }
}


void LottieParser::getValue(float& val)
{
    if (peekType() == kArrayType) {
        enterArray();
        if (nextArrayValue()) val = getFloat();
        //discard rest
        while (nextArrayValue()) getFloat();
    } else if (peekType() == kNumberType) {
        val = getFloat();
    } else {
        Error();
    }
}


void LottieParser::getValue(Point& pt)
{
    int i = 0;
    auto ptr = (float*)(&pt);

    if (peekType() == kArrayType) enterArray();

    while (nextArrayValue()) {
        auto val = getFloat();
        if (i < 2) ptr[i++] = val;
    }
}


void LottieParser::getValue(RGB24& color)
{
    int i = 0;

    if (peekType() == kArrayType) enterArray();

    while (nextArrayValue()) {
        auto val = getFloat();
        if (i < 3) color.rgb[i++] = int32_t(lroundf(val * 255.0f));
    }

    //TODO: color filter?
}


void LottieParser::getInperpolatorPoint(Point& pt)
{
    enterObject();
    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "x")) getValue(pt.x);
        else if (!strcmp(key, "y")) getValue(pt.y);
    }
}

template<typename T>
bool LottieParser::parseTangent(const char *key, LottieVectorFrame<T>& value)
{
    if (!strcmp(key, "ti")) {
        value.hasTangent = true;
        getValue(value.inTangent);
    } else if (!strcmp(key, "to")) {
        value.hasTangent = true;
        getValue(value.outTangent);
    } else return false;

    return true;
}


template<typename T>
bool LottieParser::parseTangent(const char *key, LottieScalarFrame<T>& value)
{
    return false;
}


LottieInterpolator* LottieParser::getInterpolator(const char* key, Point& in, Point& out)
{
    char buf[20];

    if (!key) {
        snprintf(buf, sizeof(buf), "%.2f_%.2f_%.2f_%.2f", in.x, in.y, out.x, out.y);
        key = buf;
    }

    LottieInterpolator* interpolator = nullptr;

    //get a cached interpolator if it has any.
    for (auto i = comp->interpolators.data; i < comp->interpolators.end(); ++i) {
        if (!strncmp((*i)->key, key, sizeof(buf))) interpolator = *i;
    }

    //new interpolator
    if (!interpolator) {
        interpolator = static_cast<LottieInterpolator*>(malloc(sizeof(LottieInterpolator)));
        interpolator->set(key, in, out);
        comp->interpolators.push(interpolator);
    }

    return interpolator;
}


template<typename T>
void LottieParser::parseKeyFrame(T& prop)
{
    Point inTangent, outTangent;
    const char* interpolatorKey = nullptr;
    auto& frame = prop.newFrame();
    bool interpolator = false;

    enterObject();

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "i")) {
            interpolator = true;
            getInperpolatorPoint(inTangent);
        } else if (!strcmp(key, "o")) {
            getInperpolatorPoint(outTangent);
        } else if (!strcmp(key, "n")) {
            if (peekType() == kStringType) {
                interpolatorKey = getString();
            } else {
                enterArray();
                while (nextArrayValue()) {
                    if (!interpolatorKey) interpolatorKey = getString();
                    else skip(nullptr);
                }
            }
        } else if (!strcmp(key, "t")) {
            frame.no = lroundf(getFloat());
        } else if (!strcmp(key, "s")) {
            getValue(frame.value);
        } else if (!strcmp(key, "e")) {
            //current end frame and the next start frame is duplicated,
            //We propagate the end value to the next frame to avoid having duplicated values.
            auto& frame2 = prop.nextFrame();
            getValue(frame2.value);
        } else if (parseTangent(key, frame)) {
            continue;
        } else skip(key);
    }

    if (interpolator) {
        frame.interpolator = getInterpolator(interpolatorKey, inTangent, outTangent);
    }
}

template<typename T>
void LottieParser::parsePropertyInternal(T& prop)
{
    //single value property
    if (peekType() == kNumberType) {
        getValue(prop.value);
    //multi value property
    } else {
        enterArray();
        while (nextArrayValue()) {
            //keyframes value
            if (peekType() == kObjectType) {
                parseKeyFrame(prop);
            //multi value property with no keyframes
            } else {
                getValue(prop.value);
                break;
            }
        }
        prop.prepare();
    }
}


template<typename T>
void LottieParser::parseProperty(T& prop)
{
    enterObject();
    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "k")) parsePropertyInternal(prop);
        else skip(key);
    }
}


LottieRect* LottieParser::parseRect()
{
    auto rect = new LottieRect;
    if (!rect) return nullptr;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "d")) rect->direction = getInt();
        else if (!strcmp(key, "s")) parseProperty(rect->size);
        else if (!strcmp(key, "p")) parseProperty(rect->position);
        else if (!strcmp(key, "r")) parseProperty(rect->round);
        else if (!strcmp(key, "nm")) rect->name = getStringCopy();
        else if (!strcmp(key, "hd")) rect->hidden = getBool();
        else skip(key);
    }
    rect->prepare();
    return rect;
}


LottieEllipse* LottieParser::parseEllipse()
{
    auto ellipse = new LottieEllipse;
    if (!ellipse) return nullptr;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "nm")) ellipse->name = getStringCopy();
        else if (!strcmp(key, "p")) parseProperty(ellipse->position);
        else if (!strcmp(key, "s")) parseProperty(ellipse->size);
        else if (!strcmp(key, "d")) ellipse->direction = getInt();
        else if (!strcmp(key, "hd")) ellipse->hidden = getBool();
        else skip(key);
    }
    ellipse->prepare();
    return ellipse;
}


LottieTransform* LottieParser::parseTransform(bool ddd)
{
    auto transform = new LottieTransform;
    if (!transform) return nullptr;

    if (ddd) TVGLOG("LOTTIE", "3d transform(ddd) is not supported");

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "p"))
        {
            enterObject();
            while (auto key = nextObjectKey()) {
                if (!strcmp(key, "k")) parsePropertyInternal(transform->position);
                else if (!strcmp(key, "s")) {
                    if (getBool()) transform->coords = new LottieTransform::SeparateCoord;
                //check separateCoord to figure out whether "x(expression)" / "x(coord)"
                } else if (transform->coords && !strcmp(key, "x")) {
                    parseProperty(transform->coords->x);
                } else if (transform->coords && !strcmp(key, "y")) {
                    parseProperty(transform->coords->y);
                } else skip(key);
            }
        }
        else if (!strcmp(key, "a")) parseProperty(transform->anchor);
        else if (!strcmp(key, "s")) parseProperty(transform->scale);
        else if (!strcmp(key, "r")) parseProperty(transform->rotation);
        else if (!strcmp(key, "o")) parseProperty(transform->opacity);
        //else if (!strcmp(key, "sk")) //skew
        //else if (!strcmp(key, "sa")) //skew axis
        //else if (!strcmp(key, "rx")  //3d rotation
        //else if (!strcmp(key, "ry")  //3d rotation
        else if (!strcmp(key, "rz")) parseProperty(transform->rotation);
        else if (!strcmp(key, "nm")) transform->name = getStringCopy();
        else skip(key);
    }
    transform->prepare();
    return transform;
}


LottieSolidFill* LottieParser::parseSolidFill()
{
    auto fill = new LottieSolidFill;
    if (!fill) return nullptr;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "nm")) fill->name = getStringCopy();
        else if (!strcmp(key, "c")) parseProperty(fill->color);
        else if (!strcmp(key, "o")) parseProperty(fill->opacity);
        else if (!strcmp(key, "fillEnabled")) fill->disabled = !getBool();
        else if (!strcmp(key, "r")) fill->rule = getFillRule();
        else if (!strcmp(key, "hd")) fill->hidden = getBool();
        else skip(key);
    }
    fill->prepare();
    return fill;
}


void LottieParser::parseStrokeDash(LottieStroke* stroke)
{
    enterArray();
    while (nextArrayValue()) {
        enterObject();
        while (auto key = nextObjectKey()) {
            if (!strcmp(key, "v")) {
            } else skip(key);
        }
    }
}


LottieSolidStroke* LottieParser::parseSolidStroke()
{
    auto stroke = new LottieSolidStroke;
    if (!stroke) return nullptr;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "c")) parseProperty(stroke->color);
        else if (!strcmp(key, "o")) parseProperty(stroke->opacity);
        else if (!strcmp(key, "w")) parseProperty(stroke->width);
        else if (!strcmp(key, "lc")) stroke->cap = getStrokeCap();
        else if (!strcmp(key, "lj")) stroke->join = getStrokeJoin();
        else if (!strcmp(key, "ml")) stroke->miterLimit = getFloat();
        else if (!strcmp(key, "nm")) stroke->name = getStringCopy();
        else if (!strcmp(key, "hd")) stroke->hidden = getBool();
        else if (!strcmp(key, "fillEnabled")) stroke->disabled = !getBool();
        else if (!strcmp(key, "d"))
        {
            TVGLOG("LOTTIE", "StrokeDash(d) is not supported");
            skip(key);
            //parseStrokeDash(stroke);
        }
        else skip(key);
    }
    stroke->prepare();
    return stroke;
}


 void LottieParser::getPathSet(LottiePathSet& path)
{
    enterObject();
    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "k")) {
            if (peekType() == kArrayType) {
                enterArray();
                while (nextArrayValue()) parseKeyFrame(path);
            } else {
                if (path.frames) {
                    Error();
                    return;
                }
                getValue(path.value);
            }
        } else skip(key);
    }
}


LottiePath* LottieParser::parsePath()
{
    auto path = new LottiePath;
    if (!path) return nullptr;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "nm")) path->name = getStringCopy();
        else if (!strcmp(key, "ks")) getPathSet(path->pathset);
        else if (!strcmp(key, "d")) path->direction = getInt();
        else if (!strcmp(key, "hd")) path->hidden = getBool();
        else skip(key);
    }
    path->prepare();
    return path;
}


LottiePolyStar* LottieParser::parsePolyStar()
{
    auto star = new LottiePolyStar;
    if (!star) return nullptr;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "nm")) star->name = getStringCopy();
        else if (!strcmp(key, "p")) parseProperty(star->position);
        else if (!strcmp(key, "pt")) parseProperty(star->ptsCnt);
        else if (!strcmp(key, "ir")) parseProperty(star->innerRadius);
        else if (!strcmp(key, "is")) parseProperty(star->innerRoundness);
        else if (!strcmp(key, "or")) parseProperty(star->outerRadius);
        else if (!strcmp(key, "os")) parseProperty(star->outerRoundness);
        else if (!strcmp(key, "r")) parseProperty(star->rotation);
        else if (!strcmp(key, "sy")) star->type = (LottiePolyStar::Type) getInt();
        else if (!strcmp(key, "d")) star->direction = getInt();
        else if (!strcmp(key, "hd")) star->hidden = getBool();
        else skip(key);
    }
    star->prepare();
    return star;
}


LottieRoundedCorner* LottieParser::parseRoundedCorner()
{
    auto corner = new LottieRoundedCorner;
    if (!corner) return nullptr;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "nm")) corner->name = getStringCopy();
        else if (!strcmp(key, "r")) parseProperty(corner->radius);
        else if (!strcmp(key, "hd")) corner->hidden = getBool();
        else skip(key);
    }
    corner->prepare();
    return corner;
}


void LottieParser::parseGradient(LottieGradient* gradient, const char* key)
{
    if (!strcmp(key, "t")) gradient->id = getInt();
    else if (!strcmp(key, "o")) parseProperty(gradient->opacity);
    else if (!strcmp(key, "g"))
    {
        enterObject();
        while (auto key = nextObjectKey()) {
            if (!strcmp(key, "p")) gradient->colorStops.count = getInt();
            else if (!strcmp(key, "k")) parseProperty(gradient->colorStops);
            else skip(key);
        }
    }
    else if (!strcmp(key, "s")) parseProperty(gradient->start);
    else if (!strcmp(key, "e")) parseProperty(gradient->end);
    else if (!strcmp(key, "h")) parseProperty(gradient->height);
    else if (!strcmp(key, "a")) parseProperty(gradient->angle);
    else skip(key);
}


LottieGradientFill* LottieParser::parseGradientFill()
{
    auto fill = new LottieGradientFill;
    if (!fill) return nullptr;

    context->gradient = fill;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "nm")) fill->name = getStringCopy();
        else if (!strcmp(key, "r")) fill->rule = getFillRule();
        else if (!strcmp(key, "hd")) fill->hidden = getBool();
        else parseGradient(fill, key);
    }

    fill->prepare();

    return fill;
}


LottieGradientStroke* LottieParser::parseGradientStroke()
{
    auto stroke = new LottieGradientStroke;
    if (!stroke) return nullptr;

    context->gradient = stroke;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "nm")) stroke->name = getStringCopy();
        else if (!strcmp(key, "lc")) stroke->cap = getStrokeCap();
        else if (!strcmp(key, "lj")) stroke->join = getStrokeJoin();
        else if (!strcmp(key, "ml")) stroke->miterLimit = getFloat();
        else if (!strcmp(key, "hd")) stroke->hidden = getBool();
        else if (!strcmp(key, "w")) parseProperty(stroke->width);
        else if (!strcmp(key, "d")) parseStrokeDash(stroke);
        else parseGradient(stroke, key);
    }
    stroke->prepare();

    return stroke;
}


LottieObject* LottieParser::parseObject()
{
    auto type = getString();
    if (!type) return nullptr;

    if (!strcmp(type, "gr")) {
        return parseGroup();
    } else if (!strcmp(type, "rc")) {
        return parseRect();
    } else if (!strcmp(type, "el")) {
        return parseEllipse();
    } else if (!strcmp(type, "tr")) {
        return parseTransform();
    } else if (!strcmp(type, "fl")) {
        return parseSolidFill();
    } else if (!strcmp(type, "st")) {
        return parseSolidStroke();
    } else if (!strcmp(type, "sh")) {
        return parsePath();
    } else if (!strcmp(type, "sr")) {
        TVGLOG("LOTTIE", "Polystar(sr) is not supported");
        return parsePolyStar();
    } else if (!strcmp(type, "rd")) {
        TVGLOG("LOTTIE", "RoundedCorner(rd) is not supported");
        return parseRoundedCorner();
    } else if (!strcmp(type, "gf")) {
        return parseGradientFill();
    } else if (!strcmp(type, "gs")) {
        return parseGradientStroke();
    } else if (!strcmp(type, "tm")) {
        TVGLOG("LOTTIE", "Trimpath(tm) is not supported");
    } else if (!strcmp(type, "rp")) {
        TVGLOG("LOTTIE", "Repeater(rp) is not supported yet");
    } else if (!strcmp(type, "mm")) {
        TVGLOG("LOTTIE", "MergePath(mm) is not supported yet");
    } else {
        TVGERR("LOTTIE", "Unkown object type(%s) is given", type);
    }
    return nullptr;
}


void LottieParser::parseObject(LottieGroup* parent)
{
    enterObject();
    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "ty")) {
            auto child = parseObject();
            if (child && !child->hidden) {
                if (child->type == LottieObject::RoundedCorner) {
                    //TODO:
                }
                parent->children.push(child);
            } else delete(child);
        } else skip(key);
    }
}


LottieImage* LottieParser::parseImage(const char* key)
{
    auto image = new LottieImage;
    if (!image) return nullptr;

    //Used for Image Asset
    const char* fileName = nullptr;
    const char* relativePath = nullptr;
    auto embedded = false;

    do {
        if (!strcmp(key, "w"))  {
            image->surface.w = getInt();
        } else if (!strcmp(key, "h")) {
            image->surface.h = getInt();
        } else if (!strcmp(key, "u")) {
            relativePath = getString();
        } else if (!strcmp(key, "e")) {
            embedded = getInt();
        } else if (!strcmp(key, "p")) {
            fileName = getString();
        } else skip(key);
    } while ((key = nextObjectKey()));

    image->prepare();

    // embeded resource should start with "data:"
    if (embedded && !strncmp(fileName, "data:", 5)) {
        //TODO:
    } else {
        //TODO:
    }

    TVGLOG("LOTTIE", "Image is not supported: (dirPath + %s + %s)", relativePath, fileName);
    return image;
}


LottieObject* LottieParser::parseAsset()
{
    enterObject();

    LottieObject* obj = nullptr;
    char *id = nullptr;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "id")) {
            if (peekType() == kStringType) {
                id = getStringCopy();
            } else {
                id = _int2str(getInt());
            }
        //Precomposition asset
        } else if (!strcmp(key, "layers")) {
            obj = parseLayers();
        //Image asset
        } else {
            obj = parseImage(key);
            break;
        }
    }
    obj->name = id;
    return obj;
}


void LottieParser::parseAssets()
{
    enterArray();
    while (nextArrayValue()) {
        comp->assets.push(parseAsset());
    }
}


LottieObject* LottieParser::parseGroup()
{
    auto group = new LottieGroup;
    if (!group) return nullptr;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "nm")) {
            group->name = getStringCopy();
        } else if (!strcmp(key, "it")) {
            enterArray();
            while (nextArrayValue()) parseObject(group);
        } else skip(key);
    }
    if (group->children.empty()) {
        delete(group);
        return nullptr;
    }
    group->prepare();

    return group;
}


void LottieParser::parseShapes(LottieLayer* layer)
{
    enterArray();
    while (nextArrayValue()) {
        parseObject(layer);
    }
}


LottieLayer* LottieParser::parseLayer()
{
    auto layer = new LottieLayer;
    if (!layer) return nullptr;

    context->layer = layer;

    auto ddd = false;

    enterObject();

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "ddd")) ddd = getInt();  //3d layer
        else if (!strcmp(key, "ind")) layer->id = getInt();
        else if (!strcmp(key, "ty")) layer->type = (LottieLayer::Type) getInt();
        else if (!strcmp(key, "nm")) layer->name = getStringCopy();
        else if (!strcmp(key, "sr")) layer->timeStretch = getFloat();
        else if (!strcmp(key, "ks"))
        {
            enterObject();
            layer->transform = parseTransform(ddd);
        }
        else if (!strcmp(key, "ao")) layer->autoOrient = getInt();
        else if (!strcmp(key, "shapes")) parseShapes(layer);
        else if (!strcmp(key, "ip")) layer->inFrame = lroundf(getFloat());
        else if (!strcmp(key, "op")) layer->outFrame = lroundf(getFloat());
        else if (!strcmp(key, "st")) layer->startFrame = lroundf(getFloat());
        else if (!strcmp(key, "bm")) layer->blendMethod = getBlendMethod();
        else if (!strcmp(key, "parent")) layer->pid = getInt();
        else if (!strcmp(key, "tm"))
        {
            TVGLOG("LOTTIE", "Time Remap(tm) is not supported");
            skip(key);
        }
        else if (!strcmp(key, "w")) layer->w = getInt();
        else if (!strcmp(key, "h")) layer->h = getInt();
        else if (!strcmp(key, "sw")) layer->w = getInt();
        else if (!strcmp(key, "sh")) layer->h = getInt();
        else if (!strcmp(key, "sc")) layer->color = getColor(getString());
        else if (!strcmp(key, "tt")) layer->matteType = getMatteType();
        else if (!strcmp(key, "hasMask")) layer->mask = getBool();
        else if (!strcmp(key, "masksProperties"))
        {
            TVGLOG("LOTTIE", "Masking(maskProperties) is not supported");
            skip(key);
        }
        else if (!strcmp(key, "hd")) layer->hidden = getBool();
        else if (!strcmp(key, "refId")) layer->refId = getStringCopy();
        else skip(key);
    }

    //Not a valid layer
    if (!layer->transform) {
        TVGERR("LOTTIE", "Invalid Layer data, id(%d), transform(%p)", layer->id, layer->transform);
        delete(layer);
        return nullptr;
    }

    layer->prepare();

    return layer;
}


LottieLayer* LottieParser::parseLayers()
{
    auto root = new LottieLayer;
    if (!root) return nullptr;

    root->type = LottieLayer::Precomp;

    enterArray();
    while (nextArrayValue()) {
        if (auto layer = parseLayer()) {
            root->statical &= layer->statical;
            root->children.push(layer);
        }
    }
    return root;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool LottieParser::parse()
{
    //verify json.
    if (!parseNext()) return false;

    enterObject();

    if (comp) delete(comp);
    comp = new LottieComposition;
    if (!comp) return false;

    //assign parsing context
    LottieParser::Context context;
    this->context = &context;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "v")) comp->version = getStringCopy();
        else if (!strcmp(key, "fr")) comp->frameRate = getFloat();
        else if (!strcmp(key, "ip")) comp->startFrame = lroundf(getFloat());
        else if (!strcmp(key, "op")) comp->endFrame = lroundf(getFloat());
        else if (!strcmp(key, "w")) comp->w = getInt();
        else if (!strcmp(key, "h")) comp->h = getInt();
        else if (!strcmp(key, "nm")) comp->name = getStringCopy();
        else if (!strcmp(key, "assets")) parseAssets();
        else if (!strcmp(key, "layers")) comp->root = parseLayers();
        else skip(key);
    }

    if (Invalid() || !comp->root) return false;

    for (auto c = comp->root->children.data; c < comp->root->children.end(); ++c) {
        auto child = static_cast<LottieLayer*>(*c);
        //Organize the parent-chlid layers.
        if (child->pid != -1) {
            for (auto p = comp->root->children.data; p < comp->root->children.end(); ++p) {
                if (c == p) continue;
                auto parent = static_cast<LottieLayer*>(*p);
                if (child->pid == parent->id) {
                    child->parent = parent;
                    break;
                }
            }
        }
        //Resolve Assets
        if (child->refId) {
            for (auto asset = comp->assets.data; asset < comp->assets.end(); ++asset) {
                if (strcmp(child->refId, (*asset)->name)) continue;
                if (child->type == LottieLayer::Precomp) {
                    child->children = static_cast<LottieLayer*>(*asset)->children;
                    child->statical &= (*asset)->statical;
                } else if (child->type == LottieLayer::Image) {
                    //TODO:
                }
            }
        }
    }
    comp->root->inFrame = comp->startFrame;
    comp->root->outFrame = comp->endFrame;
    return true;
}