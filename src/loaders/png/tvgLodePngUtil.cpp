/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

/*
    LodePNG Utils

    Copyright (c) 2005-2020 Lode Vandevenne

    This software is provided 'as-is', without any express or implied
    warranty. In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software
        in a product, an acknowledgment in the product documentation would be
        appreciated but is not required.

        2. Altered source versions must be plainly marked as such, and must not be
        misrepresented as being the original software.

        3. This notice may not be removed or altered from any source
        distribution.
*/

#include <cmath>
#include <cstdint>
#include "tvgMath.h"
#include "tvgLodePng.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

/* Parameters of a tone reproduction curve, either with a power law formula or with a lookup table. */
typedef struct
{
    unsigned type; /* 0=linear, 1=lut, 2=simple gamma */
    float* lut;    /* for type 1 */
    size_t lut_size;
    float gamma;   /* for type 2 */
} LodePNGICCCurve;

/* Values parsed from ICC profile, see _parseICC for more information about this subset.*/
typedef struct
{
    // Omit the unused ICC profile version fields from this stripped conversion path.

    /* The whitepoint of the profile connection space (PCS). Should always be D50, but parsed and used anyway.
    (to be clear, whitepoint and illuminant are synonyms in practice, but here field "illuminant" is ICC's
    "global" whitepoint that is always D50, and the field "white" below allows deriving the whitepoint of
    the particular RGB space represented here) */
    float illuminant[3];

    /* if true, has chromatic adaptation matrix that must be used. If false, you must compute a chromatic adaptation
    matrix yourself from "illuminant" and "white". */
    unsigned has_chad;
    Matrix chad;

    /* The whitepoint of the RGB color space as stored in the ICC file. If has_chad, must be adapted with the
    chad matrix to become the one we need to go to absolute XYZ (in fact ICC implies it should then be
    exactly D50 in the file, redundantly, before this transformation with chad), else use as-is (then its
    values can actually be something else than D50, and are the ones we need). */
    unsigned has_whitepoint;
    float white[3];
    /* Chromaticities of the RGB space in XYZ color space, but given such that you must still
    whitepoint adapt them from D50 to the RGB space whitepoint to go to absolute XYZ (if has_chad,
    with chad, else with bradford adaptation matrix from illuminant to white). */
    unsigned has_chromaticity;
    Matrix rgb;

    unsigned has_trc; /* TRC = tone reproduction curve (aka "gamma correction") */

    /* TRC's for the three channels */
    LodePNGICCCurve trc[3];
} LodePNGICC;

/* ICC tone response curve, nonlinear (encoded) to linear.
   Input and output in range 0-1. If color was integer 0-255, multiply with (1.0f/255)
   to get the correct floating point behavior.
   Outside of range 0-1, will not clip but either return x itself, or in cases
   where it makes sense, a value defined by the same function.
   NOTE: ICC requires clipping, but we do that only later when converting float to integer.*/
static float _iccForwardTRC(const LodePNGICCCurve* curve, float x)
{
    if (curve->type == 0) {
        return x;
    }
    if (curve->type == 1) { /* Lookup table */
        float v0, v1, fraction;
        size_t index;
        if (!curve->lut) return 0; /* error */
        if (x < 0) return x;
        index = (size_t)(x * (curve->lut_size - 1));
        if (index >= curve->lut_size) return x;

        /* LERP */
        v0 = curve->lut[index];
        v1 = (index + 1 < curve->lut_size) ? curve->lut[index + 1] : 1.0f;
        fraction = (x * (curve->lut_size - 1)) - index;
        return v0 * (1 - fraction) + v1 * fraction;
    }
    if (curve->type == 2) {
        /* Gamma expansion */
        return (x > 0) ? powf(x, curve->gamma) : x;
    }
    return 0;
}

static uint32_t _decodeICC(const unsigned char* data, size_t size, size_t* pos, uint8_t bytes)
{
    *pos += bytes;
    if (*pos > size) return 0;
    uint32_t value = 0;
    for (auto i = *pos - bytes; i < *pos; ++i) value = (value << 8) | data[i];
    return value;
}

static float _decodeICC15Fixed16(const unsigned char* data, size_t size, size_t* pos)
{
    return static_cast<int32_t>(_decodeICC(data, size, pos, 4)) / 65536.0f;
}

static void _decodeICCXYZ(float* x, float* y, float* z, const unsigned char* data, size_t size, size_t* offset)
{
    *x = _decodeICC15Fixed16(data, size, offset);
    *y = _decodeICC15Fixed16(data, size, offset);
    *z = _decodeICC15Fixed16(data, size, offset);
}

static constexpr unsigned _isICCword(uint32_t value, const char* word)
{
    return value == ((static_cast<uint32_t>(word[0]) << 24) | (static_cast<uint32_t>(word[1]) << 16) |
                     (static_cast<uint32_t>(word[2]) << 8) | static_cast<uint32_t>(word[3]));
}

/* Parses a subset of the ICC profile, supporting the necessary mix of ICC v2
   and ICC v4 required to correctly convert the RGB color space to XYZ.
   Does not parse values not related to this specific PNG-related purpose, and
   does not support non-RGB profiles or lookup-table based chroma (but it
   supports lookup tables for TRC aka "gamma"). */
static unsigned _parseICC(LodePNGICC* icc, const unsigned char* data, size_t size)
{
    size_t pos = 0;

    if (size < 132) return 1; /* Too small to be a valid icc profile. */

    pos = 16;
    if (!_isICCword(_decodeICC(data, size, &pos, 4), "RGB ")) return 1;

    /* Should always be 0.9642, 1.0, 0.8249 */
    pos = 68;
    _decodeICCXYZ(&icc->illuminant[0], &icc->illuminant[1], &icc->illuminant[2], data, size, &pos);

    pos = 128;
    auto numtags = _decodeICC(data, size, &pos, 4);
    if (pos >= size) return 1;
    /* scan for tags we want to handle */
    for (size_t i = 0; i < numtags; i++) {
        auto tag = _decodeICC(data, size, &pos, 4);
        auto offset = static_cast<size_t>(_decodeICC(data, size, &pos, 4));
        auto tagsize = _decodeICC(data, size, &pos, 4);
        if (pos >= size || offset >= size) return 1;
        if (offset + tagsize > size) return 1;
        if (tagsize < 8) return 1;

        if (_isICCword(tag, "wtpt")) {
            offset += 8; /* skip type and reserved */
            _decodeICCXYZ(&icc->white[0], &icc->white[1], &icc->white[2], data, size, &offset);
            icc->has_whitepoint = 1;
        } else if (_isICCword(tag, "rXYZ")) {
            offset += 8; /* skip type and reserved */
            _decodeICCXYZ(&icc->rgb.e11, &icc->rgb.e21, &icc->rgb.e31, data, size, &offset);
            icc->has_chromaticity = 1;
        } else if (_isICCword(tag, "gXYZ")) {
            offset += 8; /* skip type and reserved */
            _decodeICCXYZ(&icc->rgb.e12, &icc->rgb.e22, &icc->rgb.e32, data, size, &offset);
            icc->has_chromaticity = 1;
        } else if (_isICCword(tag, "bXYZ")) {
            offset += 8; /* skip type and reserved */
            _decodeICCXYZ(&icc->rgb.e13, &icc->rgb.e23, &icc->rgb.e33, data, size, &offset);
            icc->has_chromaticity = 1;
        } else if (_isICCword(tag, "chad")) {
            offset += 8; /* skip datatype keyword "sf32" and reserved */
            _decodeICCXYZ(&icc->chad.e11, &icc->chad.e12, &icc->chad.e13, data, size, &offset);
            _decodeICCXYZ(&icc->chad.e21, &icc->chad.e22, &icc->chad.e23, data, size, &offset);
            _decodeICCXYZ(&icc->chad.e31, &icc->chad.e32, &icc->chad.e33, data, size, &offset);
            icc->has_chad = 1;
        } else if (_isICCword(tag, "rTRC") || _isICCword(tag, "gTRC") || _isICCword(tag, "bTRC")) {
            auto channel = _isICCword(tag, "bTRC") ? 2 : (_isICCword(tag, "gTRC") ? 1 : 0);
            /* "curv": linear, gamma power or LUT */
            if (_isICCword(_decodeICC(data, size, &offset, 4), "curv")) {
                auto trc = &icc->trc[channel];
                icc->has_trc = 1;
                offset += 4; /* skip reserved */
                auto count = static_cast<size_t>(_decodeICC(data, size, &offset, 4));
                if (count == 0) {
                    trc->type = 0; /* linear */
                } else if (count == 1) {
                    trc->type = 2; /* gamma */
                    trc->gamma = _decodeICC(data, size, &offset, 2) / 256.0f;
                } else {
                    trc->type = 1;                                               /* LUT */
                    if (offset + count * 2 > size || count > 16777216) return 1; /* also avoid crazy count */
                    trc->lut = tvg::malloc<float>(count * sizeof(float));
                    if (!trc->lut) return 1;
                    trc->lut_size = count;
                    for (size_t j = 0; j < count; j++) {
                        trc->lut[j] = _decodeICC(data, size, &offset, 2) * (1.0f / 65535.0f);
                    }
                }
            }
            /* TODO: verify: does the "chrm" tag participate in computation so should be parsed? */
        }
        /* Return error if any parse went beyond the filesize. Note that the
        parsing itself was always safe since it bound-checks inside. */
        if (offset > size) return 1;
    }

    return 0;
}

/* Multiplies 3 vector values with 3x3 matrix */
static void _mulMatrix(float* x2, float* y2, float* z2, const Matrix& m, float x, float y, float z)
{
    *x2 = x * m.e11 + y * m.e12 + z * m.e13;
    *y2 = x * m.e21 + y * m.e22 + z * m.e23;
    *z2 = x * m.e31 + y * m.e32 + z * m.e33;
}

/* Get the matrix to go from linear RGB to XYZ given the RGB whitepoint and chromaticities in XYZ colorspace */
static unsigned _getChrmMatrixXYZ(Matrix* m, const Matrix& rgb, const float white[3])
{
    float rs, gs, bs;
    if (!inverse(&rgb, m)) return 1; /* error, not invertible */
    _mulMatrix(&rs, &gs, &bs, *m, white[0], white[1], white[2]);
    *m = {
        rs * rgb.e11, gs * rgb.e12, bs * rgb.e13,
        rs * rgb.e21, gs * rgb.e22, bs * rgb.e23,
        rs * rgb.e31, gs * rgb.e32, bs * rgb.e33
    };
    return 0;
}

// Returns a matrix adapting source whitepoint 0 to destination whitepoint 1.
// LodePNG offers XYZ scaling, Bradford, and Von Kries but effectively uses Bradford, so this path is fixed to it.
static void _getAdaptationMatrix(Matrix* m,
                                 float wx0, float wy0, float wz0,
                                 float wx1, float wy1, float wz1)
{
    static const Matrix bradford = {
        0.8951f, 0.2664f, -0.1614f,
        -0.7502f, 1.7135f, 0.0367f,
        0.0389f, -0.0685f, 1.0296f
    };
    static const Matrix bradfordInv = {
        0.9869929f, -0.1470543f, 0.1599627f,
        0.4323053f, 0.5183603f, 0.0492912f,
        -0.0085287f, 0.0400428f, 0.9684867f
    };
    float rho0, gam0, bet0, rho1, gam1, bet1, rho2, gam2, bet2;
    _mulMatrix(&rho0, &gam0, &bet0, bradford, wx0, wy0, wz0);
    _mulMatrix(&rho1, &gam1, &bet1, bradford, wx1, wy1, wz1);
    rho2 = rho1 / rho0;
    gam2 = gam1 / gam0;
    bet2 = bet1 / bet0;
    Matrix scaled = {
        rho2 * bradford.e11, rho2 * bradford.e12, rho2 * bradford.e13,
        gam2 * bradford.e21, gam2 * bradford.e22, gam2 * bradford.e23,
        bet2 * bradford.e31, bet2 * bradford.e32, bet2 * bradford.e33
    };
    *m = bradfordInv * scaled;
}

/* validate whether the ICC profile is supported here for PNG */
static unsigned _validateICC(const LodePNGICC* icc)
{
    return icc->has_chromaticity && icc->has_whitepoint && icc->has_trc;
}

/* Returns chromaticity matrix for given ICC profile, adapted from ICC's
   global illuminant as necessary.
   Also returns the profile's whitepoint. */
static unsigned _getICCChrm(Matrix* m, float whitepoint[3], const LodePNGICC* icc)
{
    /* Adaptation matrix a.
    This is an adaptation needed for ICC's file format (due to it using
    an internal global illuminant unrelated to the actual images) */
    Matrix a;
    /* If the profile has chromatic adaptation matrix "chad", use that one,
    else compute it from the illuminant and whitepoint. */
    if (icc->has_chad) {
        if (!inverse(&icc->chad, &a)) return 1;
    } else {
        _getAdaptationMatrix(&a, icc->illuminant[0], icc->illuminant[1], icc->illuminant[2],
                             icc->white[0], icc->white[1], icc->white[2]);
    }
    /* If the profile has a chad, then also the RGB's whitepoint must also be adapted from it (and the one
    given is normally D50). If it did not have a chad, then the whitepoint given is already the adapted one. */
    if (icc->has_chad) {
        _mulMatrix(&whitepoint[0], &whitepoint[1], &whitepoint[2], a, icc->white[0], icc->white[1], icc->white[2]);
    } else {
        whitepoint[0] = icc->white[0];
        whitepoint[1] = icc->white[1];
        whitepoint[2] = icc->white[2];
    }

    auto rgb = a * icc->rgb;

    if (_getChrmMatrixXYZ(m, rgb, whitepoint)) return 1;
    return 0; /* success */
}

// RGBA8 iCCP specialization of LodePNG's convertToXYZ() setup path.
static unsigned _convertToXYZ_setup(Matrix* source, float whitepoint[3],
                                    float gammatable[3][256], const LodePNGState* state)
{
    LodePNGICC icc = {};
    auto error = _parseICC(&icc, state->info_png.iccp_profile, state->info_png.iccp_profile_size);
    if (error || !_validateICC(&icc)) {
        error = 1;
        goto cleanup; /* corrupted ICC profile */
    }

    // Skip convertToXYZ()'s lodepng_convert(); lodepng_decode() already
    // normalized the source image to the required RGBA8 buffer.

    // Expanded from convertToXYZ()'s "Handle transfer function" block.
    for (size_t c = 0; c < 3; c++) {
        // convertToXYZ_gamma_table, n = 256 (8bit depth), use_icc path
        for (size_t i = 0; i < 256; i++) {
            gammatable[c][i] = _iccForwardTRC(&icc.trc[c], i * (1.0f / 255.0f));
        }
    }

    // see convertToXYZ_chrm -> getChrm only use getICChrm (see use_icc path)
    error = _getICCChrm(source, whitepoint, &icc);

cleanup:
    for (auto& trc : icc.trc) tvg::free(trc.lut);
    return error;
}

// Fixed sRGB specialization of LodePNG's convertFromXYZ() setup path.
// target is convertFromXYZ_chrm()'s "XYZ to linear RGB matrix".
static unsigned _convertFromXYZ_setup(Matrix* target, const float whitepoint[3])
{
    // From getChrm(): "the standard linear sRGB to XYZ matrix".
    Matrix srgb = {
        0.4124564f, 0.3575761f, 0.1804375f,
        0.2126729f, 0.7151522f, 0.0721750f,
        0.0193339f, 0.1191920f, 0.9503041f
    };
    // From getChrm(): sRGB's xyY whitepoint (0.3127, 0.3290, 1) in XYZ.
    static const float srgbWhitepoint[3] = {0.9504559270516716f, 1.0f, 1.0890577507598784f};

    // Invert sRGB-to-XYZ into XYZ-to-linear-sRGB.
    if (!inverse(&srgb, target)) return 1;

    /* for relative rendering intent (any except absolute "3"), must whitepoint adapt to the original whitepoint.
    this also preserves neutral RGB values (with absolute, they could become e.g. blue or sepia) */
    Matrix adaptation;
    /* "white" = absolute whitepoint of the new target RGB space, read from the target color profile.
       "whitepoint" is original absolute whitepoint (input as parameter of this function) of an
       RGB space the XYZ data once had before it was converted to XYZ, in other words the whitepoint that
       we want to adapt our current data to to make sure values that had equal R==G==B in the old space have
       the same property now (white and neutral tones stay neutral).
       Note: "absolute" whitepoint above means, can be used as-is, not needing further adaptation itself like icc.white does.*/
    _getAdaptationMatrix(&adaptation,
                         whitepoint[0], whitepoint[1], whitepoint[2],
                         srgbWhitepoint[0], srgbWhitepoint[1], srgbWhitepoint[2]);

    /* multiply the from xyz matrix with the adaptation matrix: in total,
    the resulting matrix first adapts in XYZ space, then converts to RGB*/
    *target = *target * adaptation;
    return 0;
}

// RGBA8 single-pixel specialization of LodePNG's RGB model conversion.
static inline void _convertRGBPixel(unsigned char* out, const unsigned char* in,
                                    const Matrix& transform, const float gammatable[3][256])
{
    float rgb[3];
    _mulMatrix(&rgb[0], &rgb[1], &rgb[2], transform,
               gammatable[0][in[0]], gammatable[1][in[1]], gammatable[2][in[2]]);

    for (size_t c = 0; c < 3; c++) {
        // This expands convertFromXYZ()'s "Handle transfer function" call to
        // convertFromXYZ_gamma(); its float input uses powf() instead of a lookup table.
        auto& v = rgb[c];
        v = (v < 0.0031308f) ? (v * 12.92f) : (1.055f * powf(v, 1 / 2.4f) - 0.055f);

        out[c] = static_cast<unsigned char>(0.5f + 255.0f * std::min(std::max(0.0f, v), 1.0f));
    }
    out[3] = in[3];
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

// This customizes LodePNG's generic color conversion for ThorVG's fixed RGBA8 input and sRGB output.
unsigned lodepng_toSrgb(unsigned char* out, const unsigned char* in, unsigned w, unsigned h, const LodePNGState* state_in)
{
    // Skip the temporary sRGB output state. Upstream copies info_raw only
    // to preserve the input format; ThorVG always outputs RGBA8 sRGB, 8bit.

    // --- start of the expanded convertRGBModel() path ---

    // Expand convertRGBModel() into setup and per-pixel stages to avoid
    // its full-image buffers; lodepng_decode() already provides RGBA8.

    // Skip modelsEqual(); this function is called only for iCCP input,
    // which upstream always treats as different from the default sRGB target.

    Matrix source;
    float whitepoint[3], gammatable[3][256];  // 256 = 8bit depth
    auto error = _convertToXYZ_setup(&source, whitepoint, gammatable, state_in);
    if (error) return error;

    Matrix transform;
    error = _convertFromXYZ_setup(&transform, whitepoint);
    if (error) return error;

    transform = transform * source;  //combine transforms once for the pixel path

    for (size_t i = 0, n = static_cast<size_t>(w) * h; i < n; i++) {
        _convertRGBPixel(out, in, transform, gammatable);
        out += 4;
        in += 4;
    }

    return 0;
}
