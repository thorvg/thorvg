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

#ifndef _TVG_ANDROID_MEDIA_LOADER_H_
#define _TVG_ANDROID_MEDIA_LOADER_H_

#include <thread>

#include "tvgAndroidMediaAudioSink.h"
#include "tvgAndroidMediaDecoder.h"
#include "tvgLock.h"
#include "tvgMediaLoader.h"

// every media runs on its own worker, so that one slow decoder never holds back the others
struct AndroidMediaLoader : MediaLoader
{
    ~AndroidMediaLoader();

    Result open(const char* path, const LoaderOps& ops) override;
    Result open(const char* data, uint32_t size, const LoaderOps& ops) override;
    bool read() override;
    bool sync() override;

    Result play() override;
    Result pause() override;
    Result stop() override;
    Result seek(float seconds) override;
    Result loop(bool on) override;
    Result volume(float volume) override;
    Result mute(bool on) override;

private:
    static constexpr uint32_t FRAME_COUNT = 3;

    // the worker's progress since the load or the last seek: no frame yet, a frame queued, the end reached
    enum class Status : uint8_t {Seeking = 0, Queued, Ended};

    struct Request;

    bool open(int fd, int64_t length, const char* name);
    void post();                    // requires key
    void postSeek(int64_t timeUs);  // requires key

    // decode and queue one video frame
    // Converted once queued, None when none is needed or ready yet
    Decoder::Decoded decodeFrame(const Request& taken);

    // worker loop: apply playback/seek requests, refill audio, queue video,
    // and handle looping/waits until quit or failure
    void run();

    // the latest requested playback, coalesced: the worker takes it over as a whole
    struct Request
    {
        uint32_t revision = 0;      // bumped on every change
        uint32_t seekRevision = 0;  // bumped on every seek, so that repeated seeks to one time stay apart
        int64_t seekTimeUs = 0;
        bool playing = false;  // kept over the end, so that a seek plays on from there
        bool looping = false;
        bool quit = false;
    } request;

    // decoded frames waiting for their time
    struct Ring
    {
        VideoDecoder::Frame frames[FRAME_COUNT];
        uint32_t read = 0;
        uint32_t queued = 0;
    } ring;

    StrictKey key;         // guards the request, the ring and the worker states against the caller and the renderer
    condition_variable cv;
    int64_t durationUs = 0;
    uint32_t appliedSeekRevision = 0;  // the frames wait until it catches up with request.seekRevision
    bool seekPublished = false;        // until then the first frame of a seek shows at once
    bool ended = false;
    bool failed = false;
    bool started = false;  // played since the load or the last stop, from the caller only
    VideoDecoder video;
    AudioDecoder audio;
    AudioSink sink;
    thread worker;
};

#endif  //_TVG_ANDROID_MEDIA_LOADER_H_
