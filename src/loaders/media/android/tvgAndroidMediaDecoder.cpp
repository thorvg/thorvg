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

#include <cmath>
#include <cstring>
#include <time.h>
#include <unistd.h>

#include "tvgAndroidMediaDecoder.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static constexpr int64_t CODEC_TIMEOUT_US = 0;
static constexpr int64_t ONE_SECOND_US = 1000000;
static constexpr int64_t PUMP_MAX_WAIT_US = 33000;   //one frame period at ~30fps: the scheduler's longest sleep, still()'s budget and the lateness unit

//java MediaCodecInfo.CodecCapabilities COLOR_Format* values: the ndk ships the format key only
static constexpr int32_t COLOR_FORMAT_YUV420_PLANAR = 19;
static constexpr int32_t COLOR_FORMAT_YUV420_PACKED_PLANAR = 20;
static constexpr int32_t COLOR_FORMAT_YUV420_SEMIPLANAR = 21;
static constexpr int32_t COLOR_FORMAT_YUV420_PACKED_SEMIPLANAR = 39;

//process-wide opensl singletons shared by every AudioDecoder, which owns only its player (the oboe pattern)
static StrictKey _slKey;
static SLObjectItf _slEngine = nullptr;
static SLEngineItf _slEngineItf = nullptr;
static SLObjectItf _slMixer = nullptr;
static uint32_t _slRefCnt = 0;

static int64_t _secToUs(float seconds)
{
    return static_cast<int64_t>(seconds * static_cast<float>(ONE_SECOND_US) + 0.5f);
}

static int64_t _framesToUs(uint32_t frames, int32_t sampleRate)
{
    if (sampleRate <= 0) return 0;
    return (static_cast<int64_t>(frames) * ONE_SECOND_US) / sampleRate;
}

static float _usToSec(int64_t timeUs)
{
    return static_cast<float>(timeUs) / static_cast<float>(ONE_SECOND_US);
}

static int64_t _now()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * ONE_SECOND_US + ts.tv_nsec / 1000;
}

static AMediaExtractor* _extractor(int fd, int64_t length)
{
    auto extractor = AMediaExtractor_new();
    if (!extractor) return nullptr;
    if (AMediaExtractor_setDataSourceFd(extractor, fd, 0, length) == AMEDIA_OK) return extractor;

    AMediaExtractor_delete(extractor);
    return nullptr;
}

static void _slRelease()
{
    if (_slMixer) {
        (*_slMixer)->Destroy(_slMixer);
        _slMixer = nullptr;
    }
    if (_slEngine) {
        (*_slEngine)->Destroy(_slEngine);
        _slEngine = nullptr;
        _slEngineItf = nullptr;
    }
}

static void _slClose()
{
    ScopedLock lock(_slKey);
    if (_slRefCnt == 0 || --_slRefCnt > 0) return;
    _slRelease();
}

static bool _ok(SLresult result)
{
    return result == SL_RESULT_SUCCESS;
}

static void _callback(SLAndroidSimpleBufferQueueItf, void* context)
{
    static_cast<AudioDecoder*>(context)->consume();
}

static uint8_t _byte(int value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<uint8_t>(value);
}

static pixel_t _abgr(int y, int u, int v)
{
    //bt.601 fixed-point (https://learn.microsoft.com/en-us/windows/win32/medfound/recommended-8-bit-yuv-formats-for-video-rendering)
    auto c = y - 16;
    auto d = u - 128;
    auto e = v - 128;
    if (c < 0) c = 0;

    auto r = _byte((298 * c + 409 * e + 128) >> 8);
    auto g = _byte((298 * c - 100 * d - 208 * e + 128) >> 8);
    auto b = _byte((298 * c + 516 * d + 128) >> 8);
    return 0xff000000 | (static_cast<pixel_t>(b) << 16) | (static_cast<pixel_t>(g) << 8) | r;
}

//clamped view of a codec output buffer's payload; null when there is nothing to take
static uint8_t* _payload(AMediaCodec* codec, size_t idx, const AMediaCodecBufferInfo& info, size_t& len)
{
    if (info.size <= 0 || info.offset < 0) return nullptr;

    size_t capacity = 0;
    auto buffer = AMediaCodec_getOutputBuffer(codec, idx, &capacity);
    if (!buffer || static_cast<size_t>(info.offset) >= capacity) return nullptr;

    len = static_cast<size_t>(info.size);
    if (info.offset + len > capacity) len = capacity - info.offset;
    return buffer + info.offset;
}

static bool _grow(uint8_t*& data, size_t& capacity, size_t size)
{
    if (capacity >= size) return true;

    auto buffer = tvg::realloc<uint8_t>(data, size);
    if (!buffer) return false;
    data = buffer;
    capacity = size;
    return true;
}

//select the track and bring up a running decoder for it
static AMediaCodec* _codec(AMediaExtractor* extractor, const Track& track)
{
    auto format = AMediaExtractor_getTrackFormat(extractor, track.idx);
    if (!format) return nullptr;

    const char* mime = nullptr;
    if (!AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
        AMediaFormat_delete(format);
        return nullptr;
    }

    auto codec = AMediaCodec_createDecoderByType(mime);
    if (codec) {
        if (AMediaExtractor_selectTrack(extractor, track.idx) == AMEDIA_OK && AMediaCodec_configure(codec, format, nullptr, nullptr, 0) == AMEDIA_OK && AMediaCodec_start(codec) == AMEDIA_OK) {
            AMediaFormat_delete(format);
            return codec;
        }
        AMediaCodec_delete(codec);
    }
    AMediaFormat_delete(format);
    return nullptr;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool mediaScan(int fd, int64_t length, Track* audio, Track* video)
{
    auto extractor = _extractor(fd, length);
    if (!extractor) return false;

    auto count = AMediaExtractor_getTrackCount(extractor);
    for (auto i = 0u; i < count; i++) {
        auto format = AMediaExtractor_getTrackFormat(extractor, i);
        if (!format) continue;

        const char* mime = nullptr;
        int64_t durationUs = 0;
        AMediaFormat_getInt64(format, AMEDIAFORMAT_KEY_DURATION, &durationUs);

        if (AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
            if (!strncmp(mime, "audio/", 6) && audio->idx == Track::NONE) {
                audio->idx = i;
                audio->durationUs = durationUs;
            } else if (!strncmp(mime, "video/", 6) && video->idx == Track::NONE) {
                video->idx = i;
                video->durationUs = durationUs;
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &video->width);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &video->height);
            }
        }

        AMediaFormat_delete(format);
    }

    AMediaExtractor_delete(extractor);
    return true;
}

//rewind the stream and drop in-flight codec work so decoding resumes at targetUs
bool Decoder::restart(int64_t targetUs, SeekMode mode)
{
    if (AMediaExtractor_seekTo(extractor, targetUs, mode) != AMEDIA_OK) return false;
    if (AMediaCodec_flush(codec) != AMEDIA_OK) return false;

    inputDone = outputDone = false;
    return true;
}

bool Decoder::extract()
{
    if (inputDone) return true;

    auto idx = AMediaCodec_dequeueInputBuffer(codec, CODEC_TIMEOUT_US);
    if (idx < 0) return true;

    size_t capacity = 0;
    auto buffer = AMediaCodec_getInputBuffer(codec, static_cast<size_t>(idx), &capacity);
    if (!buffer) return false;

    auto size = AMediaExtractor_readSampleData(extractor, buffer, capacity);
    if (size < 0) {
        //the track ran out: return the borrowed buffer empty, flagged as the end of the stream
        inputDone = true;
        return AMediaCodec_queueInputBuffer(codec, static_cast<size_t>(idx), 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) == AMEDIA_OK;
    }

    //the codec echoes the submitted pts back on its decoded output
    auto ptsUs = AMediaExtractor_getSampleTime(extractor);
    if (ptsUs < 0) ptsUs = 0;
    if (AMediaCodec_queueInputBuffer(codec, static_cast<size_t>(idx), 0, static_cast<size_t>(size), static_cast<uint64_t>(ptsUs), 0) != AMEDIA_OK) return false;

    AMediaExtractor_advance(extractor);
    return true;
}

void Decoder::close()
{
    if (codec) {
        AMediaCodec_stop(codec);
        AMediaCodec_delete(codec);
        codec = nullptr;
    }
    if (extractor) {
        AMediaExtractor_delete(extractor);
        extractor = nullptr;
    }
    discardBeforeUs = -1;
    inputDone = outputDone = false;
}

bool VideoDecoder::open(int fd, int64_t length, const Track& info, int64_t durationUs)
{
    extractor = _extractor(fd, length);
    if (!extractor) return false;

    if (!(codec = _codec(extractor, info))) {
        close();
        return false;
    }
    colorFormat = COLOR_FORMAT_YUV420_PLANAR;
    width = info.width;
    height = info.height;
    stride = info.width;
    sliceHeight = info.height;
    duration = _usToSec(durationUs);
    format();
    return true;
}

void VideoDecoder::close()
{
    releaseFrames();
    Decoder::close();
    tvg::free(scratch.data);
    scratch = {};
}

void VideoDecoder::clearFrames()
{
    read = write = queued = 0;
    serial++;
    for (auto& frame : frames) frame.ready = false;
}

void VideoDecoder::releaseFrames()
{
    for (auto& frame : frames) {
        tvg::free(frame.data);
        frame.data = nullptr;
    }
    clearFrames();
}

bool VideoDecoder::setupFrames(uint32_t w, uint32_t h)
{
    bufW = w;
    bufH = h;
    for (auto& frame : frames) {
        frame.data = tvg::calloc<pixel_t>(static_cast<size_t>(w) * h, sizeof(pixel_t));
        if (!frame.data) {
            releaseFrames();
            return false;
        }
    }
    return true;
}

void VideoDecoder::format()
{
    auto output = AMediaCodec_getOutputFormat(codec);
    if (!output) return;

    AMediaFormat_getInt32(output, AMEDIAFORMAT_KEY_WIDTH, &width);
    AMediaFormat_getInt32(output, AMEDIAFORMAT_KEY_HEIGHT, &height);
    if (!AMediaFormat_getInt32(output, AMEDIAFORMAT_KEY_STRIDE, &stride)) stride = width;
    //literal key: the AMEDIAFORMAT_KEY_SLICE_HEIGHT symbol requires api 28, the entry itself doesn't
    if (!AMediaFormat_getInt32(output, "slice-height", &sliceHeight)) sliceHeight = height;
    AMediaFormat_getInt32(output, AMEDIAFORMAT_KEY_COLOR_FORMAT, &colorFormat);

    if (width <= 0) width = 1;
    if (height <= 0) height = 1;
    if (stride < width) stride = width;
    if (sliceHeight < height) sliceHeight = height;

    AMediaFormat_delete(output);
}

//precise seek: decode forward from the previous keyframe, discarding everything short of the target
bool VideoDecoder::seek(float seconds)
{
    auto targetUs = _secToUs(seconds);
    clearFrames();
    if (!restart(targetUs, AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC)) return false;
    discardBeforeUs = targetUs;
    return true;
}

bool VideoDecoder::decode(bool& staged, bool& yielded, float seconds)
{
    //frames more than two periods behind the clock can never reach the screen: drop them undecoded
    auto lateUs = seconds < 0.0f ? -1 : _secToUs(seconds) - 2 * PUMP_MAX_WAIT_US;
    staged = false;
    yielded = false;

    if (outputDone || queued >= FRAME_COUNT) return true;

    //feed the codec one sample and poll for a decoded output
    if (!extract()) return false;
    AMediaCodecBufferInfo info = {};
    auto idx = AMediaCodec_dequeueOutputBuffer(codec, &info, CODEC_TIMEOUT_US);
    if (idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) return true; //nothing ready yet is a normal pass
    yielded = true;

    //the codec announces its real output spec (stride/color format) before the first frame
    if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
        format();
        return true;
    }
    if (idx < 0) return true;

    //skip pre-seek output and, while catching up, stale frames the clock already passed
    auto ret = true;
    if (info.presentationTimeUs >= discardBeforeUs && (lateUs < 0 || info.presentationTimeUs >= lateUs)) {
        discardBeforeUs = -1;
        size_t len = 0;
        //copy the payload out so the codec buffer is returned before the (unlocked) conversion
        if (auto data = _payload(codec, static_cast<size_t>(idx), info, len)) {
            if (_grow(scratch.data, scratch.capacity, len)) {
                memcpy(scratch.data, data, len);
                scratch.size = len;
                scratch.ptsUs = info.presentationTimeUs;
                staged = true;
            } else ret = false;
        }
    }

    if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) outputDone = true;
    AMediaCodec_releaseOutputBuffer(codec, static_cast<size_t>(idx), false);
    return ret;
}

bool VideoDecoder::convert()
{
    if (bufW != static_cast<uint32_t>(width) || bufH != static_cast<uint32_t>(height)) return false;

    auto planar = colorFormat == COLOR_FORMAT_YUV420_PLANAR || colorFormat == COLOR_FORMAT_YUV420_PACKED_PLANAR;
    if (!planar && colorFormat != COLOR_FORMAT_YUV420_SEMIPLANAR && colorFormat != COLOR_FORMAT_YUV420_PACKED_SEMIPLANAR) {
        TVGERR("MEDIA", "Unsupported decoder color format: %d (0x%x)", colorFormat, colorFormat);
        return false;
    }

    //locate the uv planes: trust the reported slice height, unless the payload size proves the y plane is taller
    auto yStride = stride;
    auto rows = sliceHeight;
    auto guessedRows = static_cast<int32_t>((scratch.size * 2) / (static_cast<size_t>(yStride) * 3));   //yuv420 total = y plane x 3/2, inverted
    if (guessedRows > rows) rows = guessedRows;
    auto ySize = static_cast<size_t>(yStride) * rows;
    auto uvRows = (rows + 1) / 2;

    //planar: separate u/v planes; semiplanar: one interleaved uv plane
    auto uvStride = planar ? yStride / 2 : yStride;
    auto uvSize = static_cast<size_t>(uvStride) * uvRows;
    if (uvStride <= 0 || scratch.size < ySize + uvSize * (planar ? 2u : 1u)) return false;

    auto uPlane = scratch.data + ySize;
    auto vOffset = planar ? uvSize : 1;
    auto step = planar ? 1u : 2u;
    auto w = planar ? bufW : (bufW & ~1u);   //interleaved uv: stay on even pixel pairs
    if (w == 0) return false;

    //4:2:0 upsampling: each 2x2 pixel block shares one chroma sample (hence the y/2 and x/2)
    auto& frame = frames[write];
    for (auto y = 0u; y < bufH; y++) {
        auto yRow = scratch.data + y * yStride;
        auto uRow = uPlane + (y / 2) * uvStride;
        auto out = frame.data + y * bufW;
        for (auto x = 0u; x < w; x++) {
            auto uv = (x / 2) * step;
            out[x] = _abgr(yRow[x], uRow[uv], uRow[uv + vOffset]);
        }
    }
    frame.ptsUs = scratch.ptsUs;
    return true;
}

void VideoDecoder::publish(uint32_t snapshot)
{
    if (serial != snapshot) return;
    frames[write].ready = true;
    write = (write + 1) % FRAME_COUNT;
    queued++;
}

bool VideoDecoder::present(RenderSurface* surface, float seconds, bool immediate)
{
    auto timeUs = _secToUs(seconds);
    Frame* latest = nullptr;
    while (queued > 0) {
        auto& frame = frames[read];
        if (frame.ptsUs > timeUs && !(immediate && !latest)) break;

        latest = &frame;
        frame.ready = false;
        read = (read + 1) % FRAME_COUNT;
        queued--;
        if (immediate && frame.ptsUs > timeUs) break;
    }
    if (!latest) return false;

    //copying (~1ms at 4k) keeps surface.data stable for outside holders such as Picture::data()
    memcpy(surface->data, latest->data, static_cast<size_t>(surface->w) * surface->h * sizeof(pixel_t));
    return true;
}

bool VideoDecoder::still(RenderSurface* surface, float seconds)
{
    auto deadline = _now() + PUMP_MAX_WAIT_US;
    do {
        for (auto i = 0u; i < DRAIN_MAX; i++) {
            auto staged = false;
            auto yielded = false;
            if (!decode(staged, yielded)) return false;
            if (staged) {
                if (!convert()) return false;
                publish(serial);
                break;
            }
            if (!yielded) break;
        }
        if (present(surface, seconds, true)) return true;
        if (outputDone) break;
        usleep(1000);   //give the codec a breath before the next drain round
    } while (_now() < deadline);
    return false;
}

int64_t VideoDecoder::wait(float seconds)
{
    if (queued == 0 || (queued < FRAME_COUNT && !outputDone)) return PUMP_POLL_US;

    auto waitUs = frames[read].ptsUs - _secToUs(seconds);
    if (waitUs <= 0) return PUMP_POLL_US;
    return waitUs > PUMP_MAX_WAIT_US ? PUMP_MAX_WAIT_US : waitUs;
}

bool AudioDecoder::open(int fd, int64_t length, const Track& info, float level, bool mute)
{
    extractor = _extractor(fd, length);
    if (!extractor) return false;

    auto valid = false;
    SLuint32 channelMask = 0;
    auto format = AMediaExtractor_getTrackFormat(extractor, info.idx);
    if (!format) goto fail;

    //reject what the sink can't represent (mono/stereo only) before standing up a codec
    valid = AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &sampleRate) &&
            AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels) &&
            sampleRate > 0;
    AMediaFormat_delete(format);
    channelMask = channels == 2 ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT) : (channels == 1 ? SL_SPEAKER_FRONT_CENTER : 0);
    if (!valid || channelMask == 0 || !(codec = _codec(extractor, info))) goto fail;
    durationUs = info.durationUs;

    {
        SLDataLocator_AndroidSimpleBufferQueue locQueue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, CHUNK_COUNT};
        SLDataFormat_PCM pcm = {
            SL_DATAFORMAT_PCM,
            static_cast<SLuint32>(channels),
            static_cast<SLuint32>(sampleRate * 1000), //opensl expects the sample rate in milliHz
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            channelMask,
            SL_BYTEORDER_LITTLEENDIAN
        };
        SLDataSource source = {&locQueue, &pcm};

        //the first AudioDecoder creates the shared engine + output mix; the rest only bump the refCnt
        {
            ScopedLock lock(_slKey);
            if (_slRefCnt == 0) {
                if (slCreateEngine(&_slEngine, 0, nullptr, 0, nullptr, nullptr) != SL_RESULT_SUCCESS ||
                    (*_slEngine)->Realize(_slEngine, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS ||
                    (*_slEngine)->GetInterface(_slEngine, SL_IID_ENGINE, &_slEngineItf) != SL_RESULT_SUCCESS ||
                    (*_slEngineItf)->CreateOutputMix(_slEngineItf, &_slMixer, 0, nullptr, nullptr) != SL_RESULT_SUCCESS ||
                    (*_slMixer)->Realize(_slMixer, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) {
                    TVGERR("MEDIA", "Failed to create the audio engine");
                    _slRelease();
                    goto fail;
                }
            }
            ++_slRefCnt;
        }

        //safe without _slKey: our reference above pins the engine and the mixer
        SLDataLocator_OutputMix locOut = {SL_DATALOCATOR_OUTPUTMIX, _slMixer};
        SLDataSink sink = {&locOut, nullptr};
        const SLInterfaceID ids[2] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_VOLUME};
        const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
        if ((*_slEngineItf)->CreateAudioPlayer(_slEngineItf, &player, &source, &sink, 2, ids, req) != SL_RESULT_SUCCESS) {
            TVGERR("MEDIA", "Failed to create the audio player: %dHz/%dch", sampleRate, channels);
            _slClose();
            goto fail;
        }
    }

    //from here on close() releases both the player and the shared engine reference
    if (!_ok((*player)->Realize(player, SL_BOOLEAN_FALSE))) goto fail;
    if (!_ok((*player)->GetInterface(player, SL_IID_PLAY, &playItf))) goto fail;
    if (!_ok((*player)->GetInterface(player, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &queueItf))) goto fail;
    if (!_ok((*player)->GetInterface(player, SL_IID_VOLUME, &volumeItf))) goto fail;
    if (!_ok((*queueItf)->RegisterCallback(queueItf, _callback, this))) goto fail;
    if (!volume(level, mute)) goto fail;
    return true;

fail:
    close();
    return false;
}

void AudioDecoder::close()
{
    if (player) {
        (*player)->Destroy(player);
        player = nullptr;
        playItf = nullptr;
        queueItf = nullptr;
        volumeItf = nullptr;
        _slClose();
    }

    Decoder::close();

    for (auto& chunk : chunks) {
        tvg::free(chunk.data);
        chunk.data = nullptr;
        chunk.capacity = 0;
    }
}

bool AudioDecoder::volume(float level, bool muted)
{
    if (!volumeItf) return true;   //silent media: nothing to control

    //opensl volume is attenuation in millibels, 1/100 dB. amplitude in dB = 20 * log10(gain)
    //(https://en.wikipedia.org/wiki/Decibel), so mB = 20 * 100 * log10(gain), floored at the api minimum
    auto millibel = level <= 0.0f ? SL_MILLIBEL_MIN : static_cast<SLmillibel>(2000.0f * log10f(level));
    if (millibel < SL_MILLIBEL_MIN) millibel = SL_MILLIBEL_MIN;
    if (!_ok((*volumeItf)->SetVolumeLevel(volumeItf, millibel))) return false;
    return _ok((*volumeItf)->SetMute(volumeItf, muted ? SL_BOOLEAN_TRUE : SL_BOOLEAN_FALSE));
}

bool AudioDecoder::play()
{
    //start only with a full ring or a drained stream
    if (codec && queued.load() < CHUNK_COUNT && !outputDone) {
        starting = true;
        return true;
    }

    if (queued.load() > 0) {
        auto ptsUs = chunks[read.load()].ptsUs;
        if (ptsUs > timeUs.load()) timeUs = ptsUs;
    }
    anchorUs = _now();
    if (playItf && !_ok((*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING))) return false;
    playing = true;
    starting = false;
    return true;
}

bool AudioDecoder::pause()
{
    auto nowUs = time();
    if (playItf && !_ok((*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PAUSED))) return false;
    timeUs = nowUs;
    playing = false;
    starting = false;
    return true;
}

bool AudioDecoder::seek(float seconds)
{
    auto targetUs = _secToUs(seconds);
    if (codec) {
        //stop the sink and drop the queued chunks before moving the stream
        if (!_ok((*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED))) return false;
        if (!_ok((*queueItf)->Clear(queueItf))) return false;
        {
            //an in-flight callback may still be consuming: reset the ring only once it retires
            ScopedLock lock(key);
            read = 0;
            write = 0;
            queued = 0;
        }
        if (!restart(targetUs, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC)) return false;
        discardBeforeUs = targetUs;
    }
    playing = false;
    starting = false;
    timeUs = targetUs;
    anchorUs = _now();
    return true;
}

int64_t AudioDecoder::time()
{
    auto currentUs = timeUs.load();
    if (!playing) return currentUs;

    auto elapsedUs = _now() - anchorUs.load();
    if (elapsedUs < 0) elapsedUs = 0;

    //nothing left to consume (no track, or the stream fully drained): extend with the wall clock
    if (queued.load() == 0) {
        if (!codec || outputDone) return currentUs + elapsedUs;
        return currentUs;   //starving mid-stream: hold, so video never outruns audible audio
    }

    auto& chunk = chunks[read.load()];
    auto endUs = chunk.ptsUs + _framesToUs(chunk.frames, sampleRate);
    if (currentUs + elapsedUs > endUs) return endUs;
    return currentUs + elapsedUs;
}

float AudioDecoder::curTime()
{
    return _usToSec(time());
}

bool AudioDecoder::queue(const uint8_t* data, size_t size, int64_t ptsUs)
{
    if (size == 0) return true;

    auto& chunk = chunks[write];
    if (!_grow(chunk.data, chunk.capacity, size)) return false;

    memcpy(chunk.data, data, size);
    chunk.frames = static_cast<uint32_t>(size / (static_cast<size_t>(channels) * sizeof(int16_t)));
    chunk.ptsUs = ptsUs;
    if (chunk.frames == 0) return true;

    auto empty = queued.load() == 0;
    queued++;
    if (!_ok((*queueItf)->Enqueue(queueItf, chunk.data, static_cast<SLuint32>(size)))) {
        queued--;
        return false;
    }
    //re-anchor the clock when playback resumes from a drained queue
    if (empty && playing && ptsUs > timeUs.load()) {
        timeUs = ptsUs;
        anchorUs = _now();
    }
    write = (write + 1) % CHUNK_COUNT;
    return true;
}

bool AudioDecoder::decode()
{
    if (outputDone || queued.load() >= CHUNK_COUNT) return true;
    if (!extract()) return false;

    AMediaCodecBufferInfo info = {};
    auto idx = AMediaCodec_dequeueOutputBuffer(codec, &info, CODEC_TIMEOUT_US);
    if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
        //the sink pcm spec is fixed at open — a changed rate (he-aac sbr) would play wrong; log only
        int32_t rate = sampleRate, chans = channels;
        if (auto output = AMediaCodec_getOutputFormat(codec)) {
            AMediaFormat_getInt32(output, AMEDIAFORMAT_KEY_SAMPLE_RATE, &rate);
            AMediaFormat_getInt32(output, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &chans);
            AMediaFormat_delete(output);
        }
        if (rate != sampleRate || chans != channels) TVGERR("MEDIA", "Unsupported audio format change: %dHz/%dch -> %dHz/%dch", sampleRate, channels, rate, chans);
        return true;
    }
    if (idx < 0) return true;

    auto ret = true;
    if (info.presentationTimeUs >= discardBeforeUs) {   //else: discard pre-seek output
        discardBeforeUs = -1;
        size_t len = 0;
        if (auto data = _payload(codec, static_cast<size_t>(idx), info, len)) {
            //trim the encoder padding past the track duration (it clicks at loop seams)
            if (durationUs > 0) {
                auto frames = (durationUs - info.presentationTimeUs) * sampleRate / ONE_SECOND_US;
                auto limit = frames > 0 ? static_cast<size_t>(frames) * channels * sizeof(int16_t) : 0;
                if (len > limit) len = limit;
            }
            ret = queue(data, len, info.presentationTimeUs);
        }
    }

    if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) outputDone = true;
    AMediaCodec_releaseOutputBuffer(codec, static_cast<size_t>(idx), false);
    return ret;
}

bool AudioDecoder::pump(bool rollover)
{
    if (!codec) return true;   //silent media: nothing to decode

    if (rollover) {
        //gapless wrap: rewind before the input runs dry (skipped during a seek preroll)
        if (!inputDone && discardBeforeUs < 0 && AMediaExtractor_getSampleTrackIndex(extractor) < 0) {
            if (AMediaExtractor_seekTo(extractor, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC) != AMEDIA_OK) return false;
        //drained anyway: restart from the top
        } else if (outputDone) {
            if (!restart(0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC)) return false;
            discardBeforeUs = -1;
        }
    }

    for (auto i = 0u; i < CHUNK_COUNT && queued.load() < CHUNK_COUNT && !outputDone; i++) {
        if (!decode()) return false;
    }
    return true;
}

void AudioDecoder::consume()
{
    ScopedLock lock(key);   //a concurrent seek must not reset the ring between the check and the decrement
    if (queued.load() == 0) return;

    auto idx = read.load();
    auto& chunk = chunks[idx];
    timeUs = chunk.ptsUs + _framesToUs(chunk.frames, sampleRate);
    anchorUs = _now();
    read = (idx + 1) % CHUNK_COUNT;
    queued--;
}