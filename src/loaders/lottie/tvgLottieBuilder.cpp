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

#include "tvgMath.h"
#include "tvgShapeImpl.h"
#include "tvgLottieModel.h"
#include "tvgLottieBuilder.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static void _updateChildren(LottieGroup* parent, int32_t frameNo, Shape* baseShape, bool reset);
static void _updateLayer(LottieLayer* root, LottieLayer* layer, int32_t frameNo, bool reset);

static bool _invisible(LottieGroup* group, int32_t frameNo)
{
    auto opacity = group->opacity(frameNo);
    if (group->scene) group->scene->opacity(opacity);
    if (opacity == 0) return true;
    else return false;
}


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

    auto scale = transform->scale(frameNo);
    mathScale(&matrix, scale.x * 0.01f, scale.y * 0.01f);

    auto angle = 0.0f;
    if (autoOrient) angle = transform->position.angle(frameNo);
    mathRotate(&matrix, transform->rotation(frameNo) + angle);

    //Lottie specific anchor transform.
    auto anchor = transform->anchor(frameNo);
    matrix.e13 -= (anchor.x * matrix.e11 + anchor.y * matrix.e12);
    matrix.e23 -= (anchor.x * matrix.e21 + anchor.y * matrix.e22);

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
    _updateTransform(transform, layer->remap(frameNo), layer->autoOrient, matrix, opacity);

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

    auto pmatrix = paint->transform();
    paint->transform(mathMultiply(&pmatrix, &matrix));
    paint->opacity(opacity);
    return nullptr;
}


static Shape* _updateGroup(LottieGroup* parent, LottieGroup* group, int32_t frameNo, Shape* baseShape, bool reset)
{
    //Prepare render data
    if (reset || !group->scene) {
        auto scene = Scene::gen();
        group->scene = scene.get();
        static_cast<Scene*>(parent->scene)->push(std::move(scene));
    } else {
        static_cast<Scene*>(group->scene)->clear();
        reset = true;
    }

    if (_invisible(group, frameNo)) return nullptr;

    if (group->transform) {
        Matrix matrix;
        uint8_t opacity;
        _updateTransform(group->transform, frameNo, false, matrix, opacity);
        group->scene->transform(matrix);
        group->scene->opacity(opacity);
    }
    _updateChildren(group, frameNo, baseShape, reset);
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


static Shape* _updateRect(LottieGroup* parent, LottieRect* rect, int32_t frameNo, Shape* baseShape, Shape* mergingShape)
{
    auto position = rect->position(frameNo);
    auto size = rect->size(frameNo);
    auto roundness = rect->roundness(frameNo);
    if (roundness != 0) {
        if (roundness > size.x * 0.5f)  roundness = size.x * 0.5f;
        if (roundness > size.y * 0.5f)  roundness = size.y * 0.5f;
    }
    if (!mergingShape) {
        auto newShape = cast<Shape>(baseShape->duplicate());
        mergingShape = newShape.get();
        static_cast<Scene*>(parent->scene)->push(std::move(newShape));
    }
    mergingShape->appendRect(position.x - size.x * 0.5f, position.y - size.y * 0.5f, size.x, size.y, roundness, roundness);
    return mergingShape;
}


static Shape* _updateEllipse(LottieGroup* parent, LottieEllipse* ellipse, int32_t frameNo, Shape* baseShape, Shape* mergingShape)
{
    auto position = ellipse->position(frameNo);
    auto size = ellipse->size(frameNo);
    if (!mergingShape) {
        auto newShape = cast<Shape>(baseShape->duplicate());
        mergingShape = newShape.get();
        static_cast<Scene*>(parent->scene)->push(std::move(newShape));
    }
    mergingShape->appendCircle(position.x, position.y, size.x * 0.5f, size.y * 0.5f);
    return mergingShape;
}


static Shape* _updatePath(LottieGroup* parent, LottiePath* path, int32_t frameNo, Shape* baseShape, Shape* mergingShape)
{
    if (!mergingShape) {
        auto newShape = cast<Shape>(baseShape->duplicate());
        mergingShape = newShape.get();
        static_cast<Scene*>(parent->scene)->push(std::move(newShape));
    }
    if (path->pathset(frameNo, mergingShape->pImpl->rs.path.cmds, mergingShape->pImpl->rs.path.pts)) {
        mergingShape->pImpl->update(RenderUpdateFlag::Path);
    }
    return mergingShape;
}


static void _updateChildren(LottieGroup* parent, int32_t frameNo, Shape* baseShape, bool reset)
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
                mergingShape = _updateGroup(parent, static_cast<LottieGroup*>(*child), frameNo, baseShape, reset);
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
                mergingShape = _updateRect(parent, static_cast<LottieRect*>(*child), frameNo, baseShape, mergingShape);
                break;
            }
            case LottieObject::Ellipse: {
                mergingShape = _updateEllipse(parent, static_cast<LottieEllipse*>(*child), frameNo, baseShape, mergingShape);
                break;
            }
            case LottieObject::Path: {
                mergingShape = _updatePath(parent, static_cast<LottiePath*>(*child), frameNo, baseShape, mergingShape);
                break;
            }
            case LottieObject::Polystar: {
                TVGERR("LOTTIE", "TODO: update Polystar");
                break;
            }
            case LottieObject::RoundedCorner: {
                TVGERR("LOTTIE", "TODO: update Round Corner");
                break;
            }
            default: {
                TVGERR("LOTTIE", "TODO: Missing type??");
                break;
            }
        }
    }
    delete(baseShape);
}

static void _updatePrecomp(LottieLayer* precomp, int32_t frameNo, bool reset)
{
    //TODO: skip if the layer is static.
    for (auto child = precomp->children.end() - 1; child >= precomp->children.data; --child) {
        _updateLayer(precomp, static_cast<LottieLayer*>(*child), frameNo, reset);
    }
}


static void _updateLayer(LottieLayer* root, LottieLayer* layer, int32_t frameNo, bool reset)
{
    //Prepare render data
    if (reset || !layer->scene) {
        auto scene = Scene::gen();
        layer->scene = scene.get();
        static_cast<Scene*>(root->scene)->push(std::move(scene));
    }
    //else if (layer->statical) return;   //FIXME: rendering is skipped due to no update?
    else {
        static_cast<Scene*>(layer->scene)->clear();
        reset = true;
    }

    if (_invisible(layer, frameNo)) return;

    _updateTransform(layer, frameNo);

    layer->scene->transform(layer->cache.matrix);

    //FIXME: Ignore opacity when Null layer?
    if (layer->type != LottieLayer::Null) {
        layer->scene->opacity(layer->cache.opacity);
    }

    frameNo = layer->remap(frameNo);

    switch (layer->type) {
        case LottieLayer::Precomp: {
            _updatePrecomp(layer, frameNo, reset);
            break;
        }
        case LottieLayer::Solid: {
            TVGERR("LOTTIE", "TODO: update Solid Layer");
            break;
        }
        case LottieLayer::Image: {
            TVGERR("LOTTIE", "TODO: update Image Layer");
            break;
        }
        default: {
            _updateChildren(layer, frameNo, nullptr, reset);
            break;
        }
    }
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
    }

    //update children layers
    //TODO: skip if the layer is static.
    for (auto child = root->children.end() - 1; child >= root->children.data; --child) {
        _updateLayer(root, static_cast<LottieLayer*>(*child), frameNo, false);
    }
    return true;
}


void LottieBuilder::build(LottieComposition* comp)
{
    if (comp->scene) return;

    comp->scene = Scene::gen().release();
    if (!comp->scene) return;

    //TODO: Process repeater objects?

    if (!update(comp, 0)) return;

    //viewport clip
    auto clip = Shape::gen();
    clip->appendRect(0, 0, static_cast<float>(comp->w), static_cast<float>(comp->h));
    comp->scene->composite(std::move(clip), CompositeMethod::ClipPath);
}