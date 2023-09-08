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

#include "tvgStr.h"
#include "tvgCompressor.h"
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


CompositeMethod LottieParser::getMaskMethod(bool inversed)
{
    switch (getString()[0]) {
        case 'a': {
            if (inversed) return CompositeMethod::InvAlphaMask;
            else return CompositeMethod::AddMask;
        }
        case 's': return CompositeMethod::SubtractMask;
        case 'i': return CompositeMethod::IntersectMask;
        case 'f': return CompositeMethod::DifferenceMask;
        default: return CompositeMethod::None;
    }
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
    RGB24 color = {0, 0, 0};

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

    size_t extra = closed ? 3 : 0;
    outPts.reserve(pts.count * 3 + 1 + extra);
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

    //rgb
    while (nextArrayValue()) {
        auto remains = (idx % 4);
        if (remains == 0) {
            color.data[idx / 4].offset = getFloat();
            color.data[idx / 4].a = 255;        //in default
        } else if (remains == 1) {
            color.data[idx / 4].r = lroundf(getFloat() * 255.0f);
        } else if (remains == 2) {
            color.data[idx / 4].g = lroundf(getFloat() * 255.0f);
        } else if (remains == 3) {
            color.data[idx / 4].b = lroundf(getFloat() * 255.0f);
        }
        if ((++idx / 4) == count) break;
    }

    //alpha
    idx = 0;
    while (nextArrayValue()) {
        auto offset = getFloat(); //not used for now.
        if (!mathEqual(offset, color.data[idx].offset)) TVGERR("LOTTIE", "FIXME: Gradient alpha offset is ignored");
        color.data[idx++].a = lroundf(getFloat() * 255.0f);
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
    auto interpolator = false;

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
        } else if (!strcmp(key, "h")) {
            frame.hold = getInt();
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
        if (!strcmp(key, "d")) rect->cw = getInt();
        else if (!strcmp(key, "s")) parseProperty(rect->size);
        else if (!strcmp(key, "p")) parseProperty(rect->position);
        else if (!strcmp(key, "r")) parseProperty(rect->radius);
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
        else if (!strcmp(key, "d")) ellipse->cw = getInt();
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

    if (ddd) {
        transform->rotationEx = new LottieTransform::RotationEx;
        TVGLOG("LOTTIE", "3D transform(ddd) is not totally compatible.");
    }

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
        else if (transform->rotationEx && !strcmp(key, "rx")) parseProperty(transform->rotationEx->x);
        else if (transform->rotationEx && !strcmp(key, "ry")) parseProperty(transform->rotationEx->y);
        else if (transform->rotationEx && !strcmp(key, "rz")) parseProperty(transform->rotation);
        else if (!strcmp(key, "nm")) transform->name = getStringCopy();
        //else if (!strcmp(key, "sk")) //TODO: skew
        //else if (!strcmp(key, "sa")) //TODO: skew axis
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
        else if (!strcmp(key, "fillEnabled")) fill->hidden |= !getBool();
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
        int idx = 0;
        while (auto key = nextObjectKey()) {
            if (!strcmp(key, "n")) {
                auto style = getString();
                if (!strcmp("o", style)) idx = 0;           //offset
                else if (!strcmp("d", style)) idx = 1;      //dash
                else if (!strcmp("g", style)) idx = 2;      //gap
            } else if (!strcmp(key, "v")) {
                parseProperty(stroke->dash(idx));
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
        else if (!strcmp(key, "fillEnabled")) stroke->hidden |= !getBool();
        else if (!strcmp(key, "d")) parseStrokeDash(stroke);
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
        else if (!strcmp(key, "d")) path->cw = getInt();
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
        else if (!strcmp(key, "d")) star->cw = getInt();
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

    context->layer->roundedCorner = true;

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


LottieTrimpath* LottieParser::parseTrimpath()
{
    auto trim = new LottieTrimpath;
    if (!trim) return nullptr;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "nm")) trim->name = getStringCopy();
        else if (!strcmp(key, "s")) parseProperty(trim->start);
        else if (!strcmp(key, "e")) parseProperty(trim->end);
        else if (!strcmp(key, "o")) parseProperty(trim->offset);
        else if (!strcmp(key, "m")) trim->type = static_cast<LottieTrimpath::Type>(getInt());
        else if (!strcmp(key, "hd")) trim->hidden = getBool();
        else skip(key);
    }
    trim->prepare();

    return trim;
}


LottieRepeater* LottieParser::parseRepeater()
{
    auto repeater = new LottieRepeater;
    if (!repeater) return nullptr;

    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "nm")) repeater->name = getStringCopy();
        else if (!strcmp(key, "c")) parseProperty(repeater->copies);
        else if (!strcmp(key, "o")) parseProperty(repeater->offset);
        else if (!strcmp(key, "m")) repeater->inorder = getInt();
        else if (!strcmp(key, "tr"))
        {
            enterObject();
            while (auto key2 = nextObjectKey()) {
                if (!strcmp(key2, "a")) parseProperty(repeater->anchor);
                else if (!strcmp(key2, "p")) parseProperty(repeater->position);
                else if (!strcmp(key2, "r")) parseProperty(repeater->rotation);
                else if (!strcmp(key2, "s")) parseProperty(repeater->scale);
                else if (!strcmp(key2, "so")) parseProperty(repeater->startOpacity);
                else if (!strcmp(key2, "eo")) parseProperty(repeater->endOpacity);
                else skip(key2);
            }
        }
        else if (!strcmp(key, "hd")) repeater->hidden = getBool();
        else skip(key);
    }
    repeater->prepare();

    return repeater;
}


LottieObject* LottieParser::parseObject()
{
    auto type = getString();
    if (!type) return nullptr;

    if (!strcmp(type, "gr")) return parseGroup();
    else if (!strcmp(type, "rc")) return parseRect();
    else if (!strcmp(type, "el")) return parseEllipse();
    else if (!strcmp(type, "tr")) return parseTransform();
    else if (!strcmp(type, "fl")) return parseSolidFill();
    else if (!strcmp(type, "st")) return parseSolidStroke();
    else if (!strcmp(type, "sh")) return parsePath();
    else if (!strcmp(type, "sr")) return parsePolyStar();
    else if (!strcmp(type, "rd")) return parseRoundedCorner();
    else if (!strcmp(type, "gf")) return parseGradientFill();
    else if (!strcmp(type, "gs")) return parseGradientStroke();
    else if (!strcmp(type, "tm")) return parseTrimpath();
    else if (!strcmp(type, "rp")) return parseRepeater();
    else if (!strcmp(type, "mm")) TVGERR("LOTTIE", "MergePath(mm) is not supported yet");
    else if (!strcmp(type, "pb")) TVGERR("LOTTIE", "Puker/Bloat(pb) is not supported yet");
    else if (!strcmp(type, "tw")) TVGERR("LOTTIE", "Twist(tw) is not supported yet");
    else if (!strcmp(type, "op")) TVGERR("LOTTIE", "Offset Path(op) is not supported yet");
    else if (!strcmp(type, "zz")) TVGERR("LOTTIE", "Zig Zag(zz) is not supported yet");
    return nullptr;
}


void LottieParser::parseObject(LottieGroup* parent)
{
    enterObject();
    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "ty")) {
            if (auto child = parseObject()) {
                if (child->hidden) delete(child);
                else parent->children.push(child);
            }
        } else skip(key);
    }
}


LottieImage* LottieParser::parseImage(const char* key)
{
    //Used for Image Asset
    const char* data = nullptr;
    const char* subPath = nullptr;
    auto embedded = false;

    do {
        if (!strcmp(key, "u")) {
            subPath = getString();
        } else if (!strcmp(key, "p")) {
            data = getString();
        } else if (!strcmp(key, "e")) {
            embedded = getInt();
        } else skip(key);
    } while ((key = nextObjectKey()));

    if (!data) return nullptr;

    auto image = new LottieImage;
    if (!image) return nullptr;

    //embeded image resource. should start with "data:"
    //header look like "data:image/png;base64," so need to skip till ','.
    if (embedded && !strncmp(data, "data:", 5)) {
        //figure out the mimetype
        auto mimeType = data + 11;
        auto needle = strstr(mimeType, ";");
        image->mimeType = strDuplicate(mimeType, needle - mimeType);

        //b64 data
        auto b64Data = strstr(data, ",") + 1;
        size_t length = strlen(data) - (b64Data - data);
        image->size = b64Decode(b64Data, length, &image->b64Data);
    //external image resource
    } else {
        auto len = strlen(dirName) + strlen(subPath) + strlen(data) + 1;
        image->path = static_cast<char*>(malloc(len));
        snprintf(image->path, len, "%s%s%s", dirName, subPath, data);
    }

    image->prepare();

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
        } else if (!strcmp(key, "w") || !strcmp(key, "h") || !strcmp(key, "nm") || !strcmp(key, "fr")) {
            skip(key);
        //Image asset
        } else {
            obj = parseImage(key);
            break;
        }
    }
    if (obj) obj->name = id;
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


void LottieParser::parseTimeRemap(LottieLayer* layer)
{
    parseProperty(layer->timeRemap);
}


void LottieParser::parseShapes(LottieLayer* layer)
{
    enterArray();
    while (nextArrayValue()) {
        parseObject(layer);
    }
}


void LottieParser::getLayerSize(uint32_t& val)
{
    if (val == 0) {
        val = getInt();
    } else {
        //layer might have both w(width) & sw(solid color width)
        //override one if the a new size is smaller.
        uint32_t w = getInt();
        if (w < val) val = w;
    }
}

LottieMask* LottieParser::parseMask()
{
    auto mask = new LottieMask;
    if (!mask) return nullptr;

    enterObject();
    while (auto key = nextObjectKey()) {
        if (!strcmp(key, "inv")) mask->inverse = getBool();
        else if (!strcmp(key, "mode")) mask->method = getMaskMethod(mask->inverse);
        else if (!strcmp(key, "pt")) getPathSet(mask->pathset);
        else if (!strcmp(key, "o")) parseProperty(mask->opacity);
        else skip(key);
    }

    return mask;
}


void LottieParser::parseMasks(LottieLayer* layer)
{
    enterArray();
    while (nextArrayValue()) {
        layer->masks.push(parseMask());
    }
}


LottieLayer* LottieParser::parseLayer()
{
    auto layer = new LottieLayer;
    if (!layer) return nullptr;

    layer->comp = comp;
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
        else if (!strcmp(key, "tm")) parseTimeRemap(layer);
        else if (!strcmp(key, "w") || !strcmp(key, "sw")) getLayerSize(layer->w);
        else if (!strcmp(key, "h") || !strcmp(key, "sh")) getLayerSize(layer->h);
        else if (!strcmp(key, "sc")) layer->color = getColor(getString());
        else if (!strcmp(key, "tt")) layer->matte.type = getMatteType();
        else if (!strcmp(key, "masksProperties")) parseMasks(layer);
        else if (!strcmp(key, "hd")) layer->hidden = getBool();
        else if (!strcmp(key, "refId")) layer->refId = getStringCopy();
        else if (!strcmp(key, "td")) layer->matteSrc = getInt();      //used for matte layer
        else if (!strcmp(key, "ef"))
        {
            TVGERR("LOTTIE", "layer effect(ef) is not supported!");
            skip(key);
        }
        else skip(key);
    }

    //Not a valid layer
    if (!layer->transform) {
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
    root->comp = comp;

    enterArray();
    while (nextArrayValue()) {
        if (auto layer = parseLayer()) {
            if (layer->matte.type == CompositeMethod::None) {
                root->children.push(layer);
            } else {
                //matte source must be located in the right previous.
                layer->matte.target = static_cast<LottieLayer*>(root->children.last());
                layer->statical &= layer->matte.target->statical;
                root->children.last() = layer;
            }
            root->statical &= layer->statical;
        }
    }
    root->prepare();
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

    comp->root->inFrame = comp->startFrame;
    comp->root->outFrame = comp->endFrame;
    return true;
}