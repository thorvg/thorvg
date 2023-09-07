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

#include "tvgCommon.h"
#include "tvgPaint.h"
#include "tvgShape.h"
#include "tvgLottieModel.h"
#include "tvgLottieBuilder.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct RenderContext
{
    Shape* propagator = nullptr;
    Shape* merging = nullptr;                //merging shapes if possible (if shapes have same properties)
    float roundness = 0.0f;

    struct {
        int cnt;
        float offset;
        Point position;
        Point anchor;
        Point scale;
        float rotation;
        uint8_t startOpacity;
        uint8_t endOpacity;
        bool interpOpacity;
        bool inorder;
        bool valid = false;
    } repeater;

    RenderContext()
    {
        propagator = Shape::gen().release();
    }

    ~RenderContext()
    {
        delete(propagator);
    }

    RenderContext(const RenderContext& rhs)
    {
        propagator = rhs.propagator ? static_cast<Shape*>(rhs.propagator->duplicate()) : Shape::gen().release();
        repeater = rhs.repeater;
        roundness = rhs.roundness;
    }
};


static void _updateChildren(LottieGroup* parent, int32_t frameNo, RenderContext& ctx);
static void _updateLayer(LottieLayer* root, LottieLayer* layer, int32_t frameNo);
static bool _buildPrecomp(LottieComposition* comp, LottieGroup* parent);


static void _rotateY(Matrix* m, float degree)
{
    if (degree == 0.0f) return;
    auto radian = degree / 180.0f * M_PI;
    m->e11 = cosf(radian);
    m->e31 = -sinf(radian);
}


static void _rotateX(Matrix* m, float degree)
{
    if (degree == 0.0f) return;
    auto radian = degree / 180.0f * M_PI;
    m->e22 = cosf(radian);
    m->e32 = -sinf(radian);
}


static bool _updateTransform(LottieTransform* transform, int32_t frameNo, bool autoOrient, Matrix& matrix, uint8_t& opacity)
{
    mathIdentity(&matrix);

    if (!transform) {
        opacity = 255;
        return false;
    }

    if (transform->coords) {
        mathTranslate(&matrix, transform->coords->x(frameNo), transform->coords->y(frameNo));
    } else {
        auto position = transform->position(frameNo);
        mathTranslate(&matrix, position.x, position.y);
    }

    auto angle = 0.0f;
    if (autoOrient) angle = transform->position.angle(frameNo);
    mathRotate(&matrix, transform->rotation(frameNo) + angle);

    if (transform->rotationEx) {
        _rotateY(&matrix, transform->rotationEx->y(frameNo));
        _rotateX(&matrix, transform->rotationEx->x(frameNo));
    }

    auto scale = transform->scale(frameNo);
    mathScaleR(&matrix, scale.x * 0.01f, scale.y * 0.01f);

    //Lottie specific anchor transform.
    auto anchor = transform->anchor(frameNo);
    mathTranslateR(&matrix, -anchor.x, -anchor.y);

    opacity = transform->opacity(frameNo);

    return true;
}


static void _updateTransform(LottieLayer* layer, int32_t frameNo)
{
    if (!layer || layer->cache.frameNo == frameNo) return;

    auto transform = layer->transform;
    auto parent = layer->parent;

    if (parent) _updateTransform(parent, parent->remap(frameNo));

    auto& matrix = layer->cache.matrix;

    _updateTransform(transform, frameNo, layer->autoOrient, matrix, layer->cache.opacity);

    if (parent) {
        if (!mathIdentity((const Matrix*) &parent->cache.matrix)) {
            if (mathIdentity((const Matrix*) &matrix)) layer->cache.matrix = parent->cache.matrix;
            else layer->cache.matrix = mathMultiply(&parent->cache.matrix, &matrix);
        }
    }
    layer->cache.frameNo = frameNo;
}


static void _updateTransform(LottieTransform* transform, int32_t frameNo, RenderContext& ctx)
{
    if (!transform) return;

    ctx.merging = nullptr;

    Matrix matrix;
    uint8_t opacity;
    if (!_updateTransform(transform, frameNo, false, matrix, opacity)) return;

    auto pmatrix = PP(ctx.propagator)->transform();
    ctx.propagator->transform(pmatrix ? mathMultiply(pmatrix, &matrix) : matrix);
    ctx.propagator->opacity(opacity);
}


static void _updateGroup(LottieGroup* parent, LottieGroup* group, int32_t frameNo, RenderContext& ctx)
{
    //Prepare render data
    group->scene = parent->scene;

    auto opacity = group->opacity(frameNo);
    if (opacity == 0) return;

    //TODO: move transform to layer.
    if (group->transform) TVGERR("LOTTIE", "group transform is not working?!");

    auto ctx2 = ctx;
    _updateChildren(group, frameNo, ctx2);
}


static void _updateFill(LottieSolidFill* fill, int32_t frameNo, RenderContext& ctx)
{
    //TODO: Skip if property has not been modified actually.
    ctx.merging = nullptr;

    auto color = fill->color(frameNo);
    ctx.propagator->fill(color.rgb[0], color.rgb[1], color.rgb[2], fill->opacity(frameNo));
    ctx.propagator->fill(fill->rule);

    if (ctx.propagator->strokeWidth() > 0) ctx.propagator->order(true);
}


static void _updateStroke(LottieStroke* stroke, int32_t frameNo, RenderContext& ctx)
{
    ctx.propagator->stroke(stroke->width(frameNo));
    ctx.propagator->stroke(stroke->cap);
    ctx.propagator->stroke(stroke->join);
    ctx.propagator->strokeMiterlimit(stroke->miterLimit);

    if (stroke->dashattr) {
        float dashes[2];
        dashes[0] = stroke->dashSize(frameNo);
        dashes[1] = dashes[0] + stroke->dashGap(frameNo);
        P(ctx.propagator)->strokeDash(dashes, 2, stroke->dashOffset(frameNo));
    } else {
        ctx.propagator->stroke(nullptr, 0);
    }
}


static void _updateStroke(LottieSolidStroke* stroke, int32_t frameNo, RenderContext& ctx)
{
    ctx.merging = nullptr;
    auto color = stroke->color(frameNo);
    ctx.propagator->stroke(color.rgb[0], color.rgb[1], color.rgb[2], stroke->opacity(frameNo));
    _updateStroke(static_cast<LottieStroke*>(stroke), frameNo, ctx);
}


static void _updateStroke(LottieGradientStroke* stroke, int32_t frameNo, RenderContext& ctx)
{
    ctx.merging = nullptr;
    ctx.propagator->stroke(unique_ptr<Fill>(stroke->fill(frameNo)));
    _updateStroke(static_cast<LottieStroke*>(stroke), frameNo, ctx);
}


static Shape* _updateFill(LottieGradientFill* fill, int32_t frameNo, RenderContext& ctx)
{
    ctx.merging = nullptr;

    //TODO: reuse the fill instance?
    ctx.propagator->fill(unique_ptr<Fill>(fill->fill(frameNo)));
    ctx.propagator->fill(fill->rule);

    if (ctx.propagator->strokeWidth() > 0) ctx.propagator->order(true);

    return nullptr;
}


static Shape* _draw(LottieGroup* parent, int32_t frameNo, RenderContext& ctx)
{
    if (ctx.merging) return ctx.merging;

    auto shape = cast<Shape>(ctx.propagator->duplicate());
    ctx.merging = shape.get();
    parent->scene->push(std::move(shape));

    return ctx.merging;
}


//OPTIMIZE: path?
static void _repeat(LottieGroup* parent, int32_t frameNo, unique_ptr<Shape> path, RenderContext& ctx)
{
    auto& repeater = ctx.repeater;

    Array<Shape*> shapes;
    shapes.reserve(repeater.cnt);

    for (int i = 0; i < repeater.cnt; ++i) {
        auto multiplier = repeater.offset + static_cast<float>(i);

        auto shape = static_cast<Shape*>(ctx.propagator->duplicate());
        P(shape)->rs.path = P(path.get())->rs.path;

        auto opacity = repeater.interpOpacity ? mathLerp<uint8_t>(repeater.startOpacity, repeater.endOpacity, static_cast<float>(i + 1) / repeater.cnt) : repeater.startOpacity;
        shape->opacity(opacity);

        Matrix m;
        mathIdentity(&m);
        mathTranslate(&m, repeater.position.x * multiplier + repeater.anchor.x, repeater.position.y * multiplier + repeater.anchor.y);
        mathScale(&m, powf(repeater.scale.x * 0.01f, multiplier), powf(repeater.scale.y * 0.01f, multiplier));
        mathRotate(&m, repeater.rotation * multiplier);
        mathTranslateR(&m, -repeater.anchor.x, -repeater.anchor.y);

        auto pm = PP(shape)->transform();
        shape->transform(pm ? mathMultiply(&m, pm) : m);

        if (ctx.roundness > 1.0f && P(shape)->rs.stroke) {
            TVGERR("LOTTIE", "FIXME: Path roundesss should be applied properly!");
            P(shape)->rs.stroke->join = StrokeJoin::Round;
        }

        shapes.push(shape);
    }

    //push repeat shapes in order.
    if (repeater.inorder) {
        for (auto shape = shapes.data; shape < shapes.end(); ++shape) {
            parent->scene->push(cast<Shape>(*shape));
        }
    } else {
        for (auto shape = shapes.end() - 1; shape >= shapes.data; --shape) {
            parent->scene->push(cast<Shape>(*shape));
        }
    }
}


static void _updateRect(LottieGroup* parent, LottieRect* rect, int32_t frameNo, RenderContext& ctx)
{
    auto position = rect->position(frameNo);
    auto size = rect->size(frameNo);
    auto roundness = rect->radius(frameNo);
    if (ctx.roundness > roundness) roundness = ctx.roundness;

    if (roundness > 0.0f) {
        if (roundness > size.x * 0.5f)  roundness = size.x * 0.5f;
        if (roundness > size.y * 0.5f)  roundness = size.y * 0.5f;
    }

    if (ctx.repeater.valid) {
        auto path = Shape::gen();
        path->appendRect(position.x - size.x * 0.5f, position.y - size.y * 0.5f, size.x, size.y, roundness, roundness);
        _repeat(parent, frameNo, std::move(path), ctx);
    } else {
        auto merging = _draw(parent, frameNo, ctx);
        merging->appendRect(position.x - size.x * 0.5f, position.y - size.y * 0.5f, size.x, size.y, roundness, roundness);
    }
}


static void _updateEllipse(LottieGroup* parent, LottieEllipse* ellipse, int32_t frameNo, RenderContext& ctx)
{
    auto position = ellipse->position(frameNo);
    auto size = ellipse->size(frameNo);

    if (ctx.repeater.valid) {
        auto path = Shape::gen();
        path->appendCircle(position.x, position.y, size.x * 0.5f, size.y * 0.5f);
        _repeat(parent, frameNo, std::move(path), ctx);
    } else {
        auto merging = _draw(parent, frameNo, ctx);
        merging->appendCircle(position.x, position.y, size.x * 0.5f, size.y * 0.5f);
    }
}


static void _updatePath(LottieGroup* parent, LottiePath* path, int32_t frameNo, RenderContext& ctx)
{
    if (ctx.repeater.valid) {
        auto p = Shape::gen();
        path->pathset(frameNo, P(p)->rs.path.cmds, P(p)->rs.path.pts);
        _repeat(parent, frameNo, std::move(p), ctx);
    } else {
        auto merging = _draw(parent, frameNo, ctx);

        if (path->pathset(frameNo, P(merging)->rs.path.cmds, P(merging)->rs.path.pts)) {
            P(merging)->update(RenderUpdateFlag::Path);
        }

        if (ctx.roundness > 1.0f && P(merging)->rs.stroke) {
            TVGERR("LOTTIE", "FIXME: Path roundesss should be applied properly!");
            P(merging)->rs.stroke->join = StrokeJoin::Round;
        }
    }
}


static void _updateStar(LottieGroup* parent, LottiePolyStar* star, Matrix* transform, int32_t frameNo, Shape* merging)
{
    static constexpr auto POLYSTAR_MAGIC_NUMBER = 0.47829f / 0.28f;

    auto ptsCnt = star->ptsCnt(frameNo);
    auto innerRadius = star->innerRadius(frameNo);
    auto outerRadius = star->outerRadius(frameNo);
    auto innerRoundness = star->innerRoundness(frameNo) * 0.01f;
    auto outerRoundness = star->outerRoundness(frameNo) * 0.01f;

    auto angle = -90.0f * MATH_PI / 180.0f;
    auto partialPointRadius = 0.0f;
    auto anglePerPoint = (2.0f * MATH_PI / ptsCnt);
    auto halfAnglePerPoint = anglePerPoint * 0.5f;
    auto partialPointAmount = ptsCnt - floorf(ptsCnt);
    auto longSegment = false;
    auto numPoints = size_t(ceilf(ptsCnt) * 2);
    auto direction = star->cw ? 1.0f : -1.0f;
    auto hasRoundness = false;

    float x, y;

    if (!mathZero(partialPointAmount)) {
        angle += halfAnglePerPoint * (1.0f - partialPointAmount) * direction;
    }

    if (!mathZero(partialPointAmount)) {
        partialPointRadius = innerRadius + partialPointAmount * (outerRadius - innerRadius);
        x = partialPointRadius * cosf(angle);
        y = partialPointRadius * sinf(angle);
        angle += anglePerPoint * partialPointAmount * 0.5f * direction;
    } else {
        x = outerRadius * cosf(angle);
        y = outerRadius * sinf(angle);
        angle += halfAnglePerPoint * direction;
    }

    if (mathZero(innerRoundness) && mathZero(outerRoundness)) {
        P(merging)->rs.path.pts.reserve(numPoints + 2);
        P(merging)->rs.path.cmds.reserve(numPoints + 3);
    } else {
        P(merging)->rs.path.pts.reserve(numPoints * 3 + 2);
        P(merging)->rs.path.cmds.reserve(numPoints + 3);
        hasRoundness = true;
    }

    Point in = {x, y};
    if (transform) mathTransform(transform, &in);
    merging->moveTo(in.x, in.y);

    for (size_t i = 0; i < numPoints; i++) {
        auto radius = longSegment ? outerRadius : innerRadius;
        auto dTheta = halfAnglePerPoint;
        if (!mathZero(partialPointRadius) && i == numPoints - 2) {
            dTheta = anglePerPoint * partialPointAmount * 0.5f;
        }
        if (!mathZero(partialPointRadius) && i == numPoints - 1) {
            radius = partialPointRadius;
        }
        auto previousX = x;
        auto previousY = y;
        x = radius * cosf(angle);
        y = radius * sinf(angle);

        if (hasRoundness) {
            auto cp1Theta = (atan2f(previousY, previousX) - MATH_PI2 * direction);
            auto cp1Dx = cosf(cp1Theta);
            auto cp1Dy = sinf(cp1Theta);
            auto cp2Theta = (atan2f(y, x) - MATH_PI2 * direction);
            auto cp2Dx = cosf(cp2Theta);
            auto cp2Dy = sinf(cp2Theta);

            auto cp1Roundness = longSegment ? innerRoundness : outerRoundness;
            auto cp2Roundness = longSegment ? outerRoundness : innerRoundness;
            auto cp1Radius = longSegment ? innerRadius : outerRadius;
            auto cp2Radius = longSegment ? outerRadius : innerRadius;

            auto cp1x = cp1Radius * cp1Roundness * POLYSTAR_MAGIC_NUMBER * cp1Dx / ptsCnt;
            auto cp1y = cp1Radius * cp1Roundness * POLYSTAR_MAGIC_NUMBER * cp1Dy / ptsCnt;
            auto cp2x = cp2Radius * cp2Roundness * POLYSTAR_MAGIC_NUMBER * cp2Dx / ptsCnt;
            auto cp2y = cp2Radius * cp2Roundness * POLYSTAR_MAGIC_NUMBER * cp2Dy / ptsCnt;

            if (!mathZero(partialPointAmount) && ((i == 0) || (i == numPoints - 1))) {
                cp1x *= partialPointAmount;
                cp1y *= partialPointAmount;
                cp2x *= partialPointAmount;
                cp2y *= partialPointAmount;
            }
            Point in2 = {previousX - cp1x, previousY - cp1y};
            Point in3 = {x + cp2x, y + cp2y};
            Point in4 = {x, y};
            if (transform) {
                mathTransform(transform, &in2);
                mathTransform(transform, &in3);
                mathTransform(transform, &in4);
            }
            merging->cubicTo(in2.x, in2.y, in3.x, in3.y, in4.x, in4.y);
        } else {
            Point in = {x, y};
            if (transform) mathTransform(transform, &in);
            merging->lineTo(in.x, in.y);
        }
        angle += dTheta * direction;
        longSegment = !longSegment;
    }
    merging->close();
}


static void _updatePolygon(LottieGroup* parent, LottiePolyStar* star, Matrix* transform, int32_t frameNo, Shape* merging)
{
    static constexpr auto POLYGON_MAGIC_NUMBER = 0.25f;

    auto ptsCnt = size_t(floor(star->ptsCnt(frameNo)));
    auto radius = star->outerRadius(frameNo);
    auto roundness = star->outerRoundness(frameNo) * 0.01f;

    auto angle = -90.0f * MATH_PI / 180.0f;
    auto anglePerPoint = 2.0f * MATH_PI / float(ptsCnt);
    auto direction = star->cw ? 1.0f : -1.0f;
    auto hasRoundness = false;
    auto x = radius * cosf(angle);
    auto y = radius * sinf(angle);

    angle += anglePerPoint * direction;

    if (mathZero(roundness)) {
        P(merging)->rs.path.pts.reserve(ptsCnt + 2);
        P(merging)->rs.path.cmds.reserve(ptsCnt + 3);
    } else {
        P(merging)->rs.path.pts.reserve(ptsCnt * 3 + 2);
        P(merging)->rs.path.cmds.reserve(ptsCnt + 3);
        hasRoundness = true;
    }

    Point in = {x, y};
    if (transform) mathTransform(transform, &in);
    merging->moveTo(in.x, in.y);

    for (size_t i = 0; i < ptsCnt; i++) {
        auto previousX = x;
        auto previousY = y;
        x = (radius * cosf(angle));
        y = (radius * sinf(angle));

        if (hasRoundness) {
            auto cp1Theta = atan2f(previousY, previousX) - MATH_PI2 * direction;
            auto cp1Dx = cosf(cp1Theta);
            auto cp1Dy = sinf(cp1Theta);
            auto cp2Theta = atan2f(y, x) - MATH_PI2 * direction;
            auto cp2Dx = cosf(cp2Theta);
            auto cp2Dy = sinf(cp2Theta);

            auto cp1x = radius * roundness * POLYGON_MAGIC_NUMBER * cp1Dx;
            auto cp1y = radius * roundness * POLYGON_MAGIC_NUMBER * cp1Dy;
            auto cp2x = radius * roundness * POLYGON_MAGIC_NUMBER * cp2Dx;
            auto cp2y = radius * roundness * POLYGON_MAGIC_NUMBER * cp2Dy;

            Point in2 = {previousX - cp1x, previousY - cp1y};
            Point in3 = {x + cp2x, y + cp2y};
            Point in4 = {x, y};
            if (transform) {
                mathTransform(transform, &in2);
                mathTransform(transform, &in3);
                mathTransform(transform, &in4);
            }
            merging->cubicTo(in2.x, in2.y, in3.x, in3.y, in4.x, in4.y);
        } else {
            Point in = {x, y};
            if (transform) mathTransform(transform, &in);
            merging->lineTo(in.x, in.y);
        }
        angle += anglePerPoint * direction;
    }
    merging->close();
}


static void _updatePolystar(LottieGroup* parent, LottiePolyStar* star, int32_t frameNo, RenderContext& ctx)
{
    //Optimize: Can we skip the individual coords transform?
    Matrix matrix;
    mathIdentity(&matrix);
    auto position = star->position(frameNo);
    mathTranslate(&matrix, position.x, position.y);
    mathRotate(&matrix, star->rotation(frameNo) * 2.0f);

    auto identity = mathIdentity((const Matrix*)&matrix);


    if (ctx.repeater.valid) {
        auto p = Shape::gen();
        if (star->type == LottiePolyStar::Star) _updateStar(parent, star, identity ? nullptr : &matrix, frameNo, p.get());
        else _updatePolygon(parent, star, identity  ? nullptr : &matrix, frameNo, p.get());
        _repeat(parent, frameNo, std::move(p), ctx);
    } else {
        auto merging = _draw(parent, frameNo, ctx);
        if (star->type == LottiePolyStar::Star) _updateStar(parent, star, identity ? nullptr : &matrix, frameNo, merging);
        else _updatePolygon(parent, star, identity  ? nullptr : &matrix, frameNo, merging);
        P(merging)->update(RenderUpdateFlag::Path);
    }
}


static void _updateImage(LottieGroup* parent, LottieImage* image, int32_t frameNo, RenderContext& ctx)
{
    auto picture = image->picture;

    if (!picture) {
        picture = Picture::gen().release();
        if (image->size > 0) {
            if (picture->load((const char*)image->b64Data, image->size, image->mimeType, false) != Result::Success) return;
        } else {
            if (picture->load(image->path) != Result::Success) return;
        }
    }

    if (ctx.propagator) {
        if (auto matrix = PP(ctx.propagator)->transform()) {
            picture->transform(*matrix);
        }
        picture->opacity(ctx.propagator->opacity());
    }

    //TODO: remove duplicate.
    image->picture = (Picture*)picture->duplicate();

    parent->scene->push(cast<Picture>(picture));
}


static void _updateRoundedCorner(LottieRoundedCorner* roundedCorner, int32_t frameNo, RenderContext& ctx)
{
    auto roundness = roundedCorner->radius(frameNo);
    if (ctx.roundness < roundness) ctx.roundness = roundness;
}


static void _updateRepeater(LottieRepeater* repeater, int32_t frameNo, RenderContext& ctx)
{
    ctx.repeater.cnt = repeater->copies(frameNo);
    ctx.repeater.offset = repeater->offset(frameNo);
    ctx.repeater.position = repeater->position(frameNo);
    ctx.repeater.anchor = repeater->anchor(frameNo);
    ctx.repeater.scale = repeater->scale(frameNo);
    ctx.repeater.rotation = repeater->rotation(frameNo);
    ctx.repeater.startOpacity = repeater->startOpacity(frameNo);
    ctx.repeater.endOpacity = repeater->endOpacity(frameNo);
    ctx.repeater.inorder = repeater->inorder;
    ctx.repeater.interpOpacity = (ctx.repeater.startOpacity == ctx.repeater.endOpacity) ? false : true;
    ctx.repeater.valid = true;

    ctx.merging = nullptr;
}


static void _updateTrimpath(LottieTrimpath* trimpath, int32_t frameNo, RenderContext& ctx)
{
    float begin, end;
    trimpath->segment(frameNo, begin, end);

    if (P(ctx.propagator)->rs.stroke) {
        auto pbegin = P(ctx.propagator)->rs.stroke->trim.begin;
        auto pend = P(ctx.propagator)->rs.stroke->trim.end;
        auto length = fabsf(pend - pbegin);
        begin = (length * begin) + pbegin;
        end = (length * end) + pbegin;
    }

    if (trimpath->type == LottieTrimpath::Individual) {
        TVGERR("LOTTIE", "TODO: Individual Trimpath");
    }

    P(ctx.propagator)->strokeTrim(begin, end);
}


static void _updateChildren(LottieGroup* parent, int32_t frameNo, RenderContext& ctx)
{
    if (parent->children.empty()) return;

    //Draw the parent shapes first
    for (auto child = parent->children.end() - 1; child >= parent->children.data; --child) {
        //TODO: Polymorphsim?
        switch ((*child)->type) {
            case LottieObject::Group: {
                _updateGroup(parent, static_cast<LottieGroup*>(*child), frameNo, ctx);
                break;
            }
            case LottieObject::Transform: {
                _updateTransform(static_cast<LottieTransform*>(*child), frameNo, ctx);
                break;
            }
            case LottieObject::SolidFill: {
                _updateFill(static_cast<LottieSolidFill*>(*child), frameNo, ctx);
                break;
            }
            case LottieObject::SolidStroke: {
                _updateStroke(static_cast<LottieSolidStroke*>(*child), frameNo, ctx);
                break;
            }
            case LottieObject::GradientFill: {
                _updateFill(static_cast<LottieGradientFill*>(*child), frameNo, ctx);
                break;
            }
            case LottieObject::GradientStroke: {
                _updateStroke(static_cast<LottieGradientStroke*>(*child), frameNo, ctx);
                break;
            }
            case LottieObject::Rect: {
                _updateRect(parent, static_cast<LottieRect*>(*child), frameNo, ctx);
                break;
            }
            case LottieObject::Ellipse: {
                _updateEllipse(parent, static_cast<LottieEllipse*>(*child), frameNo, ctx);
                break;
            }
            case LottieObject::Path: {
                _updatePath(parent, static_cast<LottiePath*>(*child), frameNo, ctx);
                break;
            }
            case LottieObject::Polystar: {
                _updatePolystar(parent, static_cast<LottiePolyStar*>(*child), frameNo, ctx);
                break;
            }
            case LottieObject::Image: {
                _updateImage(parent, static_cast<LottieImage*>(*child), frameNo, ctx);
                break;
            }
            case LottieObject::Trimpath: {
                _updateTrimpath(static_cast<LottieTrimpath*>(*child), frameNo, ctx);
                break;
            }
            case LottieObject::Repeater: {
                _updateRepeater(static_cast<LottieRepeater*>(*child), frameNo, ctx);
                break;
            }
            case LottieObject::RoundedCorner: {
                _updateRoundedCorner(static_cast<LottieRoundedCorner*>(*child), frameNo, ctx);
                break;
            }
            default: break;
        }
    }
}


static void _updatePrecomp(LottieLayer* precomp, int32_t frameNo)
{
    if (precomp->children.count == 0) return;

    frameNo -= precomp->startFrame;

    //TODO: skip if the layer is static.
    for (auto child = precomp->children.end() - 1; child >= precomp->children.data; --child) {
        _updateLayer(precomp, static_cast<LottieLayer*>(*child), frameNo);
    }
}


static void _updateSolid(LottieLayer* layer, int32_t frameNo)
{
    auto shape = Shape::gen();
    shape->appendRect(0, 0, static_cast<float>(layer->w), static_cast<float>(layer->h));
    shape->fill(layer->color.rgb[0], layer->color.rgb[1], layer->color.rgb[2], layer->opacity(frameNo));
    layer->scene->push(std::move(shape));
}


static void _updateMaskings(LottieLayer* layer, int32_t frameNo)
{
    if (layer->masks.count == 0) return;

    //maskings
    Shape* pmask = nullptr;
    auto pmethod = CompositeMethod::AlphaMask;

    for (auto m = layer->masks.data; m < layer->masks.end(); ++m) {
        auto mask = static_cast<LottieMask*>(*m);
        auto shape = Shape::gen().release();
        shape->fill(255, 255, 255, mask->opacity(frameNo));
        shape->transform(layer->cache.matrix);
        if (mask->pathset(frameNo, P(shape)->rs.path.cmds, P(shape)->rs.path.pts)) {
            P(shape)->update(RenderUpdateFlag::Path);
        }
        auto method = mask->method;
        if (pmask) {
            //false of false is true. invert.
            if (method == CompositeMethod::SubtractMask && pmethod == method) {
                method = CompositeMethod::AddMask;
            } else if (pmethod == CompositeMethod::DifferenceMask && pmethod == method) {
                method = CompositeMethod::IntersectMask;
            }
            pmask->composite(cast<Shape>(shape), method);
        } else {
            if (method == CompositeMethod::SubtractMask) method = CompositeMethod::InvAlphaMask;
            else if (method == CompositeMethod::AddMask) method = CompositeMethod::AlphaMask;
            else if (method == CompositeMethod::IntersectMask) method = CompositeMethod::AlphaMask;
            else if (method == CompositeMethod::DifferenceMask) method = CompositeMethod::AlphaMask;   //does this correct?
            layer->scene->composite(cast<Shape>(shape), method);
        }
        pmethod = mask->method;
        pmask = shape;
    }
}


static void _updateLayer(LottieLayer* root, LottieLayer* layer, int32_t frameNo)
{
    //visibility
    if (frameNo < layer->inFrame || frameNo > layer->outFrame) return;
    auto opacity = layer->opacity(frameNo);
    if (opacity == 0) return;

    _updateTransform(layer, frameNo);

    //Prepare render data
    layer->scene = Scene::gen().release();
    layer->scene->transform(layer->cache.matrix);

    //FIXME: Ignore opacity when Null layer?
    if (layer->type != LottieLayer::Null) {
        layer->scene->opacity(layer->cache.opacity);
    }

    auto rFrameNo = layer->remap(frameNo);

    switch (layer->type) {
        case LottieLayer::Precomp: {
            _updatePrecomp(layer, rFrameNo);
            break;
        }
        case LottieLayer::Solid: {
            _updateSolid(layer, rFrameNo);
            break;
        }
        default: {
            RenderContext ctx;
            _updateChildren(layer, rFrameNo, ctx);
            break;
        }
    }

    if (layer->matte.target && layer->masks.count > 0) {
        TVGERR("LOTTIE", "FIXME: Matte + Masking??");
    }

    //matte masking layer
    if (layer->matte.target) {
        _updateLayer(root, layer->matte.target, frameNo);
        layer->scene->composite(cast<Scene>(layer->matte.target->scene), layer->matte.type);
    }

    _updateMaskings(layer, rFrameNo);

    //clip the layer viewport
    if (layer->refId && layer->w > 0 && layer->h > 0) {
        //TODO: remove the intermediate scene....
        auto cscene = Scene::gen();
        auto clipper = Shape::gen();
        clipper->appendRect(0, 0, layer->w, layer->h);
        clipper->transform(layer->cache.matrix);
        cscene->composite(std::move(clipper), CompositeMethod::ClipPath);
        cscene->push(cast<Scene>(layer->scene));
        layer->scene = cscene.release();
    }

    //the given matte source was composited by the target earlier.
    if (!layer->matteSrc) root->scene->push(cast<Scene>(layer->scene));
}


static void _buildReference(LottieComposition* comp, LottieLayer* layer)
{
    for (auto asset = comp->assets.data; asset < comp->assets.end(); ++asset) {
        if (strcmp(layer->refId, (*asset)->name)) continue;
        if (layer->type == LottieLayer::Precomp) {
            auto assetLayer = static_cast<LottieLayer*>(*asset);
            if (_buildPrecomp(comp, assetLayer)) {
                layer->children = assetLayer->children;
            }
        } else if (layer->type == LottieLayer::Image) {
            layer->children.push(*asset);
        }
        layer->statical &= (*asset)->statical;
        break;
    }
}


static void _bulidHierarchy(LottieGroup* parent, LottieLayer* child)
{
    if (child->pid == -1) return;

    for (auto p = parent->children.data; p < parent->children.end(); ++p) {
        auto parent = static_cast<LottieLayer*>(*p);
        if (child == parent) continue;
        if (child->pid == parent->id) {
            child->parent = parent;
            parent->statical &= child->statical;
            break;
        }
    }
}


static bool _buildPrecomp(LottieComposition* comp, LottieGroup* parent)
{
    if (parent->children.count == 0) return false;

    for (auto c = parent->children.data; c < parent->children.end(); ++c) {
        auto child = static_cast<LottieLayer*>(*c);

        //attach the referencing layer.
        if (child->refId) _buildReference(comp, child);

        //parenting
        if (child->matte.target) _bulidHierarchy(parent, child->matte.target);
        if (child->pid != -1) _bulidHierarchy(parent, child);
    }
    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool LottieBuilder::update(LottieComposition* comp, int32_t frameNo)
{
    frameNo += comp->startFrame;
    if (frameNo < comp->startFrame) frameNo = comp->startFrame;
    if (frameNo > comp->endFrame) frameNo = comp->endFrame;

    //Update root layer
    auto root = comp->root;
    if (!root) return false;

    //Prepare render data
    if (!root->scene) {
        auto scene = Scene::gen();
        root->scene = scene.get();
        comp->scene->push(std::move(scene));
    } else {
        root->scene->clear();
    }

    //update children layers
    //TODO: skip if the layer is static.
    for (auto child = root->children.end() - 1; child >= root->children.data; --child) {
        _updateLayer(root, static_cast<LottieLayer*>(*child), frameNo);
    }
    return true;
}


void LottieBuilder::build(LottieComposition* comp)
{
    if (!comp || comp->scene) return;

    comp->scene = Scene::gen().release();
    if (!comp->scene) return;

    _buildPrecomp(comp, comp->root);

    //TODO: Process repeater objects?

    if (!update(comp, 0)) return;

    //viewport clip
    auto clip = Shape::gen();
    clip->appendRect(0, 0, static_cast<float>(comp->w), static_cast<float>(comp->h));
    comp->scene->composite(std::move(clip), CompositeMethod::ClipPath);
}