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

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <sys/mman.h>
#include <unistd.h>

#include "tvgGstMediaLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static float _seconds(gint64 time)
{
    return static_cast<float>(time) / static_cast<float>(GST_SECOND);
}

static gint64 _nanoseconds(float time)
{
    return static_cast<gint64>(static_cast<double>(time) * GST_SECOND);
}

void GstMediaLoader::state(GstState state)
{
    if (gst_element_set_state(playbin, state) == GST_STATE_CHANGE_FAILURE) {
        TVGERR("GST", "Failed to change pipeline state to %s.", gst_element_state_get_name(state));
    }
}

GstFlowReturn GstMediaLoader::publish(GstSample* sample)
{
    if (!sample) return GST_FLOW_OK;

    GstVideoInfo info;
    GstVideoFrame vframe;
    if (gst_video_info_from_caps(&info, gst_sample_get_caps(sample)) && gst_video_frame_map(&vframe, &info, gst_sample_get_buffer(sample), GST_MAP_READ)) {
        auto width = static_cast<uint32_t>(GST_VIDEO_FRAME_WIDTH(&vframe));
        auto height = static_cast<uint32_t>(GST_VIDEO_FRAME_HEIGHT(&vframe));
        auto src = static_cast<uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(&vframe, 0));
        auto lw = static_cast<uint32_t>(w);
        auto lh = static_cast<uint32_t>(h);

        // lw is 0 when the first preroll arrives (during open()'s NULL-to-PAUSED transition), before open() sets w/h.
        if (lw == 0 || (width == lw && height == lh)) {
            auto srcStride = static_cast<uint32_t>(GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 0));
            auto rowBytes = width * sizeof(uint32_t);

            auto dst = ring.frames[ring.write];
            if (!dst) dst = ring.frames[ring.write] = tvg::malloc<uint32_t>(rowBytes * height);

            if (srcStride == rowBytes) memcpy(dst, src, rowBytes * height);
            else {
                for (auto y = 0U; y < height; ++y) {
                    memcpy(reinterpret_cast<uint8_t*>(dst) + y * rowBytes, src + y * srcStride, rowBytes);
                }
            }

            ScopedLock lock(key);
            ring.latest = ring.write;
            ring.write = (ring.write + 1) % BUFFER_COUNT;
            ring.updated = true;
        }
        gst_video_frame_unmap(&vframe);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

bool GstMediaLoader::completeSeek()
{
    if (!seeking.active) return false;
    // The state stays ASYNC until the seek shows its first frame.
    if (gst_element_get_state(playbin, nullptr, nullptr, 0) == GST_STATE_CHANGE_ASYNC) return false;

    seeking.active = false;
    return true;
}

// Seeks finish asynchronously: while one is running, only the latest target is kept and sent after it.
bool GstMediaLoader::seekTo(gint64 position)
{
    curTime = _seconds(position);
    if (seeking.active && !completeSeek()) {
        seeking.pending = position;
        return true;
    }

    // send_event() takes ownership; keep the seqnum to reject the EOS of earlier playback.
    auto event = gst_event_new_seek(1.0, GST_FORMAT_TIME, static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                                    GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
    auto seq = gst_event_get_seqnum(event);
    seeking.pending = -1;
    if (!gst_element_send_event(playbin, event)) {
        TVGERR("GST", "Failed to seek the pipeline.");
        return false;
    }
    seeking.seq = seq;
    seeking.active = true;
    return true;
}

void GstMediaLoader::pollBus()
{
    auto bus = gst_element_get_bus(playbin);

    while (auto msg = gst_bus_pop(bus)) {
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_EOS: {
                // Ignore EOS from playback superseded by a newer seek; a looping video starts over.
                if ((seeking.seq && gst_message_get_seqnum(msg) != seeking.seq) || seeking.pending >= 0) break;
                if (looping && seekTo(0)) break;
                state(GST_STATE_PAUSED);
                ended = true;
                curTime = totalTime;
                break;
            }
            case GST_MESSAGE_ERROR: {
                GError* err = nullptr;
                gst_message_parse_error(msg, &err, nullptr);
                TVGERR("GST", "Pipeline error: %s", err ? err->message : "Unknown error");
                if (err) g_error_free(err);
                failed = true;
                break;
            }
            default: break;
        }
        gst_message_unref(msg);
    }

    // Send the seek requested while the previous one settled.
    if (completeSeek() && seeking.pending >= 0) seekTo(seeking.pending);

    gst_object_unref(bus);
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

GstMediaLoader::~GstMediaLoader()
{
    if (playbin) {
        // NULL synchronously stops streaming threads before releasing the pipeline.
        gst_element_set_state(playbin, GST_STATE_NULL);
        gst_object_unref(sink);
        gst_object_unref(playbin);
    }
    for (auto data : ring.frames) tvg::free(data);
    tvg::free(surface.data);
}

Result GstMediaLoader::open(const char* data, uint32_t size, const LoaderOps& ops)
{
    auto fd = memfd_create("thorvg-media", MFD_CLOEXEC);
    if (fd < 0) return Result::SystemError;

    uint32_t offset = 0;
    while (offset < size) {
        auto written = ::write(fd, data + offset, size - offset);
        if (written <= 0) {
            ::close(fd);
            return Result::SystemError;
        }
        offset += static_cast<uint32_t>(written);
    }

    char path[64];
    g_snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
    auto ret = open(path, ops);
    ::close(fd);
    return ret;
}

Result GstMediaLoader::open(const char* path, TVG_UNUSED const LoaderOps& ops)
{
#ifdef THORVG_FILE_IO_SUPPORT
    // 1. Create the pipeline: playbin decodes, the appsink takes the frames (no gst_deinit(): GStreamer can't restart).
    gst_init(nullptr, nullptr);

    auto playbin = gst_element_factory_make("playbin", nullptr);
    auto appsink = GST_APP_SINK(gst_element_factory_make("appsink", nullptr));
    auto uri = gst_filename_to_uri(path, nullptr);
    auto ret = Result::InvalidArguments;

    if (!playbin || !appsink || !uri) {
        TVGERR("GST", "Missing playbin/appsink element or invalid path: %s", path);
        ret = Result::SystemError;
        goto cleanup;
    }

    {
        // 2. Request the canvas pixel format.
        auto argb = BitmapLoader::cs == ColorSpace::ARGB8888 || BitmapLoader::cs == ColorSpace::ARGB8888S;
        auto little = G_BYTE_ORDER == G_LITTLE_ENDIAN;
        cs = argb ? ColorSpace::ARGB8888S : ColorSpace::ABGR8888S;
        auto filter = gst_caps_from_string(argb ? (little ? "video/x-raw,format=(string){BGRA,BGRx}" : "video/x-raw,format=(string){ARGB,xRGB}")
                                                : (little ? "video/x-raw,format=(string){RGBA,RGBx}" : "video/x-raw,format=(string){ABGR,xBGR}"));
        gst_app_sink_set_caps(appsink, filter);
        gst_caps_unref(filter);

        // 3. Take frames: preroll for open/pause/seek, samples for playback.
        GstAppSinkCallbacks callbacks = {};
        callbacks.new_preroll = [](GstAppSink* sink, gpointer data) { return static_cast<GstMediaLoader*>(data)->publish(gst_app_sink_pull_preroll(sink)); };
        callbacks.new_sample = [](GstAppSink* sink, gpointer data) { return static_cast<GstMediaLoader*>(data)->publish(gst_app_sink_pull_sample(sink)); };
        gst_app_sink_set_callbacks(appsink, &callbacks, this, nullptr);

        // 4. Attach the media and the appsink; keep a sink ref for caps and position queries.
        g_object_set(playbin, "uri", uri, "video-sink", appsink, nullptr);
        sink = GST_ELEMENT(gst_object_ref(appsink));
        appsink = nullptr;

        this->playbin = playbin;
        playbin = nullptr;

        // 5. Preroll the first frame, bounded so a stalled sink fails open().
        static constexpr auto PREROLL_TIMEOUT = 10 * GST_SECOND;
        if (gst_element_set_state(this->playbin, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE ||
            gst_element_get_state(this->playbin, nullptr, nullptr, PREROLL_TIMEOUT) != GST_STATE_CHANGE_SUCCESS) {
            TVGERR("GST", "Failed to open media: %s", path);
            goto cleanup;
        }

        // 6. Read the duration and the video format the appsink negotiated; no video fails here.
        gint64 duration = 0;
        if (!gst_element_query_duration(this->playbin, GST_FORMAT_TIME, &duration) || duration <= 0) {
            TVGERR("GST", "Invalid media duration: %s", path);
            goto cleanup;
        }

        GstVideoInfo info;
        auto prerolled = gst_pad_get_current_caps(GST_BASE_SINK_PAD(sink));
        auto valid = prerolled && gst_video_info_from_caps(&info, prerolled);
        if (prerolled) gst_caps_unref(prerolled);

        if (!valid || info.width <= 0 || info.height <= 0) {
            TVGERR("GST", "No video track found: %s", path);
            goto cleanup;
        }

        // 7. Publish size, duration, and pixel format (premultiplied without alpha).
        w = static_cast<float>(info.width);
        h = static_cast<float>(info.height);
        if (!GST_VIDEO_INFO_HAS_ALPHA(&info)) cs = cs == ColorSpace::ARGB8888S ? ColorSpace::ARGB8888 : ColorSpace::ABGR8888;
        totalTime = _seconds(duration);
        ret = Result::Success;
    }

cleanup:
    if (playbin) gst_object_unref(playbin);
    if (appsink) gst_object_unref(appsink);
    g_free(uri);
    return ret;
#else
    return Result::NonSupport;
#endif
}

bool GstMediaLoader::read()
{
    if (!Loader::read()) return true;

    surface.setup(surface.data, static_cast<uint32_t>(w), static_cast<uint32_t>(w),
                  static_cast<uint32_t>(h), sizeof(uint32_t), cs, cs == ColorSpace::ARGB8888 || cs == ColorSpace::ABGR8888);

    g_object_set(playbin, "volume", static_cast<double>(audioVolume), "mute", static_cast<gboolean>(muted), nullptr);
    paused = true;
    return sync();
}

bool GstMediaLoader::sync()
{
    pollBus();
    if (failed) return false;

    // Query the video sink: playbin returns the largest sink position, e.g. a delayed audio track's first timestamp.
    gint64 time = 0;
    if (!paused && !ended && !seeking.active && gst_element_query_position(sink, GST_FORMAT_TIME, &time) && time >= 0) curTime = _seconds(time);

    {
        ScopedLock lock(key);

        if (!ring.updated) return surface.data && sharing > 0;

        auto frame = ring.frames[ring.latest];
        ring.frames[ring.latest] = surface.data;
        surface.setup(frame, surface.stride, surface.w, surface.h,
                      surface.channelSize, cs, surface.alphaIgnored);
        ring.updated = false;
    }

    return true;
}

Result GstMediaLoader::play()
{
    if (failed) return Result::Unknown;
    // Playing a finished playback starts it over.
    if (ended) {
        ended = false;
        seekTo(0);
    }
    started = true;
    paused = false;
    state(GST_STATE_PLAYING);
    return Result::Success;
}

Result GstMediaLoader::pause()
{
    if (failed) return Result::Unknown;
    if (!started || ended) return Result::InsufficientCondition;
    paused = true;
    state(GST_STATE_PAUSED);
    return Result::Success;
}

Result GstMediaLoader::stop()
{
    if (failed) return Result::Unknown;
    started = ended = false;
    paused = true;
    state(GST_STATE_PAUSED);
    seekTo(0);
    return Result::Success;
}

Result GstMediaLoader::seek(float seconds)
{
    if (failed) return Result::Unknown;

    // No frame starts at the end: a looping video wraps around, otherwise seek into the last frame.
    auto target = _nanoseconds(seconds);
    if (seconds >= totalTime) target = looping ? 0 : _nanoseconds(totalTime) - GST_MSECOND;
    ended = seconds >= totalTime && !looping;
    seekTo(target);
    if (ended) curTime = totalTime;

    // The pipeline runs only while playing and not ended, so a seek after the end plays on.
    state(!paused && !ended ? GST_STATE_PLAYING : GST_STATE_PAUSED);
    return Result::Success;
}

Result GstMediaLoader::loop(bool on)
{
    if (failed) return Result::Unknown;
    looping = on;
    return Result::Success;
}

Result GstMediaLoader::volume(float volume)
{
    if (failed) return Result::Unknown;
    audioVolume = volume;
    g_object_set(playbin, "volume", static_cast<double>(volume), nullptr);
    return Result::Success;
}

Result GstMediaLoader::mute(bool on)
{
    if (failed) return Result::Unknown;
    muted = on;
    g_object_set(playbin, "mute", static_cast<gboolean>(on), nullptr);
    return Result::Success;
}

MediaLoader* MediaLoader::gen()
{
    return new GstMediaLoader;
}
