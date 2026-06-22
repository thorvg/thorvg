/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
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
#include <math.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <string>
#include <thread>

#include "tvgAndroidMediaLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static constexpr auto USEC = 1000000LL;
static constexpr auto COLOR_YUV420_PLANAR = 19;
static constexpr auto COLOR_YUV420_SEMIPLANAR = 21;

struct AndroidImpl
{
    AndroidMediaLoader* loader = nullptr;
    std::string path;
    AMediaFormat* format = nullptr;
    AMediaExtractor* extractor = nullptr;
    AMediaCodec* codec = nullptr;
    std::thread worker;
    std::chrono::steady_clock::time_point started;

    tvg::StrictKey key;
    tvg::StrictKey codecKey;
    uint32_t* frame = nullptr;
    uint32_t* surfaceData = nullptr;
    float latestTime = 0.0f;
    int64_t startedUs = 0;
    int64_t durationUs = 0;
    int64_t decodedUs = -1;
    int32_t track = -1;
    int32_t width = 0;
    int32_t height = 0;
    int32_t stride = 0;
    int32_t slice = 0;
    int32_t rotation = 0;
    int32_t color = COLOR_YUV420_SEMIPLANAR;
    bool frameUpdated = false;
    bool inputDone = false;
    bool outputDone = false;
    bool ready = false;
    bool quit = false;
};

static int64_t _us(float seconds)
{
    return static_cast<int64_t>(seconds * static_cast<float>(USEC) + 0.5f);
}

static uint8_t _clamp(int32_t v)
{
    return static_cast<uint8_t>(std::min(255, std::max(0, v)));
}

static int64_t _time(AndroidImpl* impl)
{
    auto loader = impl->loader;
    auto us = loader->paused ? _us(loader->curTime) : impl->startedUs + std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - impl->started).count();

    if (impl->durationUs > 0) {
        if (!loader->paused && loader->looping) us %= impl->durationUs;
        else if (us > impl->durationUs) us = impl->durationUs;
    }
    return us;
}

static void _closeCodec(AndroidImpl* impl)
{
    if (impl->codec) {
        AMediaCodec_stop(impl->codec);
        AMediaCodec_delete(impl->codec);
        impl->codec = nullptr;
    }
    if (impl->extractor) {
        AMediaExtractor_delete(impl->extractor);
        impl->extractor = nullptr;
    }
    impl->inputDone = false;
    impl->outputDone = false;
    impl->decodedUs = -1;
}

static void _clear(AndroidImpl* impl)
{
    _closeCodec(impl);
    if (impl->format) {
        AMediaFormat_delete(impl->format);
        impl->format = nullptr;
    }
    tvg::free(impl->frame);
    tvg::free(impl->surfaceData);
    impl->frame = nullptr;
    impl->surfaceData = nullptr;
    impl->path.clear();
    impl->ready = false;
    impl->frameUpdated = false;
}

static void _readFormat(AndroidImpl* impl, AMediaFormat* format)
{
    auto v = 0;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &v) && v > 0) impl->width = v;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &v) && v > 0) impl->height = v;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_STRIDE, &v) && v > 0) impl->stride = v;
    else impl->stride = impl->width;
    if (AMediaFormat_getInt32(format, "slice-height", &v) && v > 0) impl->slice = v;
    else impl->slice = impl->height;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, &v)) impl->color = v;
    if (AMediaFormat_getInt32(format, "rotation-degrees", &v)) impl->rotation = ((v % 360) + 360) % 360;
}

static bool _reset(AndroidImpl* impl, int64_t us)
{
    _closeCodec(impl);

    const char* mime = nullptr;
    if (!impl->format || !AMediaFormat_getString(impl->format, AMEDIAFORMAT_KEY_MIME, &mime)) return false;

    impl->extractor = AMediaExtractor_new();
    if (!impl->extractor || AMediaExtractor_setDataSource(impl->extractor, impl->path.c_str()) != AMEDIA_OK) return false;
    if (AMediaExtractor_selectTrack(impl->extractor, static_cast<size_t>(impl->track)) != AMEDIA_OK) return false;
    if (us > 0) AMediaExtractor_seekTo(impl->extractor, us, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);

    impl->codec = AMediaCodec_createDecoderByType(mime);
    if (!impl->codec) return false;
    if (AMediaCodec_configure(impl->codec, impl->format, nullptr, nullptr, 0) != AMEDIA_OK) return false;
    if (AMediaCodec_start(impl->codec) != AMEDIA_OK) return false;

    impl->inputDone = false;
    impl->outputDone = false;
    impl->decodedUs = -1;
    return true;
}

static bool _feed(AndroidImpl* impl, int64_t timeout)
{
    if (impl->inputDone) return false;

    auto idx = AMediaCodec_dequeueInputBuffer(impl->codec, timeout);
    if (idx < 0) return false;

    auto size = size_t(0);
    auto buffer = AMediaCodec_getInputBuffer(impl->codec, static_cast<size_t>(idx), &size);
    auto sample = buffer ? AMediaExtractor_readSampleData(impl->extractor, buffer, size) : -1;

    if (sample < 0) {
        AMediaCodec_queueInputBuffer(impl->codec, static_cast<size_t>(idx), 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
        impl->inputDone = true;
        return true;
    }

    auto time = AMediaExtractor_getSampleTime(impl->extractor);
    AMediaCodec_queueInputBuffer(impl->codec, static_cast<size_t>(idx), 0, static_cast<size_t>(sample), time > 0 ? static_cast<uint64_t>(time) : 0, 0);
    AMediaExtractor_advance(impl->extractor);
    return true;
}

static void _source(AndroidImpl* impl, uint32_t x, uint32_t y, uint32_t& sx, uint32_t& sy)
{
    auto w = static_cast<uint32_t>(impl->width);
    auto h = static_cast<uint32_t>(impl->height);

    switch (impl->rotation) {
        case 90: sx = y; sy = h - 1 - x; break;
        case 180: sx = w - 1 - x; sy = h - 1 - y; break;
        case 270: sx = w - 1 - y; sy = x; break;
        default: sx = x; sy = y; break;
    }
}

static uint32_t _rgb(uint8_t yy, uint8_t uu, uint8_t vv)
{
    auto y = static_cast<int32_t>(yy) - 16;
    auto u = static_cast<int32_t>(uu) - 128;
    auto v = static_cast<int32_t>(vv) - 128;
    auto r = (298 * y + 409 * v + 128) >> 8;
    auto g = (298 * y - 100 * u - 208 * v + 128) >> 8;
    auto b = (298 * y + 516 * u + 128) >> 8;

    return 0xff000000 | (_clamp(r) << 16) | (_clamp(g) << 8) | _clamp(b);
}

static bool _push(AndroidImpl* impl, const uint8_t* data, size_t size, int64_t time)
{
    if (impl->width <= 0 || impl->height <= 0 || impl->stride <= 0 || impl->slice <= 0) return false;

    // ponytail: CPU YUV420 path; add AHardwareBuffer import when renderers consume native buffers.
    auto dstW = impl->loader->surface.w;
    auto dstH = impl->loader->surface.h;
    auto bytes = dstW * dstH * sizeof(uint32_t);

    tvg::ScopedLock lock(impl->key);
    if (!impl->frame) impl->frame = tvg::malloc<uint32_t>(bytes);
    if (!impl->frame) return false;

    auto ySize = static_cast<size_t>(impl->stride) * static_cast<size_t>(impl->slice);
    auto uvStride = impl->color == COLOR_YUV420_PLANAR ? impl->stride / 2 : impl->stride;
    auto uvSize = static_cast<size_t>(uvStride) * static_cast<size_t>((impl->slice + 1) / 2);

    for (auto y = 0U; y < dstH; ++y) {
        for (auto x = 0U; x < dstW; ++x) {
            auto sx = 0U;
            auto sy = 0U;
            _source(impl, x, y, sx, sy);
            if (sx >= static_cast<uint32_t>(impl->width) || sy >= static_cast<uint32_t>(impl->height)) continue;

            auto yy = static_cast<size_t>(sy) * impl->stride + sx;
            auto uv = ySize + static_cast<size_t>(sy / 2) * uvStride + (sx / 2) * (impl->color == COLOR_YUV420_PLANAR ? 1 : 2);
            auto vv = impl->color == COLOR_YUV420_PLANAR ? ySize + uvSize + static_cast<size_t>(sy / 2) * uvStride + sx / 2 : uv + 1;
            if (yy >= size || uv >= size || vv >= size) continue;
            impl->frame[y * dstW + x] = _rgb(data[yy], data[uv], data[vv]);
        }
    }

    impl->latestTime = static_cast<float>(time) / static_cast<float>(USEC);
    impl->decodedUs = time;
    impl->frameUpdated = true;
    return true;
}

static bool _drain(AndroidImpl* impl, int64_t target, int64_t timeout)
{
    auto progressed = false;

    while (true) {
        AMediaCodecBufferInfo info;
        auto idx = AMediaCodec_dequeueOutputBuffer(impl->codec, &info, timeout);

        if (idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) return progressed;
        if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            if (auto format = AMediaCodec_getOutputFormat(impl->codec)) {
                _readFormat(impl, format);
                AMediaFormat_delete(format);
            }
            continue;
        }
        if (idx < 0) return progressed;

        if (info.size > 0 && !(info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)) {
            auto size = size_t(0);
            auto buffer = AMediaCodec_getOutputBuffer(impl->codec, static_cast<size_t>(idx), &size);
            if (buffer && _push(impl, buffer + info.offset, static_cast<size_t>(info.size), info.presentationTimeUs)) progressed = true;
        }

        AMediaCodec_releaseOutputBuffer(impl->codec, static_cast<size_t>(idx), false);
        if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
            impl->outputDone = true;
            return progressed;
        }
        if (impl->decodedUs >= target) return true;
    }
}

static bool _decode(AndroidImpl* impl, int64_t target, bool wait)
{
    auto timeout = wait ? 2000LL : 0LL;

    for (auto i = 0; i < (wait ? 1000 : 20); ++i) {
        if (!impl->codec) return false;
        auto fed = _feed(impl, timeout);
        auto got = _drain(impl, target, timeout);
        if (impl->decodedUs >= target || impl->outputDone) return got;
        if (!fed && !got && !wait) return false;
    }
    return impl->decodedUs >= target;
}

static void _work(AndroidImpl* impl)
{
    while (true) {
        auto target = int64_t(0);
        auto paused = false;
        auto looping = false;

        {
            tvg::ScopedLock lock(impl->key);
            if (impl->quit) return;
            target = _time(impl);
            paused = impl->loader->paused;
            looping = impl->loader->looping;
            impl->loader->curTime = static_cast<float>(target) / static_cast<float>(USEC);
            if (!looping && !paused && target >= impl->durationUs) impl->loader->paused = paused = true;
        }

        if (!paused) {
            tvg::ScopedLock lock(impl->codecKey);
            if (!impl->codec || impl->outputDone || (looping && impl->decodedUs > target + 50000)) _reset(impl, target);
            _decode(impl, target, false);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
}

static void _stop(AndroidImpl* impl)
{
    {
        tvg::ScopedLock lock(impl->key);
        impl->quit = true;
    }

    if (impl->worker.joinable()) impl->worker.join();

    {
        tvg::ScopedLock lock(impl->key);
        impl->quit = false;
    }
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

AndroidMediaLoader::AndroidMediaLoader() :
    MediaLoader(FileType::Media),
    pImpl(new AndroidImpl)
{
    pImpl->loader = this;
}

AndroidMediaLoader::~AndroidMediaLoader()
{
    _stop(pImpl);
    {
        tvg::ScopedLock lock(pImpl->codecKey);
        _clear(pImpl);
    }
    delete pImpl;
}

bool AndroidMediaLoader::open(const char* path, TVG_UNUSED const LoaderOps* ops)
{
    if (!path) return false;

    _stop(pImpl);
    {
        tvg::ScopedLock lock(pImpl->codecKey);
        _clear(pImpl);
    }

    auto extractor = AMediaExtractor_new();
    if (!extractor || AMediaExtractor_setDataSource(extractor, path) != AMEDIA_OK) {
        if (extractor) AMediaExtractor_delete(extractor);
        return false;
    }

    for (auto i = 0U; i < AMediaExtractor_getTrackCount(extractor); ++i) {
        auto format = AMediaExtractor_getTrackFormat(extractor, i);
        const char* mime = nullptr;
        if (format && AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime) && !strncmp(mime, "video/", 6)) {
            pImpl->format = format;
            pImpl->track = static_cast<int32_t>(i);
            break;
        }
        if (format) AMediaFormat_delete(format);
    }
    AMediaExtractor_delete(extractor);

    if (!pImpl->format) return false;

    auto duration = int64_t(0);
    if (!AMediaFormat_getInt64(pImpl->format, AMEDIAFORMAT_KEY_DURATION, &duration) || duration <= 0) return false;

    pImpl->path = path;
    pImpl->durationUs = duration;
    _readFormat(pImpl, pImpl->format);

    if (pImpl->width <= 0 || pImpl->height <= 0) return false;

    w = static_cast<float>((pImpl->rotation == 90 || pImpl->rotation == 270) ? pImpl->height : pImpl->width);
    h = static_cast<float>((pImpl->rotation == 90 || pImpl->rotation == 270) ? pImpl->width : pImpl->height);
    totalTime = static_cast<float>(duration) / static_cast<float>(USEC);
    curTime = 0.0f;
    return true;
}

bool AndroidMediaLoader::read()
{
    if (!Loader::read()) return true;
    if (!pImpl->format || w == 0 || h == 0) return false;

    surface.cs = ColorSpace::ARGB8888S;
    surface.w = static_cast<uint32_t>(w);
    surface.h = static_cast<uint32_t>(h);
    surface.stride = surface.w;
    surface.channelSize = sizeof(uint32_t);
    surface.premultiplied = false;
    surface.alphaIgnored = true;

    paused = true;
    pImpl->ready = true;
    pImpl->startedUs = 0;
    pImpl->started = std::chrono::steady_clock::now();

    {
        tvg::ScopedLock lock(pImpl->codecKey);
        if (!_reset(pImpl, 0) || !_decode(pImpl, 0, true)) {
            pImpl->ready = false;
            _closeCodec(pImpl);
            return false;
        }
    }

    pImpl->worker = std::thread(_work, pImpl);
    return true;
}

bool AndroidMediaLoader::close()
{
    if (!Loader::close()) return false;

    _stop(pImpl);
    {
        tvg::ScopedLock lock(pImpl->codecKey);
        _clear(pImpl);
    }
    return true;
}

RenderSurface* AndroidMediaLoader::bitmap()
{
    tvg::ScopedLock lock(pImpl->key);

    curTime = static_cast<float>(_time(pImpl)) / static_cast<float>(USEC);
    if (!pImpl->frameUpdated) return nullptr;

    pImpl->frameUpdated = false;
    std::swap(pImpl->frame, pImpl->surfaceData);
    surface.data = pImpl->surfaceData;
    surface.cs = ColorSpace::ARGB8888S;
    surface.premultiplied = false;
    curTime = pImpl->latestTime;
    return &surface;
}

Result AndroidMediaLoader::play()
{
    tvg::ScopedLock lock(pImpl->key);
    if (!pImpl->ready) return Result::InsufficientCondition;

    if (curTime >= totalTime) curTime = 0.0f;
    pImpl->startedUs = _us(curTime);
    pImpl->started = std::chrono::steady_clock::now();
    paused = false;
    return Result::Success;
}

Result AndroidMediaLoader::pause()
{
    tvg::ScopedLock lock(pImpl->key);
    if (!pImpl->ready) return Result::InsufficientCondition;

    curTime = static_cast<float>(_time(pImpl)) / static_cast<float>(USEC);
    paused = true;
    return Result::Success;
}

Result AndroidMediaLoader::stop()
{
    if (!pImpl->ready) return Result::InsufficientCondition;

    {
        tvg::ScopedLock lock(pImpl->key);
        curTime = 0.0f;
        pImpl->startedUs = 0;
        pImpl->started = std::chrono::steady_clock::now();
        paused = true;
    }

    tvg::ScopedLock lock(pImpl->codecKey);
    return (_reset(pImpl, 0) && _decode(pImpl, 0, true)) ? Result::Success : Result::InsufficientCondition;
}

Result AndroidMediaLoader::seek(float seconds)
{
    if (seconds < 0.0f || (totalTime > 0.0f && seconds > totalTime)) return Result::InvalidArguments;
    if (!pImpl->ready) return Result::InsufficientCondition;

    auto target = _us(seconds);
    {
        tvg::ScopedLock lock(pImpl->key);
        curTime = seconds;
        pImpl->startedUs = target;
        pImpl->started = std::chrono::steady_clock::now();
    }

    tvg::ScopedLock lock(pImpl->codecKey);
    return (_reset(pImpl, target) && _decode(pImpl, target, true)) ? Result::Success : Result::InsufficientCondition;
}

Result AndroidMediaLoader::loop(bool on)
{
    tvg::ScopedLock lock(pImpl->key);
    if (!pImpl->ready) return Result::InsufficientCondition;

    looping = on;
    return Result::Success;
}

Result AndroidMediaLoader::volume(float volume)
{
    tvg::ScopedLock lock(pImpl->key);
    if (!pImpl->ready) return Result::InsufficientCondition;

    // ponytail: video-only decoder; wire native audio when playback owns an audio path.
    audioVolume = volume;
    return Result::Success;
}

Result AndroidMediaLoader::mute(bool on)
{
    tvg::ScopedLock lock(pImpl->key);
    if (!pImpl->ready) return Result::InsufficientCondition;

    muted = on;
    return Result::Success;
}
