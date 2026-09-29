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

#ifndef _TVG_ANDROID_MEDIA_DECODER_H_
#define _TVG_ANDROID_MEDIA_DECODER_H_

#include <condition_variable>
#include <time.h>
#include <media/NdkImageReader.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>

#include "tvgRender.h"

static constexpr int64_t MICROSEC = 1000000;

inline int64_t monotonic()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * MICROSEC + ts.tv_nsec / 1000;
}

struct AudioSink;

// the extractor and the codec of one track, driven by the media worker
struct Decoder
{
    enum Decoded : uint8_t {None = 0, Converted, Failure};

    AMediaExtractor* extractor = nullptr;
    AMediaCodec* codec = nullptr;
    int64_t durationUs = 0;
    int64_t seekTargetUs = -1;  // seek target: earlier output is dropped, -1 once reached
    int64_t maxPtsUs = -1;      // the largest pts queued since the seek
    bool fed = false;           // the codec took input since its last flush
    bool inputDone = false;
    bool outputDone = false;

    AMediaFormat* open(int fd, int64_t length, const char* kind);  // the format of the first track of the kind, which the caller configures the codec with and deletes
    void close();
    bool seek(int64_t timeUs);
    bool feed();  // fills the free input buffers
};

// renders the codec output into an image reader and converts one image at a time into a frame
struct VideoDecoder : Decoder
{
    struct Frame
    {
        pixel_t* data = nullptr;
        int64_t ptsUs = 0;
        ColorSpace cs = ColorSpace::ABGR8888;
    };

    AImageReader* reader = nullptr;
    int32_t width = 0;
    int32_t height = 0;
    int64_t latestConvertedPtsUs = INT64_MIN;  // the last converted frame
    int64_t averageConvertUs = 0;              // exponential moving average of the conversion cost, 0.1 weight on each new sample

    // the output rendered into the reader, not converted yet
    struct
    {
        int64_t ptsUs = -1;      // -1 when none
        int64_t deadlineUs = 0;  // it fails the pipeline past this time
    } pending;

    // MediaFormat color info, BT.601 limited for untagged content
    struct
    {
        int32_t standard = 0;
        int32_t range = 2;
    } color;

    bool open(int fd, int64_t length, condition_variable& cv);
    void close();
    bool seek(int64_t timeUs);
    Decoded decode(int64_t playheadUs, Frame& frame);  // converts one frame at most

    bool drained()
    {
        return outputDone && pending.ptsUs < 0;
    }

private:
    Decoded acquire(Frame& frame);
    bool parse(AMediaFormat* format);  // reads and deletes the format
};

// decodes the pcm straight into the sink
struct AudioDecoder : Decoder
{
    int64_t endUs = 0;  // track end: the encoder padding past it is dropped
    int32_t sampleRate = 0;
    int32_t channels = 0;
    // the output buffer kept in the codec until the sink is ready for it
    struct
    {
        ssize_t idx = -1;  // -1 when none
        AMediaCodecBufferInfo info;
    } pending;

    bool open(int fd, int64_t length);
    bool seek(int64_t timeUs);
    bool refill(AudioSink& sink);  // fills the sink, false once the track can't play on

private:
    bool parse(AMediaFormat* format);  // reads and deletes the format
};

#endif  //_TVG_ANDROID_MEDIA_DECODER_H_
