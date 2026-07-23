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

//probe the container and pick the first audio/video track of each kind
bool mediaScan(int fd, int64_t length, Track* audio, Track* video);

struct Decoder
{
    static constexpr int64_t PUMP_POLL_US = 4000;   //poll interval while the pipeline is starved

    bool outputDone = false;

protected:
    AMediaExtractor* extractor = nullptr;
    AMediaCodec* codec = nullptr;
    int64_t discardBeforeUs = -1;   //discard decoded output older than this (seek preroll)
    bool inputDone = false;

    bool restart(int64_t targetUs, SeekMode mode);
    bool extract();
    void close();
};

struct VideoDecoder : Decoder
{
    static constexpr uint32_t FRAME_COUNT = 3;
    static constexpr uint32_t DRAIN_MAX = 32;   //max codec outputs consumed per pump pass (seek catch-up)

    float duration = 0.0f;
    uint32_t queued = 0;
    uint32_t serial = 0;           //bumped on clearFrames so a stale unlocked conversion gets dropped

    bool open(int fd, int64_t length, const Track& info, int64_t durationUs);
    void close();
    bool setupFrames(uint32_t w, uint32_t h);
    bool seek(float seconds);
    bool decode(bool& staged, bool& yielded, float seconds = -1.0f);     //requires the loader lock; staged = a frame landed in scratch, yielded = the codec output anything
    bool convert();                                                      //no lock needed: scratch and the unpublished slot are scheduler-only
    void publish(uint32_t snapshot);                                     //requires the loader lock; commit the converted frame (dropped if the ring was reset meanwhile)
    bool present(RenderSurface* surface, float seconds, bool immediate); //requires the loader lock; copy the latest due frame (first available if immediate) onto the surface
    bool still(RenderSurface* surface, float seconds);                   //requires the loader lock; decode synchronously until the frame at seconds lands on the surface
    int64_t wait(float seconds);                                         //µs until the next useful pump: 4ms while filling, up to a frame period when full

private:
    //staged codec output, converted to pixels outside the loader lock
    struct Scratch
    {
        uint8_t* data = nullptr;
        size_t capacity = 0;
        size_t size = 0;
        int64_t ptsUs = 0;
    } scratch;

    int32_t width = 1;             //live codec output geometry: diverges from the fixed buffers on a mid-stream format change
    int32_t height = 1;
    int32_t stride = 1;
    int32_t sliceHeight = 1;
    int32_t colorFormat = 0;
    uint32_t bufW = 0;             //frame buffer size, fixed at open
    uint32_t bufH = 0;
    uint32_t read = 0;
    uint32_t write = 0;

    struct Frame
    {
        pixel_t* data = nullptr;
        int64_t ptsUs = 0;
        bool ready = false;
    } frames[FRAME_COUNT];

    void clearFrames();
    void releaseFrames();
    void format();
};

struct AudioDecoder : Decoder
{
    bool playing = false;
    bool starting = false;

    bool open(int fd, int64_t length, const Track& info, float level, bool mute);
    void close();
    bool volume(float level, bool muted);
    bool play();
    bool pause();
    bool seek(float seconds);
    float curTime();
    bool pump(bool rollover);
    void consume();   //the opensl buffer-queue callback entry

private:
    static constexpr uint32_t CHUNK_COUNT = 8;

    SLObjectItf player = nullptr;   //engine and output mix are process-wide, shared across instances
    SLPlayItf playItf = nullptr;
    SLAndroidSimpleBufferQueueItf queueItf = nullptr;
    SLVolumeItf volumeItf = nullptr;
    StrictKey key;   //guards the ring reset against an in-flight opensl callback (see seek/consume)
    atomic<int64_t> timeUs{0};
    atomic<int64_t> anchorUs{0};
    int64_t durationUs = 0;         //authored track length: decode() trims encoder padding beyond it
    atomic<uint32_t> read{0};
    atomic<uint32_t> queued{0};
    uint32_t write = 0;
    int32_t sampleRate = 0;
    int32_t channels = 0;

    struct Chunk
    {
        uint8_t* data = nullptr;
        size_t capacity = 0;
        int64_t ptsUs = 0;
        uint32_t frames = 0;
    } chunks[CHUNK_COUNT];

    int64_t time();
    bool queue(const uint8_t* data, size_t size, int64_t ptsUs);
    bool decode();
};

#endif  //_TVG_ANDROID_MEDIA_DECODER_H_