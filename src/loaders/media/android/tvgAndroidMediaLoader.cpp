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

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tvgAndroidMediaLoader.h"

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

AndroidMediaLoader::AndroidMediaLoader() : MediaLoader(FileType::Media)
{
}

AndroidMediaLoader::~AndroidMediaLoader()
{
    androidMediaScheduler().detach(this);
    audio.close();
    video.close();
    tvg::free(surface.data);
}

bool AndroidMediaLoader::open(const char* path, TVG_UNUSED const LoaderOps* ops)
{
    if (!path) return false;

    auto fd = ::open(path, O_RDONLY);
    if (fd < 0) return false;

    struct stat info;
    Track audioTrack, videoTrack;
    int64_t length, durationUs;

    if (fstat(fd, &info) != 0 || info.st_size <= 0) goto fail;

    length = static_cast<int64_t>(info.st_size);
    if (!mediaScan(fd, length, &audioTrack, &videoTrack)) {
        TVGERR("MEDIA", "Failed to probe the media container: %s", path);
        goto fail;
    }

    durationUs = std::max(audioTrack.durationUs, videoTrack.durationUs);
    if (videoTrack.idx == Track::NONE || durationUs <= 0) {
        TVGERR("MEDIA", "No playable video track found: %s", path);
        goto fail;
    }

    if (!video.open(fd, length, videoTrack, durationUs)) {
        TVGERR("MEDIA", "Failed to open the video decoder: %s", path);
        goto fail;
    }
    //audio is optional: without a track (or once it drains) the wall clock drives playback
    if (audioTrack.idx != Track::NONE && !audio.open(fd, length, audioTrack, audioVolume, muted)) {
        TVGERR("MEDIA", "Failed to open the audio decoder: %s", path);
        video.close();
        goto fail;
    }
    ::close(fd);
    {
        //muxers commonly leave the audio track slightly shorter than the media: allow half a second of slack
        audioCovers = audioTrack.idx != Track::NONE && audioTrack.durationUs + 500000 >= durationUs;

        w = static_cast<float>(videoTrack.width);
        h = static_cast<float>(videoTrack.height);
        totalTime = video.duration;
        curTime = 0.0f;
        paused = true;

        surface.cs = ColorSpace::ABGR8888;
        surface.w = static_cast<uint32_t>(videoTrack.width);
        surface.h = static_cast<uint32_t>(videoTrack.height);
        surface.stride = surface.w;
        surface.channelSize = sizeof(uint32_t);
        surface.premultiplied = true;
        surface.alphaIgnored = true;
        surface.data = tvg::calloc<pixel_t>(static_cast<size_t>(surface.w) * surface.h, sizeof(pixel_t));
        if (!surface.data || !video.setupFrames(surface.w, surface.h)) return false;

        //poster frame: pre-attach, so it never races the scheduler
        frameUpdated = video.still(&surface, 0.0f);
        seeking = !frameUpdated;
        androidMediaScheduler().attach(this);
        return true;
    }

fail:
    ::close(fd);
    return false;
}

float AndroidMediaLoader::time()
{
    auto seconds = audio.curTime();
    //the clock jumped backwards: the audio rolled over into a new loop
    if (looping && seconds + 0.05f < curTime) video.seek(seconds);
    if (totalTime > 0.0f && seconds > totalTime) seconds = totalTime;
    curTime = seconds;
    return seconds;
}

bool AndroidMediaLoader::start()
{
    if (audio.playing) return true;
    if (!audio.pump(looping && audioCovers)) return false;
    return audio.play();   //not yet buffered: play() defers itself via the starting flag
}

int64_t AndroidMediaLoader::pumpFrame()
{
    auto published = 0u;
    for (auto i = 0u; i < VideoDecoder::DRAIN_MAX && published < VideoDecoder::FRAME_COUNT; i++) {
        auto staged = false;
        auto yielded = false;
        auto snapshot = 0u;
        {
            ScopedLock lock(key);
            if (!running && !seeking) return -1;
            if (!audio.pump(looping && audioCovers)) return Decoder::PUMP_POLL_US;
            if (audio.starting && !audio.play()) return Decoder::PUMP_POLL_US;
            auto seconds = time();
            //end of media: wrap the loop, or park on the last frame until the user acts
            if (running && totalTime > 0.0f && seconds >= totalTime && video.queued == 0) {
                if (looping) {
                    //covered media loops via the audio rollover; silent/short tracks wrap here
                    if (!audioCovers) {
                        if (!audio.seek(0.0f) || !video.seek(0.0f) || !start()) return Decoder::PUMP_POLL_US;
                        curTime = 0.0f;
                    }
                    return Decoder::PUMP_POLL_US;
                }
                if (video.outputDone) {
                    audio.pause();
                    running = false;
                    paused = true;
                    return -1;
                }
            }
            if (!video.decode(staged, yielded, (running && !seeking) ? seconds : -1.0f)) return Decoder::PUMP_POLL_US;
            snapshot = video.serial;
        }

        if (!staged) {
            if (!yielded) break;
            continue;             //discarded output: keep draining toward the target
        }

        //the heavy yuv conversion runs outside the lock so the renderer's sync() never waits on it
        if (!video.convert()) return Decoder::PUMP_POLL_US;

        ScopedLock lock(key);
        video.publish(snapshot);
        published++;
    }

    ScopedLock lock(key);
    //surface writes belong to sync(): for a paused seek just park once the target frame is decoded
    if (!running && seeking) return (video.queued > 0 || video.outputDone) ? -1 : Decoder::PUMP_POLL_US;
    return video.wait(curTime);
}

bool AndroidMediaLoader::sync()
{
    {
        ScopedLock lock(key);
        if (frameUpdated) {
            frameUpdated = false;
        } else {
            auto seconds = time();
            if (!video.present(&surface, seconds, seeking)) {
                if (seeking && video.outputDone) seeking = false;
                return false;
            }
            seeking = false;
        }
    }
    androidMediaScheduler().wake();
    return true;
}

Result AndroidMediaLoader::play()
{
    {
        ScopedLock lock(key);
        time();
        //play at the end = replay from the top
        if (totalTime > 0.0f && curTime >= totalTime) {
            if (!audio.seek(0.0f) || !video.seek(0.0f)) return Result::Unknown;
            curTime = 0.0f;
        }

        if (!start()) return Result::Unknown;
        running = true;
        seeking = false;
        paused = false;
    }
    androidMediaScheduler().wake();
    return Result::Success;
}

Result AndroidMediaLoader::pause()
{
    ScopedLock lock(key);
    time();
    //audio owns the media clock
    if (!audio.pause()) return Result::Unknown;
    running = false;
    paused = true;
    return Result::Success;
}

Result AndroidMediaLoader::stop()
{
    {
        ScopedLock lock(key);
        if (!audio.seek(0.0f)) return Result::Unknown;
        if (!video.seek(0.0f)) return Result::Unknown;
        running = false;
        seeking = true;
        paused = true;
    }
    androidMediaScheduler().wake();
    return Result::Success;
}

Result AndroidMediaLoader::seek(float seconds)
{
    //seeking at or past the end is rejected: playback just runs to its natural finish
    if (seconds < 0.0f || (totalTime > 0.0f && seconds >= totalTime)) return Result::InvalidArguments;

    {
        ScopedLock lock(key);
        auto resuming = running;
        if (!audio.seek(seconds)) return Result::Unknown;
        if (!video.seek(seconds)) return Result::Unknown;
        curTime = seconds;
        if (resuming && !start()) return Result::Unknown;
        running = resuming;
        seeking = true;
    }
    androidMediaScheduler().wake();
    return Result::Success;
}

Result AndroidMediaLoader::loop(bool on)
{
    ScopedLock lock(key);
    looping = on;
    return Result::Success;
}

Result AndroidMediaLoader::volume(float volume)
{
    audioVolume = volume;
    return audio.volume(volume, muted) ? Result::Success : Result::Unknown;
}

Result AndroidMediaLoader::mute(bool on)
{
    muted = on;
    return audio.volume(audioVolume, on) ? Result::Success : Result::Unknown;
}