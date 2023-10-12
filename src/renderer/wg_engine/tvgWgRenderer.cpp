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

#include "tvgWgRenderer.h"

WgRenderer::WgRenderer() {
    initialize();
}

WgRenderer::~WgRenderer() {
    release();
}

void WgRenderer::initialize() {
    TVGERR("WG_ENGINE", "TODO: WgRenderer::initialize()");
}

void WgRenderer::release() {
    TVGERR("WG_ENGINE", "TODO: WgRenderer::release()");
}

RenderData WgRenderer::prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper) {
    return nullptr;
}

RenderData WgRenderer::prepare(const Array<RenderData>& scene, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) {
    return nullptr;
}

RenderData WgRenderer::prepare(Surface* surface, const RenderMesh* mesh, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) {
    return nullptr;
}

bool WgRenderer::preRender() {
    return true;
}

bool WgRenderer::renderShape(RenderData data) {
    return true;
}

bool WgRenderer::renderImage(RenderData data) {
    return true;
}

bool WgRenderer::postRender() {
    return true;
}

bool WgRenderer::dispose(RenderData data) {
    return true;
}

RenderRegion WgRenderer::region(RenderData data) {
    return { 0, 0, INT32_MAX, INT32_MAX };
}

RenderRegion WgRenderer::viewport() {
    return { 0, 0, INT32_MAX, INT32_MAX };
}

bool WgRenderer::viewport(const RenderRegion& vp) {
    return true;
}

bool WgRenderer::blend(BlendMethod method) {
    return false;
}

ColorSpace WgRenderer::colorSpace() {
    return ColorSpace::Unsupported;
}

bool WgRenderer::clear() {
    return true;
}

bool WgRenderer::sync() {
    return true;
}

bool WgRenderer::target(void* window, uint32_t w, uint32_t h) {
    return true;
}

Compositor* WgRenderer::target(const RenderRegion& region, ColorSpace cs) {
    return nullptr;
}

bool WgRenderer::beginComposite(Compositor* cmp, CompositeMethod method, uint8_t opacity) {
    return false;
}

bool WgRenderer::endComposite(Compositor* cmp) {
    return false;
}

WgRenderer* WgRenderer::gen() {
    return new WgRenderer();
}

bool WgRenderer::init(uint32_t threads) {
    return true;
}

bool WgRenderer::term() {
    return true;
}
