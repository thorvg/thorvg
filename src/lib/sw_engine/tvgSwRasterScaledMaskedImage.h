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

#ifdef SCALED_IMAGE_INT_MASK
{
    auto cbuffer = surface->compositor->image.buf32 + (surface->compositor->bbox.min.y * cstride + surface->compositor->bbox.min.x);

    for (uint32_t y = surface->compositor->bbox.min.y; y < surface->compositor->bbox.max.y; ++y) {
        if (y == region.min.y) {
            auto cbuffer2 = cbuffer;
            for (uint32_t y2 = y; y2 < region.max.y; ++y2) {
                auto sy = (uint32_t)(y2 * itransform->e22 + itransform->e23);
                if (sy >= image->h) continue;
                auto tmp = cbuffer2;
                auto x = surface->compositor->bbox.min.x;
                while (x < surface->compositor->bbox.max.x) {
                    if (x == region.min.x) {                          
                        if (opacity == 255) {
                            //Down-Scaled
                            if (image->scale < DOWN_SCALE_TOLERANCE) {
                                for (uint32_t i = 0; i < w; ++i, ++tmp) {
                                    auto sx = (uint32_t)((x + i) * itransform->e11 + itransform->e13);
                                    if (sx >= image->w) continue;
                                    auto src = _interpDownScaler(image->buf32, image->stride, image->w, image->h, sx, sy, halfScale);
                                    *tmp = ALPHA_BLEND(*tmp, ALPHA(src));
                                }
                            //Up-Scaled
                            } else {
                                 for (uint32_t i = 0; i < w; ++i, ++tmp) {
                                    auto sx = (uint32_t)((x + i) * itransform->e11 + itransform->e13);
                                    if (sx >= image->w) continue;
                                    auto src = _interpUpScaler(image->buf32, image->w, image->h, sx, sy);
                                    *tmp = ALPHA_BLEND(*tmp, ALPHA(src));
                                }                               
                            }
                        } else {
                            //Down-Scaled
                            if (image->scale < DOWN_SCALE_TOLERANCE) {
                                for (uint32_t i = 0; i < w; ++i, ++tmp) {
                                    auto sx = (uint32_t)((x + i) * itransform->e11 + itransform->e13);
                                    if (sx >= image->w) continue;
                                    auto src = ALPHA_BLEND(_interpDownScaler(image->buf32, image->stride, image->w, image->h, sx, sy, halfScale), opacity);
                                    *tmp = ALPHA_BLEND(*tmp, ALPHA(src));
                                }
                            //Up-Scaled
                            } else {
                                for (uint32_t i = 0; i < w; ++i, ++tmp) {
                                    auto sx = (uint32_t)((x + i) * itransform->e11 + itransform->e13);
                                    if (sx >= image->w) continue;
                                    auto src = ALPHA_BLEND(_interpUpScaler(image->buf32, image->w, image->h, sx, sy), opacity);
                                    *tmp = ALPHA_BLEND(*tmp, ALPHA(src));
                                }
                            }
                        }
                        x += w;
                    } else {
                        *tmp = 0;
                        ++tmp;
                        ++x;
                    }
                }
                cbuffer2 += cstride;
            }
            y += (h - 1);
        } else {
            auto tmp = cbuffer;
            for (uint32_t x = surface->compositor->bbox.min.x; x < surface->compositor->bbox.max.x; ++x, ++tmp) {
                *tmp = 0;
            }
        }
        cbuffer += cstride;
    }
}
#else
{
    auto cbuffer = surface->compositor->image.buf32 + (region.min.y * cstride + region.min.x);

    // Down-Scaled
    if (image->scale < DOWN_SCALE_TOLERANCE) {
        for (auto y = region.min.y; y < region.max.y; ++y) {
            auto sy = (uint32_t)(y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto cmp = cbuffer;
            if (opacity == 255) {
                for (auto x = region.min.x; x < region.max.x; ++x, ++cmp) {
                    auto sx = (uint32_t)(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto src = _interpDownScaler(image->buf32, image->stride, image->w, image->h, sx, sy, halfScale);
#ifdef SCALED_IMAGE_ADD_MASK
                    *cmp = src + ALPHA_BLEND(*cmp, IALPHA(src));
#elif defined(SCALED_IMAGE_SUB_MASK)
                    *cmp = ALPHA_BLEND(*cmp, IALPHA(src));
#elif defined(SCALED_IMAGE_DIF_MASK)
                    *cmp = ALPHA_BLEND(src, IALPHA(*cmp)) + ALPHA_BLEND(*cmp, IALPHA(src));
#endif
                }
            } else {
                for (auto x = region.min.x; x < region.max.x; ++x, ++cmp) {
                    auto sx = (uint32_t)(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto src = _interpDownScaler(image->buf32, image->stride, image->w, image->h, sx, sy, halfScale);
#ifdef SCALED_IMAGE_ADD_MASK
                    *cmp = INTERPOLATE(src, *cmp, opacity);
#elif defined(SCALED_IMAGE_SUB_MASK)
                    *cmp = ALPHA_BLEND(*cmp, IALPHA(ALPHA_BLEND(src, opacity)));
#elif defined(SCALED_IMAGE_DIF_MASK)
                    src = ALPHA_BLEND(src, opacity);
                    *cmp = ALPHA_BLEND(src, IALPHA(*cmp)) + ALPHA_BLEND(*cmp, IALPHA(src));
#endif
                }
            }
            cbuffer += cstride;
        }
    // Up-Scaled
    } else {
        for (auto y = region.min.y; y < region.max.y; ++y) {
            auto sy = y * itransform->e22 + itransform->e23;
            if ((uint32_t)sy >= image->h) continue;
            auto cmp = cbuffer;
            if (opacity == 255) {
                for (auto x = region.min.x; x < region.max.x; ++x, ++cmp) {
                    auto sx = x * itransform->e11 + itransform->e13;
                    if ((uint32_t)sx >= image->w) continue;
                    auto src = _interpUpScaler(image->buf32, image->w, image->h, sx, sy);
#ifdef SCALED_IMAGE_ADD_MASK
                    *cmp = src + ALPHA_BLEND(*cmp, IALPHA(src));
#elif defined(SCALED_IMAGE_SUB_MASK)
                    *cmp = ALPHA_BLEND(*cmp, IALPHA(src));
#elif defined(SCALED_IMAGE_DIF_MASK)
                    *cmp = ALPHA_BLEND(src, IALPHA(*cmp)) + ALPHA_BLEND(*cmp, IALPHA(src));
#endif
                }
            } else {
                for (auto x = region.min.x; x < region.max.x; ++x, ++cmp) {
                    auto sx = x * itransform->e11 + itransform->e13;
                    if ((uint32_t)sx >= image->w) continue;
                    auto src = _interpUpScaler(image->buf32, image->w, image->h, sx, sy);
#ifdef SCALED_IMAGE_ADD_MASK
                    *cmp = INTERPOLATE(src, *cmp, opacity);
#elif defined(SCALED_IMAGE_SUB_MASK)
                    *cmp = ALPHA_BLEND(*cmp, IALPHA(ALPHA_BLEND(src, opacity)));
#elif defined(SCALED_IMAGE_DIF_MASK)
                    src = ALPHA_BLEND(src, opacity);
                    *cmp = ALPHA_BLEND(src, IALPHA(*cmp)) + ALPHA_BLEND(*cmp, IALPHA(src));
#endif
                }
            }
            cbuffer += cstride;
        }
    }
}
#endif
