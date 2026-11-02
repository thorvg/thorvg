// Copyright 2010 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  Main decoding functions for WebP images.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_WEBP_DECODE_H_
#define WEBP_WEBP_DECODE_H_

#include "./types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WEBP_DECODER_ABI_VERSION 0x0205    // MAJOR(8b) + MINOR(8b)

// Note: forward declaring enumerations is not allowed in (strict) C and C++,
// the types are left here for reference.
// typedef enum VP8StatusCode VP8StatusCode;
// typedef enum WEBP_CSP_MODE WEBP_CSP_MODE;
typedef struct WebPRGBABuffer WebPRGBABuffer;
typedef struct WebPYUVABuffer WebPYUVABuffer;
typedef struct WebPDecBuffer WebPDecBuffer;
typedef struct WebPBitstreamFeatures WebPBitstreamFeatures;

// Retrieve basic header information: width, height.
// This function will also validate the header and return 0 in
// case of formatting error.
// Pointers 'width' and 'height' can be passed NULL if deemed irrelevant.
WEBP_EXTERN(int) WebPGetInfo(const uint8_t* data, size_t data_size,
                             int* width, int* height);

// Decodes WebP images pointed to by 'data' and returns RGBA samples, along
// with the dimensions in *width and *height. The ordering of samples in
// memory is R, G, B, A, R, G, B, A... in scan order (endian-independent).
// The returned pointer should be deleted calling free().
// Returns NULL in case of error.
WEBP_EXTERN(uint8_t*) WebPDecodeRGBA(const uint8_t* data, size_t data_size,
                                     int* width, int* height);

// Same as WebPDecodeRGBA, but returning B, G, R, A, B, G, R, A... ordered data.
WEBP_EXTERN(uint8_t*) WebPDecodeBGRA(const uint8_t* data, size_t data_size,
                                     int* width, int* height);


//------------------------------------------------------------------------------
// Output colorspaces and buffer

// Premultiplied output colorspaces, named by byte order in memory.
typedef enum WEBP_CSP_MODE
{
    MODE_rgbA = 0,
    MODE_bgrA = 1,
    MODE_LAST = 2
} WEBP_CSP_MODE;

static WEBP_INLINE int WebPIsPremultipliedMode(WEBP_CSP_MODE mode) {
    return (mode == MODE_rgbA || mode == MODE_bgrA);
}

static WEBP_INLINE int WebPIsAlphaMode(WEBP_CSP_MODE mode) {
    return WebPIsPremultipliedMode(mode);
}

//------------------------------------------------------------------------------
// WebPDecBuffer: Generic structure for describing the output sample buffer.

struct WebPRGBABuffer {    // view as RGBA
  uint8_t* rgba;    // pointer to RGBA samples
  int stride;       // stride in bytes from one scanline to the next.
  size_t size;      // total size of the *rgba buffer.
};

struct WebPYUVABuffer {              // view as YUVA
  uint8_t* y, *u, *v, *a;     // pointer to luma, chroma U/V, alpha samples
  int y_stride;               // luma stride
  int u_stride, v_stride;     // chroma strides
  int a_stride;               // alpha stride
  size_t y_size;              // luma plane size
  size_t u_size, v_size;      // chroma planes size
  size_t a_size;              // alpha-plane size
};

// Output buffer
struct WebPDecBuffer {
  WEBP_CSP_MODE colorspace;  // Colorspace.
  int width, height;         // Dimensions.
  union {
    WebPRGBABuffer RGBA;
    WebPYUVABuffer YUVA;
  } u;                       // Nameless union of buffer parameters.
  uint32_t       pad[4];     // padding for later use

  uint8_t* private_memory;  // Internally allocated memory, accessed via
                            // the buffer union.
};

// Internal, version-checked, entry point
WEBP_EXTERN(int) WebPInitDecBufferInternal(WebPDecBuffer*, int);

// Initialize the structure as empty. Must be called before any other use.
// Returns false in case of version mismatch
static WEBP_INLINE int WebPInitDecBuffer(WebPDecBuffer* buffer) {
  return WebPInitDecBufferInternal(buffer, WEBP_DECODER_ABI_VERSION);
}

// Free any memory associated with the buffer. Must always be called last.
// Note: doesn't free the 'buffer' structure itself.
WEBP_EXTERN(void) WebPFreeDecBuffer(WebPDecBuffer* buffer);

//------------------------------------------------------------------------------
// Enumeration of the status codes

typedef enum VP8StatusCode {
  VP8_STATUS_OK = 0,
  VP8_STATUS_OUT_OF_MEMORY,
  VP8_STATUS_INVALID_PARAM,
  VP8_STATUS_BITSTREAM_ERROR,
  VP8_STATUS_UNSUPPORTED_FEATURE,
  VP8_STATUS_SUSPENDED,
  VP8_STATUS_USER_ABORT,
  VP8_STATUS_NOT_ENOUGH_DATA
} VP8StatusCode;

//------------------------------------------------------------------------------

// Features gathered from the bitstream
struct WebPBitstreamFeatures {
  int width;          // Width in pixels, as read from the bitstream.
  int height;         // Height in pixels, as read from the bitstream.
  int has_alpha;      // True if the bitstream contains an alpha channel.
  int has_animation;  // True if the bitstream is an animation.
  int format;         // 0 = undefined (/mixed), 1 = lossy, 2 = lossless

  // Unused for now:
  int no_incremental_decoding;  // if true, using incremental decoding is not
                                // recommended.
  int rotate;                   // TODO(later)
  int uv_sampling;              // should be 0 for now. TODO(later)
  uint32_t pad[2];              // padding for later use
};

// Retrieve features from the bitstream. The *features structure is filled
// with information gathered from the bitstream.
// Returns VP8_STATUS_OK when the features are successfully retrieved. Returns
// VP8_STATUS_NOT_ENOUGH_DATA when more data is needed to retrieve the
// features from headers. Returns error in other cases.
WEBP_EXTERN(VP8StatusCode) WebPGetFeatures(const uint8_t*, size_t, WebPBitstreamFeatures*);

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  /* WEBP_WEBP_DECODE_H_ */
