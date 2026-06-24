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
#include <condition_variable>
#include <fcntl.h>
#include <math.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <SLES/OpenSLES_Android.h>
#include <string.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "tvgArray.h"
#include "tvgAndroidMediaLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static constexpr auto USEC = 1000000LL;
static constexpr auto DEFAULT_FPS = 30;
static constexpr auto CODEC_TIMEOUT_US = 2000LL;
static constexpr auto DECODE_WAIT_TRIES = 1000;
static constexpr auto DECODE_TICK_TRIES = 20;
static constexpr auto LOOP_REWIND_MARGIN_US = 50000LL;
static constexpr auto AUDIO_QUEUE_COUNT = 4;
static constexpr auto AUDIO_TICK_TRIES = 16;
static constexpr auto COLOR_YUV420_PLANAR = 19;
static constexpr auto COLOR_YUV420_SEMIPLANAR = 21;

struct SourceInfo
{
    int32_t w = 0;
    int32_t h = 0;
    int32_t stride = 0;
    int32_t slice = 0;
    int32_t rotation = 0;
    int32_t color = COLOR_YUV420_SEMIPLANAR;
    int64_t frameDurationUs = USEC / DEFAULT_FPS;
};

struct AudioInfo
{
    int32_t sampleRate = 0;
    int32_t channels = 0;
    int32_t track = -1;
};

struct AndroidImpl;

struct WorkerInfo
{
    std::thread thread;
    std::condition_variable cv;
    tvg::StrictKey key;
    tvg::Array<AndroidImpl*> loaders;
    bool quit = false;
};

static WorkerInfo _worker;

struct AndroidImpl
{
    AndroidMediaLoader* loader = nullptr;
    AMediaExtractor* extractor = nullptr;
    AMediaExtractor* audioExtractor = nullptr;
    AMediaCodec* codec = nullptr;
    AMediaCodec* audioCodec = nullptr;
    SLObjectItf slEngineObj = nullptr;
    SLEngineItf slEngine = nullptr;
    SLObjectItf slMixObj = nullptr;
    SLObjectItf slPlayerObj = nullptr;
    SLPlayItf slPlay = nullptr;
    SLVolumeItf slVolume = nullptr;
    SLAndroidSimpleBufferQueueItf slQueue = nullptr;
    std::chrono::steady_clock::time_point nextTick;
    std::chrono::steady_clock::time_point started;

    tvg::StrictKey key;
    tvg::StrictKey codecKey;
    tvg::StrictKey audioKey;
    uint32_t* frame = nullptr;
    uint32_t* surfaceData = nullptr;
    uint8_t* audioData[AUDIO_QUEUE_COUNT] = {};
    size_t audioCap[AUDIO_QUEUE_COUNT] = {};
    int64_t startedUs = 0;
    int64_t durationUs = 0;
    int64_t decodedUs = -1;
    int64_t publishFromUs = 0;
    int64_t audioTickUs = USEC / DEFAULT_FPS;
    uint32_t audioQueued = 0;
    uint32_t audioWrite = 0;
    int32_t track = -1;
    SourceInfo src;
    AudioInfo audio;
    bool frameUpdated = false;
    bool inputDone = false;
    bool outputDone = false;
    bool audioInputDone = false;
    bool audioOutputDone = false;
    bool audioReady = false;
    bool ready = false;
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

static void _audioDone(SLAndroidSimpleBufferQueueItf, void* context)
{
    auto impl = static_cast<AndroidImpl*>(context);
    if (!impl) return;

    tvg::ScopedLock lock(impl->audioKey);
    if (impl->audioQueued > 0) --impl->audioQueued;
}

static void _clearAudioQueue(AndroidImpl* impl)
{
    if (impl->slQueue) (*impl->slQueue)->Clear(impl->slQueue);

    tvg::ScopedLock lock(impl->audioKey);
    impl->audioQueued = 0;
    impl->audioWrite = 0;
}

static void _setAudioState(AndroidImpl* impl, SLuint32 state)
{
    if (impl->slPlay) (*impl->slPlay)->SetPlayState(impl->slPlay, state);
}

static void _destroyAudioOutput(AndroidImpl* impl)
{
    _setAudioState(impl, SL_PLAYSTATE_STOPPED);
    _clearAudioQueue(impl);

    if (impl->slPlayerObj) (*impl->slPlayerObj)->Destroy(impl->slPlayerObj);
    if (impl->slMixObj) (*impl->slMixObj)->Destroy(impl->slMixObj);
    if (impl->slEngineObj) (*impl->slEngineObj)->Destroy(impl->slEngineObj);

    impl->slQueue = nullptr;
    impl->slVolume = nullptr;
    impl->slPlay = nullptr;
    impl->slPlayerObj = nullptr;
    impl->slMixObj = nullptr;
    impl->slEngine = nullptr;
    impl->slEngineObj = nullptr;
}

static SLuint32 _speakerMask(int32_t channels)
{
    if (channels == 1) return SL_SPEAKER_FRONT_CENTER;
    if (channels == 2) return SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    return 0;
}

static void _syncAudioVolume(AndroidImpl* impl)
{
    if (!impl->slVolume) return;

    auto volume = 0.0f;
    auto muted = false;
    {
        tvg::ScopedLock lock(impl->key);
        volume = impl->loader->audioVolume;
        muted = impl->loader->muted;
    }

    (*impl->slVolume)->SetMute(impl->slVolume, (muted || volume <= 0.0f) ? SL_BOOLEAN_TRUE : SL_BOOLEAN_FALSE);
    if (volume > 0.0f) {
        auto level = volume >= 1.0f ? 0 : static_cast<SLmillibel>(2000.0f * log10f(std::max(volume, 0.0001f)));
        (*impl->slVolume)->SetVolumeLevel(impl->slVolume, level);
    }
}

static bool _openAudioOutput(AndroidImpl* impl)
{
    if (impl->slPlayerObj) return true;

    auto mask = _speakerMask(impl->audio.channels);
    if (!mask || impl->audio.sampleRate <= 0) return false;

    if (slCreateEngine(&impl->slEngineObj, 0, nullptr, 0, nullptr, nullptr) != SL_RESULT_SUCCESS) return false;
    if ((*impl->slEngineObj)->Realize(impl->slEngineObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) goto fail;
    if ((*impl->slEngineObj)->GetInterface(impl->slEngineObj, SL_IID_ENGINE, &impl->slEngine) != SL_RESULT_SUCCESS) goto fail;
    if ((*impl->slEngine)->CreateOutputMix(impl->slEngine, &impl->slMixObj, 0, nullptr, nullptr) != SL_RESULT_SUCCESS) goto fail;
    if ((*impl->slMixObj)->Realize(impl->slMixObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) goto fail;

    {
        SLDataLocator_AndroidSimpleBufferQueue queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, AUDIO_QUEUE_COUNT};
        SLDataFormat_PCM pcm = {SL_DATAFORMAT_PCM,
                                static_cast<SLuint32>(impl->audio.channels),
                                static_cast<SLuint32>(impl->audio.sampleRate * 1000),
                                SL_PCMSAMPLEFORMAT_FIXED_16,
                                SL_PCMSAMPLEFORMAT_FIXED_16,
                                mask,
                                SL_BYTEORDER_LITTLEENDIAN};
        SLDataSource src = {&queue, &pcm};
        SLDataLocator_OutputMix output = {SL_DATALOCATOR_OUTPUTMIX, impl->slMixObj};
        SLDataSink sink = {&output, nullptr};
        const SLInterfaceID ids[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_VOLUME};
        const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

        if ((*impl->slEngine)->CreateAudioPlayer(impl->slEngine, &impl->slPlayerObj, &src, &sink, 2, ids, req) != SL_RESULT_SUCCESS) goto fail;
        if ((*impl->slPlayerObj)->Realize(impl->slPlayerObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) goto fail;
        if ((*impl->slPlayerObj)->GetInterface(impl->slPlayerObj, SL_IID_PLAY, &impl->slPlay) != SL_RESULT_SUCCESS) goto fail;
        if ((*impl->slPlayerObj)->GetInterface(impl->slPlayerObj, SL_IID_VOLUME, &impl->slVolume) != SL_RESULT_SUCCESS) goto fail;
        if ((*impl->slPlayerObj)->GetInterface(impl->slPlayerObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &impl->slQueue) != SL_RESULT_SUCCESS) goto fail;
        if ((*impl->slQueue)->RegisterCallback(impl->slQueue, _audioDone, impl) != SL_RESULT_SUCCESS) goto fail;
    }
    _syncAudioVolume(impl);
    return true;

fail:
    _destroyAudioOutput(impl);
    return false;
}

static void _closeAudio(AndroidImpl* impl)
{
    _destroyAudioOutput(impl);
    if (impl->audioCodec) {
        AMediaCodec_stop(impl->audioCodec);
        AMediaCodec_delete(impl->audioCodec);
        impl->audioCodec = nullptr;
    }
    if (impl->audioExtractor) {
        AMediaExtractor_delete(impl->audioExtractor);
        impl->audioExtractor = nullptr;
    }
    for (auto i = 0U; i < AUDIO_QUEUE_COUNT; ++i) {
        tvg::free(impl->audioData[i]);
        impl->audioData[i] = nullptr;
        impl->audioCap[i] = 0;
    }
    impl->audio = {};
    impl->audioTickUs = USEC / DEFAULT_FPS;
    impl->audioInputDone = false;
    impl->audioOutputDone = false;
    impl->audioReady = false;
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
    _closeAudio(impl);
    tvg::free(impl->frame);
    tvg::free(impl->surfaceData);
    impl->frame = nullptr;
    impl->surfaceData = nullptr;
    impl->durationUs = 0;
    impl->track = -1;
    impl->publishFromUs = 0;
    impl->src = {};
    impl->ready = false;
    impl->frameUpdated = false;
}

static void _readFormat(SourceInfo& src, AMediaFormat* format)
{
    auto v = 0;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &v) && v > 0) src.w = v;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &v) && v > 0) src.h = v;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_STRIDE, &v) && v > 0) src.stride = v;
    else src.stride = src.w;
    if (AMediaFormat_getInt32(format, "slice-height", &v) && v > 0) src.slice = v;
    else src.slice = src.h;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, &v)) src.color = v;
    if (AMediaFormat_getInt32(format, "rotation-degrees", &v)) src.rotation = ((v % 360) + 360) % 360;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, &v) && v > 0) src.frameDurationUs = USEC / v;
}

static bool _setDataSource(AMediaExtractor* extractor, const char* path)
{
    if (!extractor || !path) return false;

    auto fd = ::open(path, O_RDONLY);
    if (fd < 0) return false;

    struct stat statbuf;
    auto ok = ::fstat(fd, &statbuf) == 0 && statbuf.st_size > 0 &&
              AMediaExtractor_setDataSourceFd(extractor, fd, 0, statbuf.st_size) == AMEDIA_OK;
    ::close(fd);
    return ok;
}

static bool _startCodec(AndroidImpl* impl)
{
    if (!impl->extractor || impl->track < 0) return false;

    auto format = AMediaExtractor_getTrackFormat(impl->extractor, static_cast<size_t>(impl->track));
    const char* mime = nullptr;
    if (!format || !AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
        if (format) AMediaFormat_delete(format);
        return false;
    }

    impl->codec = AMediaCodec_createDecoderByType(mime);
    auto ok = impl->codec &&
              AMediaCodec_configure(impl->codec, format, nullptr, nullptr, 0) == AMEDIA_OK &&
              AMediaCodec_start(impl->codec) == AMEDIA_OK;
    AMediaFormat_delete(format);
    if (!ok) return false;

    impl->inputDone = false;
    impl->outputDone = false;
    impl->decodedUs = -1;
    return true;
}

static bool _startAudio(AndroidImpl* impl)
{
    if (!impl->audioExtractor || impl->audio.track < 0) return false;

    auto format = AMediaExtractor_getTrackFormat(impl->audioExtractor, static_cast<size_t>(impl->audio.track));
    const char* mime = nullptr;
    if (!format || !AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
        if (format) AMediaFormat_delete(format);
        return false;
    }

    impl->audioCodec = AMediaCodec_createDecoderByType(mime);
    auto ok = impl->audioCodec &&
              AMediaCodec_configure(impl->audioCodec, format, nullptr, nullptr, 0) == AMEDIA_OK &&
              AMediaCodec_start(impl->audioCodec) == AMEDIA_OK &&
              _openAudioOutput(impl);
    AMediaFormat_delete(format);
    if (!ok) return false;

    impl->audioInputDone = false;
    impl->audioOutputDone = false;
    impl->audioReady = true;
    return true;
}

static bool _seek(AndroidImpl* impl, int64_t us)
{
    if (!impl->codec || !impl->extractor) return false;

    if (AMediaCodec_flush(impl->codec) != AMEDIA_OK) return false;
    if (AMediaExtractor_seekTo(impl->extractor, us, AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC) != AMEDIA_OK) return false;

    impl->publishFromUs = us;
    impl->inputDone = false;
    impl->outputDone = false;
    impl->decodedUs = -1;
    return true;
}

static bool _seekAudio(AndroidImpl* impl, int64_t us)
{
    if (!impl->audioReady || !impl->audioCodec || !impl->audioExtractor) return false;

    _clearAudioQueue(impl);
    if (AMediaCodec_flush(impl->audioCodec) != AMEDIA_OK) return false;
    if (AMediaExtractor_seekTo(impl->audioExtractor, us, AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC) != AMEDIA_OK) return false;

    impl->audioInputDone = false;
    impl->audioOutputDone = false;
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

static bool _feedAudio(AndroidImpl* impl, int64_t timeout)
{
    if (impl->audioInputDone) return false;

    auto idx = AMediaCodec_dequeueInputBuffer(impl->audioCodec, timeout);
    if (idx < 0) return false;

    auto size = size_t(0);
    auto buffer = AMediaCodec_getInputBuffer(impl->audioCodec, static_cast<size_t>(idx), &size);
    auto sample = buffer ? AMediaExtractor_readSampleData(impl->audioExtractor, buffer, size) : -1;

    if (sample < 0) {
        AMediaCodec_queueInputBuffer(impl->audioCodec, static_cast<size_t>(idx), 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
        impl->audioInputDone = true;
        return true;
    }

    auto time = AMediaExtractor_getSampleTime(impl->audioExtractor);
    AMediaCodec_queueInputBuffer(impl->audioCodec, static_cast<size_t>(idx), 0, static_cast<size_t>(sample), time > 0 ? static_cast<uint64_t>(time) : 0, 0);
    AMediaExtractor_advance(impl->audioExtractor);
    return true;
}

static void _source(const SourceInfo& src, uint32_t x, uint32_t y, uint32_t& sx, uint32_t& sy)
{
    auto w = static_cast<uint32_t>(src.w);
    auto h = static_cast<uint32_t>(src.h);

    switch (src.rotation) {
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
    if (time < impl->publishFromUs) {
        impl->decodedUs = time;
        return true;
    }

    auto& src = impl->src;
    if (src.w <= 0 || src.h <= 0 || src.stride <= 0 || src.slice <= 0) return false;

    // ponytail: CPU YUV420 path; add AHardwareBuffer import when renderers consume native buffers.
    auto dstW = impl->loader->surface.w;
    auto dstH = impl->loader->surface.h;
    auto bytes = dstW * dstH * sizeof(uint32_t);

    tvg::ScopedLock lock(impl->key);
    if (!impl->frame) impl->frame = tvg::malloc<uint32_t>(bytes);
    if (!impl->frame) return false;

    auto ySize = static_cast<size_t>(src.stride) * static_cast<size_t>(src.slice);
    auto uvStride = src.color == COLOR_YUV420_PLANAR ? src.stride / 2 : src.stride;
    auto uvSize = static_cast<size_t>(uvStride) * static_cast<size_t>((src.slice + 1) / 2);

    for (auto y = 0U; y < dstH; ++y) {
        for (auto x = 0U; x < dstW; ++x) {
            auto sx = 0U;
            auto sy = 0U;
            _source(src, x, y, sx, sy);
            if (sx >= static_cast<uint32_t>(src.w) || sy >= static_cast<uint32_t>(src.h)) continue;

            auto yy = static_cast<size_t>(sy) * src.stride + sx;
            auto uv = ySize + static_cast<size_t>(sy / 2) * uvStride + (sx / 2) * (src.color == COLOR_YUV420_PLANAR ? 1 : 2);
            auto vv = src.color == COLOR_YUV420_PLANAR ? ySize + uvSize + static_cast<size_t>(sy / 2) * uvStride + sx / 2 : uv + 1;
            if (yy >= size || uv >= size || vv >= size) continue;
            impl->frame[y * dstW + x] = _rgb(data[yy], data[uv], data[vv]);
        }
    }

    impl->publishFromUs = 0;
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
                _readFormat(impl->src, format);
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

static bool _queueAudio(AndroidImpl* impl, const uint8_t* data, size_t size)
{
    if (!data || size == 0) return true;

    tvg::ScopedLock lock(impl->audioKey);
    if (!impl->slQueue || impl->audioQueued >= AUDIO_QUEUE_COUNT) return false;

    auto idx = impl->audioWrite;
    if (size > impl->audioCap[idx]) {
        auto buffer = tvg::realloc<uint8_t>(impl->audioData[idx], size);
        if (!buffer) return false;
        impl->audioData[idx] = buffer;
        impl->audioCap[idx] = size;
    }

    auto dst = impl->audioData[idx];
    memcpy(dst, data, size);
    if ((*impl->slQueue)->Enqueue(impl->slQueue, dst, static_cast<SLuint32>(size)) != SL_RESULT_SUCCESS) return false;
    if (impl->audio.sampleRate > 0 && impl->audio.channels > 0) {
        auto samples = size / (static_cast<size_t>(impl->audio.channels) * sizeof(int16_t));
        if (samples > 0) impl->audioTickUs = static_cast<int64_t>(samples) * USEC / impl->audio.sampleRate;
    }
    impl->audioWrite = (impl->audioWrite + 1) % AUDIO_QUEUE_COUNT;
    ++impl->audioQueued;
    return true;
}

static bool _drainAudio(AndroidImpl* impl, int64_t timeout)
{
    if (impl->audioOutputDone) return false;

    {
        tvg::ScopedLock lock(impl->audioKey);
        if (impl->audioQueued >= AUDIO_QUEUE_COUNT) return false;
    }

    auto progressed = false;
    while (true) {
        AMediaCodecBufferInfo info;
        auto idx = AMediaCodec_dequeueOutputBuffer(impl->audioCodec, &info, timeout);

        if (idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) return progressed;
        if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            if (auto format = AMediaCodec_getOutputFormat(impl->audioCodec)) AMediaFormat_delete(format);
            continue;
        }
        if (idx < 0) return progressed;

        if (info.size > 0 && !(info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)) {
            auto size = size_t(0);
            auto buffer = AMediaCodec_getOutputBuffer(impl->audioCodec, static_cast<size_t>(idx), &size);
            if (buffer && _queueAudio(impl, buffer + info.offset, static_cast<size_t>(info.size))) progressed = true;
        }

        AMediaCodec_releaseOutputBuffer(impl->audioCodec, static_cast<size_t>(idx), false);
        if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
            impl->audioOutputDone = true;
            return progressed;
        }
        if (!progressed) return false;
        {
            tvg::ScopedLock lock(impl->audioKey);
            if (impl->audioQueued >= AUDIO_QUEUE_COUNT) return true;
        }
    }
}

static bool _decodeAudio(AndroidImpl* impl, bool wait)
{
    if (!impl->audioReady || impl->audioOutputDone) return false;

    auto timeout = wait ? CODEC_TIMEOUT_US : 0LL;
    auto progressed = false;
    for (auto i = 0; i < (wait ? DECODE_WAIT_TRIES : AUDIO_TICK_TRIES); ++i) {
        if (!impl->audioCodec) return false;
        {
            tvg::ScopedLock lock(impl->audioKey);
            if (impl->audioQueued >= AUDIO_QUEUE_COUNT) return progressed;
        }
        auto fed = _feedAudio(impl, timeout);
        auto got = _drainAudio(impl, timeout);
        progressed |= fed || got;
        if (impl->audioOutputDone) return progressed;
        if (!fed && !got) return progressed;
    }
    return progressed;
}

static void _primeAudio(AndroidImpl* impl, int64_t us)
{
    if (!impl->audioReady) return;
    if (_seekAudio(impl, us)) _decodeAudio(impl, true);
    else _closeAudio(impl);
}

static bool _decode(AndroidImpl* impl, int64_t target, bool wait)
{
    if (impl->decodedUs >= target) return true;

    auto timeout = wait ? CODEC_TIMEOUT_US : 0LL;

    for (auto i = 0; i < (wait ? DECODE_WAIT_TRIES : DECODE_TICK_TRIES); ++i) {
        if (!impl->codec) return false;
        auto fed = _feed(impl, timeout);
        auto got = _drain(impl, target, timeout);
        if (impl->decodedUs >= target || impl->outputDone) return got;
        if (!fed && !got && !wait) return false;
    }
    return impl->decodedUs >= target;
}

static bool _tick(AndroidImpl* impl)
{
    auto target = int64_t(0);
    auto paused = false;
    auto looping = false;

    {
        tvg::ScopedLock lock(impl->key);
        target = _time(impl);
        paused = impl->loader->paused;
        looping = impl->loader->looping;
        impl->loader->curTime = static_cast<float>(target) / static_cast<float>(USEC);
        if (!looping && !paused && target >= impl->durationUs) impl->loader->paused = paused = true;
    }

    if (paused) {
        _setAudioState(impl, SL_PLAYSTATE_PAUSED);
        return false;
    }

    tvg::ScopedLock lock(impl->codecKey);
    if (impl->outputDone || (looping && impl->decodedUs > target + LOOP_REWIND_MARGIN_US)) {
        if (!_seek(impl, target)) return true;
        if (impl->audioReady && !_seekAudio(impl, target)) _closeAudio(impl);
    }
    _decodeAudio(impl, false);
    _decode(impl, target, false);
    return true;
}

static void _work()
{
    std::unique_lock<std::mutex> lock(_worker.key.mtx);

    while (!_worker.quit) {
        auto now = std::chrono::steady_clock::now();
        auto next = std::chrono::steady_clock::time_point::max();

        for (auto i = 0U; i < _worker.loaders.count; ++i) {
            auto impl = _worker.loaders[i];
            if (impl->nextTick <= now) {
                if (_tick(impl)) {
                    auto delay = impl->src.frameDurationUs;
                    if (impl->audioReady) {
                        tvg::ScopedLock lock(impl->audioKey);
                        delay = std::min(delay, impl->audioTickUs);
                    }
                    impl->nextTick = std::chrono::steady_clock::now() + std::chrono::microseconds(delay);
                }
                else impl->nextTick = std::chrono::steady_clock::time_point::max();
            }
            if (impl->nextTick < next) next = impl->nextTick;
        }

        if (next == std::chrono::steady_clock::time_point::max()) _worker.cv.wait(lock);
        else _worker.cv.wait_until(lock, next);
    }
}

static void _refWorker(AndroidImpl* impl)
{
    {
        tvg::ScopedLock lock(_worker.key);
        for (auto i = 0U; i < _worker.loaders.count; ++i) {
            if (_worker.loaders[i] == impl) {
                impl->nextTick = std::chrono::steady_clock::now();
                _worker.cv.notify_one();
                return;
            }
        }
        impl->nextTick = std::chrono::steady_clock::now();
        auto start = _worker.loaders.empty();
        _worker.loaders.push(impl);
        if (start) {
            _worker.quit = false;
            _worker.thread = std::thread(_work);
        }
    }
    _worker.cv.notify_one();
}

static void _unrefWorker(AndroidImpl* impl)
{
    auto join = false;
    auto notify = false;

    {
        tvg::ScopedLock lock(_worker.key);
        for (auto i = 0U; i < _worker.loaders.count; ++i) {
            if (_worker.loaders[i] != impl) continue;
            for (auto j = i + 1; j < _worker.loaders.count; ++j) _worker.loaders[j - 1] = _worker.loaders[j];
            _worker.loaders.pop();
            notify = true;
            if (_worker.loaders.empty()) {
                _worker.quit = true;
                join = true;
            }
            break;
        }
    }

    if (notify) _worker.cv.notify_one();
    if (join && _worker.thread.joinable()) _worker.thread.join();
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
    _unrefWorker(pImpl);
    {
        tvg::ScopedLock lock(pImpl->codecKey);
        _clear(pImpl);
    }
    delete pImpl;
}

bool AndroidMediaLoader::open(const char* path, TVG_UNUSED const LoaderOps* ops)
{
    if (!path) return false;

    _unrefWorker(pImpl);
    {
        tvg::ScopedLock lock(pImpl->codecKey);
        _clear(pImpl);
    }

    auto extractor = AMediaExtractor_new();
    if (!extractor || !_setDataSource(extractor, path)) {
        if (extractor) AMediaExtractor_delete(extractor);
        return false;
    }

    auto fail = [&]() {
        if (extractor) AMediaExtractor_delete(extractor);
        _clear(pImpl);
        return false;
    };

    for (auto i = 0U; i < AMediaExtractor_getTrackCount(extractor); ++i) {
        auto format = AMediaExtractor_getTrackFormat(extractor, i);
        const char* mime = nullptr;
        if (format && AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime) && pImpl->track < 0 && !strncmp(mime, "video/", 6)) {
            pImpl->track = static_cast<int32_t>(i);
            auto duration = int64_t(0);
            if (!AMediaFormat_getInt64(format, AMEDIAFORMAT_KEY_DURATION, &duration) || duration <= 0) {
                AMediaFormat_delete(format);
                return fail();
            }
            pImpl->durationUs = duration;
            _readFormat(pImpl->src, format);
        } else if (format && mime && pImpl->audio.track < 0 && !strncmp(mime, "audio/", 6)) {
            auto sampleRate = 0;
            auto channels = 0;
            if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &sampleRate) &&
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels) &&
                sampleRate > 0 && channels > 0 && channels <= 2) {
                pImpl->audio.track = static_cast<int32_t>(i);
                pImpl->audio.sampleRate = sampleRate;
                pImpl->audio.channels = channels;
            }
        }
        if (format) AMediaFormat_delete(format);
    }

    if (pImpl->track < 0) return fail();
    if (AMediaExtractor_selectTrack(extractor, static_cast<size_t>(pImpl->track)) != AMEDIA_OK) return fail();

    pImpl->extractor = extractor;
    extractor = nullptr;

    if (pImpl->audio.track >= 0) {
        auto audioExtractor = AMediaExtractor_new();
        if (audioExtractor && _setDataSource(audioExtractor, path) &&
            AMediaExtractor_selectTrack(audioExtractor, static_cast<size_t>(pImpl->audio.track)) == AMEDIA_OK) {
            pImpl->audioExtractor = audioExtractor;
        } else {
            if (audioExtractor) AMediaExtractor_delete(audioExtractor);
            pImpl->audio = {};
        }
    }

    auto& src = pImpl->src;
    if (src.w <= 0 || src.h <= 0) return fail();

    w = static_cast<float>((src.rotation == 90 || src.rotation == 270) ? src.h : src.w);
    h = static_cast<float>((src.rotation == 90 || src.rotation == 270) ? src.w : src.h);
    totalTime = static_cast<float>(pImpl->durationUs) / static_cast<float>(USEC);
    curTime = 0.0f;
    return true;
}

bool AndroidMediaLoader::read()
{
    if (!Loader::read()) return true;
    if (pImpl->track < 0 || w == 0 || h == 0) return false;

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
        if (!_startCodec(pImpl) || !_decode(pImpl, 0, true)) {
            pImpl->ready = false;
            _closeCodec(pImpl);
            return false;
        }
        if (pImpl->audioExtractor) {
            if (_startAudio(pImpl)) _decodeAudio(pImpl, true);
            else _closeAudio(pImpl);
        }
    }

    return true;
}

bool AndroidMediaLoader::close()
{
    if (!Loader::close()) return false;

    _unrefWorker(pImpl);
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
    return &surface;
}

Result AndroidMediaLoader::play()
{
    auto restart = false;
    {
        tvg::ScopedLock lock(pImpl->key);
        if (!pImpl->ready) return Result::InsufficientCondition;

        if (curTime >= totalTime) {
            curTime = 0.0f;
            restart = true;
        }
        pImpl->startedUs = _us(curTime);
        pImpl->started = std::chrono::steady_clock::now();
        paused = false;
    }

    if (restart) {
        tvg::ScopedLock lock(pImpl->codecKey);
        if (pImpl->audioReady) {
            _setAudioState(pImpl, SL_PLAYSTATE_STOPPED);
            _primeAudio(pImpl, 0);
        }
        if (!_seek(pImpl, 0) || !_decode(pImpl, 0, true)) {
            tvg::ScopedLock lock(pImpl->key);
            paused = true;
            return Result::InsufficientCondition;
        }
    }

    _setAudioState(pImpl, SL_PLAYSTATE_PLAYING);
    _refWorker(pImpl);
    return Result::Success;
}

Result AndroidMediaLoader::pause()
{
    {
        tvg::ScopedLock lock(pImpl->key);
        if (!pImpl->ready) return Result::InsufficientCondition;

        curTime = static_cast<float>(_time(pImpl)) / static_cast<float>(USEC);
        paused = true;
    }

    _setAudioState(pImpl, SL_PLAYSTATE_PAUSED);
    _unrefWorker(pImpl);
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

    _unrefWorker(pImpl);

    tvg::ScopedLock lock(pImpl->codecKey);
    _setAudioState(pImpl, SL_PLAYSTATE_STOPPED);
    _primeAudio(pImpl, 0);
    return (_seek(pImpl, 0) && _decode(pImpl, 0, true)) ? Result::Success : Result::InsufficientCondition;
}

Result AndroidMediaLoader::seek(float seconds)
{
    if (seconds < 0.0f || (totalTime > 0.0f && seconds > totalTime)) return Result::InvalidArguments;
    if (!pImpl->ready) return Result::InsufficientCondition;

    auto target = _us(seconds);
    auto wasPaused = false;
    {
        tvg::ScopedLock lock(pImpl->key);
        curTime = seconds;
        pImpl->startedUs = target;
        pImpl->started = std::chrono::steady_clock::now();
        wasPaused = paused;
    }

    tvg::ScopedLock lock(pImpl->codecKey);
    if (pImpl->audioReady) {
        _setAudioState(pImpl, SL_PLAYSTATE_PAUSED);
        _primeAudio(pImpl, target);
        if (!wasPaused) _setAudioState(pImpl, SL_PLAYSTATE_PLAYING);
    }
    return (_seek(pImpl, target) && _decode(pImpl, target, true)) ? Result::Success : Result::InsufficientCondition;
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
    {
        tvg::ScopedLock lock(pImpl->key);
        if (!pImpl->ready) return Result::InsufficientCondition;

        audioVolume = volume;
    }
    _syncAudioVolume(pImpl);
    return Result::Success;
}

Result AndroidMediaLoader::mute(bool on)
{
    {
        tvg::ScopedLock lock(pImpl->key);
        if (!pImpl->ready) return Result::InsufficientCondition;

        muted = on;
    }
    _syncAudioVolume(pImpl);
    return Result::Success;
}
