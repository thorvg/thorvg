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
#include <cmath>
#include <cstring>
#include <utility>
#include <time.h>
#include <unistd.h>

#include "tvgAndroidMediaDecoder.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static constexpr int64_t CODEC_TIMEOUT_US = 0;
static constexpr int64_t ONE_SECOND_US = 1000000;
static constexpr int64_t PTS_JITTER_US = 50000;  // ignore timestamp regressions up to 50ms when detecting a loop rollover
static constexpr int64_t PUMP_MAX_WAIT_US = 33000;  // one frame period at ~30fps: the scheduler's longest sleep and the lateness unit

// java MediaCodecInfo.CodecCapabilities COLOR_Format* values: the ndk ships the format key only
static constexpr int32_t COLOR_FORMAT_YUV420_PLANAR = 19;
static constexpr int32_t COLOR_FORMAT_YUV420_PACKED_PLANAR = 20;
static constexpr int32_t COLOR_FORMAT_YUV420_SEMIPLANAR = 21;
static constexpr int32_t COLOR_FORMAT_YUV420_PACKED_SEMIPLANAR = 39;

// process-wide opensl singletons shared by every AudioDecoder, which owns only its player (the oboe pattern)
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
    if (--_slRefCnt > 0) return;
    _slRelease();
}

static bool _ok(SLresult result)
{
    return result == SL_RESULT_SUCCESS;
}

static uint8_t _byte(int value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<uint8_t>(value);
}

static pixel_t _pixel(int y, int u, int v, bool abgr)
{
    // bt.601 fixed-point (https://learn.microsoft.com/en-us/windows/win32/medfound/recommended-8-bit-yuv-formats-for-video-rendering)
    auto c = y - 16;
    auto d = u - 128;
    auto e = v - 128;
    if (c < 0) c = 0;

    auto r = _byte((298 * c + 409 * e + 128) >> 8);
    auto g = _byte((298 * c - 100 * d - 208 * e + 128) >> 8);
    auto b = _byte((298 * c + 516 * d + 128) >> 8);
    if (abgr) return 0xff000000 | (static_cast<pixel_t>(b) << 16) | (static_cast<pixel_t>(g) << 8) | r;
    return 0xff000000 | (static_cast<pixel_t>(r) << 16) | (static_cast<pixel_t>(g) << 8) | b;
}

// validated view of a codec output buffer's data; null when there is nothing to take
static uint8_t* _outputData(AMediaCodec* codec, size_t idx, const AMediaCodecBufferInfo& info, size_t& len)
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

// select the track and bring up a running decoder for it
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

// reposition the stream and drop in-flight codec work so decoding resumes at targetUs
bool Decoder::seek(int64_t targetUs, SeekMode mode)
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
        // the track ran out: return the borrowed buffer empty, flagged as the end of the stream
        inputDone = true;
        return AMediaCodec_queueInputBuffer(codec, static_cast<size_t>(idx), 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) == AMEDIA_OK;
    }

    // the codec echoes the submitted pts back on its decoded output
    auto ptsUs = AMediaExtractor_getSampleTime(extractor);
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
    seekTargetUs = -1;
    inputDone = outputDone = false;
}

bool VideoDecoder::open(int fd, int64_t length, const Track& info)
{
    extractor = _extractor(fd, length);
    if (!extractor) return false;

    if (!(codec = _codec(extractor, info))) {
        close();
        return false;
    }
    format.color = COLOR_FORMAT_YUV420_PLANAR;
    format.width = info.width;
    format.height = info.height;
    format.stride = info.width;
    format.sliceHeight = info.height;
    updateFormat();
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
    ring.read = ring.write = ring.queued = 0;
    frameRequested = false;
}

void VideoDecoder::releaseFrames()
{
    for (auto& frame : ring.frames) {
        tvg::free(frame.data);
        frame.data = nullptr;
    }
    clearFrames();
}

bool VideoDecoder::setupFrames(uint32_t w, uint32_t h)
{
    ring.width = w;
    ring.height = h;
    for (auto& frame : ring.frames) {
        frame.data = tvg::calloc<pixel_t>(static_cast<size_t>(w) * h, sizeof(pixel_t));
        if (!frame.data) {
            releaseFrames();
            return false;
        }
    }
    return true;
}

void VideoDecoder::updateFormat()
{
    auto output = AMediaCodec_getOutputFormat(codec);
    if (!output) return;

    AMediaFormat_getInt32(output, AMEDIAFORMAT_KEY_WIDTH, &format.width);
    AMediaFormat_getInt32(output, AMEDIAFORMAT_KEY_HEIGHT, &format.height);
    if (!AMediaFormat_getInt32(output, AMEDIAFORMAT_KEY_STRIDE, &format.stride)) format.stride = format.width;
    // literal key: the AMEDIAFORMAT_KEY_SLICE_HEIGHT symbol requires api 28, the entry itself doesn't
    if (!AMediaFormat_getInt32(output, "slice-height", &format.sliceHeight)) format.sliceHeight = format.height;
    AMediaFormat_getInt32(output, AMEDIAFORMAT_KEY_COLOR_FORMAT, &format.color);

    if (format.width <= 0) format.width = 1;
    if (format.height <= 0) format.height = 1;
    if (format.stride < format.width) format.stride = format.width;
    if (format.sliceHeight < format.height) format.sliceHeight = format.height;

    AMediaFormat_delete(output);
}

// precise seek: decode forward from the previous keyframe, discarding everything short of the target
bool VideoDecoder::seek(float seconds)
{
    auto targetUs = _secToUs(seconds);
    clearFrames();
    if (!Decoder::seek(targetUs, AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC)) return false;
    seekTargetUs = targetUs;
    frameRequested = true;
    return true;
}

bool VideoDecoder::decode(bool& staged, bool& yielded, float seconds)
{
    // discard output before the seek target or over two frame periods behind playback
    auto cutoffUs = seekTargetUs;
    if (seconds >= 0.0f) cutoffUs = std::max(cutoffUs, _secToUs(seconds) - 2 * PUMP_MAX_WAIT_US);
    staged = false;
    yielded = false;

    if (outputDone || ring.queued >= FRAME_COUNT) return true;

    // feed the codec one sample and poll for a decoded output
    if (!extract()) return false;
    AMediaCodecBufferInfo info = {};
    auto idx = AMediaCodec_dequeueOutputBuffer(codec, &info, CODEC_TIMEOUT_US);
    if (idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) return true;  // nothing ready yet is a normal pass
    yielded = true;

    // the codec announces its real output spec (stride/color format) before the first frame
    if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
        updateFormat();
        return true;
    }
    if (idx < 0) return true;

    auto ret = true;
    if (info.presentationTimeUs >= cutoffUs) {
        seekTargetUs = -1;
        size_t len = 0;
        // copy the payload out so the codec buffer is returned before the (unlocked) conversion
        if (auto data = _outputData(codec, static_cast<size_t>(idx), info, len)) {
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

bool VideoDecoder::convert(ColorSpace cs)
{
    if (ring.width != static_cast<uint32_t>(format.width) || ring.height != static_cast<uint32_t>(format.height)) return false;

    auto planar = format.color == COLOR_FORMAT_YUV420_PLANAR || format.color == COLOR_FORMAT_YUV420_PACKED_PLANAR;
    if (!planar && format.color != COLOR_FORMAT_YUV420_SEMIPLANAR && format.color != COLOR_FORMAT_YUV420_PACKED_SEMIPLANAR) {
        TVGERR("MEDIA", "Unsupported decoder color format: %d (0x%x)", format.color, format.color);
        return false;
    }

    // locate the uv planes: trust the reported slice height, unless the payload size proves the y plane is taller
    auto yStride = format.stride;
    auto rows = format.sliceHeight;
    auto guessedRows = static_cast<int32_t>((scratch.size * 2) / (static_cast<size_t>(yStride) * 3));  // yuv420 total = y plane x 3/2, inverted
    if (guessedRows > rows) rows = guessedRows;
    auto ySize = static_cast<size_t>(yStride) * rows;
    auto uvRows = (rows + 1) / 2;

    // planar: separate u/v planes; semiplanar: one interleaved uv plane
    auto uvStride = planar ? yStride / 2 : yStride;
    auto uvSize = static_cast<size_t>(uvStride) * uvRows;
    if (uvStride <= 0 || scratch.size < ySize + uvSize * (planar ? 2u : 1u)) return false;

    auto uPlane = scratch.data + ySize;
    auto vOffset = planar ? uvSize : 1;
    auto step = planar ? 1u : 2u;
    auto w = planar ? ring.width : (ring.width & ~1u);  // interleaved uv: stay on even pixel pairs
    auto abgr = cs == ColorSpace::ABGR8888;
    if (w == 0) return false;

    // use 4:2:0 upsampling because only the accepted MediaCodec YUV420 layouts reach here;
    // expand each chroma sample across its 2x2 luma block
    auto& frame = ring.frames[ring.write];
    for (auto y = 0u; y < ring.height; y++) {
        auto yRow = scratch.data + y * yStride;
        auto uRow = uPlane + (y / 2) * uvStride;
        auto out = frame.data + y * ring.width;
        for (auto x = 0u; x < w; x++) {
            auto uv = (x / 2) * step;
            out[x] = _pixel(yRow[x], uRow[uv], uRow[uv + vOffset], abgr);
        }
    }
    frame.ptsUs = scratch.ptsUs;
    return true;
}

void VideoDecoder::commit()
{
    ring.write = (ring.write + 1) % FRAME_COUNT;
    ring.queued++;
}

bool VideoDecoder::present(RenderSurface* surface, float seconds)
{
    auto timeUs = _secToUs(seconds);
    Frame* latest = nullptr;
    // consume every due frame and keep only the newest one, skipping stale intermediates
    while (ring.queued > 0) {
        auto& frame = ring.frames[ring.read];
        if (frame.ptsUs > timeUs && !(frameRequested && !latest)) break;

        latest = &frame;
        ring.read = (ring.read + 1) % FRAME_COUNT;
        ring.queued--;
    }
    // no frame is ready: cancel the request at EOS, otherwise leave it pending
    if (!latest) {
        if (outputDone) frameRequested = false;
        return false;
    }

    // swap ownership instead of copying pixels, recycling the previous surface buffer into the ring
    std::swap(surface->data, latest->data);
    frameRequested = false;
    return true;
}

int64_t VideoDecoder::delay(float seconds)
{
    if (ring.queued < FRAME_COUNT && !outputDone) return PUMP_POLL_US;
    if (ring.queued == 0) return PUMP_MAX_WAIT_US;

    auto delayUs = ring.frames[ring.read].ptsUs - _secToUs(seconds);
    if (delayUs <= 0) return PUMP_POLL_US;
    return std::min(delayUs, PUMP_MAX_WAIT_US);
}

bool AudioDecoder::open(int fd, int64_t length, const Track& info, float level, bool muted)
{
    extractor = _extractor(fd, length);
    if (!extractor) return false;

    auto valid = false;
    SLuint32 channelMask = 0;
    auto format = AMediaExtractor_getTrackFormat(extractor, info.idx);
    if (!format) goto fail;

    // reject what the sink can't represent (mono/stereo only) before standing up a codec
    valid = AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &track.sampleRate) && AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &track.channels) && track.sampleRate > 0;
    AMediaFormat_delete(format);
    channelMask = track.channels == 2 ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT) : (track.channels == 1 ? SL_SPEAKER_FRONT_CENTER : 0);
    if (!valid || channelMask == 0 || !(codec = _codec(extractor, info))) goto fail;
    track.durationUs = info.durationUs;

    {
        SLDataLocator_AndroidSimpleBufferQueue locQueue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, CHUNK_COUNT};
        SLDataFormat_PCM pcm = {
            SL_DATAFORMAT_PCM,
            static_cast<SLuint32>(track.channels),
            static_cast<SLuint32>(track.sampleRate * 1000),  // opensl expects the sample rate in milliHz
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            channelMask,
            SL_BYTEORDER_LITTLEENDIAN};
        SLDataSource source = {&locQueue, &pcm};

        // the first AudioDecoder creates the shared engine + output mix; the rest only bump the refCnt
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

        // safe without _slKey: our reference above pins the engine and the mixer
        SLDataLocator_OutputMix locOut = {SL_DATALOCATOR_OUTPUTMIX, _slMixer};
        SLDataSink target = {&locOut, nullptr};
        const SLInterfaceID ids[2] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_VOLUME};
        const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
        if ((*_slEngineItf)->CreateAudioPlayer(_slEngineItf, &sink.object, &source, &target, 2, ids, req) != SL_RESULT_SUCCESS) {
            TVGERR("MEDIA", "Failed to create the audio player: %dHz/%dch", track.sampleRate, track.channels);
            _slClose();
            goto fail;
        }
    }

    // from here on close() releases both the player and the shared engine reference
    if (!_ok((*sink.object)->Realize(sink.object, SL_BOOLEAN_FALSE))) goto fail;
    if (!_ok((*sink.object)->GetInterface(sink.object, SL_IID_PLAY, &sink.player))) goto fail;
    if (!_ok((*sink.object)->GetInterface(sink.object, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &sink.queue))) goto fail;
    if (!_ok((*sink.object)->GetInterface(sink.object, SL_IID_VOLUME, &sink.volume))) goto fail;
    if (!_ok((*sink.queue)->RegisterCallback(sink.queue, callback, this))) goto fail;
    if (!volume(level) || !mute(muted)) goto fail;
    return true;

fail:
    close();
    return false;
}

void AudioDecoder::close()
{
    if (sink.object) {
        (*sink.object)->Destroy(sink.object);
        sink.object = nullptr;
        sink.player = nullptr;
        sink.queue = nullptr;
        sink.volume = nullptr;
        _slClose();
    }

    Decoder::close();

    for (auto& chunk : ring.chunks) {
        tvg::free(chunk.data);
        chunk.data = nullptr;
        chunk.capacity = 0;
    }
}

bool AudioDecoder::volume(float level)
{
    if (!sink.volume) return true;  // silent media: nothing to control

    // opensl volume is attenuation in millibels, 1/100 dB. amplitude in dB = 20 * log10(gain)
    //(https://en.wikipedia.org/wiki/Decibel), so mB = 20 * 100 * log10(gain), floored at the api minimum
    auto millibel = level <= 0.0f ? SL_MILLIBEL_MIN : static_cast<SLmillibel>(2000.0f * log10f(level));
    if (millibel < SL_MILLIBEL_MIN) millibel = SL_MILLIBEL_MIN;
    return _ok((*sink.volume)->SetVolumeLevel(sink.volume, millibel));
}

bool AudioDecoder::mute(bool on)
{
    if (!sink.volume) return true;  // silent media: nothing to control
    return _ok((*sink.volume)->SetMute(sink.volume, on ? SL_BOOLEAN_TRUE : SL_BOOLEAN_FALSE));
}

// time() evaluates positionUs + (_now() - anchorUs), so reset both whenever the media position changes
void AudioDecoder::anchor(int64_t positionUs)
{
    sink.positionUs = positionUs;
    sink.anchorUs = _now();
}

bool AudioDecoder::buffered()
{
    return !codec || ring.queued.load() >= CHUNK_COUNT || outputDone;
}

bool AudioDecoder::play()
{
    // align the clock with the first PCM buffer when OpenSL starts ahead of the stored position
    auto positionUs = sink.positionUs.load();
    if (ring.queued.load() > 0) {
        auto ptsUs = ring.chunks[ring.read.load()].ptsUs;
        if (ptsUs > positionUs) positionUs = ptsUs;
    }
    anchor(positionUs);
    if (sink.player && !_ok((*sink.player)->SetPlayState(sink.player, SL_PLAYSTATE_PLAYING))) return false;
    sink.playing = true;
    return true;
}

bool AudioDecoder::pause()
{
    auto nowUs = time();
    if (sink.player && !_ok((*sink.player)->SetPlayState(sink.player, SL_PLAYSTATE_PAUSED))) return false;
    anchor(nowUs);
    sink.playing = false;
    return true;
}

bool AudioDecoder::seek(float seconds)
{
    auto targetUs = _secToUs(seconds);
    // the scheduler restarts the stopped sink after refilling PCM if playback remains Playing
    if (codec && !_ok((*sink.player)->SetPlayState(sink.player, SL_PLAYSTATE_STOPPED))) return false;
    sink.playing = false;

    if (codec) {
        if (!_ok((*sink.queue)->Clear(sink.queue))) {
            return false;
        }
        {
            // an in-flight callback may still be consuming: reset the ring only once it retires
            ScopedLock lock(ring.key);
            ring.read = 0;
            ring.write = 0;
            ring.queued = 0;
            ring.nextPtsUs = targetUs;
        }
        if (!Decoder::seek(targetUs, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC)) return false;
        seekTargetUs = targetUs;
    }
    anchor(targetUs);
    sink.realignPending = true;
    return true;
}

// derive playback time from the last PCM position and elapsed monotonic time
int64_t AudioDecoder::time()
{
    auto positionUs = sink.positionUs.load();
    if (!sink.playing) return positionUs;

    auto elapsedUs = _now() - sink.anchorUs.load();
    if (elapsedUs < 0) elapsedUs = 0;

    // nothing left to consume (no track, or the stream fully drained): extend with the monotonic clock
    if (ring.queued.load() == 0) {
        if (!codec || outputDone) return positionUs + elapsedUs;
        return positionUs;  // starving mid-stream: hold, so video never outruns audible audio
    }

    auto& chunk = ring.chunks[ring.read.load()];
    auto endUs = chunk.ptsUs + _framesToUs(chunk.frames, track.sampleRate);
    if (positionUs + elapsedUs > endUs) return endUs;
    return positionUs + elapsedUs;
}

float AudioDecoder::curTime()
{
    return _usToSec(time());
}

bool AudioDecoder::enqueue(const uint8_t* data, size_t size, int64_t ptsUs)
{
    if (size == 0) return true;

    auto frameSize = static_cast<size_t>(track.channels) * sizeof(int16_t);
    auto frames = size / frameSize;
    if (frames == 0) return true;

    // OpenSL plays queued buffers back-to-back, so preserve authored timestamp gaps as silent PCM.
    auto gapUs = ptsUs - ring.nextPtsUs;
    auto silentFrames = gapUs > 0 ? static_cast<size_t>(gapUs * track.sampleRate / ONE_SECOND_US) : 0;
    auto silentSize = silentFrames * frameSize;
    auto queuedSize = silentSize + size;
    auto& chunk = ring.chunks[ring.write];
    if (!_grow(chunk.data, chunk.capacity, queuedSize)) return false;

    if (silentSize > 0) {
        memset(chunk.data, 0, silentSize);
        memcpy(chunk.data + silentSize, data, size);
        chunk.ptsUs = ring.nextPtsUs;
    } else {
        memcpy(chunk.data, data, size);
        chunk.ptsUs = ptsUs;
    }
    chunk.frames = static_cast<uint32_t>(silentFrames + frames);

    auto empty = ring.queued.load() == 0;
    ring.queued++;
    if (!_ok((*sink.queue)->Enqueue(sink.queue, chunk.data, static_cast<SLuint32>(queuedSize)))) {
        ring.queued--;
        return false;
    }
    ring.nextPtsUs = ptsUs + _framesToUs(static_cast<uint32_t>(frames), track.sampleRate);
    // re-anchor the clock when playback resumes from a drained queue
    if (empty && sink.playing && chunk.ptsUs > sink.positionUs.load()) {
        anchor(chunk.ptsUs);
    }
    ring.write = (ring.write + 1) % CHUNK_COUNT;
    return true;
}

bool AudioDecoder::decode()
{
    if (!extract()) return false;

    AMediaCodecBufferInfo info = {};
    auto idx = AMediaCodec_dequeueOutputBuffer(codec, &info, CODEC_TIMEOUT_US);
    if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
        // the sink pcm spec is fixed at open — a changed rate (he-aac sbr) would play wrong; log only
        int32_t rate = track.sampleRate, chans = track.channels;
        if (auto output = AMediaCodec_getOutputFormat(codec)) {
            AMediaFormat_getInt32(output, AMEDIAFORMAT_KEY_SAMPLE_RATE, &rate);
            AMediaFormat_getInt32(output, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &chans);
            AMediaFormat_delete(output);
        }
        if (rate != track.sampleRate || chans != track.channels) { 
            TVGERR("MEDIA", "Unsupported audio format change: %dHz/%dch -> %dHz/%dch", track.sampleRate, track.channels, rate, chans);
        }
        return true;
    }
    if (idx < 0) return true;

    auto ret = true;
    if (info.presentationTimeUs >= seekTargetUs) {  // else: discard pre-seek output
        seekTargetUs = -1;
        size_t len = 0;
        if (auto data = _outputData(codec, static_cast<size_t>(idx), info, len)) {
            // trim the encoder padding past the track duration (it clicks at loop seams)
            if (track.durationUs > 0) {
                auto frames = (track.durationUs - info.presentationTimeUs) * track.sampleRate / ONE_SECOND_US;
                auto limit = frames > 0 ? static_cast<size_t>(frames) * track.channels * sizeof(int16_t) : 0;
                if (len > limit) len = limit;
            }
            ret = enqueue(data, len, info.presentationTimeUs);
        }
    }

    if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) outputDone = true;
    AMediaCodec_releaseOutputBuffer(codec, static_cast<size_t>(idx), false);
    return ret;
}

bool AudioDecoder::pump(bool gaplessLoop)
{
    if (!codec) return true;  // silent media: nothing to decode

    if (gaplessLoop) {
        // recover a drained seek preroll; normally rewind the extractor before EOS for a gapless wrap
        if (outputDone) {
            if (!Decoder::seek(0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC)) return false;
            seekTargetUs = -1;
        } else if (!inputDone && seekTargetUs < 0 &&
                   AMediaExtractor_getSampleTrackIndex(extractor) < 0 &&
                   AMediaExtractor_seekTo(extractor, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC) != AMEDIA_OK) {
            return false;
        }
    }

    for (auto i = 0u; i < CHUNK_COUNT && ring.queued.load() < CHUNK_COUNT && !outputDone; i++) {
        if (!decode()) return false;
    }
    return true;
}

void AudioDecoder::onBufferConsumed()
{
    ScopedLock lock(ring.key);  // a concurrent seek must not reset the ring between the check and the decrement
    if (ring.queued.load() == 0) return;

    auto idx = ring.read.load();
    auto& chunk = ring.chunks[idx];
    auto positionUs = chunk.ptsUs + _framesToUs(chunk.frames, track.sampleRate);
    auto realign = positionUs + PTS_JITTER_US < sink.positionUs.load();  // a PTS rollback beyond jitter means looped audio reached the sink
    anchor(positionUs);
    if (realign) sink.realignPending = true;  // seek video to the restarted audio clock on the next pump
    ring.read = (idx + 1) % CHUNK_COUNT;
    ring.queued--;
}

void AudioDecoder::callback(SLAndroidSimpleBufferQueueItf, void* context)
{
    static_cast<AudioDecoder*>(context)->onBufferConsumed();
}
