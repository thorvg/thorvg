// Copyright 2013 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Utilities for processing transparent channel.
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include "./dsp.h"

//------------------------------------------------------------------------------
// Premultiplied modes

// non dithered-modes

// (x * a * 32897) >> 23 is bit-wise equivalent to (int)(x * a / 255.)
// for all 8bit x or a. For bit-wise equivalence to (int)(x * a / 255. + .5),
// one can use instead: (x * a * 65793 + (1 << 23)) >> 24
#if 1     // (int)(x * a / 255.)
#define MULTIPLIER(a)   ((a) * 32897U)
#define PREMULTIPLY(x, m) (((x) * (m)) >> 23)
#else     // (int)(x * a / 255. + .5)
#define MULTIPLIER(a) ((a) * 65793U)
#define PREMULTIPLY(x, m) (((x) * (m) + (1U << 23)) >> 24)
#endif

static void ApplyAlphaMultiply(uint8_t* rgba, int alpha_first,
                               int w, int h, int stride) {
  while (h-- > 0) {
    uint8_t* const rgb = rgba + (alpha_first ? 1 : 0);
    const uint8_t* const alpha = rgba + (alpha_first ? 0 : 3);
    int i;
    for (i = 0; i < w; ++i) {
      const uint32_t a = alpha[4 * i];
      if (a != 0xff) {
        const uint32_t mult = MULTIPLIER(a);
        rgb[4 * i + 0] = PREMULTIPLY(rgb[4 * i + 0], mult);
        rgb[4 * i + 1] = PREMULTIPLY(rgb[4 * i + 1], mult);
        rgb[4 * i + 2] = PREMULTIPLY(rgb[4 * i + 2], mult);
      }
    }
    rgba += stride;
  }
}
#undef MULTIPLIER
#undef PREMULTIPLY

static int DispatchAlpha(const uint8_t* alpha, int alpha_stride,
                         int width, int height,
                         uint8_t* dst, int dst_stride) {
  uint32_t alpha_mask = 0xff;
  int i, j;

  for (j = 0; j < height; ++j) {
    for (i = 0; i < width; ++i) {
      const uint32_t alpha_value = alpha[i];
      dst[4 * i] = alpha_value;
      alpha_mask &= alpha_value;
    }
    alpha += alpha_stride;
    dst += dst_stride;
  }

  return (alpha_mask != 0xff);
}

void (*WebPApplyAlphaMultiply)(uint8_t*, int, int, int, int);
int (*WebPDispatchAlpha)(const uint8_t*, int, int, int, uint8_t*, int);

//------------------------------------------------------------------------------
// Init function

extern void WebPInitAlphaProcessingMIPSdspR2(void);
extern void WebPInitAlphaProcessingSSE2(void);

static volatile VP8CPUInfo alpha_processing_last_cpuinfo_used =
    (VP8CPUInfo)&alpha_processing_last_cpuinfo_used;

WEBP_TSAN_IGNORE_FUNCTION void WebPInitAlphaProcessing(void) {
  if (alpha_processing_last_cpuinfo_used == VP8GetCPUInfo) return;

  WebPApplyAlphaMultiply = ApplyAlphaMultiply;
  WebPDispatchAlpha = DispatchAlpha;

  // If defined, use CPUInfo() to overwrite some pointers with faster versions.
  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_USE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPInitAlphaProcessingSSE2();
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      WebPInitAlphaProcessingMIPSdspR2();
    }
#endif
  }
  alpha_processing_last_cpuinfo_used = VP8GetCPUInfo;
}
