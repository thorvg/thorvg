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
#include <linux/memfd.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cstring>

#include "tvgGstMediaLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static constexpr auto PREROLL_TIMEOUT = 10 * GST_SECOND;
static constexpr gint64 AUDIO_BUFFER_TIME_US = 100000;  // Halves PulseAudio's buffer for responsive seeking.
static constexpr gint64 AUDIO_LATENCY_TIME_US = 10000;  // Retains PulseAudio's default write interval.

static float _nanoToSec(gint64 time)
{
    if (!GST_CLOCK_TIME_IS_VALID(time) || time < 0) return 0.0f;
    return static_cast<float>(time) / static_cast<float>(GST_SECOND);
}

static void _state(GstMediaLoader& loader, GstState state)
{
    if (gst_element_set_state(loader.playbin, state) == GST_STATE_CHANGE_FAILURE) {
        TVGERR("GST", "Failed to change pipeline state to %s.", gst_element_state_get_name(state));
    }
}

static void _setupAudio(TVG_UNUSED GstElement* playbin, GstElement* element, TVG_UNUSED gpointer data)
{
    auto factory = gst_element_get_factory(element);
    auto klass = factory ? gst_element_factory_get_metadata(factory, GST_ELEMENT_METADATA_KLASS) : nullptr;
    if (!klass || !g_strrstr(klass, "Audio") || !g_strrstr(klass, "Sink")) return;

    auto obj = G_OBJECT_GET_CLASS(element);
    if (g_object_class_find_property(obj, "buffer-time")) {
        g_object_set(element, "buffer-time", AUDIO_BUFFER_TIME_US, nullptr);
    }
    if (g_object_class_find_property(obj, "latency-time")) {
        g_object_set(element, "latency-time", AUDIO_LATENCY_TIME_US, nullptr);
    }
}

static GstCaps* _caps(ColorSpace& cs)
{
    auto argb = BitmapLoader::cs == ColorSpace::ARGB8888 || BitmapLoader::cs == ColorSpace::ARGB8888S;
    cs = argb ? ColorSpace::ARGB8888S : ColorSpace::ABGR8888S;

    if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        return gst_caps_from_string(argb ? "video/x-raw,format=(string){BGRA,BGRx}"
                                         : "video/x-raw,format=(string){RGBA,RGBx}");
    }
    return gst_caps_from_string(argb ? "video/x-raw,format=(string){ARGB,xRGB}"
                                     : "video/x-raw,format=(string){ABGR,xBGR}");
}

static bool _push(GstMediaLoader& loader, GstSample* sample)
{
    auto caps = gst_sample_get_caps(sample);
    auto buffer = gst_sample_get_buffer(sample);

    GstVideoInfo info;
    if (!caps || !buffer || !gst_video_info_from_caps(&info, caps)) return false;

    GstVideoFrame vframe;
    if (!gst_video_frame_map(&vframe, &info, buffer, GST_MAP_READ)) return false;

    auto width = static_cast<uint32_t>(GST_VIDEO_FRAME_WIDTH(&vframe));
    auto height = static_cast<uint32_t>(GST_VIDEO_FRAME_HEIGHT(&vframe));
    auto src = static_cast<uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(&vframe, 0));

    auto ret = false;
    auto lw = static_cast<uint32_t>(loader.w);
    auto lh = static_cast<uint32_t>(loader.h);

    // lw is 0 when the first preroll arrives (during open()'s NULL-to-PAUSED transition), before open() sets w/h.
    if (src && width > 0 && (lw == 0 || (width == lw && height == lh))) {
        auto srcStride = static_cast<uint32_t>(GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 0));
        auto rowBytes = width * sizeof(uint32_t);
        auto& ring = loader.ring;

        auto dst = ring.frames[ring.write];
        if (!dst) dst = ring.frames[ring.write] = tvg::malloc<uint32_t>(rowBytes * height);

        if (dst) {
            if (srcStride == rowBytes) memcpy(dst, src, rowBytes * height);
            else {
                for (auto y = 0U; y < height; ++y) {
                    memcpy(reinterpret_cast<uint8_t*>(dst) + y * rowBytes, src + y * srcStride, rowBytes);
                }
            }

            ScopedLock lock(loader.key);
            ring.time = _nanoToSec(static_cast<gint64>(GST_BUFFER_PTS(buffer)));
            ring.latest = ring.write;
            ring.write = (ring.write + 1) % BUFFER_COUNT;
            ring.updated = true;
            ret = true;
        }
    }

    gst_video_frame_unmap(&vframe);
    return ret;
}

static GstFlowReturn _publish(GstMediaLoader& loader, GstSample* sample)
{
    if (sample) {
        _push(loader, sample);
        gst_sample_unref(sample);
    }
    return GST_FLOW_OK;
}

// Delivers the preroll frame during open, pause, and seek.
static GstFlowReturn _onPreroll(GstAppSink* sink, gpointer data)
{
    return _publish(*static_cast<GstMediaLoader*>(data), gst_app_sink_pull_preroll(sink));
}

// Delivers each decoded frame during playback.
static GstFlowReturn _onSample(GstAppSink* sink, gpointer data)
{
    return _publish(*static_cast<GstMediaLoader*>(data), gst_app_sink_pull_sample(sink));
}

static void _close(GstMediaLoader& loader)
{
    if (!loader.playbin) return;

    // NULL synchronously stops streaming threads before releasing the pipeline.
    gst_element_set_state(loader.playbin, GST_STATE_NULL);
    gst_object_unref(loader.playbin);
    loader.playbin = nullptr;
    loader.segmented = false;
    loader.seeking = SeekState::None;
    loader.seekSeq = 0;
}

static void _finishPlayback(GstMediaLoader& loader)
{
    gst_element_set_state(loader.playbin, GST_STATE_PAUSED);
    loader.segmented = false;
    loader.seeking = SeekState::None;
    loader.paused = true;

    ScopedLock lock(loader.key);
    loader.ring.time = loader.totalTime;
}

// Sends a seek and tracks its seqnum and segment mode for later bus messages.
static bool _sendSeek(GstMediaLoader& loader, GstSeekType type, gint64 position, bool flush)
{
    auto flags = static_cast<uint32_t>(type == GST_SEEK_TYPE_NONE ? GST_SEEK_FLAG_NONE : GST_SEEK_FLAG_ACCURATE);
    if (flush) flags |= GST_SEEK_FLAG_FLUSH;

    // Segment seeks finish with SEGMENT_DONE so _pollBus() can restart the loop.
    // See: https://gstreamer.freedesktop.org/documentation/additional/design/seeking.html
    if (loader.looping) flags |= GST_SEEK_FLAG_SEGMENT;

    auto event = gst_event_new_seek(1.0, GST_FORMAT_TIME, static_cast<GstSeekFlags>(flags),
                                    type, position, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

    // send_event() takes ownership; preserve the seqnum to reject earlier completions.
    auto seq = gst_event_get_seqnum(event);
    if (!gst_element_send_event(loader.playbin, event)) {
        TVGERR("GST", "Failed to seek the pipeline.");
        return false;
    }

    loader.seekSeq = seq;
    loader.segmented = loader.looping;
    return true;
}

static bool _completeSeek(GstMediaLoader& loader)
{
    if (loader.seeking == SeekState::None) return false;
    if (gst_element_get_state(loader.playbin, nullptr, nullptr, 0) == GST_STATE_CHANGE_ASYNC) return false;

    loader.seeking = SeekState::None;
    return true;
}

static bool _seek(GstMediaLoader& loader, gint64 position)
{
    if (loader.seeking != SeekState::None && !_completeSeek(loader)) return true;

    if (!_sendSeek(loader, GST_SEEK_TYPE_SET, position, true)) return false;
    loader.curTime = _nanoToSec(position);
    loader.seeking = SeekState::Active;
    return true;
}

static bool _updateLoop(GstMediaLoader& loader)
{
    if (loader.seeking != SeekState::None && !_completeSeek(loader)) return true;
    return _sendSeek(loader, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE, false);
}

// Restarts completed playback or applies a deferred loop update before playing.
static void _preparePlay(GstMediaLoader& loader)
{
    if (loader.seeking != SeekState::None) return;
    if (loader.curTime >= loader.totalTime) {
        _seek(loader, 0);
        return;
    }
    if (loader.looping == loader.segmented) return;

    gint64 position = 0;
    if (!gst_element_query_position(loader.playbin, GST_FORMAT_TIME, &position) ||
        !GST_CLOCK_TIME_IS_VALID(position) || position < 0) {
        TVGERR("GST", "Failed to query the pipeline position.");
        return;
    }
    _seek(loader, position);
}

static bool _pollBus(GstMediaLoader& loader)
{
    bool ret = true;
    auto bus = gst_element_get_bus(loader.playbin);
    if (!bus) return ret;

    while (auto msg = gst_bus_pop(bus)) {
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_SEGMENT_DONE: {
                // Ignore a SEGMENT_DONE from an earlier seek.
                if (gst_message_get_seqnum(msg) != loader.seekSeq) break;
                if (loader.paused) {
                    // Replace the completed seek with a flushing restart; non-flushing seeks block in PAUSED.
                    loader.seeking = SeekState::None;
                    if (!_seek(loader, 0)) _finishPlayback(loader);
                } else if (loader.looping) {
                    if (!_sendSeek(loader, GST_SEEK_TYPE_SET, 0, false)) _finishPlayback(loader);
                } else {
                    GstFormat format;
                    gint64 position = 0;
                    gst_message_parse_segment_done(msg, &format, &position);

                    // Loop was disabled after the segment seek. Re-seek its end without SEGMENT
                    // or FLUSH so queued audio/video drains and ordinary EOS completes playback.
                    if (format != GST_FORMAT_TIME ||
                        !_sendSeek(loader, GST_SEEK_TYPE_SET, position, false)) {
                        _finishPlayback(loader);
                    }
                }
                break;
            }
            case GST_MESSAGE_EOS: {
                // Ignore EOS from playback superseded by a newer seek.
                if (loader.seekSeq && gst_message_get_seqnum(msg) != loader.seekSeq) break;
                if (!loader.looping || !_seek(loader, 0)) _finishPlayback(loader);
                break;
            }
            case GST_MESSAGE_ERROR: {
                GError* err = nullptr;
                gst_message_parse_error(msg, &err, nullptr);
                TVGERR("GST", "Pipeline error: %s", err ? err->message : "Unknown error");
                if (err) g_error_free(err);
                ret = false;
                break;
            }
            default: break;
        }
        gst_message_unref(msg);
    }

    // Synchronize loop mode changes deferred while a seek was active.
    if (!loader.paused && _completeSeek(loader) &&
        loader.looping != loader.segmented) _updateLoop(loader);

    gst_object_unref(bus);
    return ret;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

GstMediaLoader::~GstMediaLoader()
{
    _close(*this);
    for (auto data : ring.frames) tvg::free(data);
    tvg::free(surface.data);
}

bool GstMediaLoader::open(const char* data, uint32_t size, const LoaderOps* ops, TVG_UNUSED bool copy)
{
    auto fd = static_cast<int>(syscall(SYS_memfd_create, "thorvg-media", MFD_CLOEXEC));
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

    char path[64];
    g_snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
    auto ret = open(path, ops);
    ::close(fd);
    return ret;
}

bool GstMediaLoader::open(const char* path, TVG_UNUSED const LoaderOps* ops)
{
    // Safe to call per open(); omit gst_deinit() because GStreamer cannot be used afterwards.
    gst_init(nullptr, nullptr);

    auto playbin = gst_element_factory_make("playbin", nullptr);
    auto appsink = GST_APP_SINK(gst_element_factory_make("appsink", nullptr));
    auto uri = gst_filename_to_uri(path, nullptr);
    auto ret = false;

    if (!playbin || !appsink || !uri) {
        TVGERR("GST", "Missing playbin/appsink element or invalid path: %s", path);
        goto cleanup;
    }

    {
        g_signal_connect(playbin, "element-setup", G_CALLBACK(_setupAudio), nullptr);

        auto caps = _caps(cs);
        gst_app_sink_set_caps(appsink, caps);
        gst_caps_unref(caps);

        // Drop older queued frames instead of blocking the streaming thread.
        g_object_set(appsink, "max-buffers", 1u, "drop", TRUE, nullptr);

        GstAppSinkCallbacks callbacks = {};
        callbacks.new_preroll = _onPreroll;
        callbacks.new_sample = _onSample;
        gst_app_sink_set_callbacks(appsink, &callbacks, this, nullptr);

        g_object_set(playbin, "uri", uri, "video-sink", appsink, nullptr);
        // playbin owns the floating reference; keep only a borrowed alias for caps inspection.
        auto sink = appsink;
        appsink = nullptr;

        // Use the system clock so clock queries cannot block on a stalled audio sink.
        auto clock = gst_system_clock_obtain();
        gst_pipeline_use_clock(GST_PIPELINE(playbin), clock);
        gst_object_unref(clock);

        this->playbin = playbin;
        playbin = nullptr;

        // Bound preroll so a stalled sink fails open() instead of blocking indefinitely.
        if (gst_element_set_state(this->playbin, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE ||
            gst_element_get_state(this->playbin, nullptr, nullptr, PREROLL_TIMEOUT) != GST_STATE_CHANGE_SUCCESS) {
            TVGERR("GST", "Failed to open media: %s", path);
            goto cleanup;
        }

        gint64 duration = 0;
        if (!gst_element_query_duration(this->playbin, GST_FORMAT_TIME, &duration) || duration <= 0) {
            TVGERR("GST", "Invalid media duration: %s", path);
            goto cleanup;
        }

        GstVideoInfo info;
        auto pad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
        auto prerolled = pad ? gst_pad_get_current_caps(pad) : nullptr;
        auto valid = prerolled && gst_video_info_from_caps(&info, prerolled);
        if (prerolled) gst_caps_unref(prerolled);
        if (pad) gst_object_unref(pad);

        if (!valid || info.width <= 0 || info.height <= 0) {
            TVGERR("GST", "No video track found: %s", path);
            goto cleanup;
        }

        w = static_cast<float>(info.width);
        h = static_cast<float>(info.height);
        alphaIgnored = !GST_VIDEO_INFO_HAS_ALPHA(&info);
        if (alphaIgnored) cs = cs == ColorSpace::ARGB8888S ? ColorSpace::ARGB8888 : ColorSpace::ABGR8888;
        totalTime = _nanoToSec(duration);
        curTime = 0.0f;
        ret = true;
    }

cleanup:
    if (playbin) gst_object_unref(playbin);
    if (appsink) gst_object_unref(appsink);
    g_free(uri);
    return ret;
}

bool GstMediaLoader::read()
{
    if (!Loader::read()) return true;

    surface.setup(surface.data, static_cast<uint32_t>(w), static_cast<uint32_t>(w),
                  static_cast<uint32_t>(h), sizeof(uint32_t), cs, alphaIgnored);

    g_object_set(playbin, "volume", static_cast<double>(audioVolume), "mute", static_cast<gboolean>(muted), nullptr);
    paused = true;
    return sync();
}

bool GstMediaLoader::sync()
{
    if (!_pollBus(*this)) return false;

    gint64 position = 0;
    auto queried = gst_element_query_position(playbin, GST_FORMAT_TIME, &position);
    auto validPosition = queried && GST_CLOCK_TIME_IS_VALID(position) && position >= 0;

    {
        ScopedLock lock(key);

        if (seeking == SeekState::None) curTime = validPosition ? _nanoToSec(position) : ring.time;
        if (!ring.updated) return surface.data && sharing > 0;

        auto frame = ring.frames[ring.latest];
        ring.frames[ring.latest] = surface.data;
        surface.setup(frame, surface.stride, surface.w, surface.h,
                      surface.channelSize, cs, alphaIgnored);
        ring.updated = false;
    }

    return true;
}

Result GstMediaLoader::play()
{
    _preparePlay(*this);
    paused = false;
    _state(*this, GST_STATE_PLAYING);
    return Result::Success;
}

Result GstMediaLoader::pause()
{
    paused = true;
    _state(*this, GST_STATE_PAUSED);
    return Result::Success;
}

Result GstMediaLoader::stop()
{
    _state(*this, GST_STATE_PAUSED);
    paused = true;
    _seek(*this, 0);
    return Result::Success;
}

Result GstMediaLoader::seek(float seconds)
{
    _seek(*this, static_cast<gint64>(static_cast<double>(seconds) * GST_SECOND));
    return Result::Success;
}

Result GstMediaLoader::loop(bool on)
{
    looping = on;

    // Defer paused updates to _preparePlay() for a flushing seek; non-flushing seeks block on prerolled sinks.
    if (!paused && looping != segmented) _updateLoop(*this);
    return Result::Success;
}

Result GstMediaLoader::volume(float volume)
{
    audioVolume = volume;
    g_object_set(playbin, "volume", static_cast<double>(volume), nullptr);
    return Result::Success;
}

Result GstMediaLoader::mute(bool on)
{
    muted = on;
    g_object_set(playbin, "mute", static_cast<gboolean>(on), nullptr);
    return Result::Success;
}

MediaLoader* MediaLoader::gen()
{
    return new GstMediaLoader;
}
