/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.

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
{
    float _dudx = dudx, _dvdx = dvdx;
    float _dxdya = dxdya, _dxdyb = dxdyb, _dudya = dudya, _dvdya = dvdya;
    float _xa = xa, _xb = xb, _ua = ua, _va = va;
    auto sbuf = image->data;
    auto dbuf = surface->buffer;
    int sw = static_cast<int>(image->stride);
    int sh = image->h;
    int dw = surface->stride;
    int x1, x2, x, y, ar, ab, iru, irv, px;
    int vv = 0;
    int uu = 0;
    float dx, u, v, iptr;
    uint32_t* buf;
#ifdef TEXMAP_MASKING
    uint32_t* cmp;
#endif

    //Range exception handling
    if (ystart >= region.max.y) return;
    if (ystart < region.min.y) ystart = region.min.y;
    if (yend > region.max.y) yend = region.max.y;

    //Loop through all lines in the segment
    y = ystart;

    while (y < yend) {
        x1 = _xa;
        x2 = _xb;

        //Range exception handling
        if (x1 < region.min.x) x1 = region.min.x;
        if (x2 > region.max.x) x2 = region.max.x;

        if ((x2 - x1) < 1) goto next;
        if ((x1 >= region.max.x) || (x2 <= region.min.x)) goto next;

        //Perform subtexel pre-stepping on UV
        dx = 1 - (_xa - x1);
        u = _ua + dx * _dudx;
        v = _va + dx * _dvdx;

        buf = dbuf + ((y * dw) + x1);

        x = x1;

#ifdef TEXMAP_MASKING
        cmp = &surface->compositor->image.data[y * surface->compositor->image.stride + x1];
#endif
        //Draw horizontal line
        while (x++ < x2) {
            uu = (int) u;
            vv = (int) v;
            /* FIXME: sometimes u and v are < 0 - don'tc crash */
            if (uu < 0) uu = 0;
            if (vv < 0) vv = 0;

            /* Range exception handling */
            /* OPTIMIZE ME, handle in advance? */
            if (uu >= sw) uu = sw - 1;
            if (vv >= sh) vv = sh - 1;

            ar = (int)(255 * (1 - modff(u, &iptr)));
            ab = (int)(255 * (1 - modff(v, &iptr)));
            iru = uu + 1;
            irv = vv + 1;
            px = *(sbuf + (vv * sw) + uu);

            /* horizontal interpolate */
            if (iru < sw) {
                /* right pixel */
                int px2 = *(sbuf + (vv * sw) + iru);
                px = INTERPOLATE(ar, px, px2);
            }
            /* vertical interpolate */
            if (irv < sh) {
                /* bottom pixel */
                int px2 = *(sbuf + (irv * sw) + uu);

                /* horizontal interpolate */
                if (iru < sw) {
                    /* bottom right pixel */
                    int px3 = *(sbuf + (irv * sw) + iru);
                    px2 = INTERPOLATE(ar, px2, px3);
                }
                px = INTERPOLATE(ab, px, px2);
            }
#if defined(TEXMAP_MASKING) && defined(TEXMAP_TRANSLUCENT)
            auto src = ALPHA_BLEND(px, _multiplyAlpha(opacity, blendMethod(*cmp)));
#elif defined(TEXMAP_MASKING)
            auto src = ALPHA_BLEND(px, blendMethod(*cmp));
#elif defined(TEXMAP_TRANSLUCENT)
            auto src = ALPHA_BLEND(px, opacity);
#else
            auto src = px;            
#endif
            *buf = src + ALPHA_BLEND(*buf, surface->blender.ialpha(src));
            ++buf;
#ifdef TEXMAP_MASKING
            ++cmp;
#endif
            //Step UV horizontally
            u += _dudx;
            v += _dvdx;
        }
next:
        //Step along both edges
        _xa += _dxdya;
        _xb += _dxdyb;
        _ua += _dudya;
        _va += _dvdya;

        y++;
    }
    xa = _xa;
    xb = _xb;
    ua = _ua;
    va = _va;
}