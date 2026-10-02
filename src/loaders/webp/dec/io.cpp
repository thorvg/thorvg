// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// functions for sample output.
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include "tvgCommon.h"
#include "../dec/vp8i.h"
#include "./webpi.h"
#include "../dsp/dsp.h"
#include "../dsp/yuv.h"
#include "../utils/utils.h"


// Point-sampling U/V sampler.
static int EmitSampledRGB(const VP8Io* const io, WebPDecParams* const p) {
  WebPDecBuffer* const output = p->output;
  WebPRGBABuffer* const buf = &output->u.RGBA;
  uint8_t* const dst = buf->rgba + io->mb_y * buf->stride;
  WebPSamplerProcessPlane(io->y, io->y_stride,
                          io->u, io->v, io->uv_stride,
                          dst, buf->stride, io->mb_w, io->mb_h,
                          WebPSamplers[output->colorspace]);
  return io->mb_h;
}

//------------------------------------------------------------------------------
// Fancy upsampling

#ifdef FANCY_UPSAMPLING
static int EmitFancyRGB(const VP8Io* const io, WebPDecParams* const p) {
  int num_lines_out = io->mb_h;   // a priori guess
  const WebPRGBABuffer* const buf = &p->output->u.RGBA;
  uint8_t* dst = buf->rgba + io->mb_y * buf->stride;
  WebPUpsampleLinePairFunc upsample = WebPUpsamplers[p->output->colorspace];
  const uint8_t* cur_y = io->y;
  const uint8_t* cur_u = io->u;
  const uint8_t* cur_v = io->v;
  const uint8_t* top_u = p->tmp_u;
  const uint8_t* top_v = p->tmp_v;
  int y = io->mb_y;
  const int y_end = io->mb_y + io->mb_h;
  const int mb_w = io->mb_w;
  const int uv_w = (mb_w + 1) / 2;

  if (y == 0) {
    // First line is special cased. We mirror the u/v samples at boundary.
    upsample(cur_y, NULL, cur_u, cur_v, cur_u, cur_v, dst, NULL, mb_w);
  } else {
    // We can finish the left-over line from previous call.
    upsample(p->tmp_y, cur_y, top_u, top_v, cur_u, cur_v,
             dst - buf->stride, dst, mb_w);
    ++num_lines_out;
  }
  // Loop over each output pairs of row.
  for (; y + 2 < y_end; y += 2) {
    top_u = cur_u;
    top_v = cur_v;
    cur_u += io->uv_stride;
    cur_v += io->uv_stride;
    dst += 2 * buf->stride;
    cur_y += 2 * io->y_stride;
    upsample(cur_y - io->y_stride, cur_y,
             top_u, top_v, cur_u, cur_v,
             dst - buf->stride, dst, mb_w);
  }
  // move to last row
  cur_y += io->y_stride;
  if (io->crop_top + y_end < io->crop_bottom) {
    // Save the unfinished samples for next call (as we're not done yet).
    memcpy(p->tmp_y, cur_y, mb_w * sizeof(*p->tmp_y));
    memcpy(p->tmp_u, cur_u, uv_w * sizeof(*p->tmp_u));
    memcpy(p->tmp_v, cur_v, uv_w * sizeof(*p->tmp_v));
    // The fancy upsampler leaves a row unfinished behind
    // (except for the very last row)
    num_lines_out--;
  } else {
    // Process the very last row of even-sized picture
    if (!(y_end & 1)) {
      upsample(cur_y, NULL, cur_u, cur_v, cur_u, cur_v,
               dst + buf->stride, NULL, mb_w);
    }
  }
  return num_lines_out;
}

#endif    /* FANCY_UPSAMPLING */

//------------------------------------------------------------------------------


static int GetAlphaSourceRow(const VP8Io* const io,
                             const uint8_t** alpha, int* const num_rows) {
  int start_y = io->mb_y;
  *num_rows = io->mb_h;

  // Compensate for the 1-line delay of the fancy upscaler.
  // This is similar to EmitFancyRGB().
  if (io->fancy_upsampling) {
    if (start_y == 0) {
      // We don't process the last row yet. It'll be done during the next call.
      --*num_rows;
    } else {
      --start_y;
      // Fortunately, *alpha data is persistent, so we can go back
      // one row and finish alpha blending, now that the fancy upscaler
      // completed the YUV->RGB interpolation.
      *alpha -= io->width;
    }
    if (io->crop_top + io->mb_y + io->mb_h == io->crop_bottom) {
      // If it's the very last call, we process all the remaining rows!
      *num_rows = io->crop_bottom - io->crop_top - start_y;
    }
  }
  return start_y;
}

static int EmitAlphaRGB(const VP8Io* const io, WebPDecParams* const p) {
  const uint8_t* alpha = io->a;
  if (alpha != NULL) {
    const int mb_w = io->mb_w;
    const WEBP_CSP_MODE colorspace = p->output->colorspace;
    const WebPRGBABuffer* const buf = &p->output->u.RGBA;
    int num_rows;
    const int start_y = GetAlphaSourceRow(io, &alpha, &num_rows);
    uint8_t* const base_rgba = buf->rgba + start_y * buf->stride;
    uint8_t* const dst = base_rgba + 3;   // alpha is last for (rgbA|bgrA|RGBA|BGRA)
    const int has_alpha = WebPDispatchAlpha(alpha, io->width, mb_w,
                                            num_rows, dst, buf->stride);

    // has_alpha is true if there's non-trivial alpha to premultiply with.
    if (has_alpha && WebPIsPremultipliedMode(colorspace)) {
      WebPApplyAlphaMultiply(base_rgba, 0,
                             mb_w, num_rows, buf->stride);
    }
  }
  return 0;
}


//------------------------------------------------------------------------------
// Default custom functions

static int CustomSetup(VP8Io* io) {
  WebPDecParams* const p = (WebPDecParams*)io->opaque;
  const WEBP_CSP_MODE colorspace = p->output->colorspace;
  const int is_alpha = WebPIsAlphaMode(colorspace);

  p->memory = NULL;
  p->emit = NULL;
  p->emit_alpha = NULL;

  WebPIoInitFrame(io);
  if (is_alpha && WebPIsPremultipliedMode(colorspace)) {
    WebPInitUpsamplers();
  }
  p->emit = EmitSampledRGB;   // default
  if (io->fancy_upsampling) {
#ifdef FANCY_UPSAMPLING
    const int uv_width = (io->mb_w + 1) >> 1;
    p->memory = WebPSafeMalloc(1ULL, (size_t)(io->mb_w + 2 * uv_width));
    if (p->memory == NULL) {
      return 0;   // memory error.
    }
    p->tmp_y = (uint8_t*)p->memory;
    p->tmp_u = p->tmp_y + io->mb_w;
    p->tmp_v = p->tmp_u + uv_width;
    p->emit = EmitFancyRGB;
    WebPInitUpsamplers();
#endif
  } else {
    WebPInitSamplers();
  }
  if (is_alpha) {  // need transparency output
    p->emit_alpha = EmitAlphaRGB;
    WebPInitAlphaProcessing();
  }
  VP8YUVInit();
  return 1;
}

//------------------------------------------------------------------------------

static int CustomPut(const VP8Io* io) {
  WebPDecParams* const p = (WebPDecParams*)io->opaque;
  const int mb_w = io->mb_w;
  const int mb_h = io->mb_h;
  int num_lines_out;
  assert(!(io->mb_y & 1));

  if (mb_w <= 0 || mb_h <= 0) {
    return 0;
  }
  num_lines_out = p->emit(io, p);
  if (p->emit_alpha != NULL) {
    p->emit_alpha(io, p);
  }
  p->last_y += num_lines_out;
  return 1;
}

//------------------------------------------------------------------------------

static void CustomTeardown(const VP8Io* io) {
  WebPDecParams* const p = (WebPDecParams*)io->opaque;
  tvg::free(p->memory);
  p->memory = NULL;
}

//------------------------------------------------------------------------------
// Main entry point

void WebPInitCustomIo(WebPDecParams* const params, VP8Io* const io) {
  io->put      = CustomPut;
  io->setup    = CustomSetup;
  io->teardown = CustomTeardown;
  io->opaque   = params;
}

//------------------------------------------------------------------------------
