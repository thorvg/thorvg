// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Everything about WebPDecBuffer
//
// Author: Skal (pascal.massimino@gmail.com)

#include "tvgCommon.h"

#include "./vp8i.h"
#include "./webpi.h"
#include "../utils/utils.h"

//------------------------------------------------------------------------------
// WebPDecBuffer

// Number of bytes per pixel for the different color-spaces.
static const int kModeBpp[MODE_LAST] = {
  3, 4, 3, 4, 4, 2, 2,
  4, 4, 4, 2,    // pre-multiplied modes
  1, 1 };

// Check that webp_csp_mode is within the bounds of WEBP_CSP_MODE.
// Convert to an integer to handle both the unsigned/signed enum cases
// without the need for casting to remove type limit warnings.
static int IsValidColorspace(int webp_csp_mode) {
  return (webp_csp_mode >= MODE_RGB && webp_csp_mode < MODE_LAST);
}

static VP8StatusCode CheckDecBuffer(const WebPDecBuffer* const buffer) {
  int ok = 1;
  const WEBP_CSP_MODE mode = buffer->colorspace;
  const int width = buffer->width;
  const int height = buffer->height;
  if (!IsValidColorspace(mode)) {
    ok = 0;
  } else if (!WebPIsRGBMode(mode)) {   // YUV checks
    const WebPYUVABuffer* const buf = &buffer->u.YUVA;
    const int y_stride = abs(buf->y_stride);
    const int u_stride = abs(buf->u_stride);
    const int v_stride = abs(buf->v_stride);
    const int a_stride = abs(buf->a_stride);
    const uint64_t y_size = (uint64_t)y_stride * height;
    const uint64_t u_size = (uint64_t)u_stride * ((height + 1) / 2);
    const uint64_t v_size = (uint64_t)v_stride * ((height + 1) / 2);
    const uint64_t a_size = (uint64_t)a_stride * height;
    ok &= (y_size <= buf->y_size);
    ok &= (u_size <= buf->u_size);
    ok &= (v_size <= buf->v_size);
    ok &= (y_stride >= width);
    ok &= (u_stride >= (width + 1) / 2);
    ok &= (v_stride >= (width + 1) / 2);
    ok &= (buf->y != NULL);
    ok &= (buf->u != NULL);
    ok &= (buf->v != NULL);
    if (mode == MODE_YUVA) {
      ok &= (a_stride >= width);
      ok &= (a_size <= buf->a_size);
      ok &= (buf->a != NULL);
    }
  } else {    // RGB checks
    const WebPRGBABuffer* const buf = &buffer->u.RGBA;
    const int stride = abs(buf->stride);
    const uint64_t size = (uint64_t)stride * height;
    ok &= (size <= buf->size);
    ok &= (stride >= width * kModeBpp[mode]);
    ok &= (buf->rgba != NULL);
  }
  return ok ? VP8_STATUS_OK : VP8_STATUS_INVALID_PARAM;
}

static VP8StatusCode AllocateBuffer(WebPDecBuffer* const buffer) {
  const int w = buffer->width;
  const int h = buffer->height;
  const WEBP_CSP_MODE mode = buffer->colorspace;

  if (w <= 0 || h <= 0 || !IsValidColorspace(mode)) {
    return VP8_STATUS_INVALID_PARAM;
  }

  if (!buffer->is_external_memory && buffer->private_memory == NULL) {
    uint8_t* output;
    int uv_stride = 0, a_stride = 0;
    uint64_t uv_size = 0, a_size = 0, total_size;
    // We need memory and it hasn't been allocated yet.
    // => initialize output buffer, now that dimensions are known.
    const int stride = w * kModeBpp[mode];
    const uint64_t size = (uint64_t)stride * h;

    if (!WebPIsRGBMode(mode)) {
      uv_stride = (w + 1) / 2;
      uv_size = (uint64_t)uv_stride * ((h + 1) / 2);
      if (mode == MODE_YUVA) {
        a_stride = w;
        a_size = (uint64_t)a_stride * h;
      }
    }
    total_size = size + 2 * uv_size + a_size;

    // Security/sanity checks
    output = tvg::malloc<uint8_t>(total_size * sizeof(*output));
    if (output == NULL) {
      return VP8_STATUS_OUT_OF_MEMORY;
    }
    buffer->private_memory = output;

    if (!WebPIsRGBMode(mode)) {   // YUVA initialization
      WebPYUVABuffer* const buf = &buffer->u.YUVA;
      buf->y = output;
      buf->y_stride = stride;
      buf->y_size = (size_t)size;
      buf->u = output + size;
      buf->u_stride = uv_stride;
      buf->u_size = (size_t)uv_size;
      buf->v = output + size + uv_size;
      buf->v_stride = uv_stride;
      buf->v_size = (size_t)uv_size;
      if (mode == MODE_YUVA) {
        buf->a = output + size + 2 * uv_size;
      }
      buf->a_size = (size_t)a_size;
      buf->a_stride = a_stride;
    } else {  // RGBA initialization
      WebPRGBABuffer* const buf = &buffer->u.RGBA;
      buf->rgba = output;
      buf->stride = stride;
      buf->size = (size_t)size;
    }
  }
  return CheckDecBuffer(buffer);
}

VP8StatusCode WebPAllocateDecBuffer(int w, int h,
                                    WebPDecBuffer* const out) {
  if (out == NULL || w <= 0 || h <= 0) {
    return VP8_STATUS_INVALID_PARAM;
  }
  out->width = w;
  out->height = h;
  return AllocateBuffer(out);
}

//------------------------------------------------------------------------------
// constructors / destructors

int WebPInitDecBufferInternal(WebPDecBuffer* buffer, int version) {
  if (WEBP_ABI_IS_INCOMPATIBLE(version, WEBP_DECODER_ABI_VERSION)) {
    return 0;  // version mismatch
  }
  if (buffer == NULL) return 0;
  memset(buffer, 0, sizeof(*buffer));
  return 1;
}

void WebPFreeDecBuffer(WebPDecBuffer* buffer) {
  if (buffer != NULL) {
    if (!buffer->is_external_memory) {
      tvg::free(buffer->private_memory);
    }
    buffer->private_memory = NULL;
  }
}

//------------------------------------------------------------------------------

