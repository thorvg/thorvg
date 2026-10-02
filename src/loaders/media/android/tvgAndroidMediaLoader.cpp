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
#include <sys/syscall.h>
#include <unistd.h>

#include "tvgAndroidMediaLoader.h"
#include "tvgMath.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static float _seconds(int64_t timeUs)
{
    return static_cast<float>(static_cast<double>(timeUs) / MICROSEC);
}

static ColorSpace _cs()
{
    // MediaCodec decodes into yuv, which has no alpha: an opaque frame is straight and premultiplied alike
    const auto cs = BitmapLoader::cs.load();
    return (cs == ColorSpace::ARGB8888 || cs == ColorSpace::ARGB8888S) ? ColorSpace::ARGB8888 : ColorSpace::ABGR8888;
}

void AndroidMediaLoader::post()
{
    ++request.revision;
    cv.notify_one();
}

void AndroidMediaLoader::postSeek(int64_t timeUs)
{
    request.seekTimeUs = timeUs;
    ++request.seekRevision;
    ring.queued = 0;
    seekPublished = false;
    post();
}

Decoder::Decoded AndroidMediaLoader::decodeFrame(const Request& taken)
{
    const auto nowUs = sink.time();
    VideoDecoder::Frame* frame;
    // the free slot next to the queued frames
    {
        ScopedLock lock(key);
        if (request.seekRevision != taken.seekRevision || request.quit) return Decoder::None;
        // only the latest due frame is kept, as the renderer may lag behind
        while (ring.queued > 1 && ring.frames[(ring.read + 1) % FRAME_COUNT].ptsUs <= nowUs) {
            ring.read = (ring.read + 1) % FRAME_COUNT;
            --ring.queued;
        }
        if (ring.queued == FRAME_COUNT) return Decoder::None;
        frame = &ring.frames[(ring.read + ring.queued) % FRAME_COUNT];
    }

    // only the worker touches a free slot, so the decode runs without the lock
    frame->cs = _cs();
    const auto ret = video.decode(nowUs, *frame);
    if (ret != Decoder::Converted) return ret;

    // the decoded frame joins the queue
    {
        ScopedLock lock(key);
        // a seek during the decode reset the queue, so the frame is out
        if (request.seekRevision != taken.seekRevision) return Decoder::None;
        ++ring.queued;
    }
    return Decoder::Converted;
}

void AndroidMediaLoader::run()
{
    auto status = Status::Seeking;

    unique_lock<mutex> lock(key.mtx);
    while (!request.quit) {
        const auto taken = request;
        lock.unlock();

        // 1. seek: the progress starts over at the target
        if (taken.seekRevision != appliedSeekRevision) {
            if (!sink.seek(taken.seekTimeUs) || !video.seek(taken.seekTimeUs) || (audio.codec && !audio.seek(taken.seekTimeUs))) break;
            status = Status::Seeking;
        }

        // 2. pause: a paused or finished playback holds the clock and the sound
        const auto running = taken.playing && status != Status::Ended;
        if (!running && !sink.play(false)) break;

        // 3. audio refill: a paused playback keeps its sound ready to resume
        if (audio.codec && (running || !sink.primed()) && !audio.refill(sink)) {
            TVGERR("MEDIA", "The audio track failed, the playback goes on silently");
            sink.close();
            audio.close();
        }

        // 4. video decode: a paused playback needs its poster only
        const auto decoded = (running || status == Status::Seeking) ? decodeFrame(taken) : Decoder::None;
        if (decoded == Decoder::Failure) break;
        if (decoded == Decoder::Converted) status = Status::Queued;

        // 5. clock: it starts and stops only with a picture out, or a seek to the end would freeze the previous one
        const auto videoReady = status == Status::Queued || video.drained();
        if (running && videoReady) {
            if (sink.primed() && !sink.play(true)) break;
            if (sink.time() >= durationUs) {
                if (!sink.play(false)) break;
                status = Status::Ended;
            }
        }

        // 6. wait: for the next request once idle, shortly while busy
        const auto idle = status == Status::Ended || (!running && videoReady && sink.primed());

        lock.lock();
        appliedSeekRevision = taken.seekRevision;
        ended = status == Status::Ended;
        if (request.revision != taken.revision || decoded == Decoder::Converted) continue;
        if (ended && taken.looping && taken.playing) postSeek(0);
        else if (idle) cv.wait(lock, [&] { return request.revision != taken.revision; });
        // 20ms for the renderer on a full ring, far within the queued audio, else 4ms to poll the codec output
        else cv.wait_for(lock, chrono::microseconds(ring.queued == FRAME_COUNT ? 20000 : 4000));
    }
    // a quit leaves the loop with the lock, a failure breaks out of the pass without it
    if (!lock.owns_lock()) lock.lock();
    if (!request.quit) {
        failed = true;
        TVGERR("MEDIA", "The media pipeline failed, the playback freezes on its last frame");
    }
    lock.unlock();
    sink.play(false);
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

AndroidMediaLoader::~AndroidMediaLoader()
{
    {
        ScopedLock lock(key);
        request.quit = true;
        post();
    }
    if (worker.joinable()) worker.join();
    sink.close();
    audio.close();
    video.close();
    for (auto& frame : ring.frames) tvg::free(frame.data);
    tvg::free(surface.data);
}

Result AndroidMediaLoader::open(const char* path, TVG_UNUSED const LoaderOps& ops)
{
#ifdef THORVG_FILE_IO_SUPPORT
    const auto fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return Result::InvalidArguments;

    struct stat info;
    const auto ret = fstat(fd, &info) == 0 && open(fd, info.st_size, path);
    ::close(fd);
    return ret ? Result::Success : Result::InvalidArguments;
#else
    return Result::NonSupport;
#endif
}

Result AndroidMediaLoader::open(const char* data, uint32_t size, TVG_UNUSED const LoaderOps& ops)
{
    const auto fd = static_cast<int>(syscall(SYS_memfd_create, "thorvg-media", 0));
    if (fd < 0) return Result::SystemError;

    auto ret = Result::SystemError;
    if (::write(fd, data, size) == static_cast<ssize_t>(size)) ret = open(fd, size, "memory buffer") ? Result::Success : Result::InvalidArguments;
    ::close(fd);
    return ret;
}

bool AndroidMediaLoader::open(int fd, int64_t length, const char* name)
{
    if (!video.open(fd, length, cv)) {
        TVGERR("MEDIA", "No playable video track: %s", name);
        return false;
    }
    // audio is optional: without it the monotonic clock drives the playback
    if (!audio.open(fd, length) || !sink.open(audio.sampleRate, audio.channels)) {
        TVGLOG("MEDIA", "No playable audio track, playing silently: %s", name);
        audio.close();
    }
    durationUs = std::max(video.durationUs, audio.codec ? audio.endUs : 0);
    w = static_cast<float>(video.width);
    h = static_cast<float>(video.height);
    totalTime = _seconds(durationUs);
    paused = true;
    return true;
}

bool AndroidMediaLoader::read()
{
    if (!Loader::read()) return true;

    const auto size = static_cast<size_t>(video.width) * video.height * sizeof(pixel_t);
    // decoded video is opaque: its pixels are premultiplied as they are
    surface.setup(tvg::calloc<pixel_t>(size, 1), video.width, video.width, video.height, sizeof(pixel_t), _cs(), true);
    // the destructor frees the buffers allocated before a failure
    if (!surface.data) return false;
    for (auto& frame : ring.frames) {
        if (!(frame.data = tvg::malloc<pixel_t>(size))) return false;
    }
    worker = thread([this] { run(); });
    return true;
}

bool AndroidMediaLoader::sync()
{
    ScopedLock lock(key);
    // until the worker takes a seek over, its target stands for the time
    if (request.seekRevision != appliedSeekRevision) {
        curTime = _seconds(request.seekTimeUs);
        return false;
    }
    const auto nowUs = std::min(sink.time(), durationUs);
    curTime = _seconds(nowUs);

    // the latest due frame, while the first one of a seek shows at once
    VideoDecoder::Frame* latest = nullptr;
    while (ring.queued > 0) {
        auto& frame = ring.frames[ring.read];
        if (frame.ptsUs > nowUs && (seekPublished || latest)) break;
        latest = &frame;
        ring.read = (ring.read + 1) % FRAME_COUNT;
        --ring.queued;
    }
    // a duplicated picture needs the image update even when another one took the frame
    if (!latest) return sharing > 0 && seekPublished;

    // publish by swapping the buffers: the renderer may convert the surface in place, and the worker overwrites the slot it gets back
    {
        ScopedLock pixels(surface.key);
        std::swap(surface.data, latest->data);
        surface.cs = latest->cs;
    }
    seekPublished = true;
    cv.notify_one();
    return true;
}

Result AndroidMediaLoader::play()
{
    {
        ScopedLock lock(key);
        if (failed) return Result::Unknown;
        // playing a finished playback starts it over
        if (request.seekRevision == appliedSeekRevision && (ended || curTime >= totalTime)) postSeek(0);
        request.playing = true;
        post();
    }
    started = true;
    paused = false;
    return Result::Success;
}

Result AndroidMediaLoader::pause()
{
    {
        ScopedLock lock(key);
        if (failed) return Result::Unknown;
        if (!started || (request.seekRevision == appliedSeekRevision && ended)) return Result::InsufficientCondition;
        request.playing = false;
        post();
    }
    paused = true;
    return Result::Success;
}

Result AndroidMediaLoader::stop()
{
    {
        ScopedLock lock(key);
        if (failed) return Result::Unknown;
        request.playing = false;
        postSeek(0);
    }
    curTime = 0.0f;
    started = false;
    paused = true;
    return Result::Success;
}

Result AndroidMediaLoader::seek(float seconds)
{
    const auto timeUs = std::min(static_cast<int64_t>(static_cast<double>(seconds) * MICROSEC), durationUs);
    {
        ScopedLock lock(key);
        if (failed) return Result::Unknown;
        postSeek(timeUs);
    }
    curTime = _seconds(timeUs);
    return Result::Success;
}

Result AndroidMediaLoader::loop(bool on)
{
    {
        ScopedLock lock(key);
        if (failed) return Result::Unknown;
        request.looping = on;
        post();
    }
    looping = on;
    return Result::Success;
}

Result AndroidMediaLoader::volume(float volume)
{
    {
        ScopedLock lock(key);
        if (failed) return Result::Unknown;
    }
    if (!sink.volume(volume)) return Result::Unknown;
    audioVolume = volume;
    return Result::Success;
}

Result AndroidMediaLoader::mute(bool on)
{
    {
        ScopedLock lock(key);
        if (failed) return Result::Unknown;
    }
    if (!sink.mute(on)) return Result::Unknown;
    muted = on;
    return Result::Success;
}

MediaLoader* MediaLoader::gen()
{
    return new AndroidMediaLoader;
}
