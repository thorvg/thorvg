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
#include "tvgShapeImpl.h"
#include "tvgLottieModel.h"
#include "tvgLottieBuilder.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static void _updateChildren(LottieGroup* parent, int32_t frameNo, Shape* baseShape, float roundedCorner);
static void _updateLayer(LottieLayer* root, LottieLayer* layer, int32_t frameNo);
static bool _buildPrecomp(LottieComposition* comp, LottieGroup* parent);


static bool _updateTransform(LottieTransform* transform, int32_t frameNo, bool autoOrient, Matrix& matrix, uint8_t& opacity)
{
    if (!transform) return false;

    mathIdentity(&matrix);

    if (transform->coords) {
        mathTranslate(&matrix, transform->coords->x(frameNo), transform->coords->y(frameNo));
    } else {
        auto position = transform->position(frameNo);
        mathTranslate(&matrix, position.x, position.y);
    }

    auto angle = 0.0f;
    if (autoOrient) angle = transform->position.angle(frameNo);
    mathRotate(&matrix, transform->rotation(frameNo) + angle);

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
    uint8_t opacity;

    _updateTransform(transform, frameNo, layer->autoOrient, matrix, opacity);

    if (parent) {
        layer->cache.matrix = mathMultiply(&parent->cache.matrix, &matrix);
        layer->cache.opacity = MULTIPLY(opacity, parent->cache.opacity);
    }
    layer->cache.opacity = opacity;
    layer->cache.frameNo = frameNo;
}


static Shape* _updateTransform(Paint* paint, LottieTransform* transform, int32_t frameNo)
{
    if (!transform) return nullptr;

    Matrix matrix;
    uint8_t opacity;
    if (!_updateTransform(transform, frameNo, false, matrix, opacity)) return nullptr;

    auto pmatrix = P(paint)->transform();
    paint->transform(pmatrix ? mathMultiply(pmatrix, &matrix) : matrix);
    paint->opacity(opacity);

    return nullptr;
}


static Shape* _updateGroup(LottieGroup* parent, LottieGroup* group, int32_t frameNo, Shape* baseShape, float roundedCorner)
{
    //Prepare render data
    group->scene = parent->scene;

    auto opacity = group->opacity(frameNo);
    if (opacity == 0) return nullptr;

    if (group->transform) {
        TVGERR("LOTTIE", "group transform is not working!");
#if 0
        Matrix matrix;
        _updateTransform(group->transform, frameNo, false, matrix, opacity);
        auto pmatrix = P((Paint*)group->scene)->transform();
        group->scene->transform(pmatrix ? mathMultiply(pmatrix, &matrix) : matrix);
        if (opacity < 255) group->scene->opacity(MULTIPLY(opacity, group->scene->opacity()));
#endif
    }

    _updateChildren(group, frameNo, baseShape, roundedCorner);
    return nullptr;
}


static Shape* _updateFill(LottieSolidFill* fill, int32_t frameNo, Shape* baseShape)
{
    if (fill->disabled) return nullptr;
    auto color = fill->color(frameNo);
    baseShape->fill(color.rgb[0], color.rgb[1], color.rgb[2], fill->opacity(frameNo));
    baseShape->fill(fill->rule);
    return nullptr;
}


static Shape* _updateStroke(LottieSolidStroke* stroke, int32_t frameNo, Shape* baseShape)
{
    if (stroke->disabled) return nullptr;
    baseShape->stroke(stroke->width(frameNo));
    auto color = stroke->color(frameNo);
    baseShape->stroke(color.rgb[0], color.rgb[1], color.rgb[2], stroke->opacity(frameNo));
    baseShape->stroke(stroke->cap);
    baseShape->stroke(stroke->join);
    baseShape->strokeMiterlimit(stroke->miterLimit);

    if (stroke->dashattr) {
        float dashes[2] = { stroke->dashSize(frameNo), stroke->dashGap(frameNo) };
        P(baseShape)->strokeDash(dashes, 2, stroke->dashOffset(frameNo));
    } else {
        baseShape->stroke(nullptr, 0);
    }

    return nullptr;
}


static Shape* _updateFill(LottieGradientFill* fill, int32_t frameNo, Shape* baseShape)
{
    baseShape->opacity(fill->opacity(frameNo));
    //TODO: reuse the fill instance?
    baseShape->fill(unique_ptr<Fill>(fill->fill(frameNo)));
    baseShape->fill(fill->rule);
    return nullptr;
}


static Shape* _updateStroke(LottieGradientStroke* stroke, int32_t frameNo, Shape* baseShape)
{
    baseShape->opacity(stroke->opacity(frameNo));
    baseShape->stroke(stroke->width(frameNo));
    baseShape->stroke(unique_ptr<Fill>(stroke->fill(frameNo)));
    baseShape->stroke(stroke->cap);
    baseShape->stroke(stroke->join);
    baseShape->strokeMiterlimit(stroke->miterLimit);
    return nullptr;
}


static Shape* _updateRect(LottieGroup* parent, LottieRect* rect, int32_t frameNo, Shape* baseShape, Shape* mergingShape, float roundedCorner)
{
    auto position = rect->position(frameNo);
    auto size = rect->size(frameNo);
    auto round = rect->radius(frameNo);
    if (roundedCorner > round) round = roundedCorner;

    if (round > 0.0f) {
        if (round > size.x * 0.5f)  round = size.x * 0.5f;
        if (round > size.y * 0.5f)  round = size.y * 0.5f;
    }
    if (!mergingShape) {
        auto newShape = cast<Shape>(baseShape->duplicate());
        mergingShape = newShape.get();
        parent->scene->push(std::move(newShape));
    }
    mergingShape->appendRect(position.x - size.x * 0.5f, position.y - size.y * 0.5f, size.x, size.y, round, round);
    return mergingShape;
}


static Shape* _updateEllipse(LottieGroup* parent, LottieEllipse* ellipse, int32_t frameNo, Shape* baseShape, Shape* mergingShape)
{
    auto position = ellipse->position(frameNo);
    auto size = ellipse->size(frameNo);
    if (!mergingShape) {
        auto newShape = cast<Shape>(baseShape->duplicate());
        mergingShape = newShape.get();
        parent->scene->push(std::move(newShape));
    }
    mergingShape->appendCircle(position.x, position.y, size.x * 0.5f, size.y * 0.5f);
    return mergingShape;
}


static Shape* _updatePath(LottieGroup* parent, LottiePath* path, int32_t frameNo, Shape* baseShape, Shape* mergingShape, float roundedCorner)
{
    if (!mergingShape) {
        auto newShape = cast<Shape>(baseShape->duplicate());
        mergingShape = newShape.get();
        parent->scene->push(std::move(newShape));
    }
    if (path->pathset(frameNo, P(mergingShape)->rs.path.cmds, P(mergingShape)->rs.path.pts)) {
        P(mergingShape)->update(RenderUpdateFlag::Path);
    }

    if (roundedCorner > 1.0f && P(mergingShape)->rs.stroke) {
        TVGERR("LOTTIE", "FIXME: Path roundesss should be applied properly!");
        P(mergingShape)->rs.stroke->join = StrokeJoin::Round;
    }

    return mergingShape;
}


static void _updateStar(LottieGroup* parent, LottiePolyStar* star, Matrix* transform, int32_t frameNo, Shape* mergingShape)
{
    static constexpr auto K_PI = 3.141592f;
    static constexpr auto POLYSTAR_MAGIC_NUMBER = 0.47829f / 0.28f;

    auto ptsCnt = star->ptsCnt(frameNo);
    auto innerRadius = star->innerRadius(frameNo);
    auto outerRadius = star->outerRadius(frameNo);
    auto innerRoundness = star->innerRoundness(frameNo) * 0.01f;
    auto outerRoundness = star->outerRoundness(frameNo) * 0.01f;

    auto angle = -90.0f * K_PI / 180.0f;
    auto partialPointRadius = 0.0f;
    auto anglePerPoint = (2.0f * K_PI / ptsCnt);
    auto halfAnglePerPoint = anglePerPoint * 0.5f;
    auto partialPointAmount = ptsCnt - floorf(ptsCnt);
    auto longSegment = false;
    auto numPoints = size_t(ceilf(ptsCnt) * 2);
    auto direction = star->direction ? 1.0f : -1.0f;
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
        P(mergingShape)->rs.path.pts.reserve(numPoints + 2);
        P(mergingShape)->rs.path.cmds.reserve(numPoints + 3);
    } else {
        P(mergingShape)->rs.path.pts.reserve(numPoints * 3 + 2);
        P(mergingShape)->rs.path.cmds.reserve(numPoints + 3);
        hasRoundness = true;
    }

    Point in = {x, y};
    if (transform) mathTransform(transform, &in);
    mergingShape->moveTo(in.x, in.y);

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
            auto cp1Theta = (atan2f(previousY, previousX) - K_PI / 2.0f * direction);
            auto cp1Dx = cosf(cp1Theta);
            auto cp1Dy = sinf(cp1Theta);
            auto cp2Theta = (atan2f(y, x) - K_PI / 2.0f * direction);
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
            mergingShape->cubicTo(in2.x, in2.y, in3.x, in3.y, in4.x, in4.y);
        } else {
            Point in = {x, y};
            if (transform) mathTransform(transform, &in);
            mergingShape->lineTo(in.x, in.y);
        }
        angle += dTheta * direction;
        longSegment = !longSegment;
    }
    mergingShape->close();
}


static void _updatePolygon(LottieGroup* parent, LottiePolyStar* star, Matrix* transform, int32_t frameNo, Shape* mergingShape)
{
    static constexpr auto POLYGON_MAGIC_NUMBER = 0.25f;
    static constexpr auto K_PI = 3.141592f;

    auto ptsCnt = size_t(floor(star->ptsCnt(frameNo)));
    auto radius = star->outerRadius(frameNo);
    auto roundness = star->outerRoundness(frameNo) * 0.01f;

    auto angle = -90.0f * K_PI / 180.0f;
    auto anglePerPoint = 2.0f * K_PI / float(ptsCnt);
    auto direction = star->direction ? 1.0f : -1.0f;
    auto hasRoundness = false;
    auto x = radius * cosf(angle);
    auto y = radius * sinf(angle);

    angle += anglePerPoint * direction;

    if (mathZero(roundness)) {
        P(mergingShape)->rs.path.pts.reserve(ptsCnt + 2);
        P(mergingShape)->rs.path.cmds.reserve(ptsCnt + 3);
    } else {
        P(mergingShape)->rs.path.pts.reserve(ptsCnt * 3 + 2);
        P(mergingShape)->rs.path.cmds.reserve(ptsCnt + 3);
        hasRoundness = true;
    }

    Point in = {x, y};
    if (transform) mathTransform(transform, &in);
    mergingShape->moveTo(in.x, in.y);

    for (size_t i = 0; i < ptsCnt; i++) {
        auto previousX = x;
        auto previousY = y;
        x = (radius * cosf(angle));
        y = (radius * sinf(angle));

        if (hasRoundness) {
            auto cp1Theta = atan2f(previousY, previousX) - K_PI * 0.5f * direction;
            auto cp1Dx = cosf(cp1Theta);
            auto cp1Dy = sinf(cp1Theta);
            auto cp2Theta = atan2f(y, x) - K_PI * 0.5f * direction;
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
            mergingShape->cubicTo(in2.x, in2.y, in3.x, in3.y, in4.x, in4.y);
        } else {
            Point in = {x, y};
            if (transform) mathTransform(transform, &in);
            mergingShape->lineTo(in.x, in.y);
        }
        angle += anglePerPoint * direction;
    }
    mergingShape->close();
}


static Shape* _updatePolystar(LottieGroup* parent, LottiePolyStar* star, int32_t frameNo, Shape* baseShape, Shape* mergingShape)
{
    if (!mergingShape) {
        auto newShape = cast<Shape>(baseShape->duplicate());
        mergingShape = newShape.get();
        parent->scene->push(std::move(newShape));
    }

    //Optimize: Can we skip the individual coords transform?
    Matrix matrix;
    mathIdentity(&matrix);
    auto position = star->position(frameNo);
    mathTranslate(&matrix, position.x, position.y);
    mathRotate(&matrix, star->rotation(frameNo) * 2.0f);

    auto identity = mathIdentity((const Matrix*)&matrix);

    if (star->type == LottiePolyStar::Star) _updateStar(parent, star, identity ? nullptr : &matrix, frameNo, mergingShape);
    else _updatePolygon(parent, star, identity  ? nullptr : &matrix, frameNo, mergingShape);

    P(mergingShape)->update(RenderUpdateFlag::Path);

    return mergingShape;
}


static void _updateImage(LottieGroup* parent, LottieImage* image, int32_t frameNo, Paint* baseShape)
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

    if (baseShape) {
        if (auto matrix = P(baseShape)->transform()) {
            picture->transform(*matrix);
        }
        picture->opacity(baseShape->opacity());
    }

    //TODO: remove duplicate.
    image->picture = (Picture*)picture->duplicate();

    parent->scene->push(cast<Picture>(picture));
}


static float _updateRoundedCorner(LottieRoundedCorner* roundedCorner, int32_t frameNo, float baseValue)
{
    auto round = roundedCorner->radius(frameNo);
    if (baseValue > round) round = baseValue;
    return round;
}


static void _updateChildren(LottieGroup* parent, int32_t frameNo, Shape* baseShape, float roundedCorner)
{
    if (parent->children.empty()) return;

    //inherits the parent's shape properties.
    baseShape = baseShape ? static_cast<Shape*>(baseShape->duplicate()) : Shape::gen().release();

    //merge shapes if they are continuously appended?
    Shape* mergingShape = nullptr;

    //Draw the parent shapes first
    for (auto child = parent->children.end() - 1; child >= parent->children.data; --child) {
        switch ((*child)->type) {
            case LottieObject::Group: {
                mergingShape = _updateGroup(parent, static_cast<LottieGroup*>(*child), frameNo, baseShape, roundedCorner);
                break;
            }
            case LottieObject::Transform: {
                mergingShape = _updateTransform(baseShape, static_cast<LottieTransform*>(*child), frameNo);
                break;
            }
            case LottieObject::SolidFill: {
                mergingShape = _updateFill(static_cast<LottieSolidFill*>(*child), frameNo, baseShape);
                break;
            }
            case LottieObject::SolidStroke: {
                mergingShape = _updateStroke(static_cast<LottieSolidStroke*>(*child), frameNo, baseShape);
                break;
            }
            case LottieObject::GradientFill: {
                mergingShape = _updateFill(static_cast<LottieGradientFill*>(*child), frameNo, baseShape);
                break;
            }
            case LottieObject::GradientStroke: {
                mergingShape = _updateStroke(static_cast<LottieGradientStroke*>(*child), frameNo, baseShape);
                break;
            }
            case LottieObject::Rect: {
                mergingShape = _updateRect(parent, static_cast<LottieRect*>(*child), frameNo, baseShape, mergingShape, roundedCorner);
                break;
            }
            case LottieObject::Ellipse: {
                mergingShape = _updateEllipse(parent, static_cast<LottieEllipse*>(*child), frameNo, baseShape, mergingShape);
                break;
            }
            case LottieObject::Path: {
                mergingShape = _updatePath(parent, static_cast<LottiePath*>(*child), frameNo, baseShape, mergingShape, roundedCorner);
                break;
            }
            case LottieObject::Polystar: {
                mergingShape = _updatePolystar(parent, static_cast<LottiePolyStar*>(*child), frameNo, baseShape, mergingShape);
                break;
            }
            case LottieObject::RoundedCorner: {
                roundedCorner = _updateRoundedCorner(static_cast<LottieRoundedCorner*>(*child), frameNo, roundedCorner);
                break;
            }
            case LottieObject::Image: {
                _updateImage(parent, static_cast<LottieImage*>(*child), frameNo, baseShape);
                break;
            }
            default: {
                break;
            }
        }
    }
    delete(baseShape);
}


static void _updatePrecomp(LottieLayer* precomp, int32_t frameNo)
{
    if (precomp->children.count == 0) return;

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

    //maskings+clipping
    Shape* mergingMask = nullptr;

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
        if (mergingMask) {
            //false of false is true. invert.
            if (method == CompositeMethod::SubtractMask && pmethod == method) {
                method = CompositeMethod::AddMask;
            } else if (pmethod == CompositeMethod::DifferenceMask && pmethod == method) {
                method = CompositeMethod::IntersectMask;
            }
            mergingMask->composite(cast<Shape>(shape), method);
        } else {
            if (method == CompositeMethod::SubtractMask) method = CompositeMethod::InvAlphaMask;
            else if (method == CompositeMethod::AddMask) method = CompositeMethod::AlphaMask;
            else if (method == CompositeMethod::IntersectMask) method = CompositeMethod::AlphaMask;
            else if (method == CompositeMethod::DifferenceMask) method = CompositeMethod::AlphaMask;   //does this correct?
            layer->scene->composite(cast<Shape>(shape), method);
        }
        pmethod = mask->method;
        mergingMask = shape;
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
            _updateChildren(layer, rFrameNo, nullptr, 0);
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
#if 0
    //clip the layer viewport
    if (layer->clipself) {
        //TODO: remove the intermediate scene....
        auto cscene = Scene::gen();
        auto clipper = Shape::gen();
        clipper->appendRect(0, 0, layer->w, layer->h);
        clipper->transform(layer->cache.matrix);
        cscene->composite(std::move(clipper), CompositeMethod::ClipPath);
        cscene->push(cast<Scene>(layer->scene));
        layer->scene = cscene.release();
    }
#endif
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


static void _buildSize(LottieComposition* comp, LottieLayer* layer)
{
    // default size is 0x0
    if (layer->w == 0 || layer->h == 0) return;

    //compact layer size
    if (layer->w > comp->w) layer->w = comp->w;
    if (layer->h > comp->h) layer->h = comp->h;

    if (layer->w < comp->w || layer->h < comp->h) {
        layer->clipself = true;
    }
}


static bool _buildPrecomp(LottieComposition* comp, LottieGroup* parent)
{
    if (parent->children.count == 0) return false;

    for (auto c = parent->children.data; c < parent->children.end(); ++c) {
        auto child = static_cast<LottieLayer*>(*c);

        //compact layer size
        _buildSize(comp, child);

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