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

#include <atomic>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "tvgCommon.h"
#include "tvgLock.h"
#include "tvgRender.h"

struct Track
{
    static constexpr uint32_t NONE = UINT32_MAX;

    int64_t durationUs = 0;
    uint32_t idx = NONE;
    int32_t width = 1;
    int32_t height = 1;
};

// probe the container and pick the first audio/video track of each kind
bool mediaScan(int fd, int64_t length, Track* audio, Track* video);

struct Decoder
{
    static constexpr int64_t PUMP_POLL_US = 4000;  // measured tradeoff between seek latency and scheduler wake-ups

protected:
    friend struct AndroidMediaLoader;

    AMediaExtractor* extractor = nullptr;
    AMediaCodec* codec = nullptr;
    int64_t seekTargetUs = -1;  // pending seek target PTS; earlier output is discarded, -1 once aligned
    bool inputDone = false;
    bool outputDone = false;

    bool seek(int64_t targetUs, SeekMode mode);
    bool extract();
    void close();
};

struct VideoDecoder : Decoder
{
    static constexpr uint32_t FRAME_COUNT = 3;
    static constexpr uint32_t DRAIN_MAX = 32;  // max codec outputs consumed per pump pass (seek catch-up)

    bool open(int fd, int64_t length, const Track& info);
    void close();
    bool setupFrames(uint32_t w, uint32_t h);
    bool seek(float seconds);
    bool decode(bool& staged, bool& yielded, float seconds = -1.0f);      // requires the loader lock; staged = a frame landed in scratch, yielded = the codec output anything
    bool convert(ColorSpace cs);                                          // no lock needed: scratch and the unpublished slot are scheduler-only
    void commit();                                                        // requires the loader lock
    bool present(RenderSurface* surface, float seconds);  // requires the loader lock; copy the latest due frame (first available after a seek) onto the surface
    int64_t delay(float seconds);                         // µs until the next useful pump: 4ms while filling, up to a frame period when full

private:
    friend struct AndroidMediaLoader;

    struct Frame
    {
        pixel_t* data = nullptr;
        int64_t ptsUs = 0;
    };

    // live MediaCodec output layout, refreshed when the decoder reports a format change
    struct Format
    {
        int32_t width = 1;
        int32_t height = 1;
        int32_t stride = 1;
        int32_t sliceHeight = 1;
        int32_t color = 0;
    } format;

    // copy of one MediaCodec output, allowing its buffer to be released before unlocked YUV conversion
    struct Scratch
    {
        uint8_t* data = nullptr;
        size_t capacity = 0;
        size_t size = 0;
        int64_t ptsUs = 0;
    } scratch;

    struct Ring
    {
        uint32_t width = 0;  // frame buffer size, fixed at open
        uint32_t height = 0;
        Frame frames[FRAME_COUNT];
        uint32_t read = 0;
        uint32_t write = 0;
        uint32_t queued = 0;
    } ring;

    bool frameRequested = false;

    void clearFrames();
    void releaseFrames();
    void updateFormat();
};

struct AudioDecoder : Decoder
{
    static constexpr uint32_t CHUNK_COUNT = 8;

    bool open(int fd, int64_t length, const Track& info, float level, bool muted);
    void close();
    bool volume(float level);
    bool mute(bool on);
    bool play();
    bool pause();
    bool seek(float seconds);
    float curTime();
    bool pump(bool gaplessLoop);

private:
    friend struct AndroidMediaLoader;

    static void callback(SLAndroidSimpleBufferQueueItf, void* context);
    void onBufferConsumed();
    void anchor(int64_t positionUs);
    bool buffered();

    struct Chunk
    {
        uint8_t* data = nullptr;
        size_t capacity = 0;
        int64_t ptsUs = 0;
        uint32_t frames = 0;
    };

    // per-decoder OpenSL sink and playback clock; engine and output mix are process-wide
    struct Sink
    {
        SLObjectItf object = nullptr;
        SLPlayItf player = nullptr;
        SLAndroidSimpleBufferQueueItf queue = nullptr;
        SLVolumeItf volume = nullptr;
        atomic<int64_t> positionUs{0};  // media position recorded at anchorUs
        atomic<int64_t> anchorUs{0};  // monotonic time paired with positionUs
        atomic<bool> realignPending{false};  // clock discontinuity requires video realignment
        bool playing = false;
    } sink;

    struct TrackInfo
    {
        int64_t durationUs = 0;  // authored track length: decode() trims encoder padding beyond it
        int32_t sampleRate = 0;
        int32_t channels = 0;
    } track;

    struct Ring
    {
        StrictKey key;  // guards the ring reset against an in-flight opensl callback (see seek/onBufferConsumed)
        Chunk chunks[CHUNK_COUNT];
        int64_t nextPtsUs = 0;  // next PCM sample time: timestamp gaps before it are filled with silence
        atomic<uint32_t> read{0};
        uint32_t write = 0;
        atomic<uint32_t> queued{0};
    } ring;

    int64_t time();
    bool enqueue(const uint8_t* data, size_t size, int64_t ptsUs);
    bool decode();
};

#endif  //_TVG_ANDROID_MEDIA_DECODER_H_
