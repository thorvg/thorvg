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

#ifndef _TVG_GST_LOADER_H_
#define _TVG_GST_LOADER_H_

#include <gst/gst.h>

#include "tvgMediaLoader.h"

struct GstMediaLoader : MediaLoader
{
    ~GstMediaLoader();

    Result open(const char* data, uint32_t size, const LoaderOps& ops) override;
    Result open(const char* path, const LoaderOps& ops) override;
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
    void state(GstState state);
    GstFlowReturn publish(GstSample* sample);
    bool completeSeek();
    bool seekTo(gint64 position);
    void pollBus();

    static constexpr uint32_t BUFFER_COUNT = 3;

    struct Ring
    {
        uint32_t* frames[BUFFER_COUNT] = {};
        uint32_t write = 0;              // next frame index
        uint32_t latest = 0;             // latest published frame index
        bool updated = false;
    } ring;

    GstElement* playbin = nullptr;       // video to appsink; audio to playbin's default sink
    GstElement* sink = nullptr;          // the appsink, queried for the video position

    struct Seeking
    {
        int64_t pending = -1;            // latest target requested while the previous seek settles
        uint32_t seq = 0;                // latest seqnum, matching the EOS of the current playback
        bool active = false;             // a flushing seek is settling
    } seeking;

    ColorSpace cs = ColorSpace::Unknown;
    StrictKey key;                       // guards the ring against the streaming thread
    bool started = false;                // played since the load or the last stop
    bool ended = false;                  // at the end; the pipeline runs only while !paused && !ended
    bool failed = false;                 // the pipeline posted an error; the controls report Result::Unknown
};

#endif  //_TVG_GST_LOADER_H_
