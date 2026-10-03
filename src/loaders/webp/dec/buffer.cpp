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

// Only the 32-bit RGBA / BGRA colorspaces (straight or premultiplied) are
// supported for output.
static int IsValidColorspace(int webp_csp_mode) {
  return (webp_csp_mode == MODE_RGBA || webp_csp_mode == MODE_BGRA ||
          webp_csp_mode == MODE_rgbA || webp_csp_mode == MODE_bgrA);
}

#define BYTES_PER_PIXEL 4

static VP8StatusCode CheckDecBuffer(const WebPDecBuffer* const buffer) {
  int ok = 1;
  const WEBP_CSP_MODE mode = buffer->colorspace;
  const int width = buffer->width;
  const int height = buffer->height;
  if (!IsValidColorspace(mode)) {
    ok = 0;
  } else {    // RGB checks
    const WebPRGBABuffer* const buf = &buffer->u.RGBA;
    const int stride = abs(buf->stride);
    const uint64_t size = (uint64_t)stride * height;
    ok &= (size <= buf->size);
    ok &= (stride >= width * BYTES_PER_PIXEL);
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
    // We need memory and it hasn't been allocated yet.
    // => initialize output buffer, now that dimensions are known.
    const int stride = w * BYTES_PER_PIXEL;
    const uint64_t size = (uint64_t)stride * h;

    // Security/sanity checks
    output = tvg::malloc<uint8_t>(size * sizeof(*output));
    if (output == NULL) {
      return VP8_STATUS_OUT_OF_MEMORY;
    }
    buffer->private_memory = output;

    // RGBA initialization
    WebPRGBABuffer* const buf = &buffer->u.RGBA;
    buf->rgba = output;
    buf->stride = stride;
    buf->size = (size_t)size;
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

void WebPCopyDecBuffer(const WebPDecBuffer* const src,
                       WebPDecBuffer* const dst) {
  if (src != NULL && dst != NULL) {
    *dst = *src;
    if (src->private_memory != NULL) {
      dst->is_external_memory = 1;   // dst buffer doesn't own the memory.
      dst->private_memory = NULL;
    }
  }
}

//------------------------------------------------------------------------------

