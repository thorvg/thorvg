/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#ifndef _TVG_SVG_SCENE_BUILDER_CPP_
#define _TVG_SVG_SCENE_BUILDER_CPP_


#include "tvgSvgSceneBuilder.h"


unique_ptr<tvg::Shape> _applyProperty(SvgNode* node, unique_ptr<tvg::Shape> vg)
{
    SvgStyleProperty* style = node->style;

    //Apply the transformation
    if (node->transform) vg->transform(*node->transform);

    if (node->type == SvgNodeType::Doc) return vg;

    //If fill property is nullptr then do nothing
    if (style->fill.paint.none) {
        //Do nothing
    } else if (style->fill.paint.gradient) {
        //TODO: Support gradient style
    } else if (style->fill.paint.curColor) {
        //Apply the current style color
        float fa = ((float)style->fill.opacity / 255.0);
        vg->fill(((float)style->r) * fa, ((float)style->g) * fa, ((float)style->b) * fa, style->fill.opacity);
    } else {
        //Apply the fill color
        float fa = ((float)style->fill.opacity / 255.0);
        vg->fill(((float)style->fill.paint.r) * fa, ((float)style->fill.paint.g) * fa, ((float)style->fill.paint.b) * fa, style->fill.opacity);
    }

    //Apply node opacity
    if (style->opacity < 255) {
        uint8_t r, g, b, a;
        vg->fill(&r, &g, &b, &a);
        float fa = ((float)style->opacity / 255.0);
        vg->fill(((float)r) * fa, ((float)g) * fa, ((float)b) * fa, ((float)a) * fa);
    }

    if (node->type == SvgNodeType::G) return vg;

    //Apply the stroke style property
    vg->stroke(style->stroke.width);
    vg->stroke(style->stroke.cap);
    vg->stroke(style->stroke.join);


    //If stroke property is nullptr then do nothing
    if (style->stroke.paint.none) {
        //Do nothing
    } else if (style->stroke.paint.gradient) {
        //TODO: Support gradient style
    } else if (style->stroke.paint.url) {
        //TODO: Apply the color pointed by url
    } else if (style->stroke.paint.curColor) {
        //Apply the current style color
        vg->stroke(style->r, style->g, style->b, style->stroke.opacity);

    } else {
        //Apply the stroke color
        vg->stroke(style->stroke.paint.r, style->stroke.paint.g, style->stroke.paint.b, style->stroke.opacity);
    }

    //Apply node opacity to stroke color
    if (style->opacity < 255) {
        uint8_t r, g, b, a;
        vg->strokeColor(&r, &g, &b, &a);
        float fa = ((float)style->opacity / 255.0);
        vg->stroke(((float)r) * fa, ((float)g) * fa, ((float)b) * fa, ((float)a) * fa);
    }
    return vg;
}


unique_ptr<tvg::Shape> _shapeBuildHelper(SvgNode* node)
{
    auto shape = tvg::Shape::gen();
    switch (node->type) {
        case SvgNodeType::Path: {
            //TODO: Support path
            break;
        }
        case SvgNodeType::Ellipse: {
            //TODO: Support ellipse
            break;
        }
        case SvgNodeType::Polygon: {
            if (node->node.polygon.pointsCount < 2) break;
            shape->moveTo(node->node.polygon.points[0], node->node.polygon.points[1]);
            for (int i = 2; i < node->node.polygon.pointsCount; i += 2) {
                shape->lineTo(node->node.polygon.points[i], node->node.polygon.points[i + 1]);
            }
            shape->close();
            break;
        }
        case SvgNodeType::Polyline: {
            if (node->node.polygon.pointsCount < 2) break;
            shape->moveTo(node->node.polygon.points[0], node->node.polygon.points[1]);
            for (int i = 2; i < node->node.polygon.pointsCount; i += 2) {
                shape->lineTo(node->node.polygon.points[i], node->node.polygon.points[i + 1]);
            }
            break;
        }
        case SvgNodeType::Circle: {
            shape->appendCircle(node->node.circle.cx, node->node.circle.cy, node->node.circle.r, node->node.circle.r);
            break;
        }
        case SvgNodeType::Rect: {
            if (node->node.rect.rx == node->node.rect.ry) {
                 shape->appendRect(node->node.rect.x, node->node.rect.y, node->node.rect.w, node->node.rect.h, node->node.rect.rx);
            }
            else {
                //TODO: Support rounded rectangle
            }
            break;
        }
        case SvgNodeType::Line: {
            shape->moveTo(node->node.line.x1, node->node.line.y1);
            shape->lineTo(node->node.line.x2, node->node.line.y2);
            break;
        }
        default: {
            break;
        }
    }
    shape = move(_applyProperty(node, move(shape)));
    return shape;
}


unique_ptr<tvg::Scene> _sceneBuildHelper(SvgNode* node)
{
    if (node->type == SvgNodeType::Doc || node->type == SvgNodeType::G) {
        auto scene = tvg::Scene::gen();
        if (node->transform) scene->transform(*node->transform);

        for (vector<SvgNode*>::iterator itrChild = node->child.begin(); itrChild != node->child.end(); itrChild++) {
            SvgNode* child = *itrChild;
            if (child->type == SvgNodeType::Doc || child->type == SvgNodeType::G) scene->push(_sceneBuildHelper(*itrChild));
            else scene->push(_shapeBuildHelper(*itrChild));
        }
        return move(scene);
    }
    return nullptr;
}


SvgSceneBuilder::SvgSceneBuilder()
{
}


SvgSceneBuilder::~SvgSceneBuilder()
{
}


unique_ptr<Scene> SvgSceneBuilder::build(SvgNode* node)
{
    if (!node || (node->type != SvgNodeType::Doc)) return nullptr;

    viewBox.x = node->node.doc.vx;
    viewBox.y = node->node.doc.vy;
    viewBox.w = node->node.doc.vw;
    viewBox.h = node->node.doc.vh;
    preserveAspect = node->node.doc.preserveAspect;
    staticViewBox = true;
    return _sceneBuildHelper(node);
}


#endif //_TVG_SVG_SCENE_BUILDER_CPP_
