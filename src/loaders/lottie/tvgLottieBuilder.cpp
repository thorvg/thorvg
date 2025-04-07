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

#include <algorithm>
#include "tvgCommon.h"
#include "tvgMath.h"
#include "tvgLottieModel.h"
#include "tvgLottieBuilder.h"
#include "tvgLottieExpressions.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static bool _buildComposition(LottieComposition* comp, LottieLayer* parent);
static bool _draw(LottieGroup* parent, LottieShape* shape, RenderContext* ctx);


static void _rotate(LottieTransform* transform, float frameNo, Matrix& m, float angle, Tween& tween, LottieExpressions* exps)
{
    //rotation xyz
    if (transform->rotationEx) {
        auto radianX = deg2rad(transform->rotationEx->x(frameNo, tween, exps));
        auto radianY = deg2rad(transform->rotationEx->y(frameNo, tween, exps));
        auto radianZ = deg2rad(transform->rotation(frameNo, tween, exps)) + angle;
        auto cx = cosf(radianX), sx = sinf(radianX);
        auto cy = cosf(radianY), sy = sinf(radianY);;
        auto cz = cosf(radianZ), sz = sinf(radianZ);;
        m.e11 = cy * cz;
        m.e12 = -cy * sz;
        m.e21 = sx * sy * cz + cx * sz;
        m.e22 = -sx * sy * sz + cx * cz;
    //rotation z
    } else {
        auto degree = transform->rotation(frameNo, tween, exps) + angle;
        if (degree == 0.0f) return;
        auto radian = deg2rad(degree);
        m.e11 = cosf(radian);
        m.e12 = -sinf(radian);
        m.e21 = sinf(radian);
        m.e22 = cosf(radian);
    }
}


static void _skew(Matrix* m, float angleDeg, float axisDeg)
{
    auto angle = -deg2rad(angleDeg);
    auto tanVal = tanf(angle);

    axisDeg = fmod(axisDeg, 180.0f);
    if (fabsf(axisDeg) < 0.01f || fabsf(axisDeg - 180.0f) < 0.01f || fabsf(axisDeg + 180.0f) < 0.01f) {
        float cosVal = cosf(deg2rad(axisDeg));
        auto B = cosVal * cosVal * tanVal;
        m->e12 += B * m->e11;
        m->e22 += B * m->e21;
        return;
    } else if (fabsf(axisDeg - 90.0f) < 0.01f || fabsf(axisDeg + 90.0f) < 0.01f) {
        float sinVal = -sinf(deg2rad(axisDeg));
        auto C = sinVal * sinVal * tanVal;
        m->e11 -= C * m->e12;
        m->e21 -= C * m->e22;
        return;
    }

    auto axis = -deg2rad(axisDeg);
    auto cosVal = cosf(axis);
    auto sinVal = sinf(axis);
    auto A = sinVal * cosVal * tanVal;
    auto B = cosVal * cosVal * tanVal;
    auto C = sinVal * sinVal * tanVal;

    auto e11 = m->e11;
    auto e21 = m->e21;
    m->e11 = (1.0f - A) * e11 - C * m->e12;
    m->e12 = B * e11 + (1.0f + A) * m->e12;
    m->e21 = (1.0f - A) * e21 - C * m->e22;
    m->e22 = B * e21 + (1.0f + A) * m->e22;
}


static bool _updateTransform(LottieTransform* transform, float frameNo, Matrix& matrix, uint8_t& opacity, bool autoOrient, Tween& tween, LottieExpressions* exps)
{
    tvg::identity(&matrix);

    if (!transform) {
        opacity = 255;
        return false;
    }

    if (transform->coords) translate(&matrix, {transform->coords->x(frameNo, tween, exps), transform->coords->y(frameNo, tween, exps)});
    else translate(&matrix, transform->position(frameNo, tween, exps));

    auto angle = (autoOrient) ? transform->position.angle(frameNo, tween) : 0.0f;
    _rotate(transform, frameNo, matrix, angle, tween, exps);

    auto skewAngle = transform->skewAngle(frameNo, tween, exps);
    if (skewAngle != 0.0f) {
        // For angles where tangent explodes, the shape degenerates into an infinitely thin line.
        // This is handled by zeroing out the matrix due to finite numerical precision.
        skewAngle = fmod(skewAngle, 180.0f);
        if (fabsf(skewAngle - 90.0f) < 0.01f || fabsf(skewAngle + 90.0f) < 0.01f) return false;
        _skew(&matrix, skewAngle, transform->skewAxis(frameNo, exps));
    }

    auto scale = transform->scale(frameNo, tween, exps);
    scaleR(&matrix, scale * 0.01f);

    //Lottie specific anchor transform.
    translateR(&matrix, -transform->anchor(frameNo, tween, exps));

    //invisible just in case.
    if (scale.x == 0.0f || scale.y == 0.0f) opacity = 0;
    else opacity = transform->opacity(frameNo, tween, exps);

    return true;
}


void LottieBuilder::updateTransform(LottieLayer* layer, float frameNo)
{
    if (!layer || (!tweening() && tvg::equal(layer->cache.frameNo, frameNo))) return;

    auto transform = layer->transform;
    auto parent = layer->parent;

    if (parent) updateTransform(parent, frameNo);

    auto& matrix = layer->cache.matrix;

    _updateTransform(transform, frameNo, matrix, layer->cache.opacity, layer->autoOrient, tween, exps);

    if (parent) layer->cache.matrix = parent->cache.matrix * matrix;

    layer->cache.frameNo = frameNo;
}


void LottieBuilder::updateTransform(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto transform = static_cast<LottieTransform*>(*child);
    if (!transform) return;

    Matrix m;
    uint8_t opacity;

    if (parent->mergeable()) {
        if (ctx->transform) {
            _updateTransform(transform, frameNo, m, opacity, false, tween, exps);
            *ctx->transform *= m;
        } else {
            ctx->transform = new Matrix;
            _updateTransform(transform, frameNo, *ctx->transform, opacity, false, tween, exps);
        }
        return;
    }

    ctx->merging = nullptr;

    if (!_updateTransform(transform, frameNo, m, opacity, false, tween, exps)) return;

    ctx->propagator->transform(ctx->propagator->transform() * m);
    ctx->propagator->opacity(MULTIPLY(opacity, PAINT(ctx->propagator)->opacity));

    //FIXME: preserve the stroke width. too workaround, need a better design.
    if (SHAPE(ctx->propagator)->rs.strokeWidth() > 0.0f) {
        auto denominator = sqrtf(m.e11 * m.e11 + m.e12 * m.e12);
        if (denominator > 1.0f) ctx->propagator->strokeWidth(ctx->propagator->strokeWidth() / denominator);
    }
}


void LottieBuilder::updateGroup(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& pcontexts, RenderContext* ctx)
{
    auto group = static_cast<LottieGroup*>(*child);

    if (!group->visible) return;

    //Prepare render data
    group->scene = parent->scene;
    group->reqFragment |= ctx->reqFragment;

    //generate a merging shape to consolidate partial shapes into a single entity
    if (group->mergeable()) _draw(parent, nullptr, ctx);

    Inlist<RenderContext> contexts;
    auto propagator = group->mergeable() ? ctx->propagator : static_cast<Shape*>(PAINT(ctx->propagator)->duplicate(group->pooling()));
    contexts.back(new RenderContext(*ctx, propagator, group->mergeable()));

    updateChildren(group, frameNo, contexts);
}


static void _updateStroke(LottieStroke* stroke, float frameNo, RenderContext* ctx, Tween& tween, LottieExpressions* exps)
{
    ctx->propagator->strokeWidth(stroke->width(frameNo, tween, exps));
    ctx->propagator->strokeCap(stroke->cap);
    ctx->propagator->strokeJoin(stroke->join);
    ctx->propagator->strokeMiterlimit(stroke->miterLimit);

    if (stroke->dashattr) {
        auto size = stroke->dashattr->size == 1 ? 2 : stroke->dashattr->size;
        auto dashes = (float*)alloca(size * sizeof(float));
        for (uint8_t i = 0; i < stroke->dashattr->size; ++i) {
            auto value = stroke->dashattr->values[i](frameNo, tween, exps);
            //FIXME: allow the zero value in the engine level.
            dashes[i] = value < FLT_EPSILON ? 0.01f : value;
        }
        if (stroke->dashattr->size == 1) dashes[1] = dashes[0];
        ctx->propagator->strokeDash(dashes, size, stroke->dashattr->offset(frameNo, tween, exps));
    } else {
        ctx->propagator->strokeDash(nullptr, 0);
    }
}


bool LottieBuilder::fragmented(LottieGroup* parent, LottieObject** child, Inlist<RenderContext>& contexts, RenderContext* ctx, RenderFragment fragment)
{
    if (ctx->fragment) return true;
    if (!ctx->reqFragment) return false;

    contexts.back(new RenderContext(*ctx, (Shape*)(PAINT(ctx->propagator)->duplicate(parent->pooling()))));

    contexts.tail->begin = child - 1;
    ctx->fragment = fragment;

    return false;
}


bool LottieBuilder::updateSolidStroke(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto stroke = static_cast<LottieSolidStroke*>(*child);
    if (fragmented(parent, child, contexts, ctx, RenderFragment::ByStroke)) return false;

    auto opacity = stroke->opacity(frameNo, tween, exps);
    if (opacity == 0) return false;

    ctx->merging = nullptr;
    auto color = stroke->color(frameNo, tween, exps);
    ctx->propagator->strokeFill(color.rgb[0], color.rgb[1], color.rgb[2], opacity);
    _updateStroke(static_cast<LottieStroke*>(stroke), frameNo, ctx, tween, exps);

    return false;
}


bool LottieBuilder::updateGradientStroke(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto stroke = static_cast<LottieGradientStroke*>(*child);
    if (fragmented(parent, child, contexts, ctx, RenderFragment::ByStroke)) return false;

    auto opacity = stroke->opacity(frameNo, tween, exps);
    if (opacity == 0 && !stroke->opaque) return false;

    ctx->merging = nullptr;
    if (auto val = stroke->fill(frameNo, opacity, tween, exps)) ctx->propagator->strokeFill(val);
    _updateStroke(static_cast<LottieStroke*>(stroke), frameNo, ctx, tween, exps);

    return false;
}


bool LottieBuilder::updateSolidFill(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto fill = static_cast<LottieSolidFill*>(*child);
    auto opacity = fill->opacity(frameNo, tween, exps);

    //interrupted by fully opaque, stop the current rendering
    if (ctx->fragment == RenderFragment::ByFill && opacity == 255) return true;
    if (opacity == 0) return false;

    if (fragmented(parent, child, contexts, ctx, RenderFragment::ByFill)) return false;

    ctx->merging = nullptr;
    auto color = fill->color(frameNo, tween, exps);
    ctx->propagator->fill(color.rgb[0], color.rgb[1], color.rgb[2], opacity);
    ctx->propagator->fill(fill->rule);

    if (ctx->propagator->strokeWidth() > 0) ctx->propagator->order(true);

    return false;
}


bool LottieBuilder::updateGradientFill(LottieGroup* parent, LottieObject** child, float frameNo, Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto fill = static_cast<LottieGradientFill*>(*child);
    auto opacity = fill->opacity(frameNo, tween, exps);

    //interrupted by fully opaque, stop the current rendering
    if (ctx->fragment == RenderFragment::ByFill && fill->opaque && opacity == 255) return true;

    if (fragmented(parent, child, contexts, ctx, RenderFragment::ByFill)) return false;

    ctx->merging = nullptr;

    if (auto val = fill->fill(frameNo, opacity, tween, exps)) ctx->propagator->fill(val);
    ctx->propagator->fill(fill->rule);

    if (ctx->propagator->strokeWidth() > 0) ctx->propagator->order(true);

    return false;
}


static bool _draw(LottieGroup* parent, LottieShape* shape, RenderContext* ctx)
{
    if (ctx->merging) return false;

    if (shape) {
        ctx->merging = shape->pooling();
        PAINT(ctx->propagator)->duplicate(ctx->merging);
    } else {
        ctx->merging = static_cast<Shape*>(ctx->propagator->duplicate());
    }

    parent->scene->push(ctx->merging);

    return true;
}


static void _repeat(LottieGroup* parent, Shape* path, RenderContext* ctx)
{
    Array<Shape*> propagators;
    propagators.push(ctx->propagator);
    Array<Shape*> shapes;

    ARRAY_REVERSE_FOREACH(repeater, ctx->repeaters) {
        shapes.reserve(repeater->cnt);

        for (int i = 0; i < repeater->cnt; ++i) {
            auto multiplier = repeater->offset + static_cast<float>(i);

            ARRAY_FOREACH(p, propagators) {
                auto shape = static_cast<Shape*>((*p)->duplicate());
                SHAPE(shape)->rs.path = SHAPE(path)->rs.path;

                auto opacity = repeater->interpOpacity ? tvg::lerp<uint8_t>(repeater->startOpacity, repeater->endOpacity, static_cast<float>(i + 1) / repeater->cnt) : repeater->startOpacity;
                shape->opacity(opacity);

                Matrix m;
                tvg::identity(&m);
                translate(&m, repeater->position * multiplier + repeater->anchor);
                scale(&m, {powf(repeater->scale.x * 0.01f, multiplier), powf(repeater->scale.y * 0.01f, multiplier)});
                rotate(&m, repeater->rotation * multiplier);
                translateR(&m, -repeater->anchor);
                m = repeater->transform * m;

                Matrix inv;
                inverse(&repeater->transform, &inv);
                shape->transform(m * (inv * shape->transform()));
                shapes.push(shape);
            }
        }

        propagators.clear();
        propagators.reserve(shapes.count);

        //push repeat shapes in order.
        if (repeater->inorder) {
            ARRAY_FOREACH(p, shapes) {
                parent->scene->push(*p);
                propagators.push(*p);
            }
        } else if (!shapes.empty()) {
            ARRAY_REVERSE_FOREACH(shape, shapes) {
                parent->scene->push(*shape);
                propagators.push(*shape);
            }
        }
        shapes.clear();
    }
}


void LottieBuilder::appendRect(Shape* shape, Point& pos, Point& size, float r, bool clockwise, RenderContext* ctx)
{
    auto temp = (ctx->offset) ? Shape::gen() : shape;
    auto cnt = SHAPE(temp)->rs.path.pts.count;

    temp->appendRect(pos.x, pos.y, size.x, size.y, r, r, clockwise);

    if (ctx->transform) {
        for (auto i = cnt; i < SHAPE(temp)->rs.path.pts.count; ++i) {
            SHAPE(temp)->rs.path.pts[i] *= *ctx->transform;
        }
    }

    if (ctx->offset) {
        ctx->offset->modifyRect(SHAPE(temp)->rs.path, SHAPE(shape)->rs.path);
        delete(temp);
    }
}


void LottieBuilder::updateRect(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto rect = static_cast<LottieRect*>(*child);
    auto size = rect->size(frameNo, tween, exps);
    auto pos = rect->position(frameNo, tween, exps) - size * 0.5f;
    auto r = rect->radius(frameNo, tween, exps);

    if (r == 0.0f)  {
        if (ctx->roundness) ctx->roundness->modifyRect(size, r);
    } else {
        r = std::min({r, size.x * 0.5f, size.y * 0.5f});
    }

    if (ctx->repeaters.empty()) {
        _draw(parent, rect, ctx);
        appendRect(ctx->merging, pos, size, r, rect->clockwise, ctx);
    } else {
        auto shape = rect->pooling();
        shape->reset();
        appendRect(shape, pos, size, r, rect->clockwise, ctx);
        _repeat(parent, shape, ctx);
    }
}


static void _appendCircle(Shape* shape, Point& center, Point& radius, bool clockwise, RenderContext* ctx)
{
    if (ctx->offset) ctx->offset->modifyEllipse(radius);

    auto cnt = SHAPE(shape)->rs.path.pts.count;

    shape->appendCircle(center.x, center.y, radius.x, radius.y, clockwise);

    if (ctx->transform) {
        for (auto i = cnt; i < SHAPE(shape)->rs.path.pts.count; ++i) {
            SHAPE(shape)->rs.path.pts[i] *= *ctx->transform;
        }
    }
}


void LottieBuilder::updateEllipse(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto ellipse = static_cast<LottieEllipse*>(*child);
    auto pos = ellipse->position(frameNo, tween, exps);
    auto size = ellipse->size(frameNo, tween, exps) * 0.5f;

    if (ctx->repeaters.empty()) {
        _draw(parent, ellipse, ctx);
        _appendCircle(ctx->merging, pos, size, ellipse->clockwise, ctx);
    } else {
        auto shape = ellipse->pooling();
        shape->reset();
        _appendCircle(shape, pos, size, ellipse->clockwise, ctx);
        _repeat(parent, shape, ctx);
    }
}


void LottieBuilder::updatePath(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto path = static_cast<LottiePath*>(*child);

    if (ctx->repeaters.empty()) {
        _draw(parent, path, ctx);
        if (path->pathset(frameNo, SHAPE(ctx->merging)->rs.path, ctx->transform, tween, exps, ctx->modifier)) {
            PAINT(ctx->merging)->update(RenderUpdateFlag::Path);
        }
    } else {
        auto shape = path->pooling();
        shape->reset();
        path->pathset(frameNo, SHAPE(shape)->rs.path, ctx->transform, tween, exps, ctx->modifier);
        _repeat(parent, shape, ctx);
    }
}


void LottieBuilder::updateStar(LottiePolyStar* star, float frameNo, Matrix* transform, Shape* merging, RenderContext* ctx, Tween& tween, LottieExpressions* exps)
{
    static constexpr auto POLYSTAR_MAGIC_NUMBER = 0.47829f / 0.28f;

    auto ptsCnt = star->ptsCnt(frameNo, tween, exps);
    auto innerRadius = star->innerRadius(frameNo, tween, exps);
    auto outerRadius = star->outerRadius(frameNo, tween, exps);
    auto innerRoundness = star->innerRoundness(frameNo, tween, exps) * 0.01f;
    auto outerRoundness = star->outerRoundness(frameNo, tween, exps) * 0.01f;

    auto angle = deg2rad(-90.0f);
    auto partialPointRadius = 0.0f;
    auto anglePerPoint = (2.0f * MATH_PI / ptsCnt);
    auto halfAnglePerPoint = anglePerPoint * 0.5f;
    auto partialPointAmount = ptsCnt - floorf(ptsCnt);
    auto longSegment = false;
    auto numPoints = size_t(ceilf(ptsCnt) * 2);
    auto direction = star->clockwise ? 1.0f : -1.0f;
    auto hasRoundness = false;
    bool roundedCorner = ctx->roundness && (tvg::zero(innerRoundness) || tvg::zero(outerRoundness));

    Shape* shape;
    if (roundedCorner || ctx->offset) {
        shape = star->pooling();
        shape->reset();
    } else {
        shape = merging;
    }

    float x, y;

    if (!tvg::zero(partialPointAmount)) {
        angle += halfAnglePerPoint * (1.0f - partialPointAmount) * direction;
    }

    if (!tvg::zero(partialPointAmount)) {
        partialPointRadius = innerRadius + partialPointAmount * (outerRadius - innerRadius);
        x = partialPointRadius * cosf(angle);
        y = partialPointRadius * sinf(angle);
        angle += anglePerPoint * partialPointAmount * 0.5f * direction;
    } else {
        x = outerRadius * cosf(angle);
        y = outerRadius * sinf(angle);
        angle += halfAnglePerPoint * direction;
    }

    if (tvg::zero(innerRoundness) && tvg::zero(outerRoundness)) {
        SHAPE(shape)->rs.path.pts.reserve(numPoints + 2);
        SHAPE(shape)->rs.path.cmds.reserve(numPoints + 3);
    } else {
        SHAPE(shape)->rs.path.pts.reserve(numPoints * 3 + 2);
        SHAPE(shape)->rs.path.cmds.reserve(numPoints + 3);
        hasRoundness = true;
    }

    auto in = Point{x, y} * transform;
    shape->moveTo(in.x, in.y);

    for (size_t i = 0; i < numPoints; i++) {
        auto radius = longSegment ? outerRadius : innerRadius;
        auto dTheta = halfAnglePerPoint;
        if (!tvg::zero(partialPointRadius) && i == numPoints - 2) {
            dTheta = anglePerPoint * partialPointAmount * 0.5f;
        }
        if (!tvg::zero(partialPointRadius) && i == numPoints - 1) {
            radius = partialPointRadius;
        }
        auto previousX = x;
        auto previousY = y;
        x = radius * cosf(angle);
        y = radius * sinf(angle);

        if (hasRoundness) {
            auto cp1Theta = (tvg::atan2(previousY, previousX) - MATH_PI2 * direction);
            auto cp1Dx = cosf(cp1Theta);
            auto cp1Dy = sinf(cp1Theta);
            auto cp2Theta = (tvg::atan2(y, x) - MATH_PI2 * direction);
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

            if (!tvg::zero(partialPointAmount) && ((i == 0) || (i == numPoints - 1))) {
                cp1x *= partialPointAmount;
                cp1y *= partialPointAmount;
                cp2x *= partialPointAmount;
                cp2y *= partialPointAmount;
            }
            auto in2 = Point{previousX - cp1x, previousY - cp1y} * transform;
            auto in3 = Point{x + cp2x, y + cp2y} * transform;
            auto in4 = Point{x, y} * transform;
            shape->cubicTo(in2.x, in2.y, in3.x, in3.y, in4.x, in4.y);
        } else {
            auto in = Point{x, y} * transform;
            shape->lineTo(in.x, in.y);
        }
        angle += dTheta * direction;
        longSegment = !longSegment;
    }
    shape->close();

    if (ctx->modifier) ctx->modifier->modifyPolystar(SHAPE(shape)->rs.path, SHAPE(merging)->rs.path, outerRoundness, hasRoundness);
}


void LottieBuilder::updatePolygon(LottieGroup* parent, LottiePolyStar* star, float frameNo, Matrix* transform, Shape* merging, RenderContext* ctx, Tween& tween, LottieExpressions* exps)
{
    static constexpr auto POLYGON_MAGIC_NUMBER = 0.25f;

    auto ptsCnt = size_t(floor(star->ptsCnt(frameNo, tween, exps)));
    auto radius = star->outerRadius(frameNo, tween, exps);
    auto outerRoundness = star->outerRoundness(frameNo, tween, exps) * 0.01f;

    auto angle = deg2rad(-90.0f);
    auto anglePerPoint = 2.0f * MATH_PI / float(ptsCnt);
    auto direction = star->clockwise ? 1.0f : -1.0f;
    auto hasRoundness = !tvg::zero(outerRoundness);
    bool roundedCorner = ctx->roundness && !hasRoundness;
    auto x = radius * cosf(angle);
    auto y = radius * sinf(angle);

    angle += anglePerPoint * direction;

    Shape* shape;
    if (roundedCorner || ctx->offset) {
        shape = star->pooling();
        shape->reset();
    } else {
        shape = merging;
        if (hasRoundness) {
            SHAPE(shape)->rs.path.pts.reserve(ptsCnt * 3 + 2);
            SHAPE(shape)->rs.path.cmds.reserve(ptsCnt + 3);
        } else {
            SHAPE(shape)->rs.path.pts.reserve(ptsCnt + 2);
            SHAPE(shape)->rs.path.cmds.reserve(ptsCnt + 3);
        }
    }

    auto in = Point{x, y} * transform;
    shape->moveTo(in.x, in.y);

    for (size_t i = 0; i < ptsCnt; i++) {
        auto previousX = x;
        auto previousY = y;
        x = (radius * cosf(angle));
        y = (radius * sinf(angle));

        if (hasRoundness) {
            auto cp1Theta = tvg::atan2(previousY, previousX) - MATH_PI2 * direction;
            auto cp1Dx = cosf(cp1Theta);
            auto cp1Dy = sinf(cp1Theta);
            auto cp2Theta = tvg::atan2(y, x) - MATH_PI2 * direction;
            auto cp2Dx = cosf(cp2Theta);
            auto cp2Dy = sinf(cp2Theta);

            auto cp1x = radius * outerRoundness * POLYGON_MAGIC_NUMBER * cp1Dx;
            auto cp1y = radius * outerRoundness * POLYGON_MAGIC_NUMBER * cp1Dy;
            auto cp2x = radius * outerRoundness * POLYGON_MAGIC_NUMBER * cp2Dx;
            auto cp2y = radius * outerRoundness * POLYGON_MAGIC_NUMBER * cp2Dy;

            auto in2 = Point{previousX - cp1x, previousY - cp1y} * transform;
            auto in3 = Point{x + cp2x, y + cp2y} * transform;
            auto in4 = Point{x, y} * transform;
            shape->cubicTo(in2.x, in2.y, in3.x, in3.y, in4.x, in4.y);
        } else {
            Point in = {x, y};
            if (transform) in *= *transform;
            shape->lineTo(in.x, in.y);
        }
        angle += anglePerPoint * direction;
    }
    shape->close();

    if (ctx->modifier) ctx->modifier->modifyPolystar(SHAPE(shape)->rs.path, SHAPE(merging)->rs.path, 0.0f, false);
}


void LottieBuilder::updatePolystar(LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto star = static_cast<LottiePolyStar*>(*child);

    //Optimize: Can we skip the individual coords transform?
    Matrix matrix;
    tvg::identity(&matrix);
    translate(&matrix, star->position(frameNo, tween, exps));
    rotate(&matrix, star->rotation(frameNo, tween, exps));

    if (ctx->transform) matrix = *ctx->transform * matrix;

    auto identity = tvg::identity((const Matrix*)&matrix);

    if (ctx->repeaters.empty()) {
        _draw(parent, star, ctx);
        if (star->type == LottiePolyStar::Star) updateStar(star, frameNo, (identity ? nullptr : &matrix), ctx->merging, ctx, tween, exps);
        else updatePolygon(parent, star, frameNo, (identity  ? nullptr : &matrix), ctx->merging, ctx, tween, exps);
        PAINT(ctx->merging)->update(RenderUpdateFlag::Path);
    } else {
        auto shape = star->pooling();
        shape->reset();
        if (star->type == LottiePolyStar::Star) updateStar(star, frameNo, (identity ? nullptr : &matrix), shape, ctx, tween, exps);
        else updatePolygon(parent, star, frameNo, (identity  ? nullptr : &matrix), shape, ctx, tween, exps);
        _repeat(parent, shape, ctx);
    }
}


void LottieBuilder::updateRoundedCorner(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto roundedCorner = static_cast<LottieRoundedCorner*>(*child);
    auto r = roundedCorner->radius(frameNo, tween, exps);
    if (r < LottieRoundnessModifier::ROUNDNESS_EPSILON) return;

    if (!ctx->roundness) ctx->roundness = new LottieRoundnessModifier(&buffer, r);
    else if (ctx->roundness->r < r) ctx->roundness->r = r;

    ctx->update(ctx->roundness);
}


void LottieBuilder::updateOffsetPath(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto offset = static_cast<LottieOffsetPath*>(*child);
    if (!ctx->offset) ctx->offset = new LottieOffsetModifier(offset->offset(frameNo, tween, exps), offset->miterLimit(frameNo, tween, exps), offset->join);

    ctx->update(ctx->offset);
}


void LottieBuilder::updateRepeater(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto repeater = static_cast<LottieRepeater*>(*child);

    RenderRepeater r;
    r.cnt = static_cast<int>(repeater->copies(frameNo, tween, exps));
    r.transform = ctx->propagator->transform();
    r.offset = repeater->offset(frameNo, tween, exps);
    r.position = repeater->position(frameNo, tween, exps);
    r.anchor = repeater->anchor(frameNo, tween, exps);
    r.scale = repeater->scale(frameNo, tween, exps);
    r.rotation = repeater->rotation(frameNo, tween, exps);
    r.startOpacity = repeater->startOpacity(frameNo, tween, exps);
    r.endOpacity = repeater->endOpacity(frameNo, tween, exps);
    r.inorder = repeater->inorder;
    r.interpOpacity = (r.startOpacity == r.endOpacity) ? false : true;
    ctx->repeaters.push(r);

    ctx->merging = nullptr;
}


void LottieBuilder::updateTrimpath(TVG_UNUSED LottieGroup* parent, LottieObject** child, float frameNo, TVG_UNUSED Inlist<RenderContext>& contexts, RenderContext* ctx)
{
    auto trimpath = static_cast<LottieTrimpath*>(*child);

    float begin, end;
    trimpath->segment(frameNo, begin, end, tween, exps);

    if (SHAPE(ctx->propagator)->rs.stroke) {
        auto pbegin = SHAPE(ctx->propagator)->rs.stroke->trim.begin;
        auto pend = SHAPE(ctx->propagator)->rs.stroke->trim.end;
        auto length = fabsf(pend - pbegin);
        begin = (length * begin) + pbegin;
        end = (length * end) + pbegin;
    }

    ctx->propagator->trimpath(begin, end, trimpath->type == LottieTrimpath::Type::Simultaneous);
    ctx->merging = nullptr;
}


void LottieBuilder::updateChildren(LottieGroup* parent, float frameNo, Inlist<RenderContext>& contexts)
{
    contexts.head->begin = parent->children.end() - 1;

    while (!contexts.empty()) {
        auto ctx = contexts.front();
        ctx->reqFragment = parent->reqFragment;
        auto stop = false;
        for (auto child = ctx->begin; child >= parent->children.data; --child) {
            //Here switch-case statements are more performant than virtual methods.
            switch ((*child)->type) {
                case LottieObject::Group: {
                    updateGroup(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Transform: {
                    updateTransform(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::SolidFill: {
                    stop = updateSolidFill(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::SolidStroke: {
                    stop = updateSolidStroke(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::GradientFill: {
                    stop = updateGradientFill(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::GradientStroke: {
                    stop = updateGradientStroke(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Rect: {
                    updateRect(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Ellipse: {
                    updateEllipse(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Path: {
                    updatePath(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Polystar: {
                    updatePolystar(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Trimpath: {
                    updateTrimpath(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::Repeater: {
                    updateRepeater(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::RoundedCorner: {
                    updateRoundedCorner(parent, child, frameNo, contexts, ctx);
                    break;
                }
                case LottieObject::OffsetPath: {
                    updateOffsetPath(parent, child, frameNo, contexts, ctx);
                    break;
                }
                default: break;
            }

            //stop processing for those invisible contents
            if (stop || ctx->propagator->opacity() == 0) break;
        }
        delete(ctx);
    }
}


void LottieBuilder::updatePrecomp(LottieComposition* comp, LottieLayer* precomp, float frameNo)
{
    if (precomp->children.empty()) return;

    frameNo = precomp->remap(comp, frameNo, exps);

    ARRAY_REVERSE_FOREACH(c, precomp->children) {
        auto child = static_cast<LottieLayer*>(*c);
        if (!child->matteSrc) updateLayer(comp, precomp->scene, child, frameNo);
    }

    //clip the layer viewport
    auto clipper = precomp->statical.pooling(true);
    clipper->transform(precomp->cache.matrix);
    precomp->scene->clip(clipper);
}


void LottieBuilder::updatePrecomp(LottieComposition* comp, LottieLayer* precomp, float frameNo, Tween& tween)
{
    //record & recover the tweening frame number before remapping
    auto record = tween.frameNo;
    tween.frameNo = precomp->remap(comp, record, exps);

    updatePrecomp(comp, precomp, frameNo);

    tween.frameNo = record;
}


void LottieBuilder::updateSolid(LottieLayer* layer)
{
    auto solidFill = layer->statical.pooling(true);
    solidFill->opacity(layer->cache.opacity);
    layer->scene->push(solidFill);
}


void LottieBuilder::updateImage(LottieGroup* layer)
{
    auto image = static_cast<LottieImage*>(layer->children.first());
    layer->scene->push(image->pooling(true));
}


//TODO: unify with the updateText() building logic
static void _fontText(TextDocument& doc, Scene* scene)
{
    auto delim = "\r\n";
    auto size = doc.size * 75.0f; //1 pt = 1/72; 1 in = 96 px; -> 72/96 = 0.75
    auto lineHeight = doc.size * 100.0f;

    auto buf = (char*)alloca(strlen(doc.text) + 1);
    strcpy(buf, doc.text);
    auto token = std::strtok(buf, delim);

    auto cnt = 0;
    while (token) {
        auto txt = Text::gen();
        if (txt->font(doc.name, size) != Result::Success) {
            //fallback to any available font
            txt->font(nullptr, size);
        }

        txt->text(token);
        txt->fill(doc.color.rgb[0], doc.color.rgb[1], doc.color.rgb[2]);

        float width;
        txt->bounds(nullptr, nullptr, &width, nullptr);

        auto cursorX = width * doc.justify;
        auto cursorY = lineHeight * cnt;
        txt->translate(cursorX, -lineHeight + cursorY);

        token = std::strtok(nullptr, delim);
        scene->push(txt);
        cnt++;
    }
}


void LottieBuilder::updateText(LottieLayer* layer, float frameNo)
{
    auto text = static_cast<LottieText*>(layer->children.first());
    auto textGrouping = text->alignOption.grouping;
    auto& doc = text->doc(frameNo, exps);
    auto p = doc.text;

    if (!p || !text->font) return;

    if (text->font->origin != LottieFont::Origin::Embedded) {
        _fontText(doc, layer->scene);
        return;
    }

    auto scale = doc.size;
    Point cursor{};
    //TODO: Need to revise to alloc scene / textgroup when they are really necessary
    auto scene = Scene::gen();
    Scene* textGroup = Scene::gen();
    int line = 0;
    int space = 0;
    auto lineSpacing = 0.0f;
    auto totalLineSpacing = 0.0f;
    auto followPath = (text->followPath && ((uint32_t)text->followPath->maskIdx < layer->masks.count)) ? text->followPath : nullptr;
    auto firstMargin = followPath ? followPath->prepare(layer->masks[followPath->maskIdx], frameNo, scale, tween, exps) : 0.0f;

    //text string
    int idx = 0;
    auto totalChars = strlen(p);
    while (true) {
        //TODO: remove nested scenes.
        //end of text, new line of the cursor position
        if (*p == 13 || *p == 3 || *p == '\0') {
            //text layout position
            auto ascent = text->font->ascent * scale;
            if (ascent > doc.bbox.size.y) ascent = doc.bbox.size.y;
            Point layout = {doc.bbox.pos.x, doc.bbox.pos.y + ascent - doc.shift};

            //horizontal alignment
            layout.x += doc.justify * (-1.0f * doc.bbox.size.x + cursor.x * scale);

            //new text group, single scene based on text-grouping
            scene->push(textGroup);
            textGroup = Scene::gen();
            textGroup->translate(cursor.x, cursor.y);

            scene->translate(layout.x, layout.y);
            scene->scale(scale);

            layer->scene->push(scene);
            scene = nullptr;

            if (*p == '\0') break;
            ++p;

            totalLineSpacing += lineSpacing;
            lineSpacing = 0.0f;

            //new text group, single scene for each line
            scene = Scene::gen();
            cursor.x = 0.0f;
            cursor.y = (++line * doc.height + totalLineSpacing) / scale;
            continue;
        }

        if (*p == ' ') {
            ++space;
            if (textGrouping == LottieText::AlignOption::Group::Word) {
                //new text group, single scene for each word
                scene->push(textGroup);
                textGroup = Scene::gen();
                textGroup->translate(cursor.x, cursor.y);
            }
        }

        /* all lowercase letters are converted to uppercase in the "t" text field, making the "ca" value irrelevant, thus AllCaps is nothing to do.
           So only convert lowercase letters to uppercase (for 'SmallCaps' an extra scaling factor applied) */
        auto code = p;
        auto capScale = 1.0f;
        char capCode;
        if ((unsigned char)(p[0]) < 0x80 && doc.caps) {
            if (*p >= 'a' && *p <= 'z') {
                capCode = *p + 'A' - 'a';
                code = &capCode;
                if (doc.caps == 2) capScale = 0.7f;
            }
        }

        //find the glyph
        bool found = false;
        ARRAY_FOREACH(g, text->font->chars) {
            auto glyph = *g;
            //draw matched glyphs
            if (!strncmp(glyph->code, code, glyph->len)) {
                if (textGrouping == LottieText::AlignOption::Group::Chars || textGrouping == LottieText::AlignOption::Group::All) {
                    //new text group, single scene for each characters
                    scene->push(textGroup);
                    textGroup = Scene::gen();
                    textGroup->translate(cursor.x, cursor.y);
                }

                auto& textGroupMatrix = textGroup->transform();
                auto shape = text->pooling();
                shape->reset();
                ARRAY_FOREACH(p, glyph->children) {
                    auto group = static_cast<LottieGroup*>(*p);
                    ARRAY_FOREACH(p, group->children) {
                        if (static_cast<LottiePath*>(*p)->pathset(frameNo, SHAPE(shape)->rs.path, nullptr, tween, exps)) {
                            PAINT(shape)->update(RenderUpdateFlag::Path);
                        }
                    }
                }
                shape->fill(doc.color.rgb[0], doc.color.rgb[1], doc.color.rgb[2]);
                shape->translate(cursor.x - textGroupMatrix.e13, cursor.y - textGroupMatrix.e23);
                shape->opacity(255);

                if (doc.stroke.width > 0.0f) {
                    shape->strokeJoin(StrokeJoin::Round);
                    shape->strokeWidth(doc.stroke.width / scale);
                    shape->strokeFill(doc.stroke.color.rgb[0], doc.stroke.color.rgb[1], doc.stroke.color.rgb[2]);
                    shape->order(doc.stroke.below);
                }

                auto needGroup = false;
                //text range process
                if (!text->ranges.empty()) {
                    Point scaling = {1.0f, 1.0f};
                    auto rotation = 0.0f;
                    Point translation = {0.0f, 0.0f};
                    auto color = doc.color;
                    auto strokeColor = doc.stroke.color;
                    uint8_t opacity = 255;
                    uint8_t fillOpacity = 255;
                    uint8_t strokeOpacity = 255;

                    ARRAY_FOREACH(p, text->ranges) {
                        auto range = *p;
                        auto basedIdx = idx;
                        if (range->based == LottieTextRange::Based::CharsExcludingSpaces) basedIdx = idx - space;
                        else if (range->based == LottieTextRange::Based::Words) basedIdx = line + space;
                        else if (range->based == LottieTextRange::Based::Lines) basedIdx = line;

                        auto f = range->factor(frameNo, float(totalChars), (float)basedIdx);
                        if (tvg::zero(f)) continue;
                        needGroup = true;

                        translation = translation + f * range->style.position(frameNo, tween, exps);
                        scaling = scaling * (f * (range->style.scale(frameNo, tween, exps) * 0.01f - Point{1.0f, 1.0f}) + Point{1.0f, 1.0f});
                        rotation += f * range->style.rotation(frameNo, tween, exps);

                        opacity = (uint8_t)(opacity - f * (opacity - range->style.opacity(frameNo, tween, exps)));
                        shape->opacity(opacity);

                        range->color(frameNo, color, strokeColor, f, tween, exps);

                        fillOpacity = (uint8_t)(fillOpacity - f * (fillOpacity - range->style.fillOpacity(frameNo, tween, exps)));
                        shape->fill(color.rgb[0], color.rgb[1], color.rgb[2], fillOpacity);

                        if (range->style.flags.strokeWidth) shape->strokeWidth(f * range->style.strokeWidth(frameNo, tween, exps) / scale);
                        if (shape->strokeWidth() > 0.0f) {
                            strokeOpacity = (uint8_t)(strokeOpacity - f * (strokeOpacity - range->style.strokeOpacity(frameNo, tween, exps)));
                            shape->strokeFill(strokeColor.rgb[0], strokeColor.rgb[1], strokeColor.rgb[2], strokeOpacity);
                            shape->order(doc.stroke.below);
                        }
                        cursor.x += f * range->style.letterSpacing(frameNo, tween, exps);

                        auto spacing = f * range->style.lineSpacing(frameNo, tween, exps);
                        if (spacing > lineSpacing) lineSpacing = spacing;
                    }

                    // TextGroup transformation is performed once
                    if (textGroup->paints().size() == 0 && needGroup) {
                        tvg::identity(&textGroupMatrix);
                        translate(&textGroupMatrix, cursor);

                        auto alignment = text->alignOption.anchor(frameNo, tween, exps);

                        // center pivoting
                        textGroupMatrix.e13 += alignment.x;
                        textGroupMatrix.e23 += alignment.y;

                        rotate(&textGroupMatrix, rotation);

                        //center pivoting
                        auto pivot = alignment * -1;
                        textGroupMatrix.e13 += (pivot.x * textGroupMatrix.e11 + pivot.x * textGroupMatrix.e12);
                        textGroupMatrix.e23 += (pivot.y * textGroupMatrix.e21 + pivot.y * textGroupMatrix.e22);

                        textGroup->transform(textGroupMatrix);
                    }

                    auto& matrix = shape->transform();
                    tvg::identity(&matrix);
                    translate(&matrix, (translation / scale + cursor) - Point{textGroupMatrix.e13, textGroupMatrix.e23});
                    tvg::scale(&matrix, scaling * capScale);
                    shape->transform(matrix);
                }

                if (needGroup) {
                    textGroup->push(shape);
                } else {
                    // When text isn't selected, exclude the shape from the text group
                    // Cases with matrix scaling factors =! 1 handled in the 'needGroup' scenario
                    auto& matrix = shape->transform();

                    if (followPath) {
                        identity(&matrix);
                        auto angle = 0.0f;
                        auto halfGlyphWidth = glyph->width * 0.5f;
                        auto position = followPath->position(cursor.x + halfGlyphWidth + firstMargin, angle);
                        matrix.e11 = matrix.e22 = capScale;
                        matrix.e13 = position.x - halfGlyphWidth * matrix.e11;
                        matrix.e23 = position.y - halfGlyphWidth * matrix.e21;
                    } else {
                        matrix.e11 = matrix.e22 = capScale;
                        matrix.e13 = cursor.x;
                        matrix.e23 = cursor.y;
                    }

                    shape->transform(matrix);
                    scene->push(shape);
                }

                p += glyph->len;
                idx += glyph->len;

                //advance the cursor position horizontally
                cursor.x += (glyph->width + doc.tracking) * capScale;

                found = true;
                break;
            }
        }

        if (!found) {
            ++p;
            ++idx;
        }
    }

    delete(scene);
    delete(textGroup);
}


void LottieBuilder::updateMasks(LottieLayer* layer, float frameNo)
{
    if (layer->masks.count == 0) return;

    //Introduce an intermediate scene for embracing the matte + masking
    if (layer->matteTarget) {
        auto scene = Scene::gen();
        scene->push(layer->scene);
        layer->scene = scene;
    }

    Shape* pShape = nullptr;
    MaskMethod pMethod;
    uint8_t pOpacity;

    ARRAY_FOREACH(p, layer->masks) {
        auto mask = *p;
        if (mask->method == MaskMethod::None) continue;

        auto method = mask->method;
        auto opacity = mask->opacity(frameNo);
        auto expand = mask->expand(frameNo);
        auto fastTrack = false;  //single clipping

        //the first mask
        if (!pShape) {
            pShape = layer->pooling();
            pShape->reset();
            auto compMethod = (method == MaskMethod::Subtract || method == MaskMethod::InvAlpha) ? MaskMethod::InvAlpha : MaskMethod::Alpha;
            //Cheaper. Replace the masking with a clipper
            if (layer->masks.count == 1 && compMethod == MaskMethod::Alpha) {
                layer->scene->opacity(MULTIPLY(layer->scene->opacity(), opacity));
                layer->scene->clip(pShape);
                fastTrack = true;
            } else {
                layer->scene->mask(pShape, compMethod);
            }
        //Chain mask composition
        } else if (pMethod != method || pOpacity != opacity || (method != MaskMethod::Subtract && method != MaskMethod::Difference)) {
            auto shape = layer->pooling();
            shape->reset();
            pShape->mask(shape, method);
            pShape = shape;
        }

        pShape->trimpath(0.0f, 1.0f);
        pShape->strokeWidth(0.0f);
        pShape->fill(255, 255, 255, opacity);
        pShape->transform(layer->cache.matrix);

        //Default Masking
        if (expand == 0.0f) {
            mask->pathset(frameNo, SHAPE(pShape)->rs.path, nullptr, tween, exps);
        //Masking with Expansion (Offset)
        } else {
            //TODO: Once path direction support is implemented, ensure that the direction is ignored here
            auto offset = LottieOffsetModifier(expand);
            mask->pathset(frameNo, SHAPE(pShape)->rs.path, nullptr, tween, exps, &offset);
        }

        if (fastTrack) return;

        pOpacity = opacity;
        pMethod = method;
    }
}


bool LottieBuilder::updateMatte(LottieComposition* comp, float frameNo, Scene* scene, LottieLayer* layer)
{
    auto target = layer->matteTarget;
    if (!target) return true;

    updateLayer(comp, scene, target, frameNo);

    if (target->scene) {
        layer->scene->mask(target->scene, layer->matteType);
    } else if (layer->matteType == MaskMethod::Alpha || layer->matteType == MaskMethod::Luma) {
        //matte target is not exist. alpha blending definitely bring an invisible result
        delete(layer->scene);
        layer->scene = nullptr;
        return false;
    }
    return true;
}


void LottieBuilder::updateStrokeEffect(LottieLayer* layer, LottieFxStroke* effect, float frameNo)
{
    if (layer->masks.count == 0) return;

    auto shape = layer->pooling();
    shape->reset();

    //FIXME: all mask
    if (effect->allMask(frameNo)) {
        ARRAY_FOREACH(p, layer->masks) {
            (*p)->pathset(frameNo, SHAPE(shape)->rs.path, nullptr, tween, exps);
        }
    //A specific mask
    } else {
        auto idx = static_cast<uint32_t>(effect->mask(frameNo) - 1);
        if (idx < 0 || idx >= layer->masks.count) return;
        layer->masks[idx]->pathset(frameNo, SHAPE(shape)->rs.path, nullptr, tween, exps);
    }

    shape->transform(layer->cache.matrix);
    shape->trimpath(effect->begin(frameNo) * 0.01f, effect->end(frameNo) * 0.01f);
    shape->strokeFill(255, 255, 255, (int)(effect->opacity(frameNo) * 255.0f));
    shape->strokeJoin(StrokeJoin::Round);
    shape->strokeCap(StrokeCap::Round);

    auto size = effect->size(frameNo) * 2.0f;
    shape->strokeWidth(size);

    //fill the color to the layer shapes if any
    auto color = effect->color(frameNo);
    if (color.rgb[0] != 255 || color.rgb[1] != 255 || color.rgb[2] != 255) {
        auto accessor = tvg::Accessor::gen();
        auto stroke = (layer->type == LottieLayer::Type::Shape) ? true : false;
        auto f = [color, size, stroke](const tvg::Paint* paint, void* data) -> bool {
            if (paint->type() == tvg::Type::Shape) {
                auto shape = (tvg::Shape*) paint;
                //expand shape to fill the stroke region
                if (stroke) {
                    shape->strokeWidth(size);
                    shape->strokeFill(color.rgb[0], color.rgb[1], color.rgb[2], 255);
                }
                shape->fill(color.rgb[0], color.rgb[1], color.rgb[2], 255);
            }
            return true;
        };
        accessor->set(layer->scene, f, nullptr);
        delete(accessor);
    }

    layer->scene->mask(shape, MaskMethod::Alpha);
}


void LottieBuilder::updateEffect(LottieLayer* layer, float frameNo)
{
    constexpr int QUALITY = 25;
    constexpr float BLUR_TO_SIGMA = 0.3f;

    if (layer->effects.count == 0) return;

    ARRAY_FOREACH(p, layer->effects) {
        if (!(*p)->enable) continue;
        switch ((*p)->type) {
            case LottieEffect::Tint: {
                auto effect = static_cast<LottieFxTint*>(*p);
                auto black = effect->black(frameNo);
                auto white = effect->white(frameNo);
                layer->scene->push(SceneEffect::Tint, black.rgb[0], black.rgb[1], black.rgb[2], white.rgb[0], white.rgb[1], white.rgb[2], (double)effect->intensity(frameNo));
                break;
            }
            case LottieEffect::Fill: {
                auto effect = static_cast<LottieFxFill*>(*p);
                auto color = effect->color(frameNo);
                layer->scene->push(SceneEffect::Fill, color.rgb[0], color.rgb[1], color.rgb[2], (int)(255.0f * effect->opacity(frameNo)));
                break;
            }
            case LottieEffect::Stroke: {
                auto effect = static_cast<LottieFxStroke*>(*p);
                updateStrokeEffect(layer, effect, frameNo);
                break;
            }
            case LottieEffect::Tritone: {
                auto effect = static_cast<LottieFxTritone*>(*p);
                auto dark = effect->dark(frameNo);
                auto midtone = effect->midtone(frameNo);
                auto bright = effect->bright(frameNo);
                layer->scene->push(SceneEffect::Tritone, dark.rgb[0], dark.rgb[1], dark.rgb[2], midtone.rgb[0], midtone.rgb[1], midtone.rgb[2], bright.rgb[0], bright.rgb[1], bright.rgb[2]);
                break;
            }
            case LottieEffect::DropShadow: {
                auto effect = static_cast<LottieFxDropShadow*>(*p);
                auto color = effect->color(frameNo);
                //seems the opacity range in dropshadow is 0 ~ 256
                layer->scene->push(SceneEffect::DropShadow, color.rgb[0], color.rgb[1], color.rgb[2], std::min(255, (int)effect->opacity(frameNo)), (double)effect->angle(frameNo), (double)effect->distance(frameNo), (double)(effect->blurness(frameNo) * BLUR_TO_SIGMA), QUALITY);
                break;
            }
            case LottieEffect::GaussianBlur: {
                auto effect = static_cast<LottieFxGaussianBlur*>(*p);
                layer->scene->push(SceneEffect::GaussianBlur, (double)(effect->blurness(frameNo) * BLUR_TO_SIGMA), effect->direction(frameNo) - 1, effect->wrap(frameNo), QUALITY);
                break;
            }
            default: break;
        }
    }
}


void LottieBuilder::updateLayer(LottieComposition* comp, Scene* scene, LottieLayer* layer, float frameNo)
{
    layer->scene = nullptr;

    //visibility
    if (frameNo < layer->inFrame || frameNo >= layer->outFrame) return;

    updateTransform(layer, frameNo);

    //full transparent scene. no need to perform
    if (layer->type != LottieLayer::Null && layer->cache.opacity == 0) return;

    //Prepare render data
    layer->scene = Scene::gen();
    layer->scene->id = layer->id;

    //ignore opacity when Null layer?
    if (layer->type != LottieLayer::Null) layer->scene->opacity(layer->cache.opacity);

    layer->scene->transform(layer->cache.matrix);

    if (!updateMatte(comp, frameNo, scene, layer)) return;

    switch (layer->type) {
        case LottieLayer::Precomp: {
            if (!tweening()) updatePrecomp(comp, layer, frameNo);
            else updatePrecomp(comp, layer, frameNo, tween);
            break;
        }
        case LottieLayer::Solid: {
            updateSolid(layer);
            break;
        }
        case LottieLayer::Image: {
            updateImage(layer);
            break;
        }
        case LottieLayer::Text: {
            updateText(layer, frameNo);
            break;
        }
        default: {
            if (!layer->children.empty()) {
                Inlist<RenderContext> contexts;
                contexts.back(new RenderContext(layer->pooling()));
                updateChildren(layer, frameNo, contexts);
                contexts.free();
            }
            break;
        }
    }

    updateMasks(layer, frameNo);

    layer->scene->blend(layer->blendMethod);

    updateEffect(layer, frameNo);

    if (!layer->matteSrc) scene->push(layer->scene);
}


static void _buildReference(LottieComposition* comp, LottieLayer* layer)
{
    ARRAY_FOREACH(p, comp->assets) {
        if (layer->rid != (*p)->id) continue;
        if (layer->type == LottieLayer::Precomp) {
            auto assetLayer = static_cast<LottieLayer*>(*p);
            if (_buildComposition(comp, assetLayer)) {
                layer->children = assetLayer->children;
                layer->reqFragment = assetLayer->reqFragment;
            }
        } else if (layer->type == LottieLayer::Image) {
            layer->children.push(*p);
        }
        break;
    }
}


static void _buildHierarchy(LottieGroup* parent, LottieLayer* child)
{
    if (child->pidx == -1) return;

    if (child->matteTarget && child->pidx == child->matteTarget->idx) {
        child->parent = child->matteTarget;
        return;
    }

    ARRAY_FOREACH(p, parent->children) {
        auto parent = static_cast<LottieLayer*>(*p);
        if (child == parent) continue;
        if (child->pidx == parent->idx) {
            child->parent = parent;
            break;
        }
        if (parent->matteTarget && parent->matteTarget->idx == child->pidx) {
            child->parent = parent->matteTarget;
            break;
        }
    }
}


static void _attachFont(LottieComposition* comp, LottieLayer* parent)
{
    //TODO: Consider to migrate this attachment to the frame update time.
    ARRAY_FOREACH(p, parent->children) {
        auto text = static_cast<LottieText*>(*p);
        auto& doc = text->doc(0);
        if (!doc.name) continue;
        auto len = strlen(doc.name);
        for (uint32_t i = 0; i < comp->fonts.count; ++i) {
            auto font = comp->fonts[i];
            auto len2 = strlen(font->name);
            if (len == len2 && !strcmp(font->name, doc.name)) {
                text->font = font;
                break;
            }
        }
    }
}


static bool _buildComposition(LottieComposition* comp, LottieLayer* parent)
{
    if (parent->children.count == 0) return false;
    if (parent->buildDone) return true;
    parent->buildDone = true;

    ARRAY_FOREACH(p, parent->children) {
        auto child = static_cast<LottieLayer*>(*p);

        //attach the precomp layer.
        if (child->rid) _buildReference(comp, child);

        if (child->matteType != MaskMethod::None) {
            //no index of the matte layer is provided: the layer above is used as the matte source
            if (child->mid == -1) {
                if (p > parent->children.begin()) {
                    child->matteTarget = static_cast<LottieLayer*>(*(p - 1));
                }
            //matte layer is specified by an index.
            } else child->matteTarget = parent->layerByIdx(child->mid);
        }

        if (child->matteTarget) {
            child->matteTarget->matteSrc = true;
            //parenting
            _buildHierarchy(parent, child->matteTarget);
            //precomp referencing
            if (child->matteTarget->rid) _buildReference(comp, child->matteTarget);
        }
        _buildHierarchy(parent, child);

        //attach the necessary font data
        if (child->type == LottieLayer::Text) _attachFont(comp, child);
    }
    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool LottieBuilder::update(LottieComposition* comp, float frameNo)
{
    if (comp->root->children.empty()) return false;

    comp->clamp(frameNo);

    if (tweening()) {
        comp->clamp(tween.frameNo);
        //tweening is not necessary.
        if (equal(frameNo, tween.frameNo)) offTween();
    }

    //update children layers
    auto root = comp->root;
    root->scene->remove();

    if (exps && comp->expressions) exps->update(comp->timeAtFrame(frameNo));

    ARRAY_REVERSE_FOREACH(child, root->children) {
        auto layer = static_cast<LottieLayer*>(*child);
        if (!layer->matteSrc) updateLayer(comp, root->scene, layer, frameNo);
    }

    return true;
}


void LottieBuilder::build(LottieComposition* comp)
{
    if (!comp) return;

    comp->root->scene = Scene::gen();

    _buildComposition(comp, comp->root);

    if (!update(comp, 0)) return;

    //viewport clip
    auto clip = Shape::gen();
    clip->appendRect(0, 0, comp->w, comp->h);
    comp->root->scene->clip(clip);
}
