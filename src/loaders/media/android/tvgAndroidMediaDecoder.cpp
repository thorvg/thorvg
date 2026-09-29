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

#include "tvgAndroidMediaAudioSink.h"
#include "tvgAndroidMediaDecoder.h"
#include "tvgMath.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static constexpr int32_t COLOR_FORMAT_YUV420_FLEXIBLE = 0x7f420888;  // Java CodecCapabilities API 21; no NDK value enum
static constexpr int32_t ENCODING_PCM_16BIT = 2;                     // Java AudioFormat API 3; no NDK value enum
static constexpr int32_t COLOR_TRANSFER_ST2084 = 6;                 // MediaFormat.COLOR_TRANSFER_ST2084 (Java API 24); no NDK numeric constant
static constexpr int32_t COLOR_TRANSFER_HLG = 7;                    // MediaFormat.COLOR_TRANSFER_HLG (Java API 24); no NDK numeric constant

// a pixel pair shares one chroma sample: a constant chroma step lets the compiler vectorize the loop
template<int32_t STEP>
static void _row(const uint8_t* y, const uint8_t* u, const uint8_t* v, int32_t step, pixel_t* out, int32_t w, const int32_t* k, int32_t y0, int32_t red)
{
    if (STEP) step = STEP;
    auto pixels = [&](int32_t x, int32_t n) {
        const auto d = u[x * step] - 128;
        const auto e = v[x * step] - 128;
        const auto r = k[1] * e + 128, g = 128 - k[2] * d - k[3] * e, b = k[4] * d + 128;
        for (auto i = 2 * x; i < 2 * x + n; ++i) {
            const auto c = (y[i] - y0) * k[0];
            out[i] = 0xff000000 | (tvg::clamp((c + r) >> 8, 0, 255) << red) | (tvg::clamp((c + g) >> 8, 0, 255) << 8) | (tvg::clamp((c + b) >> 8, 0, 255) << (16 - red));
        }
    };
    for (auto x = 0; x < w / 2; ++x) pixels(x, 2);
    if (w & 1) pixels(w / 2, 1);
}

// convert the visible part of a YUV_420_888 image, planar or interleaved alike, into opaque 32-bit pixels
static bool _convert(AImage* image, pixel_t* dst, int32_t width, int32_t height, bool abgr, const int32_t* k, int32_t y0)
{
    AImageCropRect crop;
    if (AImage_getCropRect(image, &crop) != AMEDIA_OK || crop.left < 0 || crop.top < 0) return false;

    // the track size bounds the picture: a smaller decoded crop leaves the rest as it was
    const auto w = std::min(width, crop.right - crop.left);
    const auto h = std::min(height, crop.bottom - crop.top);
    if (w <= 0 || h <= 0) return false;

    uint8_t* plane[3];
    int32_t stride[3], step[3];
    for (auto i = 0; i < 3; ++i) {
        int32_t size;
        if (AImage_getPlaneData(image, i, &plane[i], &size) != AMEDIA_OK || AImage_getPlaneRowStride(image, i, &stride[i]) != AMEDIA_OK || AImage_getPlanePixelStride(image, i, &step[i]) != AMEDIA_OK) return false;
        // a plane the cpu can't reach (a vendor-private layout) may come back null with AMEDIA_OK
        if (!plane[i] || size <= 0 || stride[i] <= 0 || step[i] <= 0) return false;
        const auto shift = i > 0 ? 1 : 0;  // chroma is subsampled by two in both directions
        const auto x = static_cast<int64_t>(crop.left + w - 1) >> shift;
        const auto y = static_cast<int64_t>(crop.top + h - 1) >> shift;
        if (y * stride[i] + x * step[i] >= size) return false;
    }
    // YUV_420_888 keeps the luma samples contiguous and both chroma planes in one layout
    if (step[0] != 1 || step[1] != step[2]) return false;

    const auto red = abgr ? 0 : 16;
    for (auto y = 0; y < h; ++y) {
        const auto sy = crop.top + y;
        // the pixel pairs start at the crop edge, which 4:2:0 codecs keep on an even column
        const auto yRow = plane[0] + static_cast<size_t>(sy) * stride[0] + crop.left;
        const auto uRow = plane[1] + static_cast<size_t>(sy >> 1) * stride[1] + (crop.left >> 1) * step[1];
        const auto vRow = plane[2] + static_cast<size_t>(sy >> 1) * stride[2] + (crop.left >> 1) * step[2];
        const auto out = dst + static_cast<size_t>(y) * width;
        // most decoders interleave the chroma planes
        if (step[1] == 2) _row<2>(yRow, uRow, vRow, step[1], out, w, k, y0, red);
        else _row<0>(yRow, uRow, vRow, step[1], out, w, k, y0, red);
    }
    return true;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

AMediaFormat* Decoder::open(int fd, int64_t length, const char* kind)
{
    extractor = AMediaExtractor_new();
    if (AMediaExtractor_setDataSourceFd(extractor, fd, 0, length) != AMEDIA_OK) return nullptr;

    for (size_t i = 0; i < AMediaExtractor_getTrackCount(extractor); ++i) {
        const auto format = AMediaExtractor_getTrackFormat(extractor, i);
        const char* mime;
        // the kind is a 6-letter mime prefix, "video/" or "audio/"
        if (AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime) && !strncmp(mime, kind, 6)) {
            AMediaFormat_getInt64(format, AMEDIAFORMAT_KEY_DURATION, &durationUs);
            if (AMediaExtractor_selectTrack(extractor, i) == AMEDIA_OK && (codec = AMediaCodec_createDecoderByType(mime))) return format;
            AMediaFormat_delete(format);
            return nullptr;
        }
        AMediaFormat_delete(format);
    }
    return nullptr;
}

void Decoder::close()
{
    // a deleted codec releases itself from any state
    if (codec) AMediaCodec_delete(codec);
    if (extractor) AMediaExtractor_delete(extractor);
    codec = nullptr;
    extractor = nullptr;
}

bool Decoder::seek(int64_t timeUs)
{
    // Treat a failed seek with no remaining sample as EOS.
    if (AMediaExtractor_seekTo(extractor, timeUs, AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC) != AMEDIA_OK && AMediaExtractor_getSampleTime(extractor) >= 0) return false;
    if (fed && AMediaCodec_flush(codec) != AMEDIA_OK) return false;
    fed = inputDone = outputDone = false;
    seekTargetUs = timeUs;
    maxPtsUs = -1;
    return true;
}

bool Decoder::feed()
{
    while (!inputDone) {
        // 1. dequeue: an available input buffer, if any
        const auto idx = AMediaCodec_dequeueInputBuffer(codec, 0);
        if (idx < 0) return idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER;
        size_t capacity;
        const auto buffer = AMediaCodec_getInputBuffer(codec, static_cast<size_t>(idx), &capacity);
        if (!buffer) return false;
        fed = true;

        // 2. read: the next sample, or the borrowed buffer back empty as the end of the stream
        const auto size = AMediaExtractor_readSampleData(extractor, buffer, capacity);
        if (size < 0) {
            inputDone = true;
            return AMediaCodec_queueInputBuffer(codec, static_cast<size_t>(idx), 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) == AMEDIA_OK;
        }

        // 3. queue: the sample with its time, keeping the largest time to spot the last frame
        const auto ptsUs = AMediaExtractor_getSampleTime(extractor);
        if (AMediaCodec_queueInputBuffer(codec, static_cast<size_t>(idx), 0, static_cast<size_t>(size), static_cast<uint64_t>(ptsUs), 0) != AMEDIA_OK) return false;
        maxPtsUs = std::max(maxPtsUs, ptsUs);
        AMediaExtractor_advance(extractor);
    }
    return true;
}

bool VideoDecoder::open(int fd, int64_t length, condition_variable& cv)
{
    const auto format = Decoder::open(fd, length, "video/");
    if (!format) return false;

    // the reader holds one image, converted before the next one is released, and wakes the worker on each
    AImageReader_ImageListener listener = {&cv, [](void* context, AImageReader*) { static_cast<condition_variable*>(context)->notify_one(); }};
    ANativeWindow* window;
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width);
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height);
    // a surface output is otherwise free to take a private layout the cpu can't read
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FORMAT_YUV420_FLEXIBLE);
    const auto ret = durationUs > 0 && width > 0 && height > 0 && AImageReader_new(width, height, AIMAGE_FORMAT_YUV_420_888, 1, &reader) == AMEDIA_OK &&
                    AImageReader_setImageListener(reader, &listener) == AMEDIA_OK && AImageReader_getWindow(reader, &window) == AMEDIA_OK &&
                    AMediaCodec_configure(codec, format, window, nullptr, 0) == AMEDIA_OK && AMediaCodec_start(codec) == AMEDIA_OK;
    return parse(format) && ret;
}

void VideoDecoder::close()
{
    Decoder::close();
    // deleting the reader also retires its listener thread
    if (reader) AImageReader_delete(reader);
    reader = nullptr;
}

bool VideoDecoder::parse(AMediaFormat* format)
{
    auto transfer = 0;
    // literal keys, as their ndk constants need api 28
    AMediaFormat_getInt32(format, "color-standard", &color.standard);
    AMediaFormat_getInt32(format, "color-range", &color.range);
    AMediaFormat_getInt32(format, "color-transfer", &transfer);
    AMediaFormat_delete(format);

    // pq and hlg need a tone mapping which this sdr path doesn't have
    if (transfer == COLOR_TRANSFER_ST2084 || transfer == COLOR_TRANSFER_HLG) {
        TVGERR("MEDIA", "HDR video is not supported");
        return false;
    }
    return true;
}

bool VideoDecoder::seek(int64_t timeUs)
{
    pending.ptsUs = -1;
    latestConvertedPtsUs = INT64_MIN;
    return Decoder::seek(timeUs);
}

Decoder::Decoded VideoDecoder::acquire(Frame& frame)
{
    // 1. acquire: the pending image by its timestamp in ns, dropping any rendered before a seek flush
    AImage* image;
    int64_t timeNs;
    while (true) {
        const auto result = AImageReader_acquireNextImage(reader, &image);
        if (result == AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE) {
            if (monotonic() < pending.deadlineUs) return None;
            TVGERR("MEDIA", "Timed out waiting for a decoded image");
            return Failure;
        }
        if (result != AMEDIA_OK) return Failure;
        if (AImage_getTimestamp(image, &timeNs) == AMEDIA_OK && timeNs == pending.ptsUs * 1000) break;
        AImage_delete(image);
    }

    // 2. convert: with the 8-bit fixed-point yuv to rgb {y scale, v to r, u to g, v to g, u to b}
    // of BT.601, BT.709 and BT.2020 in limited and full range
    // see https://learn.microsoft.com/en-us/windows/win32/medfound/recommended-8-bit-yuv-formats-for-video-rendering
    static constexpr int32_t YUV_COEFFS[3][2][5] = {
        {{298, 409, 100, 208, 516}, {256, 359, 88, 183, 454}},
        {{298, 459, 55, 136, 541}, {256, 403, 48, 120, 475}},
        {{298, 430, 48, 167, 548}, {256, 377, 42, 146, 482}}
    };
    const auto begin = monotonic();
    // the MediaFormat standard 1 is BT.709 and 6 BT.2020, the range 1 full
    const auto k = YUV_COEFFS[color.standard == 1 ? 1 : (color.standard == 6 ? 2 : 0)][color.range == 1 ? 1 : 0];
    const auto ret = _convert(image, frame.data, width, height, frame.cs == ColorSpace::ABGR8888, k, color.range == 1 ? 0 : 16);
    AImage_delete(image);
    if (!ret) {
        TVGERR("MEDIA", "Unsupported decoded image layout");
        return Failure;
    }

    // 3. record: the conversion cost paces the next frames
    averageConvertUs = (averageConvertUs * 9 + monotonic() - begin) / 10;
    frame.ptsUs = latestConvertedPtsUs = pending.ptsUs;
    pending.ptsUs = -1;
    return Converted;
}

Decoder::Decoded VideoDecoder::decode(int64_t playheadUs, Frame& frame)
{
    static constexpr int64_t IMAGE_WAIT_US = 2000000;         // a rendered output that never reaches the reader fails the pipeline
    static constexpr int64_t LATE_INTERVAL_US = 250000;       // behind the playhead, one frame per this much media time is converted
    static constexpr int64_t CONVERT_MARGIN_PERCENT = 20;     // the interval over the measured conversion time

    // 1. pending: the output waiting in the reader converts before any new one
    if (pending.ptsUs >= 0) return acquire(frame);

    // 2. feed: the compressed samples into the codec
    if (!feed()) return Failure;

    while (!outputDone) {
        // 3. dequeue: a new output if the codec has any, a format change updating the color info
        AMediaCodecBufferInfo info;
        const auto idx = AMediaCodec_dequeueOutputBuffer(codec, &info, 0);
        if (idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) return None;
        if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            if (!parse(AMediaCodec_getOutputFormat(codec))) return Failure;
            continue;
        }
        if (idx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) continue;
        if (idx < 0) return Failure;

        // 4. select: which output converts
        // minIntervalUs: the minimum gap between kept frames, longer when behind the playhead
        // lastFrame: always kept, so that a seek past it still lands on a picture
        const auto ptsUs = info.presentationTimeUs;
        auto minIntervalUs = averageConvertUs * (100 + CONVERT_MARGIN_PERCENT) / 100;
        if (ptsUs < playheadUs) minIntervalUs = std::max(minIntervalUs, LATE_INTERVAL_US);
        const auto lastFrame = ptsUs >= maxPtsUs && AMediaExtractor_getSampleTime(extractor) < 0;
        const auto eligible = ptsUs >= seekTargetUs && ptsUs >= latestConvertedPtsUs + minIntervalUs;
        const auto keep = info.size > 0 && (lastFrame || eligible);

        // 5. render: the kept output goes into the reader to convert from there, the others are dropped
        if (AMediaCodec_releaseOutputBuffer(codec, static_cast<size_t>(idx), keep) != AMEDIA_OK) return Failure;
        outputDone = info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
        if (keep) {
            seekTargetUs = -1;
            pending.ptsUs = ptsUs;
            pending.deadlineUs = monotonic() + IMAGE_WAIT_US;
            return acquire(frame);
        }
    }
    return None;
}

bool AudioDecoder::open(int fd, int64_t length)
{
    const auto format = Decoder::open(fd, length, "audio/");
    if (!format) return false;
    // Duration is relative to the first sample's timestamp.
    if (durationUs > 0) endUs = AMediaExtractor_getSampleTime(extractor) + durationUs;
    const auto ret = AMediaCodec_configure(codec, format, nullptr, nullptr, 0) == AMEDIA_OK && AMediaCodec_start(codec) == AMEDIA_OK;
    return parse(format) && ret;
}

bool AudioDecoder::parse(AMediaFormat* format)
{
    auto encoding = ENCODING_PCM_16BIT;  // the default when the codec doesn't say
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &sampleRate);
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);
    AMediaFormat_getInt32(format, "pcm-encoding", &encoding);  // a literal key, as its ndk constant needs api 28
    AMediaFormat_delete(format);
    return sampleRate > 0 && channels > 0 && encoding == ENCODING_PCM_16BIT;
}

bool AudioDecoder::seek(int64_t timeUs)
{
    // the flush reclaims the pending buffer
    pending.idx = -1;
    return Decoder::seek(timeUs);
}

bool AudioDecoder::refill(AudioSink& sink)
{
    // whole pcm frames of a duration, in bytes
    auto bytes = [&](int64_t us) { return static_cast<size_t>(std::max(us, int64_t(0)) * sampleRate / MICROSEC) * channels * sizeof(int16_t); };

    // 1. feed: the compressed samples into the codec
    if (!feed()) return false;

    while (!outputDone && !sink.full()) {
        // 2. dequeue: the pending buffer first, else a new one if the codec has any
        auto& info = pending.info;
        const auto idx = pending.idx >= 0 ? pending.idx : AMediaCodec_dequeueOutputBuffer(codec, &info, 0);
        pending.idx = -1;
        if (idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) break;

        // 3. format: the sink reopens for the actual pcm, as he-aac decodes to twice its announced rate, and to stereo with ps
        if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            if (!parse(AMediaCodec_getOutputFormat(codec)) || !sink.open(sampleRate, channels)) return false;
            continue;
        }
        if (idx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) continue;
        if (idx < 0) return false;

        // 4. trim: from the seek target, so that the sound starts with the first video frame,
        // and to the track end, as the encoder padding past it clicks at the loop seams
        size_t capacity;
        // the ndk pointer already includes info.offset
        auto data = AMediaCodec_getOutputBuffer(codec, static_cast<size_t>(idx), &capacity);
        auto size = data ? static_cast<size_t>(std::max(info.size, 0)) : 0;
        auto ptsUs = info.presentationTimeUs;
        if (seekTargetUs >= 0 && ptsUs < seekTargetUs) {
            const auto skip = std::min(size, bytes(seekTargetUs - ptsUs));
            data += skip;
            size -= skip;
            ptsUs = seekTargetUs;
        }
        if (endUs > 0) size = std::min(size, bytes(endUs - ptsUs));

        // 5. push: the pcm after a long gap stays in the codec until the clock reaches it
        auto ret = true;
        if (size > 0) {
            if (!sink.ready(ptsUs)) {
                pending.idx = idx;
                return true;
            }
            seekTargetUs = -1;
            ret = sink.push(data, size, ptsUs);
        }

        // 6. release: the buffer goes back, and the end of the output finishes the sink
        outputDone = info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
        if (AMediaCodec_releaseOutputBuffer(codec, static_cast<size_t>(idx), false) != AMEDIA_OK || !ret) return false;
        if (outputDone) sink.finish();
    }
    return true;
}
