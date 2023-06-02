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

auto scaleMethod = image->scale < DOWN_SCALE_TOLERANCE ? _interpDownScaler : _interpUpScaler;

#ifdef SCALED_RLE_IMAGE_INT_MASK
{
    auto cbuffer = surface->compositor->image.buf32;
    auto cstride = surface->compositor->image.stride;

    for (uint32_t y = surface->compositor->bbox.min.y; y < surface->compositor->bbox.max.y; ++y) {
        auto cmp = &cbuffer[y * cstride];
        for (uint32_t x = surface->compositor->bbox.min.x; x < surface->compositor->bbox.max.x; ++x) {
            if (y == span->y && x == span->x && x + span->len <= surface->compositor->bbox.max.x) {
                auto sy = span->y * itransform->e22 + itransform->e23;
                if ((uint32_t)sy >= image->h) continue;
                auto alpha = MULTIPLY(span->coverage, opacity);
                if (alpha == 255) {
                    for (uint32_t i = 0; i < span->len; ++i) {
                        auto sx = (x + i) * itransform->e11 + itransform->e13;
                        if ((uint32_t)sx >= image->w) continue;
                        auto src = scaleMethod(image->buf32, image->stride, image->w, image->h, sx, sy, halfScale);
                        cmp[x + i] = ALPHA_BLEND(cmp[x + i], ALPHA(src));
                    }
                } else {
                    for (uint32_t i = 0; i < span->len; ++i) {
                        auto sx = (x + i) * itransform->e11 + itransform->e13;
                        if ((uint32_t)sx >= image->w) continue;
                        auto src = scaleMethod(image->buf32, image->stride, image->w, image->h, sx, sy, halfScale);
                        src = ALPHA_BLEND(src, alpha);
                        cmp[x + i] = ALPHA_BLEND(cmp[x + i], ALPHA(src));
                    }
                }
                x += span->len - 1;
                ++span;
            } else {
                cmp[x] = 0;
            }
        }
    }
}
#else
{
    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto sy = span->y * itransform->e22 + itransform->e23;
        if ((uint32_t)sy >= image->h) continue;
        auto cmp = &surface->compositor->image.buf32[span->y * surface->compositor->image.stride + span->x];
        auto a = MULTIPLY(span->coverage, opacity);
        if (a == 255) {
            for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++cmp) {
                auto sx = x * itransform->e11 + itransform->e13;
                if ((uint32_t)sx >= image->w) continue;
                auto src = scaleMethod(image->buf32, image->stride, image->w, image->h, sx, sy, halfScale);
#ifdef SCALED_RLE_IMAGE_ADD_MASK
                *cmp = src + ALPHA_BLEND(*cmp, IALPHA(src));
#elif defined(SCALED_RLE_IMAGE_SUB_MASK)
                *cmp = ALPHA_BLEND(*cmp, IALPHA(src));
#elif defined(SCALED_RLE_IMAGE_DIF_MASK)
                *cmp = ALPHA_BLEND(src, IALPHA(*cmp)) + ALPHA_BLEND(*cmp, IALPHA(src));
#endif
            }
        } else {
            for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++cmp) {
                auto sx = x * itransform->e11 + itransform->e13;
                if ((uint32_t)sx >= image->w) continue;
                auto src = scaleMethod(image->buf32, image->stride, image->w, image->h, sx, sy, halfScale);
#ifdef SCALED_RLE_IMAGE_ADD_MASK
                *cmp = INTERPOLATE(src, *cmp, a);
#elif defined(SCALED_RLE_IMAGE_SUB_MASK)
                *cmp = ALPHA_BLEND(*cmp, IALPHA(ALPHA_BLEND(src, a)));
#elif defined(SCALED_RLE_IMAGE_DIF_MASK)
                src = ALPHA_BLEND(src, a);
                *cmp = ALPHA_BLEND(src, IALPHA(*cmp)) + ALPHA_BLEND(*cmp, IALPHA(src));
#endif
            }
        }
    }
}
#endif