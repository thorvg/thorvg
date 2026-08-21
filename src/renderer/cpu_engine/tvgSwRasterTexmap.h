/*
 * Copyright (c) 2021 - 2026 ThorVG project. All rights reserved.

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

namespace tvg
{

struct Vertex
{
   Point pt;
   Point uv;
};

struct Polygon
{
   Vertex vertex[3];
};

struct TexmapState
{
    // Each textured polygon owns its scan-converter state.
    float dudx, dvdx;
    float dxdya, dxdyb, dudya, dvdya;
    float xa, xb, ua, va;
};


static inline int32_t _modf(float v)
{
    return 255 - ((int(v * 256.0f)) & 255);
}


static uint8_t _feathering(int iru, int irv, int ar, int ab, int sw, int sh)
{
    if (irv == 1) {
        if (iru == 1) return 255 - MULTIPLY(ar, ab);
        else if (iru == sw) return MULTIPLY(ar, 255 - ab);
        return 255 - ab;
    } else if (irv == sh) {
        if (iru == 1) return MULTIPLY(255 - ar, ab);
        else if (iru == sw) return MULTIPLY(ar, ab);
        return ab;
    } else {
        if (iru == 1) return 255 - ar;
        else if (iru == sw) return ar;
    }
    return 255;
}


static bool _rasterMaskedPolygonImageSegment(TexmapState& state, SwSurface* surface, const SwImage& image, const RenderRegion& bbox, int yStart, int yEnd, uint8_t opacity, bool needAA)
{
    TVGERR("SW_ENGINE", "TODO: _rasterMaskedPolygonImageSegment()");
    return false;
}


static void _rasterBlendingPolygonImageSegment(TexmapState& state, SwSurface* surface, const SwImage& image, const RenderRegion& bbox, int yStart, int yEnd, uint8_t opacity, bool needAA)
{
    if (surface->channelSize == sizeof(uint8_t)) {
        TVGERR("SW_ENGINE", "Not supported grayscale Textmap polygon!");
        return;
    }

    auto _dudx = state.dudx, _dvdx = state.dvdx;
    auto _dxdya = state.dxdya, _dxdyb = state.dxdyb, _dudya = state.dudya, _dvdya = state.dvdya;
    auto _xa = state.xa, _xb = state.xb, _ua = state.ua, _va = state.va;
    auto sbuf = image.buf32;
    auto dbuf = surface->buf32;
    auto sw = static_cast<int32_t>(image.w);
    auto sh = static_cast<int32_t>(image.h);
    int32_t x1, x2, x, y, ar, ab, iru, irv, px;
    int32_t vv = 0, uu = 0;
    float dx, u, v;
    uint32_t* buf;

    if (yStart < bbox.min.y) yStart = bbox.min.y;
    if (yEnd > bbox.max.y) yEnd = bbox.max.y;

    y = yStart;

    while (y < yEnd) {
        x1 = std::max((int32_t)_xa, bbox.min.x);
        x2 = std::min((int32_t)_xb, bbox.max.x);

        //Range allowed
        if ((x2 - x1) >= 1 && (x1 < bbox.max.x) && (x2 > bbox.min.x)) {

            //Perform subtexel pre-stepping on UV
            dx = 1 - (_xa - x1);
            u = _ua + dx * _dudx;
            v = _va + dx * _dvdx;
            buf = dbuf + ((y * surface->stride) + x1);
            x = x1;

            //Draw horizontal line
            while (x++ < x2) {
                uu = (int) u;
                vv = (int) v;

                if ((uint32_t) uu >= image.w || (uint32_t) vv >= image.h) continue;

                ar = _modf(u);
                ab = _modf(v);
                iru = uu + 1;
                irv = vv + 1;

                px = *(sbuf + (vv * image.stride) + uu);

                if (image.filter == FilterMethod::Bilinear) {
                    // horizontal interpolate
                    if (iru < sw) {
                        int px2 = *(sbuf + (vv * image.stride) + iru);
                        px = INTERPOLATE(px, px2, ar);
                    }
                    // vertical interpolate
                    if (irv < sh) {
                        int px2 = *(sbuf + (irv * image.stride) + uu);
                        // horizontal interpolate
                        if (iru < sw) {
                            int px3 = *(sbuf + (irv * image.stride) + iru);
                            px2 = INTERPOLATE(px2, px3, ar);
                        }
                        px = INTERPOLATE(px, px2, ab);
                    }
                }

                // anti-aliasing
                if (needAA) {
                    auto feather = _feathering(iru, irv, ar, ab, sw, sh);
                    if (feather < 255) px = ALPHA_BLEND(px, feather);
                }

                *buf = INTERPOLATE(surface->blender(surface, rasterUnpremultiply(px), *buf), *buf, MULTIPLY(opacity, A(px)));
                ++buf;

                //Step UV horizontally
                u += _dudx;
                v += _dvdx;
            }
        }

        //Step along both edges
        _xa += _dxdya;
        _xb += _dxdyb;
        _ua += _dudya;
        _va += _dvdya;

        ++y;
    }
    state.xa = _xa;
    state.xb = _xb;
    state.ua = _ua;
    state.va = _va;
}


static void _rasterPolygonImageSegment32(TexmapState& state, SwSurface* surface, const SwImage& image, const RenderRegion& bbox, int yStart, int yEnd, uint8_t opacity, bool matting, bool needAA)
{
    auto _dudx = state.dudx, _dvdx = state.dvdx;
    auto _dxdya = state.dxdya, _dxdyb = state.dxdyb, _dudya = state.dudya, _dvdya = state.dvdya;
    auto _xa = state.xa, _xb = state.xb, _ua = state.ua, _va = state.va;
    auto sbuf = image.buf32;
    auto dbuf = surface->buf32;
    auto sw = static_cast<int32_t>(image.w);
    auto sh = static_cast<int32_t>(image.h);
    int32_t x1, x2, x, y, ar, ab, iru, irv, px;
    int32_t vv = 0, uu = 0;
    float dx, u, v;
    uint32_t* buf;
    auto fullOpacity = (opacity == 255);

    //for matting(composition)
    auto csize = matting ? surface->compositor->image.channelSize: 0;
    auto alpha = matting ? surface->alpha(surface->compositor->method) : nullptr;
    uint8_t* cmp = nullptr;

    if (yStart < bbox.min.y) yStart = bbox.min.y;
    if (yEnd > bbox.max.y) yEnd = bbox.max.y;

    y = yStart;

    while (y < yEnd) {
        x1 = std::max((int32_t)_xa, bbox.min.x);
        x2 = std::min((int32_t)_xb, bbox.max.x);

        //Range allowed
        if ((x2 - x1) >= 1 && (x1 < bbox.max.x) && (x2 > bbox.min.x)) {

            //Perform subtexel pre-stepping on UV
            dx = 1 - (_xa - x1);
            u = _ua + dx * _dudx;
            v = _va + dx * _dvdx;
            buf = dbuf + ((y * surface->stride) + x1);
            x = x1;

            if (matting) cmp = &surface->compositor->image.buf8[(y * surface->compositor->image.stride + x1) * csize];

            //Draw horizontal line
            while (x++ < x2) {
                uu = (int) u;
                vv = (int) v;

                if ((uint32_t) uu >= image.w || (uint32_t) vv >= image.h) continue;

                ar = _modf(u);
                ab = _modf(v);
                iru = uu + 1;
                irv = vv + 1;

                px = *(sbuf + (vv * image.stride) + uu);

                if (image.filter == FilterMethod::Bilinear) {
                    // horizontal interpolate
                    if (iru < sw) {
                        int px2 = *(sbuf + (vv * image.stride) + iru);
                        px = INTERPOLATE(px, px2, ar);
                    }
                    // vertical interpolate
                    if (irv < sh) {
                        int px2 = *(sbuf + (irv * image.stride) + uu);
                        // horizontal interpolate
                        if (iru < sw) {
                            int px3 = *(sbuf + (irv * image.stride) + iru);
                            px2 = INTERPOLATE(px2, px3, ar);
                        }
                        px = INTERPOLATE(px, px2, ab);
                    }
                }

                uint32_t src;
                if (matting) {
                    auto a = alpha(cmp);
                    src = fullOpacity ? ALPHA_BLEND(px, a) : ALPHA_BLEND(px, MULTIPLY(opacity, a));
                    cmp += csize;
                } else {
                    src = fullOpacity ? px : ALPHA_BLEND(px, opacity);
                }

                // anti-aliasing
                if (needAA) {
                    auto feather = _feathering(iru, irv, ar, ab, sw, sh);
                    if (feather < 255) src = ALPHA_BLEND(src, feather);
                }

                *buf = src + ALPHA_BLEND(*buf, IA(src));
                ++buf;

                //Step UV horizontally
                u += _dudx;
                v += _dvdx;
            }
        }

        //Step along both edges
        _xa += _dxdya;
        _xb += _dxdyb;
        _ua += _dudya;
        _va += _dvdya;

        ++y;
    }
    state.xa = _xa;
    state.xb = _xb;
    state.ua = _ua;
    state.va = _va;
}

// no anti-aliasing, no interpolation for the fastest cheap masking
static void _rasterPolygonImageSegment8(TexmapState& state, SwSurface* surface, const SwImage& image, const RenderRegion& bbox, int yStart, int yEnd, uint8_t opacity, TVG_UNUSED bool needAA)
{
    auto _dudx = state.dudx, _dvdx = state.dvdx;
    auto _dxdya = state.dxdya, _dxdyb = state.dxdyb, _dudya = state.dudya, _dvdya = state.dvdya;
    auto _xa = state.xa, _xb = state.xb, _ua = state.ua, _va = state.va;
    auto sbuf = image.buf32;
    auto dbuf = surface->buf8;
    int32_t x1, x2, x, y;
    float dx, u, v;
    uint8_t* buf;
    uint8_t px;

    if (yStart < bbox.min.y) yStart = bbox.min.y;
    if (yEnd > bbox.max.y) yEnd = bbox.max.y;

    y = yStart;

    while (y < yEnd) {
        x1 = std::max((int32_t)_xa, bbox.min.x);
        x2 = std::min((int32_t)_xb, bbox.max.x);

        //Range allowed
        if ((x2 - x1) >= 1 && (x1 < bbox.max.x) && (x2 > bbox.min.x)) {
            //Perform subtexel pre-stepping on UV
            dx = 1 - (_xa - x1);
            u = _ua + dx * _dudx;
            v = _va + dx * _dvdx;
            buf = dbuf + ((y * surface->stride) + x1);
            x = x1;
            //Draw horizontal line
            while (x++ < x2) {
                auto uu = (int) u;
                auto vv = (int) v;
                if ((uint32_t) uu >= image.w || (uint32_t) vv >= image.h) continue;

                px = A(*(sbuf + (vv * image.stride) + uu));
                *buf = MULTIPLY(px, opacity);
                ++buf;
                //Step UV horizontally
                u += _dudx;
                v += _dvdx;
            }
        }
        //Step along both edges
        _xa += _dxdya;
        _xb += _dxdyb;
        _ua += _dudya;
        _va += _dvdya;
        ++y;
    }
    state.xa = _xa;
    state.xb = _xb;
    state.ua = _ua;
    state.va = _va;
}


static void _rasterPolygonImageSegment(TexmapState& state, SwSurface* surface, const SwImage& image, const RenderRegion& bbox, int yStart, int yEnd, uint8_t opacity, bool matting, bool needAA)
{
    if (surface->channelSize == sizeof(uint32_t)) _rasterPolygonImageSegment32(state, surface, image, bbox, yStart, yEnd, opacity, matting, needAA);
    else if (surface->channelSize == sizeof(uint8_t)) _rasterPolygonImageSegment8(state, surface, image, bbox, yStart, yEnd, opacity, needAA);
}


/* This mapping algorithm is based on Mikael Kalms's. */
static void _rasterPolygonImage(TexmapState& state, SwSurface* surface, const SwImage& image, const RenderRegion& bbox, Polygon& polygon, uint8_t opacity, bool needAA)
{
    float x[3] = {polygon.vertex[0].pt.x, polygon.vertex[1].pt.x, polygon.vertex[2].pt.x};
    float y[3] = {polygon.vertex[0].pt.y, polygon.vertex[1].pt.y, polygon.vertex[2].pt.y};
    float u[3] = {polygon.vertex[0].uv.x, polygon.vertex[1].uv.x, polygon.vertex[2].uv.x};
    float v[3] = {polygon.vertex[0].uv.y, polygon.vertex[1].uv.y, polygon.vertex[2].uv.y};

    float off_y;
    float dxdy[3] = {0.0f, 0.0f, 0.0f};

    auto upper = false;

    //Sort the vertices in ascending Y order
    if (y[0] > y[1]) {
        std::swap(x[0], x[1]);
        std::swap(y[0], y[1]);
        std::swap(u[0], u[1]);
        std::swap(v[0], v[1]);
    }
    if (y[0] > y[2])  {
        std::swap(x[0], x[2]);
        std::swap(y[0], y[2]);
        std::swap(u[0], u[2]);
        std::swap(v[0], v[2]);
    }
    if (y[1] > y[2]) {
        std::swap(x[1], x[2]);
        std::swap(y[1], y[2]);
        std::swap(u[1], u[2]);
        std::swap(v[1], v[2]);
    }

    //Y indexes
    int yi[3] = {(int)y[0], (int)y[1], (int)y[2]};

    //Skip drawing if it's too thin to cover any pixels at all.
    if ((yi[0] == yi[1] && yi[0] == yi[2]) || ((int) x[0] == (int) x[1] && (int) x[0] == (int) x[2])) return;

    //Calculate horizontal and vertical increments for UV axes (these calcs are certainly not optimal, although they're stable (handles any dy being 0)
    auto denom = ((x[2] - x[0]) * (y[1] - y[0]) - (x[1] - x[0]) * (y[2] - y[0]));

    //Skip poly if it's an infinitely thin line
    if (tvg::zero(denom)) return;

    denom = 1 / denom;   //Reciprocal for speeding up
    state.dudx = ((u[2] - u[0]) * (y[1] - y[0]) - (u[1] - u[0]) * (y[2] - y[0])) * denom;
    state.dvdx = ((v[2] - v[0]) * (y[1] - y[0]) - (v[1] - v[0]) * (y[2] - y[0])) * denom;
    auto dudy = ((u[1] - u[0]) * (x[2] - x[0]) - (u[2] - u[0]) * (x[1] - x[0])) * denom;
    auto dvdy = ((v[1] - v[0]) * (x[2] - x[0]) - (v[2] - v[0]) * (x[1] - x[0])) * denom;

    //Calculate X-slopes along the edges
    if (y[1] > y[0]) dxdy[0] = (x[1] - x[0]) / (y[1] - y[0]);
    if (y[2] > y[0]) dxdy[1] = (x[2] - x[0]) / (y[2] - y[0]);
    if (y[2] > y[1]) dxdy[2] = (x[2] - x[1]) / (y[2] - y[1]);

    //Determine which side of the polygon the longer edge is on
    auto side = (dxdy[1] > dxdy[0]) ? true : false;

    if (tvg::equal(y[0], y[1])) side = x[0] > x[1];
    if (tvg::equal(y[1], y[2])) side = x[2] > x[1];

    auto compositing = _compositing(surface);   //Composition required
    auto blending = _blending(surface);         //Blending required

    //Longer edge is on the left side
    if (!side) {
        //Calculate slopes along left edge
        state.dxdya = dxdy[1];
        state.dudya = state.dxdya * state.dudx + dudy;
        state.dvdya = state.dxdya * state.dvdx + dvdy;

        //Perform subpixel pre-stepping along left edge
        auto dy = 1.0f - (y[0] - yi[0]);
        state.xa = x[0] + dy * state.dxdya;
        state.ua = u[0] + dy * state.dudya;
        state.va = v[0] + dy * state.dvdya;

        //Draw upper segment if possibly visible
        if (yi[0] < yi[1]) {
            off_y = y[0] < bbox.min.y ? (bbox.min.y - y[0]) : 0;
            state.xa += (off_y * state.dxdya);
            state.ua += (off_y * state.dudya);
            state.va += (off_y * state.dvdya);

            // Set right edge X-slope and perform subpixel pre-stepping
            state.dxdyb = dxdy[0];
            state.xb = x[0] + dy * state.dxdyb + (off_y * state.dxdyb);

            if (compositing) {
                if (_matting(surface)) _rasterPolygonImageSegment(state, surface, image, bbox, yi[0], yi[1], opacity, true, needAA);
                else _rasterMaskedPolygonImageSegment(state, surface, image, bbox, yi[0], yi[1], opacity, needAA);
            } else if (blending) {
                _rasterBlendingPolygonImageSegment(state, surface, image, bbox, yi[0], yi[1], opacity, needAA);
            } else {
                _rasterPolygonImageSegment(state, surface, image, bbox, yi[0], yi[1], opacity, false, needAA);
            }
            upper = true;
        }
        //Draw lower segment if possibly visible
        if (yi[1] < yi[2]) {
            off_y = y[1] < bbox.min.y ? (bbox.min.y - y[1]) : 0;
            if (!upper) {
                state.xa += (off_y * state.dxdya);
                state.ua += (off_y * state.dudya);
                state.va += (off_y * state.dvdya);
            }
            // Set right edge X-slope and perform subpixel pre-stepping
            state.dxdyb = dxdy[2];
            state.xb = x[1] + (1 - (y[1] - yi[1])) * state.dxdyb + (off_y * state.dxdyb);
            if (compositing) {
                if (_matting(surface)) _rasterPolygonImageSegment(state, surface, image, bbox, yi[1], yi[2], opacity, true, needAA);
                else _rasterMaskedPolygonImageSegment(state, surface, image, bbox, yi[1], yi[2], opacity, needAA);
            } else if (blending) {
                _rasterBlendingPolygonImageSegment(state, surface, image, bbox, yi[1], yi[2], opacity, needAA);
            } else {
                _rasterPolygonImageSegment(state, surface, image, bbox, yi[1], yi[2], opacity, false, needAA);
            }
        }
    //Longer edge is on the right side
    } else {
        //Set right edge X-slope and perform subpixel pre-stepping
        state.dxdyb = dxdy[1];
        auto dy = 1.0f - (y[0] - yi[0]);
        state.xb = x[0] + dy * state.dxdyb;

        //Draw upper segment if possibly visible
        if (yi[0] < yi[1]) {
            off_y = y[0] < bbox.min.y ? (bbox.min.y - y[0]) : 0;
            state.xb += (off_y * state.dxdyb);

            // Set slopes along left edge and perform subpixel pre-stepping
            state.dxdya = dxdy[0];
            state.dudya = state.dxdya * state.dudx + dudy;
            state.dvdya = state.dxdya * state.dvdx + dvdy;

            state.xa = x[0] + dy * state.dxdya + (off_y * state.dxdya);
            state.ua = u[0] + dy * state.dudya + (off_y * state.dudya);
            state.va = v[0] + dy * state.dvdya + (off_y * state.dvdya);

            if (compositing) {
                if (_matting(surface)) _rasterPolygonImageSegment(state, surface, image, bbox, yi[0], yi[1], opacity, true, needAA);
                else _rasterMaskedPolygonImageSegment(state, surface, image, bbox, yi[0], yi[1], opacity, needAA);
            } else if (blending) {
                _rasterBlendingPolygonImageSegment(state, surface, image, bbox, yi[0], yi[1], opacity, needAA);
            } else {
                _rasterPolygonImageSegment(state, surface, image, bbox, yi[0], yi[1], opacity, false, needAA);
            }
            upper = true;
        }
        //Draw lower segment if possibly visible
        if (yi[1] < yi[2]) {
            off_y = y[1] < bbox.min.y ? (bbox.min.y - y[1]) : 0;
            if (!upper) state.xb += (off_y * state.dxdyb);

            // Set slopes along left edge and perform subpixel pre-stepping
            state.dxdya = dxdy[2];
            state.dudya = state.dxdya * state.dudx + dudy;
            state.dvdya = state.dxdya * state.dvdx + dvdy;
            dy = 1 - (y[1] - yi[1]);
            state.xa = x[1] + dy * state.dxdya + (off_y * state.dxdya);
            state.ua = u[1] + dy * state.dudya + (off_y * state.dudya);
            state.va = v[1] + dy * state.dvdya + (off_y * state.dvdya);

            if (compositing) {
                if (_matting(surface)) _rasterPolygonImageSegment(state, surface, image, bbox, yi[1], yi[2], opacity, true, needAA);
                else _rasterMaskedPolygonImageSegment(state, surface, image, bbox, yi[1], yi[2], opacity, needAA);
            } else if (blending) {
                _rasterBlendingPolygonImageSegment(state, surface, image, bbox, yi[1], yi[2], opacity, needAA);
            } else {
                _rasterPolygonImageSegment(state, surface, image, bbox, yi[1], yi[2], opacity, false, needAA);
            }
        }
    }
}

} //namespace


/*
    2 triangles constructs 1 mesh.
    below figure illustrates vert[4] index info.
    If you need better quality, please divide a mesh by more number of triangles.

    0 -- 1
    |  / |
    | /  |
    3 -- 2
*/
bool rasterTexmapPolygon(SwSurface* surface, const SwImage& image, const Matrix& transform, const RenderRegion& bbox, uint8_t opacity)
{
    //Prepare vertices. Shift XY coordinates to match the sub-pixeling technique.
    TexmapState state;
    Vertex vertices[4];
    vertices[0] = {{0.0f, 0.0f}, {0.0f, 0.0f}};
    vertices[1] = {{float(image.w), 0.0f}, {float(image.w), 0.0f}};
    vertices[2] = {{float(image.w), float(image.h)}, {float(image.w), float(image.h)}};
    vertices[3] = {{0.0f, float(image.h)}, {0.0f, float(image.h)}};

    float ys = FLT_MAX, ye = -1.0f;
    for (int i = 0; i < 4; i++) {
        vertices[i].pt *= transform;
        if (vertices[i].pt.y < ys) ys = vertices[i].pt.y;
        if (vertices[i].pt.y > ye) ye = vertices[i].pt.y;
    }

    auto needAA = rightAngle(transform) ? false : true;

    tvg::Polygon polygon;

    //Draw the first polygon
    polygon.vertex[0] = vertices[0];
    polygon.vertex[1] = vertices[1];
    polygon.vertex[2] = vertices[3];

    _rasterPolygonImage(state, surface, image, bbox, polygon, opacity, needAA);

    //Draw the second polygon
    polygon.vertex[0] = vertices[1];
    polygon.vertex[1] = vertices[2];
    polygon.vertex[2] = vertices[3];

    _rasterPolygonImage(state, surface, image, bbox, polygon, opacity, needAA);

#if 0
    if (_compositing(surface) && _masking(surface) && !_direct(surface->compositor->method)) {
        _compositeMaskImage(surface, &surface->compositor->image, surface->compositor->bbox);
    }
#endif
    return true;
}

