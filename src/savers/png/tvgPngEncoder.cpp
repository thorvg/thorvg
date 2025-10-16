/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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
  LodePNG version 20200306

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

    3. This notice may not be removed or altered from any sourcedistribution.
*/
#include "tvgCommon.h"
#include "tvgPngEncoder.h"

/* restrict is not available in C90, but use it when supported by the compiler */
#if (defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))) ||\
    (defined(_MSC_VER) && (_MSC_VER >= 1400)) || \
    (defined(__WATCOMC__) && (__WATCOMC__ >= 1250) && !defined(__cplusplus))
    #define LODEPNG_RESTRICT __restrict
#else
    #define LODEPNG_RESTRICT /* not available */
#endif

/* Try the code, if it returns error, also return the error. */
#define CERROR_TRY_RETURN(call){\
  unsigned error = call;\
  if(error) return error;\
}

#define LODEPNG_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define LODEPNG_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define LODEPNG_ABS(x) ((x) < 0 ? -(x) : (x))

#define FIRST_LENGTH_CODE_INDEX 257

/*
    Often in case of an error a value is assigned to a variable and then it breaks
    out of a loop (to go to the cleanup phase of a function). This macro does that.
    It makes the error handling code shorter and more readable.

    Example: if(!uivector_resize(&lz77_encoded, datasize)) ERROR_BREAK(83);
*/
#define CERROR_BREAK(errorvar, code){\
  errorvar = code;\
  break;\
}

/*256 literals, the end code, some length codes, and 2 unused codes*/
#define NUM_DEFLATE_CODE_SYMBOLS 288
/* version of CERROR_BREAK that assumes the common case where the error variable is named "error" */
#define ERROR_BREAK(code) CERROR_BREAK(error, code)
/*256 literals, the end code, some length codes, and 2 unused codes*/
#define NUM_DEFLATE_CODE_SYMBOLS 288
/*the distance codes have their own symbols, 30 used, 2 unused*/
#define NUM_DISTANCE_SYMBOLS 32
/*the code length codes. 0-15: code lengths, 16: copy previous 3-6 times, 17: 3-10 zeros, 18: 11-138 zeros*/
#define NUM_CODE_LENGTH_CODES 19
/*the base lengths represented by codes 257-285*/
static const unsigned LENGTHBASE[29]
  = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
     67, 83, 99, 115, 131, 163, 195, 227, 258};

/*the extra bits used by codes 257-285 (added to base length)*/
static const unsigned LENGTHEXTRA[29]
  = {0, 0, 0, 0, 0, 0, 0,  0,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,  3,  3,
      4,  4,  4,   4,   5,   5,   5,   5,   0};

/*the base backwards distances (the bits of distance codes appear after length codes and use their own huffman tree)*/
static const unsigned DISTANCEBASE[30]
  = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513,
     769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};

/*the extra bits of backwards distances (added to base)*/
static const unsigned DISTANCEEXTRA[30]
  = {0, 0, 0, 0, 1, 1, 2,  2,  3,  3,  4,  4,  5,  5,   6,   6,   7,   7,   8,
       8,    9,    9,   10,   10,   11,   11,   12,    12,    13,    13};

/*the order in which "code length alphabet code lengths" are stored as specified by deflate, out of this the huffman
tree of the dynamic huffman tree lengths is generated*/
static const unsigned CLCL_ORDER[NUM_CODE_LENGTH_CODES]
  = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};



struct ColorTree
{
    ColorTree* children[16]; /* up to 16 pointers to ColorTree of next level */
    int index; /* the payload. Only has a meaningful value if this is in the last level */
};

/*
Huffman tree struct, containing multiple representations of the tree
*/
struct HuffmanTree
{
    unsigned* codes; /*the huffman codes (bit patterns representing the symbols)*/
    unsigned* lengths; /*the lengths of the huffman codes*/
    unsigned maxbitlen; /*maximum number of bits a single code can get*/
    unsigned numcodes; /*number of symbols in the alphabet = number of codes*/
    /* for reading only */
    unsigned char* table_len; /*length of symbol from lookup table, or max length if secondary lookup needed*/
    unsigned short* table_value; /*value of symbol from lookup table, or pointer to secondary table if needed*/
};

static void HuffmanTree_init(HuffmanTree* tree)
{
    tree->codes = 0;
    tree->lengths = 0;
    tree->table_len = 0;
    tree->table_value = 0;
}

static void HuffmanTree_cleanup(HuffmanTree* tree)
{
    tvg::free(tree->codes);
    tvg::free(tree->lengths);
    tvg::free(tree->table_len);
    tvg::free(tree->table_value);
}

static void lodepng_memcpy(void* LODEPNG_RESTRICT dst, const void* LODEPNG_RESTRICT src, size_t size)
{
    size_t i;
    for (i = 0; i < size; i++) ((char*)dst)[i] = ((const char*)src)[i];
}
static void lodepng_memset(void* LODEPNG_RESTRICT dst, int value, size_t num)
{
    size_t i;
    for (i = 0; i < num; i++) ((char*)dst)[i] = (char)value;
}

/* dynamic vector of unsigned chars */
struct ucvector
{
    unsigned char* data;
    size_t size; /*used size*/
    size_t allocsize; /*allocated size*/
};

/* returns 1 if success, 0 if failure ==> nothing done */
static unsigned ucvector_resize(ucvector* p, size_t size)
{
    if (size > p->allocsize) {
        size_t newsize = size + (p->allocsize >> 1u);
        auto data = tvg::realloc(p->data, newsize);
        if(data) {
            p->allocsize = newsize;
            p->data = (unsigned char*)data;
        }
        else return 0; /*error: not enough memory*/
    }
    p->size = size;
    return 1; /*success*/
}

static ucvector ucvector_init(unsigned char* buffer, size_t size)
{
    ucvector v;
    v.data = buffer;
    v.allocsize = v.size = size;
    return v;
}


/* amount of bits for first huffman table lookup (aka root bits), see HuffmanTree_makeTable and huffmanDecodeSymbol.*/
/* values 8u and 9u work the fastest */
#define FIRSTBITS 9u

/* a symbol value too big to represent any valid symbol, to indicate reading disallowed huffman bits combination,
which is possible in case of only 0 or 1 present symbols. */
#define INVALIDSYMBOL 65535u

static unsigned reverseBits(unsigned bits, unsigned num)
{
    /*TODO: implement faster lookup table based version when needed*/
    unsigned i, result = 0;
    for (i = 0; i < num; i++) result |= ((bits >> (num - i - 1u)) & 1u) << i;
    return result;
}

/* make table for huffman decoding */
static unsigned HuffmanTree_makeTable(HuffmanTree* tree)
{
    static const unsigned headsize = 1u << FIRSTBITS; /*size of the first table*/
    static const unsigned mask = (1u << FIRSTBITS) /*headsize*/ - 1u;
    size_t i, numpresent, pointer, size; /*total table size*/
    unsigned* maxlens = tvg::malloc<unsigned*>(headsize * sizeof(unsigned));
    if (!maxlens) return 83; /*alloc fail*/

    /* compute maxlens: max total bit length of symbols sharing prefix in the first table*/
    lodepng_memset(maxlens, 0, headsize * sizeof(*maxlens));
    for (i = 0; i < tree->numcodes; i++) {
        unsigned symbol = tree->codes[i];
        unsigned l = tree->lengths[i];
        unsigned index;
        if(l <= FIRSTBITS) continue; /*symbols that fit in first table don't increase secondary table size*/
        /*get the FIRSTBITS MSBs, the MSBs of the symbol are encoded first. See later comment about the reversing*/
        index = reverseBits(symbol >> (l - FIRSTBITS), FIRSTBITS);
        maxlens[index] = LODEPNG_MAX(maxlens[index], l);
    }
    /* compute total table size: size of first table plus all secondary tables for symbols longer than FIRSTBITS */
    size = headsize;
    for (i = 0; i < headsize; ++i) {
        unsigned l = maxlens[i];
        if (l > FIRSTBITS) size += (1u << (l - FIRSTBITS));
    }
    tree->table_len = tvg::malloc<unsigned char*>(size * sizeof(*tree->table_len));
    tree->table_value = tvg::malloc<unsigned short*>(size * sizeof(*tree->table_value));
    if (!tree->table_len || !tree->table_value) {
        tvg::free(maxlens);
        /* freeing tree->table values is done at a higher scope */
        return 83; /*alloc fail*/
    }
    /*initialize with an invalid length to indicate unused entries*/
    for (i = 0; i < size; ++i) tree->table_len[i] = 16;

    /*fill in the first table for long symbols: max prefix size and pointer to secondary tables*/
    pointer = headsize;
    for (i = 0; i < headsize; ++i) {
        unsigned l = maxlens[i];
        if(l <= FIRSTBITS) continue;
        tree->table_len[i] = l;
        tree->table_value[i] = pointer;
        pointer += (1u << (l - FIRSTBITS));
    }
    tvg::free(maxlens);

    /*fill in the first table for short symbols, or secondary table for long symbols*/
    numpresent = 0;
    for (i = 0; i < tree->numcodes; ++i) {
        unsigned l = tree->lengths[i];
        unsigned symbol = tree->codes[i]; /*the huffman bit pattern. i itself is the value.*/
        /*reverse bits, because the huffman bits are given in MSB first order but the bit reader reads LSB first*/
        unsigned reverse = reverseBits(symbol, l);
        if (l == 0) continue;
        numpresent++;

        if (l <= FIRSTBITS) {
            /*short symbol, fully in first table, replicated num times if l < FIRSTBITS*/
            unsigned num = 1u << (FIRSTBITS - l);
            unsigned j;
            for (j = 0; j < num; ++j) {
                /*bit reader will read the l bits of symbol first, the remaining FIRSTBITS - l bits go to the MSB's*/
                unsigned index = reverse | (j << l);
                if(tree->table_len[index] != 16) return 55; /*invalid tree: long symbol shares prefix with short symbol*/
                tree->table_len[index] = l;
                tree->table_value[index] = i;
            }
        } else {
            /*long symbol, shares prefix with other long symbols in first lookup table, needs second lookup*/
            /*the FIRSTBITS MSBs of the symbol are the first table index*/
            unsigned index = reverse & mask;
            unsigned maxlen = tree->table_len[index];
            /*log2 of secondary table length, should be >= l - FIRSTBITS*/
            unsigned tablelen = maxlen - FIRSTBITS;
            unsigned start = tree->table_value[index]; /*starting index in secondary table*/
            unsigned num = 1u << (tablelen - (l - FIRSTBITS)); /*amount of entries of this symbol in secondary table*/
            unsigned j;
            if (maxlen < l) return 55; /*invalid tree: long symbol shares prefix with short symbol*/
            for (j = 0; j < num; ++j) {
                unsigned reverse2 = reverse >> FIRSTBITS; /* l - FIRSTBITS bits */
                unsigned index2 = start + (reverse2 | (j << (l - FIRSTBITS)));
                tree->table_len[index2] = l;
                tree->table_value[index2] = i;
            }
        }
    }

    if (numpresent < 2) {
        /* In case of exactly 1 symbol, in theory the huffman symbol needs 0 bits,
        but deflate uses 1 bit instead. In case of 0 symbols, no symbols can
        appear at all, but such huffman tree could still exist (e.g. if distance
        codes are never used). In both cases, not all symbols of the table will be
        filled in. Fill them in with an invalid symbol value so returning them from
        huffmanDecodeSymbol will cause error. */
        for (i = 0; i < size; ++i) {
            if (tree->table_len[i] == 16) {
                /* As length, use a value smaller than FIRSTBITS for the head table,
                and a value larger than FIRSTBITS for the secondary table, to ensure
                valid behavior for advanceBits when reading this symbol. */
                tree->table_len[i] = (i < headsize) ? 1 : (FIRSTBITS + 1);
                tree->table_value[i] = INVALIDSYMBOL;
            }
        }
    } else {
        /* A good huffman tree has N * 2 - 1 nodes, of which N - 1 are internal nodes.
        If that is not the case (due to too long length codes), the table will not
        have been fully used, and this is an error (not all bit combinations can be
        decoded): an oversubscribed huffman tree, indicated by error 55. */
        for (i = 0; i < size; ++i) {
            if (tree->table_len[i] == 16) return 55;
        }
    }
    return 0;
}
/*
  Second step for the ...makeFromLengths and ...makeFromFrequencies functions.
  numcodes, lengths and maxbitlen must already be filled in correctly. return
  value is error.
*/
static unsigned HuffmanTree_makeFromLengths2(HuffmanTree* tree)
{
    unsigned* blcount;
    unsigned* nextcode;
    unsigned error = 0;
    unsigned bits, n;

    tree->codes = tvg::malloc<unsigned*>(tree->numcodes * sizeof(unsigned));
    blcount = tvg::malloc<unsigned*>((tree->maxbitlen + 1) * sizeof(unsigned));
    nextcode = tvg::malloc<unsigned*>((tree->maxbitlen + 1) * sizeof(unsigned));
    if (!tree->codes || !blcount || !nextcode) error = 83; /*alloc fail*/

    if (!error) {
        for (n = 0; n != tree->maxbitlen + 1; n++) blcount[n] = nextcode[n] = 0;
        /*step 1: count number of instances of each code length*/
        for (bits = 0; bits != tree->numcodes; ++bits) ++blcount[tree->lengths[bits]];
        /*step 2: generate the nextcode values*/
        for(bits = 1; bits <= tree->maxbitlen; ++bits) {
            nextcode[bits] = (nextcode[bits - 1] + blcount[bits - 1]) << 1u;
        }
        /*step 3: generate all the codes*/
        for (n = 0; n != tree->numcodes; ++n) {
            if (tree->lengths[n] != 0) {
                tree->codes[n] = nextcode[tree->lengths[n]]++;
                /*remove superfluous bits from the code*/
                tree->codes[n] &= ((1u << tree->lengths[n]) - 1u);
            }
        }
    }

    tvg::free(blcount);
    tvg::free(nextcode);

    if (!error) error = HuffmanTree_makeTable(tree);
    return error;
}

/*
  given the code lengths (as stored in the PNG file), generate the tree as defined
  by Deflate. maxbitlen is the maximum bits that a code in the tree can have.
  return value is error.
*/
static unsigned HuffmanTree_makeFromLengths(HuffmanTree* tree, const unsigned* bitlen, size_t numcodes, unsigned maxbitlen)
{
    unsigned i;
    tree->lengths = tvg::malloc<unsigned*>(numcodes * sizeof(unsigned));
    if (!tree->lengths) return 83; /*alloc fail*/
    for (i = 0; i != numcodes; ++i) tree->lengths[i] = bitlen[i];
    tree->numcodes = (unsigned)numcodes; /*number of symbols*/
    tree->maxbitlen = maxbitlen;
    return HuffmanTree_makeFromLengths2(tree);
}


/*get the literal and length code tree of a deflated block with fixed tree, as per the deflate specification*/
static unsigned generateFixedLitLenTree(HuffmanTree* tree)
{
    unsigned i, error = 0;
    unsigned* bitlen = tvg::malloc<unsigned*>(NUM_DEFLATE_CODE_SYMBOLS * sizeof(unsigned));
    if (!bitlen) return 83; /*alloc fail*/

    /*288 possible codes: 0-255=literals, 256=endcode, 257-285=lengthcodes, 286-287=unused*/
    for (i =   0; i <= 143; ++i) bitlen[i] = 8;
    for (i = 144; i <= 255; ++i) bitlen[i] = 9;
    for (i = 256; i <= 279; ++i) bitlen[i] = 7;
    for (i = 280; i <= 287; ++i) bitlen[i] = 8;

    error = HuffmanTree_makeFromLengths(tree, bitlen, NUM_DEFLATE_CODE_SYMBOLS, 15);

    tvg::free(bitlen);
    return error;
}

/*get the distance code tree of a deflated block with fixed tree, as specified in the deflate specification*/
static unsigned generateFixedDistanceTree(HuffmanTree* tree)
{
    unsigned i, error = 0;
    unsigned* bitlen = tvg::malloc<unsigned*>(NUM_DISTANCE_SYMBOLS * sizeof(unsigned));
    if (!bitlen) return 83; /*alloc fail*/

    /*there are 32 distance codes, but 30-31 are unused*/
    for (i = 0; i != NUM_DISTANCE_SYMBOLS; ++i) bitlen[i] = 5;
    error = HuffmanTree_makeFromLengths(tree, bitlen, NUM_DISTANCE_SYMBOLS, 15);

    tvg::free(bitlen);
    return error;
}

static unsigned getNumColorChannels(LodePNGColorType colortype)
{
    switch(colortype) {
        case LCT_GREY: return 1;
        case LCT_RGB: return 3;
        case LCT_PALETTE: return 1;
        case LCT_GREY_ALPHA: return 2;
        case LCT_RGBA: return 4;
        case LCT_MAX_OCTET_VALUE: return 0;
        default: return 0;
    }
}

static unsigned lodepng_get_bpp_lct(LodePNGColorType colortype, unsigned bitdepth)
{
    return getNumColorChannels(colortype) * bitdepth;
}

static size_t lodepng_get_raw_size_lct(unsigned w, unsigned h, LodePNGColorType colortype, unsigned bitdepth)
{
    size_t bpp = lodepng_get_bpp_lct(colortype, bitdepth);
    size_t n = (size_t)w * (size_t)h;
    return ((n / 8u) * bpp) + ((n & 7u) * bpp + 7u) / 8u;
}

/* Returns the byte size of a raw image buffer with given width, height and color mode */
static size_t lodepng_get_raw_size(unsigned w, unsigned h, const LodePNGColorMode* color)
{
    return lodepng_get_raw_size_lct(w, h, color->colortype, color->bitdepth);
}

/*in an idat chunk, each scanline is a multiple of 8 bits, unlike the lodepng output buffer,
and in addition has one extra byte per line: the filter byte. So this gives a larger
result than lodepng_get_raw_size. Set h to 1 to get the size of 1 row including filter byte. */
static size_t lodepng_get_raw_size_idat(unsigned w, unsigned h, unsigned bpp)
{
    /* + 1 for the filter byte, and possibly plus padding bits per line. */
    /* Ignoring casts, the expression is equal to (w * bpp + 7) / 8 + 1, but avoids overflow of w * bpp */
    size_t line = ((size_t)(w / 8u) * bpp) + 1u + ((w & 7u) * bpp + 7u) / 8u;
    return (size_t)h * line;
}


static int color_tree_get(ColorTree* tree, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    int bit = 0;
    for (bit = 0; bit < 8; ++bit) {
        int i = 8 * ((r >> bit) & 1) + 4 * ((g >> bit) & 1) + 2 * ((b >> bit) & 1) + 1 * ((a >> bit) & 1);
        if (!tree->children[i]) return -1;
        else tree = tree->children[i];
    }
    return tree ? tree->index : -1;
}

/*allocates palette memory if needed, and initializes all colors to black*/
static void lodepng_color_mode_alloc_palette(LodePNGColorMode* info)
{
    size_t i;
    /*if the palette is already allocated, it will have size 1024 so no reallocation needed in that case*/
    /*the palette must have room for up to 256 colors with 4 bytes each.*/
    if (!info->palette) info->palette = tvg::malloc<unsigned char*>(1024);
    if (!info->palette) return; /*alloc fail*/
    for (i = 0; i != 256; ++i) {
        /*Initialize all unused colors with black, the value used for invalid palette indices.
        This is an error according to the PNG spec, but common PNG decoders make it black instead.
        That makes color conversion slightly faster due to no error handling needed.*/
        info->palette[i * 4 + 0] = 0;
        info->palette[i * 4 + 1] = 0;
        info->palette[i * 4 + 2] = 0;
        info->palette[i * 4 + 3] = 255;
    }
}
static void lodepng_palette_clear(LodePNGColorMode* info)
{
    if (info->palette) tvg::free(info->palette);
    info->palette = 0;
    info->palettesize = 0;
}
static void lodepng_color_mode_cleanup(LodePNGColorMode* info)
{
    lodepng_palette_clear(info);
}
static unsigned lodepng_color_mode_copy(LodePNGColorMode* dest, const LodePNGColorMode* source)
{
    lodepng_color_mode_cleanup(dest);
    lodepng_memcpy(dest, source, sizeof(LodePNGColorMode));
    if (source->palette) {
        dest->palette = tvg::malloc<unsigned char*>(1024);
        if (!dest->palette && source->palettesize) return 83; /*alloc fail*/
        lodepng_memcpy(dest->palette, source->palette, source->palettesize * 4);
    }
    return 0;
}
static void lodepng_info_cleanup(LodePNGInfo* info)
{
    lodepng_color_mode_cleanup(&info->color);
}

static void lodepng_color_mode_init(LodePNGColorMode* info)
{
    info->key_defined = 0;
    info->key_r = info->key_g = info->key_b = 0;
    info->colortype = LCT_RGBA;
    info->bitdepth = 8;
    info->palette = 0;
    info->palettesize = 0;
}
static void lodepng_info_init(LodePNGInfo* info)
{
    lodepng_color_mode_init(&info->color);
    info->interlace_method = 0;
    info->compression_method = 0;
    info->filter_method = 0;
}
static int lodepng_color_mode_equal(const LodePNGColorMode* a, const LodePNGColorMode* b)
{
    size_t i;
    if (a->colortype != b->colortype) return 0;
    if (a->bitdepth != b->bitdepth) return 0;
    if (a->key_defined != b->key_defined) return 0;
    if (a->key_defined) {
        if(a->key_r != b->key_r) return 0;
        if(a->key_g != b->key_g) return 0;
        if(a->key_b != b->key_b) return 0;
    }
    if (a->palettesize != b->palettesize) return 0;
    for (i = 0; i != a->palettesize * 4; ++i) {
        if (a->palette[i] != b->palette[i]) return 0;
    }
    return 1;
}
static void color_tree_init(ColorTree* tree)
{
    lodepng_memset(tree->children, 0, 16 * sizeof(*tree->children));
    tree->index = -1;
}
static unsigned color_tree_add(ColorTree* tree, unsigned char r, unsigned char g, unsigned char b, unsigned char a, unsigned index)
{
    int bit;
    for (bit = 0; bit < 8; ++bit) {
        int i = 8 * ((r >> bit) & 1) + 4 * ((g >> bit) & 1) + 2 * ((b >> bit) & 1) + 1 * ((a >> bit) & 1);
        if (!tree->children[i]) {
            tree->children[i] = tvg::malloc<ColorTree*>(sizeof(ColorTree));
            if (!tree->children[i]) return 83; /*alloc fail*/
            color_tree_init(tree->children[i]);
        }
        tree = tree->children[i];
    }
    tree->index = (int)index;
    return 0;
}
static void getPixelColorRGBA16(unsigned short* r, unsigned short* g, unsigned short* b, unsigned short* a, const unsigned char* in, size_t i, const LodePNGColorMode* mode)
{
    if (mode->colortype == LCT_GREY) {
        *r = *g = *b = 256 * in[i * 2 + 0] + in[i * 2 + 1];
        if (mode->key_defined && 256U * in[i * 2 + 0] + in[i * 2 + 1] == mode->key_r) *a = 0;
        else *a = 65535;
    } else if (mode->colortype == LCT_RGB) {
        *r = 256u * in[i * 6 + 0] + in[i * 6 + 1];
        *g = 256u * in[i * 6 + 2] + in[i * 6 + 3];
        *b = 256u * in[i * 6 + 4] + in[i * 6 + 5];
        if (mode->key_defined
          && 256u * in[i * 6 + 0] + in[i * 6 + 1] == mode->key_r
          && 256u * in[i * 6 + 2] + in[i * 6 + 3] == mode->key_g
          && 256u * in[i * 6 + 4] + in[i * 6 + 5] == mode->key_b) *a = 0;
        else *a = 65535;
    } else if (mode->colortype == LCT_GREY_ALPHA) {
        *r = *g = *b = 256u * in[i * 4 + 0] + in[i * 4 + 1];
        *a = 256u * in[i * 4 + 2] + in[i * 4 + 3];
    } else if (mode->colortype == LCT_RGBA) {
        *r = 256u * in[i * 8 + 0] + in[i * 8 + 1];
        *g = 256u * in[i * 8 + 2] + in[i * 8 + 3];
        *b = 256u * in[i * 8 + 4] + in[i * 8 + 5];
        *a = 256u * in[i * 8 + 6] + in[i * 8 + 7];
    }
}
static void rgba16ToPixel(unsigned char* out, size_t i, const LodePNGColorMode* mode, unsigned short r, unsigned short g, unsigned short b, unsigned short a)
{
    if (mode->colortype == LCT_GREY) {
        unsigned short gray = r; /*((unsigned)r + g + b) / 3u;*/
        out[i * 2 + 0] = (gray >> 8) & 255;
        out[i * 2 + 1] = gray & 255;
    } else if (mode->colortype == LCT_RGB) {
        out[i * 6 + 0] = (r >> 8) & 255;
        out[i * 6 + 1] = r & 255;
        out[i * 6 + 2] = (g >> 8) & 255;
        out[i * 6 + 3] = g & 255;
        out[i * 6 + 4] = (b >> 8) & 255;
        out[i * 6 + 5] = b & 255;
    } else if (mode->colortype == LCT_GREY_ALPHA) {
        unsigned short gray = r; /*((unsigned)r + g + b) / 3u;*/
        out[i * 4 + 0] = (gray >> 8) & 255;
        out[i * 4 + 1] = gray & 255;
        out[i * 4 + 2] = (a >> 8) & 255;
        out[i * 4 + 3] = a & 255;
    } else if (mode->colortype == LCT_RGBA) {
        out[i * 8 + 0] = (r >> 8) & 255;
        out[i * 8 + 1] = r & 255;
        out[i * 8 + 2] = (g >> 8) & 255;
        out[i * 8 + 3] = g & 255;
        out[i * 8 + 4] = (b >> 8) & 255;
        out[i * 8 + 5] = b & 255;
        out[i * 8 + 6] = (a >> 8) & 255;
        out[i * 8 + 7] = a & 255;
    }
}
/* index: bitgroup index, bits: bitgroup size(1, 2 or 4), in: bitgroup value, out: octet array to add bits to */
static void addColorBits(unsigned char* out, size_t index, unsigned bits, unsigned in)
{
    unsigned m = bits == 1 ? 7 : bits == 2 ? 3 : 1; /*8 / bits - 1*/
    /*p = the partial index in the byte, e.g. with 4 palettebits it is 0 for first half or 1 for second half*/
    unsigned p = index & m;
    in &= (1u << bits) - 1u; /*filter out any other bits of the input value*/
    in = in << (bits * (m - p));
    if(p == 0) out[index * bits / 8u] = in;
    else out[index * bits / 8u] |= in;
}
/* put a pixel, given its RGBA color, into image of any color type */
static unsigned rgba8ToPixel(unsigned char* out, size_t i, const LodePNGColorMode* mode, ColorTree* tree /*for palette*/, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    if (mode->colortype == LCT_GREY) {
        unsigned char gray = r; /*((unsigned short)r + g + b) / 3u;*/
        if (mode->bitdepth == 8) out[i] = gray;
        else if (mode->bitdepth == 16) out[i * 2 + 0] = out[i * 2 + 1] = gray;
        else {
            /*take the most significant bits of gray*/
            gray = ((unsigned)gray >> (8u - mode->bitdepth)) & ((1u << mode->bitdepth) - 1u);
            addColorBits(out, i, mode->bitdepth, gray);
        }
    } else if (mode->colortype == LCT_RGB) {
        if (mode->bitdepth == 8) {
            out[i * 3 + 0] = r;
            out[i * 3 + 1] = g;
            out[i * 3 + 2] = b;
        } else {
            out[i * 6 + 0] = out[i * 6 + 1] = r;
            out[i * 6 + 2] = out[i * 6 + 3] = g;
            out[i * 6 + 4] = out[i * 6 + 5] = b;
        }
    } else if(mode->colortype == LCT_PALETTE) {
        int index = color_tree_get(tree, r, g, b, a);
        if (index < 0) return 82; /*color not in palette*/
        if (mode->bitdepth == 8) out[i] = index;
        else addColorBits(out, i, mode->bitdepth, (unsigned)index);
    } else if (mode->colortype == LCT_GREY_ALPHA) {
        unsigned char gray = r; /*((unsigned short)r + g + b) / 3u;*/
        if (mode->bitdepth == 8) {
            out[i * 2 + 0] = gray;
            out[i * 2 + 1] = a;
        } else if (mode->bitdepth == 16) {
            out[i * 4 + 0] = out[i * 4 + 1] = gray;
            out[i * 4 + 2] = out[i * 4 + 3] = a;
        }
    } else if (mode->colortype == LCT_RGBA) {
        if (mode->bitdepth == 8) {
            out[i * 4 + 0] = r;
            out[i * 4 + 1] = g;
            out[i * 4 + 2] = b;
            out[i * 4 + 3] = a;
        } else {
            out[i * 8 + 0] = out[i * 8 + 1] = r;
            out[i * 8 + 2] = out[i * 8 + 3] = g;
            out[i * 8 + 4] = out[i * 8 + 5] = b;
            out[i * 8 + 6] = out[i * 8 + 7] = a;
        }
    }
    return 0; /*no error*/
}
static unsigned char readBitFromReversedStream(size_t* bitpointer, const unsigned char* bitstream)
{
    unsigned char result = (unsigned char)((bitstream[(*bitpointer) >> 3] >> (7 - ((*bitpointer) & 0x7))) & 1);
    ++(*bitpointer);
    return result;
}

/* TODO: make this faster */
static unsigned readBitsFromReversedStream(size_t* bitpointer, const unsigned char* bitstream, size_t nbits)
{
    unsigned result = 0;
    size_t i;
    for (i = 0 ; i < nbits; ++i) {
        result <<= 1u;
        result |= (unsigned)readBitFromReversedStream(bitpointer, bitstream);
    }
    return result;
}
static void getPixelColorsRGBA8(unsigned char* LODEPNG_RESTRICT buffer, size_t numpixels, const unsigned char* LODEPNG_RESTRICT in, const LodePNGColorMode* mode)
{
    unsigned num_channels = 4;
    size_t i;
    if (mode->colortype == LCT_GREY) {
        if (mode->bitdepth == 8) {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                buffer[0] = buffer[1] = buffer[2] = in[i];
                buffer[3] = 255;
            }
            if (mode->key_defined) {
                buffer -= numpixels * num_channels;
                for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                    if(buffer[0] == mode->key_r) buffer[3] = 0;
                }
            }
        } else if (mode->bitdepth == 16) {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                buffer[0] = buffer[1] = buffer[2] = in[i * 2];
                buffer[3] = mode->key_defined && 256U * in[i * 2 + 0] + in[i * 2 + 1] == mode->key_r ? 0 : 255;
            }
        } else {
            unsigned highest = ((1U << mode->bitdepth) - 1U); /* highest possible value for this bit depth */
            size_t j = 0;
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                unsigned value = readBitsFromReversedStream(&j, in, mode->bitdepth);
                buffer[0] = buffer[1] = buffer[2] = (value * 255) / highest;
                buffer[3] = mode->key_defined && value == mode->key_r ? 0 : 255;
            }
        }
    } else if (mode->colortype == LCT_RGB) {
        if (mode->bitdepth == 8) {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                lodepng_memcpy(buffer, &in[i * 3], 3);
                buffer[3] = 255;
            }
            if (mode->key_defined) {
                buffer -= numpixels * num_channels;
                for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                    if (buffer[0] == mode->key_r && buffer[1]== mode->key_g && buffer[2] == mode->key_b) buffer[3] = 0;
                }
            }
        } else {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                buffer[0] = in[i * 6 + 0];
                buffer[1] = in[i * 6 + 2];
                buffer[2] = in[i * 6 + 4];
                buffer[3] = mode->key_defined
                  && 256U * in[i * 6 + 0] + in[i * 6 + 1] == mode->key_r
                  && 256U * in[i * 6 + 2] + in[i * 6 + 3] == mode->key_g
                  && 256U * in[i * 6 + 4] + in[i * 6 + 5] == mode->key_b ? 0 : 255;
            }
        }
    } else if (mode->colortype == LCT_PALETTE) {
        if (mode->bitdepth == 8) {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
              unsigned index = in[i];
              /* out of bounds of palette not checked: see lodepng_color_mode_alloc_palette. */
              lodepng_memcpy(buffer, &mode->palette[index * 4], 4);
            }
        } else {
            size_t j = 0;
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
              unsigned index = readBitsFromReversedStream(&j, in, mode->bitdepth);
              /* out of bounds of palette not checked: see lodepng_color_mode_alloc_palette. */
              lodepng_memcpy(buffer, &mode->palette[index * 4], 4);
            }
        }
    } else if (mode->colortype == LCT_GREY_ALPHA) {
        if (mode->bitdepth == 8) {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
              buffer[0] = buffer[1] = buffer[2] = in[i * 2 + 0];
              buffer[3] = in[i * 2 + 1];
            }
        } else {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
              buffer[0] = buffer[1] = buffer[2] = in[i * 4 + 0];
              buffer[3] = in[i * 4 + 2];
            }
        }
    } else if (mode->colortype == LCT_RGBA) {
        if (mode->bitdepth == 8) {
            lodepng_memcpy(buffer, in, numpixels * 4);
        } else {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                buffer[0] = in[i * 8 + 0];
                buffer[1] = in[i * 8 + 2];
                buffer[2] = in[i * 8 + 4];
                buffer[3] = in[i * 8 + 6];
            }
        }
    }
}

static void color_tree_cleanup(ColorTree* tree)
{
    int i;
    for (i = 0; i != 16; ++i) {
        if(tree->children[i]) {
            color_tree_cleanup(tree->children[i]);
            tvg::free(tree->children[i]);
        }
    }
}

static void getPixelColorsRGB8(unsigned char* LODEPNG_RESTRICT buffer, size_t numpixels, const unsigned char* LODEPNG_RESTRICT in, const LodePNGColorMode* mode)
{
    const unsigned num_channels = 3;
    size_t i;
    if (mode->colortype == LCT_GREY) {
        if (mode->bitdepth == 8) {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
               buffer[0] = buffer[1] = buffer[2] = in[i];
            }
        } else if (mode->bitdepth == 16) {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                buffer[0] = buffer[1] = buffer[2] = in[i * 2];
            }
        } else {
            unsigned highest = ((1U << mode->bitdepth) - 1U); /* highest possible value for this bit depth */
            size_t j = 0;
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                unsigned value = readBitsFromReversedStream(&j, in, mode->bitdepth);
                buffer[0] = buffer[1] = buffer[2] = (value * 255) / highest;
            }
        }
    } else if (mode->colortype == LCT_RGB) {
        if (mode->bitdepth == 8) {
           lodepng_memcpy(buffer, in, numpixels * 3);
        } else {
            for(i = 0; i != numpixels; ++i, buffer += num_channels) {
                buffer[0] = in[i * 6 + 0];
                buffer[1] = in[i * 6 + 2];
                buffer[2] = in[i * 6 + 4];
            }
        }
    } else if (mode->colortype == LCT_PALETTE) {
        if (mode->bitdepth == 8) {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                unsigned index = in[i];
                /* out of bounds of palette not checked: see lodepng_color_mode_alloc_palette. */
                lodepng_memcpy(buffer, &mode->palette[index * 4], 3);
            }
        } else {
            size_t j = 0;
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                unsigned index = readBitsFromReversedStream(&j, in, mode->bitdepth);
                /* out of bounds of palette not checked: see lodepng_color_mode_alloc_palette. */
                lodepng_memcpy(buffer, &mode->palette[index * 4], 3);
            }
        }
    } else if (mode->colortype == LCT_GREY_ALPHA) {
        if (mode->bitdepth == 8) {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                buffer[0] = buffer[1] = buffer[2] = in[i * 2 + 0];
            }
        } else {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                buffer[0] = buffer[1] = buffer[2] = in[i * 4 + 0];
            }
        }
    } else if (mode->colortype == LCT_RGBA) {
        if (mode->bitdepth == 8) {
            for(i = 0; i != numpixels; ++i, buffer += num_channels) {
                lodepng_memcpy(buffer, &in[i * 4], 3);
            }
        } else {
            for (i = 0; i != numpixels; ++i, buffer += num_channels) {
                buffer[0] = in[i * 8 + 0];
                buffer[1] = in[i * 8 + 2];
                buffer[2] = in[i * 8 + 4];
            }
        }
    }
}
static void getPixelColorRGBA8(unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a, const unsigned char* in, size_t i, const LodePNGColorMode* mode)
{
    if (mode->colortype == LCT_GREY) {
        if (mode->bitdepth == 8) {
            *r = *g = *b = in[i];
            if (mode->key_defined && *r == mode->key_r) *a = 0;
            else *a = 255;
        } else if (mode->bitdepth == 16) {
            *r = *g = *b = in[i * 2 + 0];
            if (mode->key_defined && 256U * in[i * 2 + 0] + in[i * 2 + 1] == mode->key_r) *a = 0;
            else *a = 255;
        } else {
            unsigned highest = ((1U << mode->bitdepth) - 1U); /* highest possible value for this bit depth */
            size_t j = i * mode->bitdepth;
            unsigned value = readBitsFromReversedStream(&j, in, mode->bitdepth);
            *r = *g = *b = (value * 255) / highest;
            if (mode->key_defined && value == mode->key_r) *a = 0;
            else *a = 255;
        }
    } else if (mode->colortype == LCT_RGB) {
        if (mode->bitdepth == 8) {
            *r = in[i * 3 + 0]; *g = in[i * 3 + 1]; *b = in[i * 3 + 2];
            if (mode->key_defined && *r == mode->key_r && *g == mode->key_g && *b == mode->key_b) *a = 0;
            else *a = 255;
        } else {
            *r = in[i * 6 + 0];
            *g = in[i * 6 + 2];
            *b = in[i * 6 + 4];
            if (mode->key_defined && 256U * in[i * 6 + 0] + in[i * 6 + 1] == mode->key_r
              && 256U * in[i * 6 + 2] + in[i * 6 + 3] == mode->key_g
              && 256U * in[i * 6 + 4] + in[i * 6 + 5] == mode->key_b) *a = 0;
            else *a = 255;
        }
    } else if (mode->colortype == LCT_PALETTE) {
        unsigned index;
        if (mode->bitdepth == 8) index = in[i];
        else {
            size_t j = i * mode->bitdepth;
            index = readBitsFromReversedStream(&j, in, mode->bitdepth);
        }
        /* out of bounds of palette not checked: see lodepng_color_mode_alloc_palette. */
        *r = mode->palette[index * 4 + 0];
        *g = mode->palette[index * 4 + 1];
        *b = mode->palette[index * 4 + 2];
        *a = mode->palette[index * 4 + 3];
    } else if (mode->colortype == LCT_GREY_ALPHA) {
        if (mode->bitdepth == 8) {
            *r = *g = *b = in[i * 2 + 0];
            *a = in[i * 2 + 1];
        } else {
            *r = *g = *b = in[i * 4 + 0];
            *a = in[i * 4 + 2];
        }
    } else if (mode->colortype == LCT_RGBA) {
        if (mode->bitdepth == 8) {
            *r = in[i * 4 + 0];
            *g = in[i * 4 + 1];
            *b = in[i * 4 + 2];
            *a = in[i * 4 + 3];
        } else {
            *r = in[i * 8 + 0];
            *g = in[i * 8 + 2];
            *b = in[i * 8 + 4];
            *a = in[i * 8 + 6];
        }
    }
}
static unsigned lodepng_convert(unsigned char* out, const unsigned char* in, const LodePNGColorMode* mode_out, const LodePNGColorMode* mode_in, unsigned w, unsigned h)
{
    size_t i;
    ColorTree tree;
    size_t numpixels = (size_t)w * (size_t)h;
    unsigned error = 0;

    if (mode_in->colortype == LCT_PALETTE && !mode_in->palette) {
        return 107; /* error: must provide palette if input mode is palette */
    }

    if (lodepng_color_mode_equal(mode_out, mode_in)) {
        size_t numbytes = lodepng_get_raw_size(w, h, mode_in);
        lodepng_memcpy(out, in, numbytes);
        return 0;
    }

    if (mode_out->colortype == LCT_PALETTE) {
        size_t palettesize = mode_out->palettesize;
        const unsigned char* palette = mode_out->palette;
        size_t palsize = (size_t)1u << mode_out->bitdepth;
        /* if the user specified output palette but did not give the values, assume
           they want the values of the input color type (assuming that one is palette).
           Note that we never create a new palette ourselves.*/
        if (palettesize == 0) {
            palettesize = mode_in->palettesize;
            palette = mode_in->palette;
            /* if the input was also palette with same bitdepth, then the color types are also
               equal, so copy literally. This to preserve the exact indices that were in the PNG
               even in case there are duplicate colors in the palette.*/
            if (mode_in->colortype == LCT_PALETTE && mode_in->bitdepth == mode_out->bitdepth) {
                size_t numbytes = lodepng_get_raw_size(w, h, mode_in);
                lodepng_memcpy(out, in, numbytes);
                return 0;
            }
        }
        if (palettesize < palsize) palsize = palettesize;
        color_tree_init(&tree);
        for (i = 0; i != palsize; ++i) {
            const unsigned char* p = &palette[i * 4];
            error = color_tree_add(&tree, p[0], p[1], p[2], p[3], (unsigned)i);
            if (error) break;
        }
    }

    if (!error) {
        if (mode_in->bitdepth == 16 && mode_out->bitdepth == 16) {
            for (i = 0; i != numpixels; ++i) {
                unsigned short r = 0, g = 0, b = 0, a = 0;
                getPixelColorRGBA16(&r, &g, &b, &a, in, i, mode_in);
                rgba16ToPixel(out, i, mode_out, r, g, b, a);
            }
        } else if (mode_out->bitdepth == 8 && mode_out->colortype == LCT_RGBA) {
            getPixelColorsRGBA8(out, numpixels, in, mode_in);
        } else if(mode_out->bitdepth == 8 && mode_out->colortype == LCT_RGB) {
            getPixelColorsRGB8(out, numpixels, in, mode_in);
        } else {
            unsigned char r = 0, g = 0, b = 0, a = 0;
            for (i = 0; i != numpixels; ++i) {
                getPixelColorRGBA8(&r, &g, &b, &a, in, i, mode_in);
                error = rgba8ToPixel(out, i, mode_out, &tree, r, g, b, a);
                if (error) break;
            }
        }
    }

    if (mode_out->colortype == LCT_PALETTE) {
        color_tree_cleanup(&tree);
    }

    return error;
}

#define DEFAULT_WINDOWSIZE 2048

static void lodepng_compress_settings_init(LodePNGCompressSettings* settings)
{
	/*compress with dynamic huffman tree (not in the mathematical sense, just not the predefined one)*/
	settings->btype = 2;
	settings->use_lz77 = 1;
	settings->windowsize = DEFAULT_WINDOWSIZE;
	settings->minmatch = 3;
	settings->nicematch = 128;
	settings->lazymatching = 1;

	settings->custom_zlib = 0;
	settings->custom_deflate = 0;
	settings->custom_context = 0;
}
static void lodepng_encoder_settings_init(LodePNGEncoderSettings* settings)
{
	lodepng_compress_settings_init(&settings->zlibsettings);
	settings->filter_palette_zero = 1;
	settings->filter_strategy = LFS_MINSUM;
	settings->auto_convert = 1;
	settings->force_palette = 0;
	settings->predefined_filters = 0;
}

static void lodepng_state_init(LodePNGState* state)
{
    lodepng_encoder_settings_init(&state->encoder);
    lodepng_color_mode_init(&state->info_raw);
    lodepng_info_init(&state->info_png);
    state->error = 1;
}

static void lodepng_state_cleanup(LodePNGState* state)
{
    lodepng_color_mode_cleanup(&state->info_raw);
    lodepng_info_cleanup(&state->info_png);
}

/* Safely check if adding two integers will overflow (no undefined
behavior, compiler removing the code, etc...) and output result. */
static int lodepng_addofl(size_t a, size_t b, size_t* result)
{
    *result = a + b; /* Unsigned addition is well defined and safe in C90 */
    return *result < a;
}

static unsigned lodepng_read32bitInt(const unsigned char* buffer)
{
    return (((unsigned)buffer[0] << 24u) | ((unsigned)buffer[1] << 16u) | ((unsigned)buffer[2] << 8u) | (unsigned)buffer[3]);
}

/*
  Gets the length of the data of the chunk. Total chunk length has 12 bytes more.
  There must be at least 4 bytes to read from. If the result value is too large,
  it may be corrupt data.
*/
static unsigned lodepng_chunk_length(const unsigned char* chunk)
{
    return lodepng_read32bitInt(&chunk[0]);
}

static void setBitOfReversedStream(size_t* bitpointer, unsigned char* bitstream, unsigned char bit)
{
    /*the current bit in bitstream may be 0 or 1 for this to work*/
    if (bit == 0) bitstream[(*bitpointer) >> 3u] &=  (unsigned char)(~(1u << (7u - ((*bitpointer) & 7u))));
    else bitstream[(*bitpointer) >> 3u] |=  (1u << (7u - ((*bitpointer) & 7u)));
    ++(*bitpointer);
}

/* ////////////////////////////////////////////////////////////////////////// */
/* / Adler32                                                                / */
/* ////////////////////////////////////////////////////////////////////////// */

static unsigned update_adler32(unsigned adler, const unsigned char* data, unsigned len)
{
    unsigned s1 = adler & 0xffffu;
    unsigned s2 = (adler >> 16u) & 0xffffu;

    while (len != 0u) {
        unsigned i;
        /*at least 5552 sums can be done before the sums overflow, saving a lot of module divisions*/
        unsigned amount = len > 5552u ? 5552u : len;
        len -= amount;
        for (i = 0; i != amount; ++i) {
            s1 += (*data++);
            s2 += s1;
        }
        s1 %= 65521u;
        s2 %= 65521u;
    }

    return (s2 << 16u) | s1;
}

/*Return the adler32 of the bytes data[0..len-1]*/
static unsigned adler32(const unsigned char* data, unsigned len)
{
    return update_adler32(1u, data, len);
}


/* Paeth predictor, used by PNG filter type 4
   The parameters are of type short, but should come from unsigned chars, the shorts
   are only needed to make the paeth calculation correct.
*/
static unsigned char paethPredictor(short a, short b, short c)
{
    short pa = LODEPNG_ABS(b - c);
    short pb = LODEPNG_ABS(a - c);
    short pc = LODEPNG_ABS(a + b - c - c);
    /* return input value associated with smallest of pa, pb, pc (with certain priority if equal) */
    if (pb < pa) { a = b; pa = pb; }
    return (pc < pa) ? c : a;
}

/*shared values used by multiple Adam7 related functions*/
static const unsigned ADAM7_IX[7] = { 0, 4, 0, 2, 0, 1, 0 }; /*x start values*/
static const unsigned ADAM7_IY[7] = { 0, 0, 4, 0, 2, 0, 1 }; /*y start values*/
static const unsigned ADAM7_DX[7] = { 8, 8, 4, 4, 2, 2, 1 }; /*x delta values*/
static const unsigned ADAM7_DY[7] = { 8, 8, 8, 4, 4, 2, 2 }; /*y delta values*/

/* Outputs various dimensions and positions in the image related to the Adam7 reduced images.
   passw: output containing the width of the 7 passes
   passh: output containing the height of the 7 passes
   filter_passstart: output containing the index of the start and end of each
   reduced image with filter bytes
   padded_passstart output containing the index of the start and end of each
   reduced image when without filter bytes but with padded scanlines
   passstart: output containing the index of the start and end of each reduced
   image without padding between scanlines, but still padding between the images
   w, h: width and height of non-interlaced image
   bpp: bits per pixel
   "padded" is only relevant if bpp is less than 8 and a scanline or image does not
   end at a full byte */
static void Adam7_getpassvalues(unsigned passw[7], unsigned passh[7], size_t filter_passstart[8], size_t padded_passstart[8], size_t passstart[8], unsigned w, unsigned h, unsigned bpp)
{
    /* the passstart values have 8 values: the 8th one indicates the byte after the end of the 7th (= last) pass */
    unsigned i;

    /* calculate width and height in pixels of each pass */
    for (i = 0; i != 7; ++i) {
        passw[i] = (w + ADAM7_DX[i] - ADAM7_IX[i] - 1) / ADAM7_DX[i];
        passh[i] = (h + ADAM7_DY[i] - ADAM7_IY[i] - 1) / ADAM7_DY[i];
        if(passw[i] == 0) passh[i] = 0;
        if(passh[i] == 0) passw[i] = 0;
    }

    filter_passstart[0] = padded_passstart[0] = passstart[0] = 0;
    for (i = 0; i != 7; ++i) {
        /* if passw[i] is 0, it's 0 bytes, not 1 (no filtertype-byte) */
        filter_passstart[i + 1] = filter_passstart[i]
                                + ((passw[i] && passh[i]) ? passh[i] * (1u + (passw[i] * bpp + 7u) / 8u) : 0);
        /* bits padded if needed to fill full byte at end of each scanline */
        padded_passstart[i + 1] = padded_passstart[i] + passh[i] * ((passw[i] * bpp + 7u) / 8u);
        /* only padded at end of reduced image */
        passstart[i + 1] = passstart[i] + (passh[i] * passw[i] * bpp + 7u) / 8u;
    }
}

/*checks if the colortype is valid and the bitdepth bd is allowed for this colortype.
Return value is a LodePNG error code.*/
static unsigned checkColorValidity(LodePNGColorType colortype, unsigned bd)
{
    switch(colortype) {
        case LCT_GREY:       if(!(bd == 1 || bd == 2 || bd == 4 || bd == 8 || bd == 16)) return 37; break;
        case LCT_RGB:        if(!(                                 bd == 8 || bd == 16)) return 37; break;
        case LCT_PALETTE:    if(!(bd == 1 || bd == 2 || bd == 4 || bd == 8            )) return 37; break;
        case LCT_GREY_ALPHA: if(!(                                 bd == 8 || bd == 16)) return 37; break;
        case LCT_RGBA:       if(!(                                 bd == 8 || bd == 16)) return 37; break;
        case LCT_MAX_OCTET_VALUE: return 31; /* invalid color type */
        default: return 31; /* invalid color type */
    }
    return 0; /*allowed color type / bits combination*/
}

#include "tvgPngEncoder2"