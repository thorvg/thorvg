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

#ifndef _TVG_ANDROID_MEDIA_AUDIO_SINK_H_
#define _TVG_ANDROID_MEDIA_AUDIO_SINK_H_

#include <SLES/OpenSLES_Android.h>

#include "tvgLock.h"

// OpenSL player of one media and its playback clock: the clock follows the consumed pcm,
// and runs on the monotonic time over a long gap of the pcm or once no more comes (no audio track, or its end)
struct AudioSink
{
    static constexpr uint32_t CHUNK_COUNT = 16;  // pcm queued ahead: ~340ms of aac at 48kHz

    bool open(int32_t sampleRate, int32_t channels);  // (re)open the player for this pcm format, keeping the volume and mute
    void close();
    bool volume(float level);
    bool mute(bool on);
    bool play(bool on);          // run or hold the clock and the sound
    bool seek(int64_t timeUs);   // drop the queued pcm and restart the paused clock at timeUs
    bool ready(int64_t ptsUs);   // the pcm at ptsUs can be pushed: false while the clock runs over a long gap before it
    bool push(const uint8_t* data, size_t size, int64_t ptsUs);
    void finish();               // no more pcm comes
    bool full();
    bool primed();               // enough pcm to start the clock without starving, or no more comes
    int64_t time();

private:
    void anchor(int64_t timeUs);  // requires key
    int64_t head();               // requires key
    static void callback(SLAndroidSimpleBufferQueueItf queue, void* context);

    // the OpenSL player: the worker drives it, the caller only sets its volume/mute
    struct
    {
        StrictKey key;  // guards against volume/mute from the caller thread
        SLObjectItf object = nullptr;
        SLPlayItf player = nullptr;
        SLAndroidSimpleBufferQueueItf queue = nullptr;
        SLVolumeItf volume = nullptr;
        float level = 1.0f;
        bool muted = false;
    } sl;

    // pcm handed to the player, retired by the buffer callback
    struct Ring
    {
        struct Chunk
        {
            uint8_t* data = nullptr;
            size_t capacity = 0;
            int64_t ptsUs = 0;
            int64_t endUs = 0;
        } chunks[CHUNK_COUNT];
        uint32_t read = 0;
        uint32_t queued = 0;
    } ring;

    StrictKey key;             // guards the ring and the clock against the buffer callback and the renderer
    int64_t tailUs = 0;        // end of the pushed pcm: a short gap after it is filled with silence
    int64_t waitPtsUs = -1;    // the pcm held over a long gap, -1 if none: the emptied queue lets the clock run up to it
    int64_t anchorMediaUs = 0;
    int64_t anchorMonotonicUs = 0;
    int32_t sampleRate = 0;
    int32_t frameSize = 0;     // bytes per pcm frame
    bool playing = false;
    bool ended = true;
};

#endif  //_TVG_ANDROID_MEDIA_AUDIO_SINK_H_
