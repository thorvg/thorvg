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


static void _getTransformationData(Matrix* m, float* tx, float* ty, float* s, float* z)
{
    float rz, si, cs, zcs, zsi;

    *tx = m->e13;
    *ty = m->e23;

    cs = m->e11;
    si = m->e21;
    rz = atan2(si, cs);
    *z = rz * (180.0f / M_PI);
    zcs = cosf(-1.0f * rz);
    zsi = sinf(-1.0f * rz);
    m->e11 = m->e11 * zcs + m->e12 * zsi;
    m->e22 = m->e21 * (-1 * zsi) + m->e22 * zcs;
    *s = m->e11 > m->e22 ? m->e11 : m->e22;
}


unique_ptr<LinearGradient> _applyLinearGradientProperty(SvgStyleGradient* g, Shape* vg, float rx, float ry, float rw, float rh)
{
    Fill::ColorStop* stops;
    int stopCount = 0;
    float fillOpacity = 255.0f;
    float gx, gy, gw, gh;

    auto fillGrad = LinearGradient::gen();

    if (g->usePercentage) {
        g->linear->x1 = g->linear->x1 * rw + rx;
        g->linear->y1 = g->linear->y1 * rh + ry;
        g->linear->x2 = g->linear->x2 * rw + rx;
        g->linear->y2 = g->linear->y2 * rh + ry;
    }

    //In case of objectBoundingBox it need proper scaling
    if (!g->userSpace) {
        float scaleX = 1.0, scaleReversedX = 1.0;
        float scaleY = 1.0, scaleReversedY = 1.0;

        //Check the smallest size, find the scale value
        if (rh > rw) {
            scaleY = ((float)rw) / rh;
            scaleReversedY = ((float)rh) / rw;
        } else {
            scaleX = ((float)rh) / rw;
            scaleReversedX = ((float)rw) / rh;
        }

        vg->bounds(&gx, &gy, &gw, &gh);

        float cy = ((float)gh) * 0.5 + gy;
        float cy_scaled = (((float)gh) * 0.5) * scaleReversedY;
        float cx = ((float)gw) * 0.5 + gx;
        float cx_scaled = (((float)gw) * 0.5) * scaleReversedX;

        //= T(gx, gy) x S(scaleX, scaleY) x T(cx_scaled - cx, cy_scaled - cy) x (radial->x, radial->y)
        g->linear->x1 = g->linear->x1 * scaleX + scaleX * (cx_scaled - cx) + gx;
        g->linear->y1 = g->linear->y1 * scaleY + scaleY * (cy_scaled - cy) + gy;
        g->linear->x2 = g->linear->x2 * scaleX + scaleX * (cx_scaled - cx) + gx;
        g->linear->y2 = g->linear->y2 * scaleY + scaleY * (cy_scaled - cy) + gy;
    }

    if (g->transform) {
         float cy = ((float) rh) * 0.5 + ry;
         float cx = ((float) rw) * 0.5 + rx;

         //Calc start point
         //= T(x - cx, y - cy) x g->transform x T(cx, cy)
         g->linear->x1 = cx * (g->transform->e11 + g->transform->e31 * (g->linear->x1 - cx)) +
                         cx * (g->transform->e12 + g->transform->e32 * (g->linear->x1 - cx)) +
                         cx * (g->transform->e13 + g->transform->e33 * (g->linear->x1 - cx));

         g->linear->y1 = cy * (g->transform->e21 + g->transform->e31 * (g->linear->y1 - cy)) +
                         cy * (g->transform->e22 + g->transform->e32 * (g->linear->y1 - cy)) +
                         cy * (g->transform->e23 + g->transform->e33 * (g->linear->y1 - cy));

         //Calc end point
         g->linear->x2 = cx * (g->transform->e11 + g->transform->e31 * (g->linear->x2 - cx)) +
                         cx * (g->transform->e12 + g->transform->e32 * (g->linear->x2 - cx)) +
                         cx * (g->transform->e13 + g->transform->e33 * (g->linear->x2 - cx));

         g->linear->y2 = cy * (g->transform->e21 + g->transform->e31 * (g->linear->y2 - cy)) +
                         cy * (g->transform->e22 + g->transform->e32 * (g->linear->y2 - cy)) +
                         cy * (g->transform->e23 + g->transform->e33 * (g->linear->y2 - cy));
    }

    fillGrad->linear(g->linear->x1, g->linear->y1, g->linear->x2, g->linear->y2);
    fillGrad->spread(g->spread);

    //Update the stops
    stopCount = g->stops.size();
    if (stopCount > 0) {
        float opacity;
        float fopacity = fillOpacity / 255.0f; //fill opacity if any exists.
        int i = 0;
        stops = (Fill::ColorStop*)calloc(stopCount, sizeof(Fill::ColorStop));
        for (vector<Fill::ColorStop*>::iterator itrStop = g->stops.begin(); itrStop != g->stops.end(); itrStop++) {
            //Use premultiplied color
            opacity = ((float)(*itrStop)->a / 255) * fopacity;
            stops[i].r = ((*itrStop)->r * opacity);
            stops[i].g = ((*itrStop)->g * opacity);
            stops[i].b = ((*itrStop)->b * opacity);
            stops[i].a = ((*itrStop)->a * fopacity);
            stops[i].offset = (*itrStop)->offset;
            i++;
        }
        fillGrad->colorStops(stops, stopCount);
        free(stops);
    }
    return move(fillGrad);
}


unique_ptr<RadialGradient> _applyRadialGradientProperty(SvgStyleGradient* g, Shape* vg, float rx, float ry, float rw, float rh)
{
    Fill::ColorStop *stops;
    int stopCount = 0;
    float gx, gy, gw, gh;
    int radius;
    float fillOpacity = 255.0f;

    auto fillGrad = RadialGradient::gen();

    radius = sqrt(pow(rw, 2) + pow(rh, 2)) / sqrt(2.0);
    if (!g->userSpace) {
         //That is according to Units in here
         //https://www.w3.org/TR/2015/WD-SVG2-20150915/coords.html
        int min = (rh > rw) ? rw : rh;
        radius = sqrt(pow(min, 2) + pow(min, 2)) / sqrt(2.0);
    }

    if (g->usePercentage) {
        g->radial->cx = g->radial->cx * rw + rx;
        g->radial->cy = g->radial->cy * rh + ry;
        g->radial->r = g->radial->r * radius;
        g->radial->fx = g->radial->fx * rw + rx;
        g->radial->fy = g->radial->fy * rh + ry;
    }

    //In case of objectBoundingBox it need proper scaling
    if (!g->userSpace) {
        float scaleX = 1.0, scaleReversedX = 1.0;
        float scaleY = 1.0, scaleReversedY = 1.0;

        //Check the smallest size, find the scale value
        if (rh > rw) {
            scaleY = ((float)rw) / rh;
            scaleReversedY = ((float)rh) / rw;
        } else {
            scaleX = ((float)rh) / rw;
            scaleReversedX = ((float)rw) / rh;
        }

        vg->bounds(&gx, &gy, &gw, &gh);

        float cy = ((float)gh) * 0.5 + gy;
        float cy_scaled = (((float)gh) * 0.5) * scaleReversedY;
        float cx = ((float)gw) * 0.5 + gx;
        float cx_scaled = (((float)gw) * 0.5) * scaleReversedX;

         //= T(gx, gy) x S(scaleX, scaleY) x T(cx_scaled - cx, cy_scaled - cy) x (radial->x, radial->y)
        g->radial->cx = g->radial->cx * scaleX + scaleX * (cx_scaled - cx) + gx;
        g->radial->cy = g->radial->cy * scaleY + scaleY * (cy_scaled - cy) + gy;
    }

    //TODO: Radial gradient transformation is not yet supported.
    //if (g->transform) {}

    //TODO: Tvg is not support to focal
    //if (g->radial->fx != 0 && g->radial->fy != 0) {
    //    fillGrad->radial(g->radial->fx, g->radial->fy, g->radial->r);
    //}
    fillGrad->radial(g->radial->cx, g->radial->cy, g->radial->r);
    fillGrad->spread(g->spread);

    //Update the stops
    stopCount = g->stops.size();
    if (stopCount > 0) {
        float opacity;
        float fopacity = fillOpacity / 255.0f; //fill opacity if any exists.
        int i = 0;
        stops = (Fill::ColorStop*)calloc(stopCount, sizeof(Fill::ColorStop));
        for (vector<Fill::ColorStop*>::iterator itrStop = g->stops.begin(); itrStop != g->stops.end(); itrStop++) {
            //Use premultiplied color
            opacity = ((float)(*itrStop)->a / 255) * fopacity;
            stops[i].r = ((*itrStop)->r * opacity);
            stops[i].g = ((*itrStop)->g * opacity);
            stops[i].b = ((*itrStop)->b * opacity);
            stops[i].a = ((*itrStop)->a * fopacity);
            stops[i].offset = (*itrStop)->offset;
            i++;
        }
        fillGrad->colorStops(stops, stopCount);
        free(stops);
    }
    return move(fillGrad);
}


void _applyProperty(SvgNode* node, Shape* vg, float vx, float vy, float vw, float vh)
{
    SvgStyleProperty* style = node->style;

    //Apply the transformation
    if (node->transform) {
         float tx = 0, ty = 0, s = 0, z = 0;
         _getTransformationData(node->transform, &tx, &ty, &s, &z);
         if (!(fabsf(s - 1) <= FLT_EPSILON)) vg->scale(s);
         if (!(fmod(fabsf(z), 360.0) <= FLT_EPSILON)) vg->rotate(fmod(z, 360.0));
         if (!(fabsf(tx) <= FLT_EPSILON) && !(fabsf(ty) <= FLT_EPSILON)) vg->translate(tx, ty);
    }

    if (node->type == SvgNodeType::Doc) return;

    //If fill property is nullptr then do nothing
    if (style->fill.paint.none) {
        //Do nothing
    } else if (style->fill.paint.gradient) {
         if (!style->fill.paint.gradient->userSpace) vg->bounds(&vx, &vy, &vw, &vh);

        if (style->fill.paint.gradient->type == SvgGradientType::Linear) {
             auto linear = _applyLinearGradientProperty(style->fill.paint.gradient, vg, vx, vy, vw, vh);
             vg->fill(move(linear));
        } else if (style->fill.paint.gradient->type == SvgGradientType::Radial) {
             auto radial = _applyRadialGradientProperty(style->fill.paint.gradient, vg, vx, vy, vw, vh);
             vg->fill(move(radial));
        }
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

    if (node->type == SvgNodeType::G) return;

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
}


unique_ptr<Shape> _shapeBuildHelper(SvgNode* node, float vx, float vy, float vw, float vh)
{
    auto shape = Shape::gen();
    switch (node->type) {
        case SvgNodeType::Path: {
            if (node->node.path.path) {
                auto pathResult = svgPathToTvgPath(node->node.path.path->c_str());
                shape->appendPath(get<0>(pathResult).data(), get<0>(pathResult).size(), get<1>(pathResult).data(), get<1>(pathResult).size());
            }
            break;
        }
        case SvgNodeType::Ellipse: {
            shape->appendCircle(node->node.ellipse.cx, node->node.ellipse.cy, node->node.ellipse.rx, node->node.ellipse.ry);
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
            shape->appendRect(node->node.rect.x, node->node.rect.y, node->node.rect.w, node->node.rect.h, node->node.rect.rx, node->node.rect.ry);
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
    _applyProperty(node, shape.get(), vx, vy, vw, vh);
    return shape;
}


unique_ptr<Scene> _sceneBuildHelper(SvgNode* node, float vx, float vy, float vw, float vh, int parentOpacity)
{
    if (node->type == SvgNodeType::Doc || node->type == SvgNodeType::G) {
        auto scene = Scene::gen();
        if (node->transform) {
            float tx = 0, ty = 0, s = 0, z = 0;
            _getTransformationData(node->transform, &tx, &ty, &s, &z);
            if (!(fabsf(s - 1) <= FLT_EPSILON)) scene->scale(s);
            if (!(fmod(fabsf(z), 360.0) <= FLT_EPSILON)) scene->rotate(fmod(z, 360.0));
            if (!(fabsf(tx) <= FLT_EPSILON) && !(fabsf(ty) <= FLT_EPSILON)) scene->translate(tx, ty);
        }
        node->style->opacity = (node->style->opacity * parentOpacity) / 255;
        for (vector<SvgNode*>::iterator itrChild = node->child.begin(); itrChild != node->child.end(); itrChild++) {
            SvgNode* child = *itrChild;
            child->style->opacity = (child->style->opacity * node->style->opacity) / 255;
            if (child->type == SvgNodeType::Doc || child->type == SvgNodeType::G) scene->push(_sceneBuildHelper(*itrChild, vx, vy, vw, vh, node->style->opacity));
            else scene->push(_shapeBuildHelper(*itrChild, vx, vy, vw, vh));
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
    return _sceneBuildHelper(node, viewBox.x, viewBox.y, viewBox.w, viewBox.h, 255);
}


#endif //_TVG_SVG_SCENE_BUILDER_CPP_
