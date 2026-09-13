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

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <fcntl.h>
#include <mutex>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>

#include "tvgAndroidMediaLoader.h"
#include "tvgArray.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static mutex _mtx;
static condition_variable _cv;
static thread _worker;
static Array<AndroidMediaLoader*> _loaders;

// synchronize with the worker's idle transition so a notification cannot be lost before wait()
static void _wake()
{
    lock_guard<mutex> lock{_mtx};
    _cv.notify_one();
}

// sweep every loader under the lock, then sleep until the next requested wake-up
static void _run()
{
    unique_lock<mutex> lock{_mtx};
    while (!_loaders.empty()) {
        int64_t waitUs = -1;
        for (auto loader : _loaders) {
            auto delayUs = loader->pump();
            if (delayUs < 0) continue;
            if (waitUs < 0 || delayUs < waitUs) waitUs = delayUs;
        }

        if (waitUs < 0) _cv.wait(lock);  // all loaders are Paused/Stopped, or their requested frame is awaiting sync()
        else _cv.wait_for(lock, chrono::microseconds(waitUs));  // Playing, clock sync, or a frame request determines the next pump
    }
}

static void _attach(AndroidMediaLoader* loader)
{
    lock_guard<mutex> lock{_mtx};
    _loaders.push(loader);
    if (!_worker.joinable()) _worker = thread(_run);
    _cv.notify_all();
}

static void _detach(AndroidMediaLoader* loader)
{
    unique_lock<mutex> lock{_mtx};
    for (auto& current : _loaders) {
        if (current != loader) continue;
        current = _loaders.last();
        _loaders.pop();
        if (!_loaders.empty()) return;

        // join without the lock so the worker can observe the empty loader list and exit
        lock.unlock();
        _cv.notify_all();
        _worker.join();
        return;
    }
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

AndroidMediaLoader::~AndroidMediaLoader()
{
    _detach(this);
    audio.close();
    video.close();
    tvg::free(surface.data);
}

bool AndroidMediaLoader::open(const char* path, TVG_UNUSED const LoaderOps& ops)
{
    auto fd = ::open(path, O_RDONLY);
    if (fd < 0) return false;

    auto ret = false;
    struct stat info;
    if (fstat(fd, &info) == 0) ret = open(fd, static_cast<int64_t>(info.st_size), path);

    ::close(fd);
    return ret;
}

bool AndroidMediaLoader::open(const char* data, uint32_t size, TVG_UNUSED const LoaderOps& ops)
{
    auto fd = static_cast<int>(syscall(SYS_memfd_create, "thorvg-media", 0));
    if (fd < 0) return false;

    uint32_t offset = 0;
    while (offset < size) {
        auto written = ::write(fd, data + offset, size - offset);
        if (written <= 0) {
            ::close(fd);
            return false;
        }
        offset += static_cast<uint32_t>(written);
    }

    auto ret = open(fd, size, "memory buffer");
    ::close(fd);
    return ret;
}

bool AndroidMediaLoader::open(int fd, int64_t length, const char* name)
{
    Track audioTrack, videoTrack;

    if (!mediaScan(fd, length, &audioTrack, &videoTrack)) {
        TVGERR("MEDIA", "Failed to probe the media container: %s", name);
        return false;
    }

    auto durationUs = std::max(audioTrack.durationUs, videoTrack.durationUs);
    if (videoTrack.idx == Track::NONE || durationUs <= 0) {
        TVGERR("MEDIA", "No playable video track found: %s", name);
        return false;
    }

    if (!video.open(fd, length, videoTrack)) {
        TVGERR("MEDIA", "Failed to open the video decoder: %s", name);
        return false;
    }
    // audio is optional: without a track (or once it drains) the monotonic clock drives playback
    if (audioTrack.idx != Track::NONE && !audio.open(fd, length, audioTrack, audioVolume, muted)) {
        TVGERR("MEDIA", "Failed to open the audio decoder: %s", name);
        video.close();
        return false;
    }
    // allow a 500000 µs (0.5 s) gap since muxers commonly leave the audio track slightly shorter
    audioCovers = audioTrack.idx != Track::NONE && audioTrack.durationUs + 500000 >= durationUs;
    w = static_cast<float>(videoTrack.width);
    h = static_cast<float>(videoTrack.height);
    playback.duration = static_cast<float>(durationUs) / 1000000.0f;
    totalTime = playback.duration;
    playback.time = curTime = 0.0f;
    playback.state = Playback::State::Stopped;
    paused = true;
    return true;
}

bool AndroidMediaLoader::read()
{
    if (!Loader::read()) return true;

    auto cs = BitmapLoader::cs.load();
    if (cs == ColorSpace::ARGB8888 || cs == ColorSpace::ARGB8888S) cs = ColorSpace::ARGB8888;
    else cs = ColorSpace::ABGR8888;
    auto width = static_cast<uint32_t>(w);
    auto height = static_cast<uint32_t>(h);
    auto data = tvg::malloc<pixel_t>(static_cast<size_t>(width) * height * sizeof(pixel_t));
    // MediaCodec does not guarantee decoded video alpha; this YUV420 path emits opaque premultiplied pixels
    surface.setup(data, width, width, height, sizeof(uint32_t), cs, true);
    if (!surface.data || !video.setupFrames(surface.w, surface.h)) return false;

    video.frameRequested = true;
    _attach(this);
    return true;
}

float AndroidMediaLoader::syncTime()
{
    return playback.time = std::min(audio.curTime(), playback.duration);
}

bool AndroidMediaLoader::start()
{
    // replaying after a natural finish requires rewinding the master clock
    if (playback.time >= playback.duration) {
        if (!audio.seek(0.0f)) return false;
        playback.time = 0.0f;
    }
    if (audio.sink.playing) return true;
    if (!audio.pump(playback.looping && audioCovers)) return false;
    if (!audio.buffered()) return true;
    return audio.play();
}

// run one audio-mastered pipeline pass and return the next wake-up delay (-1 when idle)
int64_t AndroidMediaLoader::pump()
{
    auto published = 0u;
    for (auto i = 0u; i < VideoDecoder::DRAIN_MAX && published < VideoDecoder::FRAME_COUNT; i++) {
        auto staged = false;
        auto yielded = false;
        {
            ScopedLock lock(key);
            auto playing = playback.state == Playback::State::Playing;
            // paused/stopped playback needs no scheduler wake-up without a pending sync or frame request
            if (!playing && !audio.sink.realignPending.load() && !video.frameRequested) return -1;
            // retry shortly when the audio pipeline cannot advance this pass
            if (!audio.pump(playback.looping && audioCovers)) return Decoder::PUMP_POLL_US;
            // start the sink once enough PCM is buffered
            if (playing && !audio.sink.playing && audio.buffered() && !audio.play()) return Decoder::PUMP_POLL_US;

            auto realign = audio.sink.realignPending.exchange(false);
            auto seconds = syncTime();
            // realign video after a manual seek or an audio loop rollover
            if (realign && !video.seek(seconds)) {
                audio.sink.realignPending = true;
                return Decoder::PUMP_POLL_US;
            }

            // ended
            if (playing && seconds >= playback.duration && video.ring.queued == 0) {
                if (playback.looping) {
                    // covered media loops via the audio rollover; silent/short tracks wrap here
                    if (!audioCovers) {
                        if (!audio.seek(0.0f)) return Decoder::PUMP_POLL_US;
                        playback.time = 0.0f;
                    }
                    return Decoder::PUMP_POLL_US;
                }
                if (video.outputDone) {
                    audio.pause();
                    playback.state = Playback::State::Stopped;
                    return -1;
                }
            }

            // decode
            auto decodeTime = playing && !video.frameRequested ? seconds : -1.0f;
            if (!video.decode(staged, yielded, decodeTime)) return Decoder::PUMP_POLL_US;
        }

        if (!staged && !yielded) break;
        if (!staged) continue;  // discarded output: keep draining toward the target

        // the heavy yuv conversion runs outside the lock so the renderer's sync() never waits on it
        if (!video.convert(surface.cs)) return Decoder::PUMP_POLL_US;

        ScopedLock lock(key);
        video.commit();
        published++;
    }

    ScopedLock lock(key);
    // surface writes belong to sync(): for a paused seek just park once the target frame is decoded
    if (playback.state != Playback::State::Playing && video.frameRequested) {
        if (video.ring.queued > 0 || video.outputDone) return -1;
        return Decoder::PUMP_POLL_US;
    }
    return video.delay(playback.time);
}

bool AndroidMediaLoader::sync()
{
    auto updated = false;
    {
        ScopedLock lock(key);
        curTime = syncTime();
        if (!audio.sink.realignPending.load() && video.present(&surface, playback.time)) updated = true;
        if (playback.state == Playback::State::Stopped) paused = true;
    }
    if (updated) _wake();
    return updated;
}

Result AndroidMediaLoader::play()
{
    {
        ScopedLock lock(key);
        if (!start()) TVGERR("MEDIA", "Failed to start playback");
        playback.state = Playback::State::Playing;
        curTime = playback.time;
    }
    paused = false;
    _wake();
    return Result::Success;
}

Result AndroidMediaLoader::pause()
{
    {
        ScopedLock lock(key);
        if (playback.state == Playback::State::Stopped) return Result::InsufficientCondition;
        // audio is the master clock; pausing its sink freezes playback
        if (!audio.pause()) TVGERR("MEDIA", "Failed to pause playback");
        curTime = syncTime();
        playback.state = Playback::State::Paused;
    }
    paused = true;
    return Result::Success;
}

Result AndroidMediaLoader::stop()
{
    {
        ScopedLock lock(key);
        // audio is the master clock; video realigns from it on the next scheduler pass
        if (!audio.seek(0.0f)) TVGERR("MEDIA", "Failed to reset playback");
        curTime = playback.time = 0.0f;
        playback.state = Playback::State::Stopped;
    }
    paused = true;
    _wake();  // wake the scheduler to fetch the first frame after seek
    return Result::Success;
}

Result AndroidMediaLoader::seek(float seconds)
{
    {
        ScopedLock lock(key);
        // audio is the master clock; video realigns from it on the next scheduler pass
        if (!audio.seek(seconds)) TVGERR("MEDIA", "Failed to seek playback");
        curTime = playback.time = seconds;
    }
    _wake();
    return Result::Success;
}

Result AndroidMediaLoader::loop(bool on)
{
    ScopedLock lock(key);
    looping = playback.looping = on;
    return Result::Success;
}

Result AndroidMediaLoader::volume(float volume)
{
    if (!audio.volume(volume)) TVGERR("MEDIA", "Failed to set playback volume");
    audioVolume = volume;
    return Result::Success;
}

Result AndroidMediaLoader::mute(bool on)
{
    if (!audio.mute(on)) TVGERR("MEDIA", "Failed to set playback mute");
    muted = on;
    return Result::Success;
}

MediaLoader* MediaLoader::gen()
{
    return new AndroidMediaLoader;
}
