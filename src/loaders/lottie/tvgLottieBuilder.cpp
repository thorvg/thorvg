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
#include "tvgInlist.h"
#include "tvgLottieModel.h"
#include "tvgLottieBuilder.h"
#include "tvgTaskScheduler.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct RenderRepeater
{
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
};


struct RenderContext
{
    INLIST_ITEM(RenderContext);

    Shape* propagator = nullptr;
    Shape* merging = nullptr;  //merging shapes if possible (if shapes have same properties)
    LottieObject** begin = nullptr; //iteration entry point
    RenderRepeater* repeater = nullptr;
    float roundness = 0.0f;
    bool stroking = false;     //context has been separated by the stroking
    bool reqFragment = false;  //requirment to fragment the render context
    bool allowMerging = true;  //individual trimpath doesn't allow merging shapes

    RenderContext()
    {
        propagator = Shape::gen().release();
    }

    ~RenderContext()
    {
        delete(propagator);
        delete(repeater);
    }

    RenderContext(const RenderContext& rhs)
    {
        propagator = static_cast<Shape*>(rhs.propagator->duplicate());
        if (rhs.repeater) {
            repeater = new RenderRepeater();
            *repeater = *rhs.repeater;
        }
        roundness = rhs.roundness;
    }
};


static void _updateChildren(LottieGroup* parent, float frameNo, Inlist<RenderContext>& contexts);
static void _updateLayer(LottieLayer* root, LottieLayer* layer, float frameNo);
static bool _buildComposition(LottieComposition* comp, LottieGroup* parent);

static void _rotateX(Matrix* m, float degree)
{
    if (degree == 0.0f) return;
    auto radian = degree / 180.0f * M_PI;
    m->e22 *= cosf(radian);
}


static void _rotateY(Matrix* m, float degree)
{
    if (degree == 0.0f) return;
    auto radian = degree / 180.0f * M_PI;
    m->e11 *= cosf(radian);
}


static void _rotationZ(Matrix* m, float degree)
{
    if (degree == 0.0f) return;
    auto radian = degree / 180.0f * M_PI;
    m->e11 = cosf(radian);
    m->e12 = -sinf(radian);
    m->e21 = sinf(radian);
    m->e22 = cosf(radian);
}


static bool _updateTransform(LottieTransform* transform, float frameNo, bool autoOrient, Matrix& matrix, uint8_t& opacity)
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
    _rotationZ(&matrix, transform->rotation(frameNo) + angle);

    if (transform->rotationEx) {
        _rotateY(&matrix, transform->rotationEx->y(frameNo));
        _rotateX(&matrix, transform->rotationEx->x(frameNo));
    }

    auto scale = transform->scale(frameNo);
    mathScaleR(&matrix, scale.x * 0.01f, scale.y * 0.01f);

    //Lottie specific anchor transform.
    auto anchor = transform->anchor(frameNo);
    mathTranslateR(&matrix, -anchor.x, -anchor.y);

    //invisible just in case.
    if (scale.x == 0.0f || scale.y == 0.0f) opacity = 0;
    else opacity = transform->opacity(frameNo);

    return true;
}


static void _updateTransform(LottieLayer* layer, float frameNo)
{
    if (!layer || mathEqual(layer->cache.frameNo, frameNo)) return;

    auto transform = layer->transform;
    auto parent = layer->parent;

    if (parent) _updateTransform(parent, frameNo);

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


static void _updateTransform(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto transform = static_cast<LottieTransform*>(*child);
    if (!transform) return;

    ctx->merging = nullptr;

    Matrix matrix;
    uint8_t opacity;
    if (!_updateTransform(transform, frameNo, false, matrix, opacity)) return;

    auto pmatrix = PP(ctx->propagator)->transform();
    ctx->propagator->transform(pmatrix ? mathMultiply(pmatrix, &matrix) : matrix);
    ctx->propagator->opacity(MULTIPLY(opacity, PP(ctx->propagator)->opacity));

    //FIXME: preserve the stroke width. too workaround, need a better design.
    if (P(ctx->propagator)->rs.strokeWidth() > 0.0f) {
        auto denominator = sqrtf(matrix.e11 * matrix.e11 + matrix.e12 * matrix.e12);
        if (denominator > 1.0f) ctx->propagator->stroke(ctx->propagator->strokeWidth() / denominator);
    }
}


static void _updateGroup(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& pcontexts, RenderContext* ctx)
{
    auto group = static_cast<LottieGroup*>(*child);

    if (group->children.empty()) return;

    //Prepare render data
    group->scene = parent->scene;

    Inlist<RenderContext> contexts;
    contexts.back(new RenderContext(*ctx));

    _updateChildren(group, frameNo, contexts);

    contexts.free();
}


static void _updateStroke(LottieStroke* stroke, float frameNo, RenderContext* ctx)
{
    ctx->propagator->stroke(stroke->width(frameNo));
    ctx->propagator->stroke(stroke->cap);
    ctx->propagator->stroke(stroke->join);
    ctx->propagator->strokeMiterlimit(stroke->miterLimit);

    if (stroke->dashattr) {
        float dashes[2];
        dashes[0] = stroke->dashSize(frameNo);
        dashes[1] = dashes[0] + stroke->dashGap(frameNo);
        P(ctx->propagator)->strokeDash(dashes, 2, stroke->dashOffset(frameNo));
    } else {
        ctx->propagator->stroke(nullptr, 0);
    }
}


static bool _fragmentedStroking(LottieObject** child, Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    if (!ctx->reqFragment) return false;
    if (ctx->stroking) return true;

    contexts.back(new RenderContext(*ctx));
    auto fragment = contexts.tail;
    fragment->propagator->stroke(0.0f);
    fragment->begin = child - 1;
    ctx->stroking = true;

    return false;
}


static void _updateSolidStroke(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    if (_fragmentedStroking(child, contexts, ctx)) return;

    auto stroke = static_cast<LottieSolidStroke*>(*child);

    ctx->merging = nullptr;
    auto color = stroke->color(frameNo);
    ctx->propagator->stroke(color.rgb[0], color.rgb[1], color.rgb[2], stroke->opacity(frameNo));
    _updateStroke(static_cast<LottieStroke*>(stroke), frameNo, ctx);
}


static void _updateGradientStroke(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    if (_fragmentedStroking(child, contexts, ctx)) return;

    auto stroke = static_cast<LottieGradientStroke*>(*child);

    ctx->merging = nullptr;
    ctx->propagator->stroke(unique_ptr<Fill>(stroke->fill(frameNo)));
    _updateStroke(static_cast<LottieStroke*>(stroke), frameNo, ctx);
}


static void _updateSolidFill(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    if (ctx->stroking) return;

    auto fill = static_cast<LottieSolidFill*>(*child);

    ctx->merging = nullptr;

    auto color = fill->color(frameNo);
    ctx->propagator->fill(color.rgb[0], color.rgb[1], color.rgb[2], fill->opacity(frameNo));
    ctx->propagator->fill(fill->rule);

    if (ctx->propagator->strokeWidth() > 0) ctx->propagator->order(true);
}


static Shape* _updateGradientFill(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    if (ctx->stroking) return nullptr;

    auto fill = static_cast<LottieGradientFill*>(*child);

    ctx->merging = nullptr;

    //TODO: reuse the fill instance?
    ctx->propagator->fill(unique_ptr<Fill>(fill->fill(frameNo)));
    ctx->propagator->fill(fill->rule);
    ctx->propagator->opacity(MULTIPLY(fill->opacity(frameNo), PP(ctx->propagator)->opacity));

    if (ctx->propagator->strokeWidth() > 0) ctx->propagator->order(true);

    return nullptr;
}


static Shape* _draw(LottieGroup* parent, RenderContext* ctx)
{
    if (ctx->allowMerging && ctx->merging) return ctx->merging;

    auto shape = cast<Shape>(ctx->propagator->duplicate());
    ctx->merging = shape.get();
    parent->scene->push(std::move(shape));

    return ctx->merging;
}


//OPTIMIZE: path?
static void _repeat(LottieGroup* parent, unique_ptr<Shape> path, RenderContext* ctx)
{
    auto repeater = ctx->repeater;

    Array<Shape*> shapes;
    shapes.reserve(repeater->cnt);

    for (int i = 0; i < repeater->cnt; ++i) {
        auto multiplier = repeater->offset + static_cast<float>(i);

        auto shape = static_cast<Shape*>(ctx->propagator->duplicate());
        P(shape)->rs.path = P(path.get())->rs.path;

        auto opacity = repeater->interpOpacity ? mathLerp<uint8_t>(repeater->startOpacity, repeater->endOpacity, static_cast<float>(i + 1) / repeater->cnt) : repeater->startOpacity;
        shape->opacity(opacity);

        Matrix m;
        mathIdentity(&m);
        mathTranslate(&m, repeater->position.x * multiplier + repeater->anchor.x, repeater->position.y * multiplier + repeater->anchor.y);
        mathScale(&m, powf(repeater->scale.x * 0.01f, multiplier), powf(repeater->scale.y * 0.01f, multiplier));
        mathRotate(&m, repeater->rotation * multiplier);
        mathTranslateR(&m, -repeater->anchor.x, -repeater->anchor.y);

        auto pm = PP(shape)->transform();
        shape->transform(pm ? mathMultiply(&m, pm) : m);

        if (ctx->roundness > 1.0f && P(shape)->rs.stroke) {
            TVGERR("LOTTIE", "FIXME: Path roundesss should be applied properly!");
            P(shape)->rs.stroke->join = StrokeJoin::Round;
        }

        shapes.push(shape);
    }

    //push repeat shapes in order.
    if (repeater->inorder) {
        for (auto shape = shapes.data; shape < shapes.end(); ++shape) {
            parent->scene->push(cast(*shape));
        }
    } else {
        for (auto shape = shapes.end() - 1; shape >= shapes.data; --shape) {
            parent->scene->push(cast(*shape));
        }
    }
}


static void _updateRect(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto rect= static_cast<LottieRect*>(*child);

    auto position = rect->position(frameNo);
    auto size = rect->size(frameNo);
    auto roundness = rect->radius(frameNo);
    if (ctx->roundness > roundness) roundness = ctx->roundness;

    if (roundness > 0.0f) {
        if (roundness > size.x * 0.5f)  roundness = size.x * 0.5f;
        if (roundness > size.y * 0.5f)  roundness = size.y * 0.5f;
    }

    if (ctx->repeater) {
        auto path = Shape::gen();
        path->appendRect(position.x - size.x * 0.5f, position.y - size.y * 0.5f, size.x, size.y, roundness, roundness);
        _repeat(parent, std::move(path), ctx);
    } else {
        auto merging = _draw(parent, ctx);
        merging->appendRect(position.x - size.x * 0.5f, position.y - size.y * 0.5f, size.x, size.y, roundness, roundness);
    }
}


static void _updateEllipse(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto ellipse= static_cast<LottieEllipse*>(*child);

    auto position = ellipse->position(frameNo);
    auto size = ellipse->size(frameNo);

    if (ctx->repeater) {
        auto path = Shape::gen();
        path->appendCircle(position.x, position.y, size.x * 0.5f, size.y * 0.5f);
        _repeat(parent, std::move(path), ctx);
    } else {
        auto merging = _draw(parent, ctx);
        merging->appendCircle(position.x, position.y, size.x * 0.5f, size.y * 0.5f);
    }
}


static void _updatePath(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto path= static_cast<LottiePath*>(*child);

    if (ctx->repeater) {
        auto p = Shape::gen();
        path->pathset(frameNo, P(p)->rs.path.cmds, P(p)->rs.path.pts);
        _repeat(parent, std::move(p), ctx);
    } else {
        auto merging = _draw(parent, ctx);

        if (path->pathset(frameNo, P(merging)->rs.path.cmds, P(merging)->rs.path.pts)) {
            P(merging)->update(RenderUpdateFlag::Path);
        }

        if (ctx->roundness > 1.0f && P(merging)->rs.stroke) {
            TVGERR("LOTTIE", "FIXME: Path roundesss should be applied properly!");
            P(merging)->rs.stroke->join = StrokeJoin::Round;
        }
    }
}


static void _updateStar(LottieGroup* parent, LottiePolyStar* star, Matrix* transform, float frameNo, Shape* merging)
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


static void _updatePolygon(LottieGroup* parent, LottiePolyStar* star, Matrix* transform, float frameNo, Shape* merging)
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


static void _updatePolystar(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto star= static_cast<LottiePolyStar*>(*child);

    //Optimize: Can we skip the individual coords transform?
    Matrix matrix;
    mathIdentity(&matrix);
    auto position = star->position(frameNo);
    mathTranslate(&matrix, position.x, position.y);
    mathRotate(&matrix, star->rotation(frameNo));

    auto identity = mathIdentity((const Matrix*)&matrix);

    if (ctx->repeater) {
        auto p = Shape::gen();
        if (star->type == LottiePolyStar::Star) _updateStar(parent, star, identity ? nullptr : &matrix, frameNo, p.get());
        else _updatePolygon(parent, star, identity  ? nullptr : &matrix, frameNo, p.get());
        _repeat(parent, std::move(p), ctx);
    } else {
        auto merging = _draw(parent, ctx);
        if (star->type == LottiePolyStar::Star) _updateStar(parent, star, identity ? nullptr : &matrix, frameNo, merging);
        else _updatePolygon(parent, star, identity  ? nullptr : &matrix, frameNo, merging);
        P(merging)->update(RenderUpdateFlag::Path);
    }
}


static void _updateImage(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto image = static_cast<LottieImage*>(*child);
    auto picture = image->picture;

    if (!picture) {
        picture = Picture::gen().release();

        //force to load a picture on the same thread
        TaskScheduler::async(false);

        if (image->size > 0) {
            if (picture->load((const char*)image->b64Data, image->size, image->mimeType, false) != Result::Success) {
                delete(picture);
                return;
            }
        } else {
            if (picture->load(image->path) != Result::Success) {
                delete(picture);
                return;
            }
        }

        TaskScheduler::async(true);

        image->picture = picture;
        PP(picture)->ref();
    }

    if (ctx->propagator) {
        if (auto matrix = PP(ctx->propagator)->transform()) {
            picture->transform(*matrix);
        }
        picture->opacity(PP(ctx->propagator)->opacity);
    }
    parent->scene->push(cast<Picture>(picture));
}


static void _updateRoundedCorner(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto roundedCorner= static_cast<LottieRoundedCorner*>(*child);

    auto roundness = roundedCorner->radius(frameNo);
    if (ctx->roundness < roundness) ctx->roundness = roundness;
}


static void _updateRepeater(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto repeater= static_cast<LottieRepeater*>(*child);

    if (!ctx->repeater) ctx->repeater = new RenderRepeater();
    ctx->repeater->cnt = static_cast<int>(repeater->copies(frameNo));
    ctx->repeater->offset = repeater->offset(frameNo);
    ctx->repeater->position = repeater->position(frameNo);
    ctx->repeater->anchor = repeater->anchor(frameNo);
    ctx->repeater->scale = repeater->scale(frameNo);
    ctx->repeater->rotation = repeater->rotation(frameNo);
    ctx->repeater->startOpacity = repeater->startOpacity(frameNo);
    ctx->repeater->endOpacity = repeater->endOpacity(frameNo);
    ctx->repeater->inorder = repeater->inorder;
    ctx->repeater->interpOpacity = (ctx->repeater->startOpacity == ctx->repeater->endOpacity) ? false : true;

    ctx->merging = nullptr;
}


static void _updateTrimpath(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto trimpath= static_cast<LottieTrimpath*>(*child);

    float begin, end;
    trimpath->segment(frameNo, begin, end);

    if (P(ctx->propagator)->rs.stroke) {
        auto pbegin = P(ctx->propagator)->rs.stroke->trim.begin;
        auto pend = P(ctx->propagator)->rs.stroke->trim.end;
        auto length = fabsf(pend - pbegin);
        begin = (length * begin) + pbegin;
        end = (length * end) + pbegin;
    }

    P(ctx->propagator)->strokeTrim(begin, end);

    if (trimpath->type == LottieTrimpath::Individual) ctx->allowMerging = false;
}


static void _updateChildren(LottieGroup* parent, float frameNo, Inlist<RenderContext>& contexts)
{
    contexts.head->begin = parent->children.end() - 1;

    while (!contexts.empty()) {
        auto ctx = contexts.front();
        ctx->reqFragment = parent->reqFragment;
        for (auto child = ctx->begin; child >= parent->children.data; --child) {
            switch ((*child)->type) {
                case LottieObject::Group: {
                    _updateGroup(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Transform: {
                    _updateTransform(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::SolidFill: {
                    _updateSolidFill(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::SolidStroke: {
                    _updateSolidStroke(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::GradientFill: {
                    _updateGradientFill(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::GradientStroke: {
                    _updateGradientStroke(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Rect: {
                    _updateRect(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Ellipse: {
                    _updateEllipse(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Path: {
                    _updatePath(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Polystar: {
                    _updatePolystar(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Image: {
                    _updateImage(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Trimpath: {
                    _updateTrimpath(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Repeater: {
                    _updateRepeater(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::RoundedCorner: {
                    _updateRoundedCorner(parent, child, frameNo, contexts, ctx);
                    break;
                }
                default: break;
            }
        }
        delete(ctx);
    }
}


static void _updatePrecomp(LottieLayer* precomp, float frameNo)
{
    if (precomp->children.count == 0) return;

    frameNo = precomp->remap(frameNo);

    for (auto child = precomp->children.end() - 1; child >= precomp->children.data; --child) {
        _updateLayer(precomp, static_cast<LottieLayer*>(*child), frameNo);
    }

    //clip the layer viewport
    if (precomp->w > 0 && precomp->h > 0) {
        if (!precomp->cache.clipper) {
            precomp->cache.clipper = Shape::gen().release();
            PP(precomp->cache.clipper)->ref();
            precomp->cache.clipper->appendRect(0, 0, static_cast<float>(precomp->w), static_cast<float>(precomp->h));
        }
        auto clipper = precomp->cache.clipper;
        clipper->transform(precomp->cache.matrix);

        //TODO: remove the intermediate scene....
        auto cscene = Scene::gen();
        cscene->composite(cast(clipper), CompositeMethod::ClipPath);
        cscene->push(cast(precomp->scene));
        precomp->scene = cscene.release();
    }
}


static void _updateSolid(LottieLayer* layer)
{
    auto shape = Shape::gen();
    shape->appendRect(0, 0, static_cast<float>(layer->w), static_cast<float>(layer->h));
    shape->fill(layer->color.rgb[0], layer->color.rgb[1], layer->color.rgb[2], layer->cache.opacity);
    layer->scene->push(std::move(shape));
}


static void _updateMaskings(LottieLayer* layer, float frameNo)
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


static bool _updateMatte(LottieLayer* root, LottieLayer* layer, float frameNo)
{
    auto target = layer->matte.target;
    if (!target) return true;

    _updateLayer(root, target, frameNo);

    if (target->scene) {
        layer->scene->composite(cast(target->scene), layer->matte.type);
    } else  if (layer->matte.type == CompositeMethod::AlphaMask || layer->matte.type == CompositeMethod::LumaMask) {
        //matte target is not exist. alpha blending definitely bring an invisible result
        delete(layer->scene);
        layer->scene = nullptr;
        return false;
    }
    return true;
}


static void _updateLayer(LottieLayer* root, LottieLayer* layer, float frameNo)
{
    layer->scene = nullptr;

    //visibility
    if (frameNo < layer->inFrame || frameNo >= layer->outFrame) return;

    _updateTransform(layer, frameNo);

    //full transparent scene. no need to perform
    if (layer->type != LottieLayer::Null && layer->cache.opacity == 0) return;

    //Prepare render data
    layer->scene = Scene::gen().release();

    //ignore opacity when Null layer?
    if (layer->type != LottieLayer::Null) layer->scene->opacity(layer->cache.opacity);

    layer->scene->transform(layer->cache.matrix);

    if (layer->matte.target && layer->masks.count > 0) TVGERR("LOTTIE", "FIXME: Matte + Masking??");

    if (!_updateMatte(root, layer, frameNo)) return;

    _updateMaskings(layer, frameNo);

    switch (layer->type) {
        case LottieLayer::Precomp: {
            if (!layer->children.empty()) {
                _updatePrecomp(layer, frameNo);
            }
            break;
        }
        case LottieLayer::Solid: {
            _updateSolid(layer);
            break;
        }
        default: {
            if (!layer->children.empty()) {
                Inlist<RenderContext> contexts;
                contexts.back(new RenderContext);
                _updateChildren(layer, frameNo, contexts);
                contexts.free();
            }
            break;
        }
    }

    layer->scene->blend(layer->blendMethod);

    //the given matte source was composited by the target earlier.
    if (!layer->matteSrc) root->scene->push(cast(layer->scene));
}


static void _buildReference(LottieComposition* comp, LottieLayer* layer)
{
    for (auto asset = comp->assets.data; asset < comp->assets.end(); ++asset) {
        if (strcmp(layer->refId, (*asset)->name)) continue;
        if (layer->type == LottieLayer::Precomp) {
            auto assetLayer = static_cast<LottieLayer*>(*asset);
            if (_buildComposition(comp, assetLayer)) {
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

    if (child->matte.target && child->pid == child->matte.target->id) {
        child->parent = child->matte.target;
        return;
    }

    for (auto p = parent->children.data; p < parent->children.end(); ++p) {
        auto parent = static_cast<LottieLayer*>(*p);
        if (child == parent) continue;
        if (child->pid == parent->id) {
            child->parent = parent;
            break;
        }
        if (parent->matte.target && parent->matte.target->id == child->pid) {
            child->parent = parent->matte.target;
            break;
        }
    }
}


//TODO: Optimize this. Can we preprocess in the parsing stage?
static void _checkFragment(LottieGroup* parent)
{
    if (parent->children.count == 0) return;

    int strokeCnt = 0;

    /* Figure out if the rendering context should be fragmented.
       Multiple stroking or grouping with a stroking would occur this.
       This fragment resolves the overlapped stroke outlines. */
    for (auto c = parent->children.end() - 1; c >= parent->children.data; --c) {
        switch ((*c)->type) {
            case LottieObject::Group: {
                if (strokeCnt > 0) {
                    parent->reqFragment = true;
                    return;
                }
                break;
            }
            case LottieObject::SolidStroke:
            case LottieObject::GradientStroke: {
                if (strokeCnt > 0) {
                    parent->reqFragment = true;
                    return;
                }
                ++strokeCnt;
                break;
            }
            default: break;
        }
    }
}


static bool _buildComposition(LottieComposition* comp, LottieGroup* parent)
{
    if (parent->children.count == 0) return false;

    for (auto c = parent->children.data; c < parent->children.end(); ++c) {
        auto child = static_cast<LottieLayer*>(*c);

        //attach the precomp layer.
        if (child->refId) _buildReference(comp, child);

        if (child->matte.target) {
            //parenting
            _bulidHierarchy(parent, child->matte.target);
            //precomp referencing
            if (child->matte.target->refId) _buildReference(comp, child->matte.target);
            child->statical &= child->matte.target->statical;
        }
        _bulidHierarchy(parent, child);

        _checkFragment(static_cast<LottieGroup*>(*c));

        child->statical &= parent->statical;
        parent->statical &= child->statical;
    }
    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool LottieBuilder::update(LottieComposition* comp, float frameNo)
{
    frameNo += comp->startFrame;
    if (frameNo < comp->startFrame) frameNo = comp->startFrame;
    if (frameNo >= comp->endFrame) frameNo = (comp->endFrame - 1);

    //Update root layer
    auto root = comp->root;

    //Prepare render data
    if (!root->scene) {
        auto scene = Scene::gen();
        root->scene = scene.get();
        comp->scene->push(std::move(scene));
    } else {
        root->scene->clear();
    }

    //update children layers
    for (auto child = root->children.end() - 1; child >= root->children.data; --child) {
        _updateLayer(root, static_cast<LottieLayer*>(*child), frameNo);
    }
    return true;
}


void LottieBuilder::build(LottieComposition* comp)
{
    if (!comp || !comp->root || comp->scene) return;

    comp->scene = Scene::gen().release();
    if (!comp->scene) return;

    _buildComposition(comp, comp->root);

    if (!update(comp, 0)) return;

    //viewport clip
    auto clip = Shape::gen();
    clip->appendRect(0, 0, static_cast<float>(comp->w), static_cast<float>(comp->h));
    comp->scene->composite(std::move(clip), CompositeMethod::ClipPath);
}