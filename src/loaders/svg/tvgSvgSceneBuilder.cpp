/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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
#include <math.h>
#include <string>
#include "tvgSvgSceneBuilder.h"
#include "tvgSvgPath.h"

void _appendShape(SvgNode* node, Shape* shape, float vx, float vy, float vw, float vh);

bool _isGroupType(SvgNodeType type)
{
    if (type == SvgNodeType::Doc || type == SvgNodeType::G || type == SvgNodeType::ClipPath) return true;
    return false;
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

         //= T(x - cx, y - cy) x g->transform x T(cx, cy)
         //Calc start point
         g->linear->x1 = (g->transform->e11 * cx) + (g->transform->e12 * cy) + g->linear->x1 + g->transform->e13 - cx;
         g->linear->y1 = (g->transform->e21 * cx) + (g->transform->e22 * cy) + g->linear->y1 + g->transform->e23 - cy;

         //Calc end point
         g->linear->x2 = (g->transform->e11 * cx) + (g->transform->e12 * cy) + g->linear->x2 + g->transform->e13 - cx;
         g->linear->y2 = (g->transform->e21 * cx) + (g->transform->e22 * cy) + g->linear->y2 + g->transform->e23 - cy;
    }

    fillGrad->linear(g->linear->x1, g->linear->y1, g->linear->x2, g->linear->y2);
    fillGrad->spread(g->spread);

    //Update the stops
    stopCount = g->stops.cnt;
    if (stopCount > 0) {
        stops = (Fill::ColorStop*)calloc(stopCount, sizeof(Fill::ColorStop));
        for (uint32_t i = 0; i < g->stops.cnt; ++i) {
            auto colorStop = g->stops.list[i];
            //Use premultiplied color
            stops[i].r = colorStop->r;
            stops[i].g = colorStop->g;
            stops[i].b = colorStop->b;
            stops[i].a = (colorStop->a * fillOpacity) / 255.0f;
            stops[i].offset = colorStop->offset;
        }
        fillGrad->colorStops(stops, stopCount);
        free(stops);
    }
    return fillGrad;
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
    stopCount = g->stops.cnt;
    if (stopCount > 0) {
        stops = (Fill::ColorStop*)calloc(stopCount, sizeof(Fill::ColorStop));
        for (uint32_t i = 0; i < g->stops.cnt; ++i) {
            auto colorStop = g->stops.list[i];
            //Use premultiplied color
            stops[i].r = colorStop->r;
            stops[i].g = colorStop->g;
            stops[i].b = colorStop->b;
            stops[i].a = (colorStop->a * fillOpacity) / 255.0f;
            stops[i].offset = colorStop->offset;
        }
        fillGrad->colorStops(stops, stopCount);
        free(stops);
    }
    return fillGrad;
}

void _appendChildShape(SvgNode* node, Shape* shape, float vx, float vy, float vw, float vh)
{
    _appendShape(node, shape, vx, vy, vw, vh);
    if (node->child.cnt > 0) {
        auto child = node->child.list;
        for (uint32_t i = 0; i < node->child.cnt; ++i, ++child) _appendChildShape(*child, shape, vx, vy, vw, vh);
    }
}

void _applyProperty(SvgNode* node, Shape* vg, float vx, float vy, float vw, float vh)
{
    SvgStyleProperty* style = node->style;

    if (node->transform) vg->transform(*node->transform);
    if (node->type == SvgNodeType::Doc || !node->display) return;

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
        vg->fill(style->r, style->g, style->b, style->fill.opacity);
    } else {
        //Apply the fill color
        vg->fill(style->fill.paint.r, style->fill.paint.g, style->fill.paint.b, style->fill.opacity);
    }

    //Apply node opacity
    if (style->opacity < 255) {
        uint8_t r, g, b, a;
        vg->fillColor(&r, &g, &b, &a);
        vg->fill(r, g, b, (a * style->opacity) / 255.0f);
    }

    if (node->type == SvgNodeType::G) return;

    //Apply the stroke style property
    vg->stroke(style->stroke.width);
    vg->stroke(style->stroke.cap);
    vg->stroke(style->stroke.join);
    if (style->stroke.dash.array.cnt > 0)
        vg->stroke(style->stroke.dash.array.list, style->stroke.dash.array.cnt);

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
        vg->stroke(r, g, b, (a * style->opacity) / 255.0f);
    }

    //Apply composite node
    if (style->comp.node) {
        //Composite ClipPath
        if (((int)style->comp.flags & (int)SvgCompositeFlags::ClipPath)) {
            auto compNode = style->comp.node;
            if (compNode->child.cnt > 0) {
                auto comp = Shape::gen();
                auto child = compNode->child.list;
                for (uint32_t i = 0; i < compNode->child.cnt; ++i, ++child) _appendChildShape(*child, comp.get(), vx, vy, vw, vh);
                vg->composite(move(comp), CompositeMethod::ClipPath);
            }
        }
    }
}

unique_ptr<Shape> _shapeBuildHelper(SvgNode* node, float vx, float vy, float vw, float vh)
{
    auto shape = Shape::gen();
    _appendShape(node, shape.get(), vx, vy, vw, vh);
    return shape;
}

void _appendShape(SvgNode* node, Shape* shape, float vx, float vy, float vw, float vh)
{
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

    _applyProperty(node, shape, vx, vy, vw, vh);
}

unique_ptr<Scene> _sceneBuildHelper(SvgNode* node, float vx, float vy, float vw, float vh, int parentOpacity)
{
    if (_isGroupType(node->type)) {
        auto scene = Scene::gen();
        if (node->transform) scene->transform(*node->transform);
        node->style->opacity = (node->style->opacity * parentOpacity) / 255.0f;

        if (node->display) {
            auto child = node->child.list;
            for (uint32_t i = 0; i < node->child.cnt; ++i, ++child) {
                if (_isGroupType((*child)->type)) {
                    scene->push(_sceneBuildHelper(*child, vx, vy, vw, vh, node->style->opacity));
                } else {
                    (*child)->style->opacity = ((*child)->style->opacity * node->style->opacity) / 255.0f;
                    scene->push(_shapeBuildHelper(*child, vx, vy, vw, vh));
                }
            }
            //Apply composite node
            if (node->style->comp.node) {
                //Composite ClipPath
                if (((int)node->style->comp.flags & (int)SvgCompositeFlags::ClipPath)) {
                    auto compNode = node->style->comp.node;
                    if (compNode->child.cnt > 0) {
                        auto comp = Shape::gen();
                        auto child = compNode->child.list;
                        for (uint32_t i = 0; i < compNode->child.cnt; ++i, ++child) _appendChildShape(*child, comp.get(), vx, vy, vw, vh);
                        scene->composite(move(comp), CompositeMethod::ClipPath);
                    }
                }
            }
        }
        return scene;
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

    return _sceneBuildHelper(node, node->node.doc.vx, node->node.doc.vy, node->node.doc.vw, node->node.doc.vh, 255);
}
