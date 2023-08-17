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

static void _updateChildren(LottieGroup* parent, int32_t frameNo, Shape* baseShape);
static void _updateLayer(LottieLayer* root, LottieLayer* layer, int32_t frameNo);
static bool _buildPrecomp(LottieComposition* comp, LottieGroup* parent);

static bool _invisible(LottieGroup* group, int32_t frameNo)
{
    auto opacity = group->opacity(frameNo);
    if (group->scene) group->scene->opacity(opacity);
    if (opacity == 0) return true;
    else return false;
}


static bool _invisible(LottieLayer* layer, int32_t frameNo)
{
    if (frameNo < layer->inFrame || frameNo > layer->outFrame) {
        if (layer->scene) layer->scene->opacity(0);
        return true;
    }

    return _invisible(static_cast<LottieGroup*>(layer), frameNo);
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


static Shape* _updateGroup(LottieGroup* parent, LottieGroup* group, int32_t frameNo, Shape* baseShape)
{
    //Prepare render data
    auto scene = Scene::gen();
    group->scene = scene.get();
    parent->scene->push(std::move(scene));

    if (_invisible(group, frameNo)) return nullptr;

    if (group->transform) {
        Matrix matrix;
        uint8_t opacity;
        _updateTransform(group->transform, frameNo, false, matrix, opacity);
        group->scene->transform(matrix);
        group->scene->opacity(opacity);
    }
    _updateChildren(group, frameNo, baseShape);
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

    //TODO: offset
    if (stroke->dashattr) {
        float dashes[2] = { stroke->dashSize(frameNo), stroke->dashGap(frameNo) };
        baseShape->stroke(dashes, 2);
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
        parent->scene->push(std::move(newShape));
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
        parent->scene->push(std::move(newShape));
    }
    mergingShape->appendCircle(position.x, position.y, size.x * 0.5f, size.y * 0.5f);
    return mergingShape;
}


static Shape* _updatePath(LottieGroup* parent, LottiePath* path, int32_t frameNo, Shape* baseShape, Shape* mergingShape)
{
    if (!mergingShape) {
        auto newShape = cast<Shape>(baseShape->duplicate());
        mergingShape = newShape.get();
        parent->scene->push(std::move(newShape));
    }
    if (path->pathset(frameNo, P(mergingShape)->rs.path.cmds, P(mergingShape)->rs.path.pts)) {
        P(mergingShape)->update(RenderUpdateFlag::Path);
    }
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


static void _updateChildren(LottieGroup* parent, int32_t frameNo, Shape* baseShape)
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
                mergingShape = _updateGroup(parent, static_cast<LottieGroup*>(*child), frameNo, baseShape);
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
    shape->appendRect(0, 0, layer->w, layer->h);
    shape->fill(layer->color.rgb[0], layer->color.rgb[1], layer->color.rgb[2], layer->opacity(frameNo));
    layer->scene->push(std::move(shape));
}


static void _updateMaskings(LottieLayer* layer, int32_t frameNo)
{
    if (layer->masks.count == 0) return;

    //maskings+clipping
    Shape* mergingMask = nullptr;

    for (auto m = layer->masks.data; m < layer->masks.end(); ++m) {
        auto mask = static_cast<LottieMask*>(*m);
        auto shape = Shape::gen().release();
        shape->fill(255, 255, 255, mask->opacity(frameNo));
        shape->transform(layer->cache.matrix);
        if (mask->pathset(frameNo, P(shape)->rs.path.cmds, P(shape)->rs.path.pts)) {
            P(shape)->update(RenderUpdateFlag::Path);
        }
        if (mergingMask) {
            mergingMask->composite(cast<Shape>(shape), mask->method);
        }
        else {
            auto method = mask->method;
            if (method == CompositeMethod::AddMask) method = CompositeMethod::AlphaMask;
            if (method == CompositeMethod::SubtractMask) method = CompositeMethod::InvAlphaMask;
            layer->scene->composite(cast<Shape>(shape), method);
        }
        mergingMask = shape;
    }
}


static void _updateLayer(LottieLayer* root, LottieLayer* layer, int32_t frameNo)
{
    layer->scene = nullptr;

    if (_invisible(layer, frameNo)) return;

    //Prepare render data
    layer->scene = Scene::gen().release();

    _updateTransform(layer, frameNo);

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
            _updateChildren(layer, rFrameNo, nullptr);
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
    if (layer->clipself) {
        //TODO: remove the intermediate scene....
        auto cscene = Scene::gen();
        auto clipper = Shape::gen();
        clipper->appendRect(0, 0, layer->w, layer->h);
        clipper->transform(layer->cache.matrix);
        cscene->composite(move(clipper), CompositeMethod::ClipPath);
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