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
#include <mutex>

#include "tvgGstMediaLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static std::once_flag _gstOnce;  //gst_init() is process-global.
static constexpr auto SeekFlags = static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT);

//Common entry guard for the control methods; also binds 'pipeline'.
#define PIPELINE \
    if (!pImpl || !pImpl->pipeline) return Result::InsufficientCondition; \
    auto pipeline = pImpl->pipeline

struct GstImpl
{
    GstMediaLoader* loader = nullptr;
    GstElement* pipeline = nullptr;     // playbin
    GstAppSink* sink = nullptr;         // BGRA video-sink
    uint32_t* frames[3] = {};           // ring: producer fills 'write', consumer reads 'latest'
    uint32_t write = 0, latest = 0;
    bool frameUpdated = false;
    tvg::StrictKey key;
};

static void _freeFrames(GstImpl* impl)
{
    for (auto& f : impl->frames) { tvg::free(f); f = nullptr; }
}

//Copy a decoded BGRA sample into the loader-owned ring buffer.
static bool _push(GstImpl* impl, GstSample* sample)
{
    if (!impl || !impl->loader || !sample) return false;

    auto caps = gst_sample_get_caps(sample);
    auto buffer = gst_sample_get_buffer(sample);
    GstVideoInfo info;
    GstVideoFrame vframe;
    if (!caps || !buffer || !gst_video_info_from_caps(&info, caps)) return false;
    if (!gst_video_frame_map(&vframe, &info, buffer, GST_MAP_READ)) return false;

    auto w = static_cast<uint32_t>(GST_VIDEO_FRAME_WIDTH(&vframe));
    auto h = static_cast<uint32_t>(GST_VIDEO_FRAME_HEIGHT(&vframe));
    auto stride = static_cast<uint32_t>(GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 0));
    auto src = static_cast<uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(&vframe, 0));

    if (src && w > 0 && h > 0) {
        tvg::ScopedLock lock(impl->key);
        auto& surface = impl->loader->surface;
        if (surface.w != w || surface.h != h) {
            _freeFrames(impl);
            surface.w = surface.stride = w;  surface.h = h;
            impl->loader->w = w;  impl->loader->h = h;
        }
        auto& dst = impl->frames[impl->write];
        if (!dst) dst = tvg::malloc<uint32_t>(sizeof(uint32_t) * w * h);
        if (dst) {
            for (auto y = 0U; y < h; ++y) memcpy(reinterpret_cast<uint8_t*>(dst + y * w), src + y * stride, w * 4);
            impl->latest = impl->write;
            impl->write = (impl->write + 1) % 3;
            impl->frameUpdated = true;
        }
    }

    auto pts = GST_BUFFER_PTS(buffer);  //playback position from the frame timestamp.
    if (GST_CLOCK_TIME_IS_VALID(pts)) impl->loader->curTime = static_cast<float>(pts) / GST_SECOND;
    gst_video_frame_unmap(&vframe);
    return true;
}

static GstFlowReturn _onSample(GstAppSink* sink, gpointer data)
{
    if (auto sample = gst_app_sink_pull_sample(sink)) {
        _push(static_cast<GstImpl*>(data), sample);
        gst_sample_unref(sample);
    }
    return GST_FLOW_OK;
}

//Drain bus messages at render cadence: handle looping/end-of-stream and errors.
static void _pump(GstImpl* impl)
{
    auto bus = gst_element_get_bus(impl->pipeline);
    if (!bus) return;

    while (auto msg = gst_bus_pop(bus)) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            if (impl->loader->looping) gst_element_seek_simple(impl->pipeline, GST_FORMAT_TIME, SeekFlags, 0);
            else { impl->loader->paused = true; impl->loader->curTime = impl->loader->totalTime; }
        } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError* err = nullptr;
            gst_message_parse_error(msg, &err, nullptr);
            TVGLOG("GST", "Pipeline error: %s", err ? err->message : "unknown");
            if (err) g_error_free(err);
            impl->loader->paused = true;
        }
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
}

static void _close(GstImpl* impl)
{
    if (!impl->pipeline) return;
    gst_element_set_state(impl->pipeline, GST_STATE_NULL);  //synchronous; stops streaming threads.
    gst_object_unref(impl->pipeline);
    impl->pipeline = nullptr;
    impl->sink = nullptr;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

GstMediaLoader::~GstMediaLoader()
{
    if (!pImpl) return;
    _close(pImpl);
    _freeFrames(pImpl);
    delete pImpl;
}

bool GstMediaLoader::open(const char* path, TVG_UNUSED const LoaderOps* ops)
{
    if (!path) return false;
    std::call_once(_gstOnce, []() { gst_init(nullptr, nullptr); });

    if (!pImpl) { pImpl = new GstImpl; pImpl->loader = this; }
    _close(pImpl);

    auto pipeline = gst_element_factory_make("playbin", nullptr);
    auto sink = gst_element_factory_make("appsink", nullptr);
    auto uri = gst_filename_to_uri(path, nullptr);
    if (!pipeline || !sink || !uri) {
        TVGLOG("GST", "Missing playbin/appsink or bad path: %s", path);
        if (pipeline) gst_object_unref(pipeline);
        if (sink) gst_object_unref(sink);
        g_free(uri);
        return false;
    }

    //Force BGRA output (== ColorSpace::ARGB8888S); playsink inserts videoconvert.
    auto appsink = GST_APP_SINK(sink);
    auto caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "BGRA", nullptr);
    gst_app_sink_set_caps(appsink, caps);
    gst_app_sink_set_max_buffers(appsink, 1);
    gst_app_sink_set_drop(appsink, true);
    gst_caps_unref(caps);

    GstAppSinkCallbacks cbs = {};
    cbs.new_sample = _onSample;
    gst_app_sink_set_callbacks(appsink, &cbs, pImpl, nullptr);

    g_object_set(pipeline, "uri", uri, "video-sink", sink,
                 "volume", static_cast<double>(audioVolume), "mute", static_cast<gboolean>(muted), nullptr);
    g_free(uri);

    pImpl->pipeline = pipeline;
    pImpl->sink = appsink;

    surface.cs = ColorSpace::ARGB8888S;
    surface.channelSize = sizeof(uint32_t);
    surface.premultiplied = false;
    surface.alphaIgnored = true;

    //Preroll to PAUSED so size, duration, and the first still are ready.
    GstState state;
    if (gst_element_set_state(pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE ||
        gst_element_get_state(pipeline, &state, nullptr, 5 * GST_SECOND) == GST_STATE_CHANGE_FAILURE) {
        TVGLOG("GST", "Failed to open media: %s", path);
        _close(pImpl);
        return false;
    }

    gint64 dur = 0;
    if (gst_element_query_duration(pipeline, GST_FORMAT_TIME, &dur) && dur > 0) totalTime = static_cast<float>(dur) / GST_SECOND;
    curTime = 0.0f;
    paused = true;

    if (auto preroll = gst_app_sink_try_pull_preroll(appsink, 0)) {
        _push(pImpl, preroll);
        gst_sample_unref(preroll);
    }

    return surface.w > 0 && surface.h > 0;
}

bool GstMediaLoader::read()
{
    if (!Loader::read()) return true;
    return pImpl && pImpl->pipeline;
}

bool GstMediaLoader::close()
{
    if (!Loader::close()) return false;
    if (pImpl) _close(pImpl);
    return true;
}

RenderSurface* GstMediaLoader::bitmap()
{
    if (!pImpl) return nullptr;
    _pump(pImpl);

    tvg::ScopedLock lock(pImpl->key);
    auto frame = pImpl->frames[pImpl->latest];
    if (!pImpl->frameUpdated || !frame) return nullptr;
    pImpl->frameUpdated = false;
    surface.data = frame;
    return &surface;
}

Result GstMediaLoader::play()  { PIPELINE; paused = false; gst_element_set_state(pipeline, GST_STATE_PLAYING); return Result::Success; }
Result GstMediaLoader::pause() { PIPELINE; paused = true;  gst_element_set_state(pipeline, GST_STATE_PAUSED);  return Result::Success; }

Result GstMediaLoader::stop()
{
    PIPELINE;
    gst_element_set_state(pipeline, GST_STATE_PAUSED);
    gst_element_seek_simple(pipeline, GST_FORMAT_TIME, SeekFlags, 0);
    paused = true;
    curTime = 0.0f;
    return Result::Success;
}

Result GstMediaLoader::seek(float seconds)
{
    if (seconds < 0.0f || (totalTime > 0.0f && seconds > totalTime)) return Result::InvalidArguments;
    PIPELINE;
    gst_element_seek_simple(pipeline, GST_FORMAT_TIME, SeekFlags, static_cast<gint64>(static_cast<double>(seconds) * GST_SECOND));
    curTime = seconds;
    return Result::Success;
}

Result GstMediaLoader::loop(bool on)  //handled on EOS in _pump().
{
    if (!pImpl || !pImpl->pipeline) return Result::InsufficientCondition;
    looping = on;
    return Result::Success;
}

Result GstMediaLoader::volume(float vol)
{
    PIPELINE;
    audioVolume = vol;
    g_object_set(pipeline, "volume", static_cast<double>(vol), nullptr);
    return Result::Success;
}

Result GstMediaLoader::mute(bool on)
{
    PIPELINE;
    muted = on;
    g_object_set(pipeline, "mute", static_cast<gboolean>(on), nullptr);
    return Result::Success;
}
