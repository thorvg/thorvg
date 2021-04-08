/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All rights reserved.

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

bool _appendShape(SvgNode* node, Shape* shape, float vx, float vy, float vw, float vh);

bool _isGroupType(SvgNodeType type)
{
    if (type == SvgNodeType::Doc || type == SvgNodeType::G || type == SvgNodeType::ClipPath) return true;
    return false;
}

unique_ptr<LinearGradient> _applyLinearGradientProperty(SvgStyleGradient* g, const Shape* vg, float rx, float ry, float rw, float rh)
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
        //Calc start point
        auto x = g->linear->x1;
        g->linear->x1 = x * g->transform->e11 + g->linear->y1 * g->transform->e12 + g->transform->e13;
        g->linear->y1 = x * g->transform->e21 + g->linear->y1 * g->transform->e22 + g->transform->e23;

        //Calc end point
        x = g->linear->x2;
        g->linear->x2 = x * g->transform->e11 + g->linear->y2 * g->transform->e12 + g->transform->e13;
        g->linear->y2 = x * g->transform->e21 + g->linear->y2 * g->transform->e22 + g->transform->e23;
    }

    fillGrad->linear(g->linear->x1, g->linear->y1, g->linear->x2, g->linear->y2);
    fillGrad->spread(g->spread);

    //Update the stops
    stopCount = g->stops.count;
    if (stopCount > 0) {
        stops = (Fill::ColorStop*)calloc(stopCount, sizeof(Fill::ColorStop));
        if (!stops) return fillGrad;
        for (uint32_t i = 0; i < g->stops.count; ++i) {
            auto colorStop = g->stops.data[i];
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


unique_ptr<RadialGradient> _applyRadialGradientProperty(SvgStyleGradient* g, const Shape* vg, float rx, float ry, float rw, float rh)
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
    stopCount = g->stops.count;
    if (stopCount > 0) {
        stops = (Fill::ColorStop*)calloc(stopCount, sizeof(Fill::ColorStop));
        if (!stops) return fillGrad;
        for (uint32_t i = 0; i < g->stops.count; ++i) {
            auto colorStop = g->stops.data[i];
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
    if (node->child.count > 0) {
        auto child = node->child.data;
        for (uint32_t i = 0; i < node->child.count; ++i, ++child) _appendChildShape(*child, shape, vx, vy, vw, vh);
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

    //Apply the fill rule
    vg->fill((tvg::FillRule)style->fill.fillRule);

    //Apply node opacity
    if (style->opacity < 255) vg->opacity(style->opacity);

    if (node->type == SvgNodeType::G) return;

    //Apply the stroke style property
    vg->stroke(style->stroke.width);
    vg->stroke(style->stroke.cap);
    vg->stroke(style->stroke.join);
    if (style->stroke.dash.array.count > 0) {
        vg->stroke(style->stroke.dash.array.data, style->stroke.dash.array.count);
    }

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

    //Apply composite node
    if (style->comp.node && (style->comp.method != CompositeMethod::None)) {
        auto compNode = style->comp.node;
        if (compNode->child.count > 0) {
            auto comp = Shape::gen();
            auto child = compNode->child.data;
            for (uint32_t i = 0; i < compNode->child.count; ++i, ++child) _appendChildShape(*child, comp.get(), vx, vy, vw, vh);
            if (style->comp.method == CompositeMethod::ClipPath) comp->fill(0, 0, 0, 255);
            vg->composite(move(comp), style->comp.method);
        }
    }
}

unique_ptr<Shape> _shapeBuildHelper(SvgNode* node, float vx, float vy, float vw, float vh)
{
    auto shape = Shape::gen();
    if (_appendShape(node, shape.get(), vx, vy, vw, vh)) return shape;
    else return nullptr;
}

bool _appendShape(SvgNode* node, Shape* shape, float vx, float vy, float vw, float vh)
{
    Array<PathCommand> cmds;
    Array<Point> pts;

    switch (node->type) {
        case SvgNodeType::Path: {
            if (node->node.path.path) {
                if (svgPathToTvgPath(node->node.path.path->c_str(), cmds, pts)) {
                    shape->appendPath(cmds.data, cmds.count, pts.data, pts.count);
                }
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
            return false;
        }
    }

    _applyProperty(node, shape, vx, vy, vw, vh);
    return true;
}

unique_ptr<Scene> _sceneBuildHelper(const SvgNode* node, float vx, float vy, float vw, float vh)
{
    if (_isGroupType(node->type)) {
        auto scene = Scene::gen();
        if (node->transform) scene->transform(*node->transform);

        if (node->display && node->style->opacity != 0) {
            auto child = node->child.data;
            for (uint32_t i = 0; i < node->child.count; ++i, ++child) {
                if (_isGroupType((*child)->type)) {
                    scene->push(_sceneBuildHelper(*child, vx, vy, vw, vh));
                } else {
                    auto shape = _shapeBuildHelper(*child, vx, vy, vw, vh);
                    if (shape) scene->push(move(shape));
                }
            }
            //Apply composite node
            if (node->style->comp.node && (node->style->comp.method != CompositeMethod::None)) {
                auto compNode = node->style->comp.node;
                if (compNode->child.count > 0) {
                    auto comp = Shape::gen();
                    auto child = compNode->child.data;
                    for (uint32_t i = 0; i < compNode->child.count; ++i, ++child) _appendChildShape(*child, comp.get(), vx, vy, vw, vh);
                    if (node->style->comp.method == CompositeMethod::ClipPath) comp->fill(0, 0, 0, 255);
                    scene->composite(move(comp), node->style->comp.method);
                }
            }
            scene->opacity(node->style->opacity);
        }
        return scene;
    }
    return nullptr;
}

unique_ptr<Scene> _buildRoot(const SvgNode* node, float vx, float vy, float vw, float vh)
{
    unique_ptr<Scene> root;
    auto docNode = _sceneBuildHelper(node, vx, vy, vw, vh);
    float x, y, w, h;

    if (docNode->bounds(&x, &y, &w, &h) != Result::Success) return nullptr;

    if (x < vx || y < vy || w > vh || h > vh) {
        auto viewBoxClip = Shape::gen();
        viewBoxClip->appendRect(vx, vy ,vw, vh, 0, 0);
        viewBoxClip->fill(0, 0, 0, 255);

        auto compositeLayer = Scene::gen();
        compositeLayer->composite(move(viewBoxClip), CompositeMethod::ClipPath);
        compositeLayer->push(move(docNode));

        root = Scene::gen();
        root->push(move(compositeLayer));
    } else {
        root = move(docNode);
    }
    return root;
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

    return _buildRoot(node, node->node.doc.vx, node->node.doc.vy, node->node.doc.vw, node->node.doc.vh);
}
