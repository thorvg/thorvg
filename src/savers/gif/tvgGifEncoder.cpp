/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

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


//
// gif.h
// by Charlie Tangora
// Public domain.
// Email me : ctangora -at- gmail -dot- com
//
// This file offers a simple, very limited way to create animated GIFs directly in code.
//
// Those looking for particular cleverness are likely to be disappointed; it's pretty
// much a straight-ahead implementation of the GIF format with optional Floyd-Steinberg
// dithering. (It does at least use delta encoding - only the changed portions of each
// frame are saved.)
//
// So resulting files are often quite large. The hope is that it will be handy nonetheless
// as a quick and easily-integrated way for programs to spit out animations.
//
// Only RGBA8 is currently supported as an input format. (The alpha is ignored.)
//
// USAGE:
// Create a GifWriter struct. Pass it to GifBegin() to initialize and write the header.
// Pass subsequent frames to GifWriteFrame().
// Finally, call GifEnd() to close the file handle and free memory.
//

#include "tvgMath.h"
#include "tvgGifEncoder.h"


#define TRANSPARENT_IDX 0
#define TRANSPARENT_THRESHOLD 127
#define BIT_DEPTH 8
#define LEAF_NODE 0xff
#define MAX_PAL_COLORS ((1 << BIT_DEPTH) - 1)

// Simple structure to write out the LZW-compressed portion of the image
// one bit at a time
typedef struct
{
    uint8_t bitIndex;  // how many bits in the partial byte written so far
    uint8_t byte;      // current partial byte

    uint32_t chunkIndex;
    uint8_t chunk[256];   // bytes are written in here until we have 256 of them, then written to the file
} GifBitStatus;


// The LZW dictionary is a 256-ary tree constructed as the file is encoded,
// this is one node
typedef struct
{
    uint16_t m_next[256];
} GifLzwNode;

// The boxes the quantization cuts the pixels into.
// Box i covers the pixels [start[i], start[i] + len[i]) of the working image.
typedef struct
{
    int64_t sse[MAX_PAL_COLORS];      // SSE (Sum of Squared Errors) over the box
    int start[MAX_PAL_COLORS];        // first pixel of this box within tmpImage
    int len[MAX_PAL_COLORS];          // how many pixels it holds
    uint8_t axis[MAX_PAL_COLORS];     // the channel carrying most of the error
    uint8_t avg[MAX_PAL_COLORS * 3];  // the color that represents the box. alpha channel is not included.
} GifBoxes;

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

// walks the k-d tree to pick the palette entry for a desired color.
// Takes as in/out parameters the current best color and its error -
// only changes them if it finds a better color in its subtree.
// this is the major hotspot in the code at the moment.
static void _getClosestPaletteColor( GifPalette* pPal, int r, int g, int b, int* bestInd, int* bestDiff, int treeRoot )
{
    // base case, reached a leaf or a node that holds a single color
    if (treeRoot > (1 << BIT_DEPTH) - 1 || pPal->treeSplitElt[treeRoot] == LEAF_NODE) {
        // A node marked LEAF_NODE keeps its palette entry in treeSplit
        int ind = (treeRoot > (1 << BIT_DEPTH) - 1) ? treeRoot - (1 << BIT_DEPTH) : pPal->treeSplit[treeRoot];
        if(ind == TRANSPARENT_IDX) return;

        // check whether this color is better than the current winner
        int r_err = r - ((int32_t)pPal->color[ind].r);
        int g_err = g - ((int32_t)pPal->color[ind].g);
        int b_err = b - ((int32_t)pPal->color[ind].b);
        int diff = abs(r_err)+ abs(g_err) + abs(b_err);

        if(diff < *bestDiff) {
            *bestInd = ind;
            *bestDiff = diff;
        }
        return;
    }

    // take the appropriate color (r, g, or b) for this node of the k-d tree
    int comps[3] = {r, g, b};

    int splitComp = comps[pPal->treeSplitElt[treeRoot]];

    int splitPos = pPal->treeSplit[treeRoot];
    if (splitPos > splitComp) {
        // check the left subtree
        _getClosestPaletteColor(pPal, r, g, b, bestInd, bestDiff, treeRoot*2);
        if (*bestDiff > splitPos - splitComp) {
            // cannot prove there's not a better value in the right subtree, check that too
            _getClosestPaletteColor(pPal, r, g, b, bestInd, bestDiff, treeRoot*2+1);
        }
    } else {
        _getClosestPaletteColor(pPal, r, g, b, bestInd, bestDiff, treeRoot*2+1);
        if (*bestDiff > splitComp - splitPos) {
            _getClosestPaletteColor(pPal, r, g, b, bestInd, bestDiff, treeRoot*2);
        }
    }
}

static void _swapPixels(uint8_t* image, int pixA, int pixB, int stride)
{
    for (int cc = 0; cc < stride; ++cc) {
        uint8_t temp = image[pixA * stride + cc];
        image[pixA * stride + cc] = image[pixB * stride + cc];
        image[pixB * stride + cc] = temp;
    }
}

// Moves every pixel whose 'elt' component is below splitVal to the front and
// returns how many were moved. Since the comparison is one-sided, pixels that
// share the same value always end up on the same side of the boundary.
static int _partitionByValue(uint8_t* image, int numPixels, int elt, int splitVal)
{
    int storeIndex = 0;
    for (int ii = 0; ii < numPixels; ++ii) {
        if (image[ii * 4 + elt] < splitVal) {
            _swapPixels(image, ii, storeIndex, 4);
            ++storeIndex;
        }
    }
    return storeIndex;
}

// Returns the value carried by the count/2-th element along the given channel.
// 'below' receives how many elements fall strictly under the returned value.
static int _findMedianValue(const uint8_t* data, int count, int axis, int stride, int* below)
{
    uint32_t hist[256] = {};
    for (int ii = 0; ii < count; ++ii)
        ++hist[data[ii * stride + axis]];

    int acc = 0;
    for (int v = 0; v < 256; ++v) {
        acc += hist[v];
        if (acc > count / 2) {
            if (below) *below = acc - (int)hist[v];
            return v;
        }
    }
    return 0;
}

// Computes the SSE, axis and average color of a box.
// The SSE is how much color error it holds, the axis is the channel holding most of it.
// A box with one color has an SSE of 0, so it won't be split.
static void _computeBoxStats(const uint8_t* image, GifBoxes* boxes, int idx)
{
    auto pixels = image + boxes->start[idx] * 4;
    int64_t sum[3] = {};
    int64_t sqsum[3] = {};

    auto len = boxes->len[idx];

    for (int ii = 0; ii < len; ++ii) {
        for (int cc = 0; cc < 3; ++cc) {
            int64_t v = pixels[ii * 4 + cc];
            sum[cc] += v;
            sqsum[cc] += v * v;
        }
    }

    int64_t worst = -1;
    boxes->sse[idx] = 0;
    boxes->axis[idx] = 0;

    // Compute the squared error of each axis
    // and store average color
    for (int cc = 0; cc < 3; ++cc) {
        // sqerr = sqsum - sum*sum/len
        // sum*sum/len = q*q*len + 2*q*r + r*r/len
        auto q = sum[cc] / len;
        auto r = sum[cc] % len;
        auto sqerr = sqsum[cc] - (q * q * len + 2 * q * r + r * r / len);

        boxes->sse[idx] += sqerr;
        if (sqerr > worst) {
            worst = sqerr;
            boxes->axis[idx] = (uint8_t)cc;
        }
        boxes->avg[idx * 3 + cc] = (uint8_t)((sum[cc] + len / 2) / len);
    }
}

// Cuts the box in two at the median value of the axis that carries the most error.
// The left stays in idx, the right goes to rhsIdx.
static void _splitBox(uint8_t* image, GifBoxes* boxes, int idx, int rhsIdx)
{
    auto pixels = image + boxes->start[idx] * 4;

    // Find the median value along that axis from a histogram
    int below = -1;
    int splitVal = _findMedianValue(pixels, boxes->len[idx], boxes->axis[idx], 4, &below);

    // If the median is the smallest value in this box,
    // move the boundary up by one to keep its whole run on the left, otherwise
    // the left comes out empty and _computeBoxStats will be broken.
    if (below == 0) ++splitVal;

    auto left = _partitionByValue(pixels, boxes->len[idx], boxes->axis[idx], splitVal);

    boxes->start[rhsIdx] = boxes->start[idx] + left;
    boxes->len[rhsIdx] = boxes->len[idx] - left;

    boxes->len[idx] = left;

    _computeBoxStats(image, boxes, idx);
    _computeBoxStats(image, boxes, rhsIdx);
}

// Finds all pixels that have changed from the previous image and
// moves them to the from of the buffer.
// This allows us to build a palette optimized for the colors of the
// changed pixels only.
// With no previous frame, every visible pixel counts as changed.
static int _pickChangedPixels(const uint8_t* lastFrame, uint8_t* frame, int numPixels, bool transparent)
{
    int numChanged = 0;
    uint8_t* writeIter = frame;

    for (int ii=0; ii < numPixels; ++ii) {
        if (frame[3] >= TRANSPARENT_THRESHOLD) {
            if (!lastFrame || transparent || (lastFrame[0] != frame[0] || lastFrame[1] != frame[1] || lastFrame[2] != frame[2])) {
                writeIter[0] = frame[0];
                writeIter[1] = frame[1];
                writeIter[2] = frame[2];
                ++numChanged;
                writeIter += 4;
            }
        }
        if (lastFrame) lastFrame += 4;
        frame += 4;
    }

    return numChanged;
}

// Builds the k-d tree the lookup walks, over the finished palette colors.
// Every node splits its colors in half, so the walk never goes deeper than BIT_DEPTH.
// firstElt/lastElt are the palette slots this subtree owns.
// The root owns [1, 256), since 0 is reserved for transparency.
static void _buildSearchTree(GifPalette* pal, uint8_t* colors, int count, int firstElt, int lastElt, int treeRoot)
{
    // Nothing left to split
    if (count == 1) {
        pal->color[firstElt] = {colors[0], colors[1], colors[2]};

        // If current depth is not 8
        if (treeRoot <= (1 << BIT_DEPTH) - 1) {
            pal->treeSplitElt[treeRoot] = LEAF_NODE;
            pal->treeSplit[treeRoot] = (uint8_t)firstElt;
        }

        // Palette entries in range [firstElt + 1, lastElt) remain the same as prior frame,
        // but never accessed by _getClosestPaletteColor. So it is safe.
        // Palette dump may show these unused 'ghost entries'.
        return;
    }

    // Cut on the channel the colors are most spread out over
    uint8_t min[3] = {255, 255, 255};
    uint8_t max[3] = {0, 0, 0};
    for (int ii = 0; ii < count; ++ii) {
        for (int cc = 0; cc < 3; ++cc) {
            if (colors[ii * 3 + cc] < min[cc]) min[cc] = colors[ii * 3 + cc];
            if (colors[ii * 3 + cc] > max[cc]) max[cc] = colors[ii * 3 + cc];
        }
    }

    // Find the axis with the largest range
    int axis = 0;
    for (int cc = 1; cc < 3; ++cc) {
        if (max[cc] - min[cc] > max[axis] - min[axis])
            axis = cc;
    }

    // Find the median value along that axis from a histogram
    int splitVal = _findMedianValue(colors, count, axis, 3, nullptr);

    // Use Dutch National Flag to reorder colors
    int low = 0;
    int mid = 0;
    int high = count;
    while (mid < high) {
        int v = colors[mid * 3 + axis];
        if (v < splitVal) {
            _swapPixels(colors, mid, low, 3);
            ++low;
            ++mid;
        } else if (v > splitVal) {
            --high;
            _swapPixels(colors, mid, high, 3);
        } else {
            ++mid;
        }
    }

    auto splitElt = (firstElt + lastElt) / 2;

    pal->treeSplitElt[treeRoot] = (uint8_t)axis;
    pal->treeSplit[treeRoot] = (uint8_t)splitVal;

    _buildSearchTree(pal, colors, count / 2, firstElt, splitElt, treeRoot * 2);
    _buildSearchTree(pal, colors + (count / 2) * 3, count - count / 2, splitElt, lastElt, treeRoot * 2 + 1);
}

// Quantizes the pixels into up to MAX_PAL_COLORS colors by cutting them into boxes in RGB space and averaging each box.
// A k-d tree is then built over those colors, which is what fills in the palette entries.
static void _makePalette(GifWriter* writer, const uint8_t* lastFrame, const uint8_t* nextFrame, uint32_t width, uint32_t height, bool transparent)
{
    auto& pal = writer->pal;

    size_t imageSize = (size_t)(width * height * 4 * sizeof(uint8_t));
    memcpy(writer->tmpImage, nextFrame, imageSize);

    int numPixels = (int)(width * height);
    numPixels = _pickChangedPixels(lastFrame, writer->tmpImage, numPixels, transparent);

    if (numPixels == 0) return;

    // Start with a single large box that contains the picked pixels
    GifBoxes boxes;
    boxes.start[0] = 0;
    boxes.len[0] = numPixels;
    _computeBoxStats(writer->tmpImage, &boxes, 0);

    int numBoxes = 1;

    // Split boxes until there are MAX_PAL_COLORS of them
    while (numBoxes < MAX_PAL_COLORS) {
        // Select the box with the highest error for splitting
        int target = -1;
        int64_t worst = 0;
        for (int ii = 0; ii < numBoxes; ++ii) {
            if (boxes.sse[ii] > worst) {
                worst = boxes.sse[ii];
                target = ii;
            }
        }

        // If every box holds a single color
        if (target < 0) break;

        _splitBox(writer->tmpImage, &boxes, target, numBoxes);
        ++numBoxes;
    }

    _buildSearchTree(&pal, boxes.avg, numBoxes, 1, 1 << BIT_DEPTH, 1);
}


void _palettizePixel(const uint8_t* nextFrame, uint8_t* outFrame, GifPalette* pPal)
{
    int32_t bestDiff = 1000000;
    int32_t bestInd = 1;
    _getClosestPaletteColor(pPal, nextFrame[0], nextFrame[1], nextFrame[2], &bestInd, &bestDiff, 1);

    // Write the resulting color to the output buffer
    outFrame[0] = pPal->color[bestInd].r;
    outFrame[1] = pPal->color[bestInd].g;
    outFrame[2] = pPal->color[bestInd].b;
    outFrame[3] = (uint8_t)bestInd;
}


// Picks palette colors for the image using simple threshholding, no dithering
static void _thresholdImage(GifWriter* writer, const uint8_t* lastFrame, const uint8_t* nextFrame,  uint32_t width, uint32_t height, bool transparent)
{
    auto outFrame = writer->oldImage;
    uint32_t numPixels = width*height;

    if (transparent) {
        for (uint32_t ii = 0; ii < numPixels; ++ii) {
            if (nextFrame[3] < TRANSPARENT_THRESHOLD) {
                outFrame[0] = 0;
                outFrame[1] = 0;
                outFrame[2] = 0;
                outFrame[3] = TRANSPARENT_IDX;
            } else {
                _palettizePixel(nextFrame, outFrame, &writer->pal);
            }
            if (lastFrame) lastFrame += 4;
            outFrame += 4;
            nextFrame += 4;
        }
    } else {
        for (uint32_t ii = 0; ii < numPixels; ++ii) {
            // if a previous color is available, and it matches the current color,
            // set the pixel to transparent
            if(lastFrame && lastFrame[0] == nextFrame[0] && lastFrame[1] == nextFrame[1] && lastFrame[2] == nextFrame[2]) {
                outFrame[0] = lastFrame[0];
                outFrame[1] = lastFrame[1];
                outFrame[2] = lastFrame[2];
                outFrame[3] = TRANSPARENT_IDX;
            } else {
                _palettizePixel(nextFrame, outFrame, &writer->pal);
            }
            if (lastFrame) lastFrame += 4;
            outFrame += 4;
            nextFrame += 4;
        }
    }
}


// insert a single bit
static void _writeBit(GifBitStatus* stat, uint32_t bit)
{
    bit = bit & 1;
    bit = bit << stat->bitIndex;
    stat->byte |= bit;

    ++stat->bitIndex;
    if (stat->bitIndex > 7) {
        // move the newly-finished byte to the chunk buffer
        stat->chunk[stat->chunkIndex++] = stat->byte;
        // and start a new byte
        stat->bitIndex = 0;
        stat->byte = 0;
    }
}


// write all bytes so far to the file
static void _writeChunk(FILE* f, GifBitStatus* stat)
{
    fputc((int)stat->chunkIndex, f);
    fwrite(stat->chunk, 1, stat->chunkIndex, f);

    stat->bitIndex = 0;
    stat->byte = 0;
    stat->chunkIndex = 0;
}


static void _writeCode(FILE* f, GifBitStatus* stat, uint32_t code, uint32_t length)
{
    for (uint32_t ii = 0; ii < length; ++ii) {
        _writeBit(stat, code);
        code = code >> 1;
        if (stat->chunkIndex == 255) _writeChunk(f, stat);
    }
}


// write a 256-color (8-bit) image palette to the file
static void _writePalette(const GifPalette* pPal, FILE* f)
{
    fputc(0, f);  // first color: transparency
    fputc(0, f);
    fputc(0, f);

    for (int ii = 1; ii < (1 << BIT_DEPTH); ++ii) {
        fputc((int)pPal->color[ii].r, f);
        fputc((int)pPal->color[ii].g, f);
        fputc((int)pPal->color[ii].b, f);
    }
}


// write the image header, LZW-compress and write out the image
static void _writeLzwImage(GifWriter* writer, uint32_t width, uint32_t height, uint32_t delay, bool transparent)
{
    auto f = writer->f;
    auto image = writer->oldImage;

    // graphics control extension
    fputc(0x21, f);
    fputc(0xf9, f);
    fputc(0x04, f);
    fputc((transparent ? 0x09 : 0x05), f);  //clear prev frame or not.
    fputc(delay & 0xff, f);
    fputc((delay >> 8) & 0xff, f);
    fputc(TRANSPARENT_IDX, f); // transparent color index
    fputc(0, f);

    fputc(0x2c, f); // image descriptor block

    // corner of image (left, top) in canvas space
    fputc(0, f);
    fputc(0, f);
    fputc(0, f);
    fputc(0, f);

    fputc(width & 0xff, f);          // width and height of image
    fputc((width >> 8) & 0xff, f);
    fputc(height & 0xff, f);
    fputc((height >> 8) & 0xff, f);

    //fputc(0, f); // no local color table, no transparency
    //fputc(0x80, f); // no local color table, but transparency

    fputc(0x80 + BIT_DEPTH - 1, f); // local color table present, 2 ^ bitDepth entries
    _writePalette(&writer->pal, f);

    const int minCodeSize = BIT_DEPTH;
    const uint32_t clearCode = 1 << BIT_DEPTH;

    fputc(minCodeSize, f); // min code size 8 bits

    GifLzwNode* codetree = tvg::malloc<GifLzwNode>(sizeof(GifLzwNode)*4096);

    memset(codetree, 0, sizeof(GifLzwNode)*4096);
    int32_t curCode = -1;
    uint32_t codeSize = (uint32_t)minCodeSize + 1;
    uint32_t maxCode = clearCode+1;

    GifBitStatus stat;
    stat.byte = 0;
    stat.bitIndex = 0;
    stat.chunkIndex = 0;

    _writeCode(f, &stat, clearCode, codeSize);  // start with a fresh LZW dictionary

    for (uint32_t yy = 0; yy < height; ++yy) {
        for (uint32_t xx=0; xx<width; ++xx) {
            // top-left origin
            uint8_t nextValue = image[(yy*width+xx)*4+3];

            // "loser mode" - no compression, every single code is followed immediately by a clear
            //WriteCode( f, stat, nextValue, codeSize );
            //WriteCode( f, stat, 256, codeSize );

            if (curCode < 0) {
                // first value in a new run
                curCode = nextValue;
            } else if (codetree[curCode].m_next[nextValue]) {
                // current run already in the dictionary
                curCode = codetree[curCode].m_next[nextValue];
            } else {
                // finish the current run, write a code
                _writeCode(f, &stat, (uint32_t)curCode, codeSize);

                // insert the new run into the dictionary
                codetree[curCode].m_next[nextValue] = (uint16_t)++maxCode;

                if (maxCode >= (1ul << codeSize)) {
                    // dictionary entry count has broken a size barrier,
                    // we need more bits for codes
                    codeSize++;
                }
                if (maxCode == 4095) {
                    // the dictionary is full, clear it out and begin anew
                    _writeCode(f, &stat, clearCode, codeSize); // clear tree

                    memset(codetree, 0, sizeof(GifLzwNode)*4096);
                    codeSize = (uint32_t)(minCodeSize + 1);
                    maxCode = clearCode+1;
                }

                curCode = nextValue;
            }
        }
    }

    // compression footer
    _writeCode(f, &stat, (uint32_t)curCode, codeSize);
    _writeCode(f, &stat, clearCode, codeSize);
    _writeCode(f, &stat, clearCode + 1, (uint32_t)minCodeSize + 1);

    // write out the last partial chunk
    while (stat.bitIndex) _writeBit(&stat, 0);
    if (stat.chunkIndex) _writeChunk(f, &stat);

    fputc(0, f); // image block terminator

    tvg::free(codetree);
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


bool gifBegin(GifWriter* writer, const char* filename, uint32_t width, uint32_t height, uint32_t delay)
{
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
	writer->f = 0;
    fopen_s(&writer->f, filename, "wb");
#else
    writer->f = fopen(filename, "wb");
#endif
    if (!writer->f) return false;

    writer->firstFrame = true;

    // allocate
    writer->oldImage = tvg::malloc<uint8_t>(width*height*4);
    writer->tmpImage = tvg::malloc<uint8_t>(width*height*4);

    fputs("GIF89a", writer->f);

    // screen descriptor
    fputc(width & 0xff, writer->f);
    fputc((width >> 8) & 0xff, writer->f);
    fputc(height & 0xff, writer->f);
    fputc((height >> 8) & 0xff, writer->f);

    fputc(0xf0, writer->f);  // there is an unsorted global color table of 2 entries
    fputc(0, writer->f);     // background color
    fputc(0, writer->f);     // pixels are square (we need to specify this because it's 1989)

    // now the "global" palette (really just a dummy palette)
    // color 0: black
    fputc(0, writer->f);
    fputc(0, writer->f);
    fputc(0, writer->f);
    // color 1: also black
    fputc(0, writer->f);
    fputc(0, writer->f);
    fputc(0, writer->f);

    if(delay != 0) {
        // animation header
        fputc(0x21, writer->f); // extension
        fputc(0xff, writer->f); // application specific
        fputc(11, writer->f); // length 11
        fputs("NETSCAPE2.0", writer->f); // yes, really
        fputc(3, writer->f); // 3 bytes of NETSCAPE2.0 data

        fputc(1, writer->f); // JUST BECAUSE
        fputc(0, writer->f); // loop infinitely (byte 0)
        fputc(0, writer->f); // loop infinitely (byte 1)

        fputc(0, writer->f); // block terminator
    }

    memset(&writer->pal, 0, sizeof(GifPalette));

    return true;
}


bool gifWriteFrame(GifWriter* writer, const uint8_t* image, uint32_t width, uint32_t height, uint32_t delay, bool transparent)
{
    if (!writer->f) return false;

    const uint8_t* oldImage = writer->firstFrame? NULL : writer->oldImage;
    writer->firstFrame = false;

    _makePalette(writer, oldImage, image, width, height, transparent);
    _thresholdImage(writer, oldImage, image, width, height, transparent);
    _writeLzwImage(writer, width, height, delay, transparent);

    return true;
}


bool gifEnd(GifWriter* writer)
{
    if (!writer->f) return false;

    fputc(0x3b, writer->f); // end of file
    fclose(writer->f);
    tvg::free(writer->oldImage);
    tvg::free(writer->tmpImage);

    writer->f = NULL;
    writer->oldImage = NULL;

    return true;
}
