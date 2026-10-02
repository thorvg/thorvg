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

static constexpr int64_t GAP_THRESHOLD_US = 200000;  // longest audio gap filled with silence, as media3's DefaultAudioSink
static constexpr uint32_t PRIME_COUNT = 4;           // chunks queued before the clock starts: ~85ms of aac

#define SL_OK(stmt) ((stmt) == SL_RESULT_SUCCESS)

// OpenSL engine and output mix shared by every sink, which owns only its player
static struct
{
    StrictKey key;
    SLObjectItf object = nullptr;
    SLEngineItf engine = nullptr;
    SLObjectItf mixer = nullptr;
    uint32_t refCnt = 0;
} _sl;

// opensl volume is an attenuation in millibels, which android turns back into a gain of 10^(mB / 2000): floored at the api minimum
static SLmillibel _millibel(float level)
{
    if (level <= 0.0f) return SL_MILLIBEL_MIN;
    return static_cast<SLmillibel>(std::max(2000.0f * log10f(level), static_cast<float>(SL_MILLIBEL_MIN)));
}

static void _slRelease()
{
    if (_sl.mixer) (*_sl.mixer)->Destroy(_sl.mixer);
    if (_sl.object) (*_sl.object)->Destroy(_sl.object);
    _sl.mixer = _sl.object = nullptr;
    _sl.engine = nullptr;
}

static bool _slOpen()
{
    ScopedLock lock(_sl.key);
    if (_sl.refCnt == 0 && (!SL_OK(slCreateEngine(&_sl.object, 0, nullptr, 0, nullptr, nullptr)) || !SL_OK((*_sl.object)->Realize(_sl.object, SL_BOOLEAN_FALSE)) ||
        !SL_OK((*_sl.object)->GetInterface(_sl.object, SL_IID_ENGINE, &_sl.engine)) || !SL_OK((*_sl.engine)->CreateOutputMix(_sl.engine, &_sl.mixer, 0, nullptr, nullptr)) ||
        !SL_OK((*_sl.mixer)->Realize(_sl.mixer, SL_BOOLEAN_FALSE)))) {
        _slRelease();
        return false;
    }
    ++_sl.refCnt;
    return true;
}

static void _slClose()
{
    ScopedLock lock(_sl.key);
    if (--_sl.refCnt == 0) _slRelease();
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool AudioSink::open(int32_t rate, int32_t channels)
{
    // the codec announces its output format even when unchanged: only a real change reopens the player
    if (sl.object && rate == sampleRate && channels * static_cast<int32_t>(sizeof(int16_t)) == frameSize) return true;
    close();

    // 16-bit mono or stereo pcm only
    const auto mask = channels == 2 ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT) : (channels == 1 ? SL_SPEAKER_FRONT_CENTER : 0);
    if (mask == 0 || !_slOpen()) return false;

    SLDataLocator_AndroidSimpleBufferQueue locator = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, CHUNK_COUNT};
    SLDataFormat_PCM pcm = {SL_DATAFORMAT_PCM, static_cast<SLuint32>(channels), static_cast<SLuint32>(rate) * 1000,  // the rate in milliHz
                            SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16, mask, SL_BYTEORDER_LITTLEENDIAN};
    SLDataSource source = {&locator, &pcm};
    SLDataLocator_OutputMix output = {SL_DATALOCATOR_OUTPUTMIX, _sl.mixer};  // our engine reference pins the mixer
    SLDataSink target = {&output, nullptr};
    const SLInterfaceID ids[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean required[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    if (!SL_OK((*_sl.engine)->CreateAudioPlayer(_sl.engine, &sl.object, &source, &target, sizeof(ids) / sizeof(ids[0]), ids, required))) {
        sl.object = nullptr;
        _slClose();
        return false;
    }
    // from here on close() releases both the player and the engine reference
    auto ret = SL_OK((*sl.object)->Realize(sl.object, SL_BOOLEAN_FALSE)) && SL_OK((*sl.object)->GetInterface(sl.object, SL_IID_PLAY, &sl.player)) &&
              SL_OK((*sl.object)->GetInterface(sl.object, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &sl.queue)) && SL_OK((*sl.queue)->RegisterCallback(sl.queue, callback, this));
    if (ret) {
        // the caller reaches the player through its volume only: hand it over with the settings made so far
        ScopedLock lock(sl.key);
        ret = SL_OK((*sl.object)->GetInterface(sl.object, SL_IID_VOLUME, &sl.volume)) && SL_OK((*sl.volume)->SetVolumeLevel(sl.volume, _millibel(sl.level))) &&
             SL_OK((*sl.volume)->SetMute(sl.volume, sl.muted ? SL_BOOLEAN_TRUE : SL_BOOLEAN_FALSE));
    }
    if (!ret) {
        close();
        return false;
    }
    sampleRate = rate;
    frameSize = channels * sizeof(int16_t);

    ScopedLock lock(key);
    ended = false;
    return true;
}

void AudioSink::close()
{
    // the caller reaches the player through its volume only: cut it off before the player goes
    {
        ScopedLock lock(sl.key);
        sl.volume = nullptr;
    }
    if (sl.object) {
        (*sl.object)->Destroy(sl.object);  // no callback runs after this
        _slClose();
    }
    sl.object = nullptr;
    sl.player = nullptr;
    sl.queue = nullptr;

    ScopedLock lock(key);
    // hold the clock where the sound stops
    anchor(head());
    playing = false;
    ended = true;
    ring.read = ring.queued = 0;
    waitPtsUs = -1;
    for (auto& chunk : ring.chunks) {
        tvg::free(chunk.data);
        chunk = {};
    }
}

bool AudioSink::volume(float level)
{
    ScopedLock lock(sl.key);
    if (sl.volume && !SL_OK((*sl.volume)->SetVolumeLevel(sl.volume, _millibel(level)))) return false;

    sl.level = level;
    return true;
}

bool AudioSink::mute(bool on)
{
    ScopedLock lock(sl.key);
    if (sl.volume && !SL_OK((*sl.volume)->SetMute(sl.volume, on ? SL_BOOLEAN_TRUE : SL_BOOLEAN_FALSE))) return false;

    sl.muted = on;
    return true;
}

// head() is anchorMediaUs + (monotonic() - anchorMonotonicUs): move both whenever the media position changes
void AudioSink::anchor(int64_t timeUs)
{
    anchorMediaUs = timeUs;
    anchorMonotonicUs = monotonic();
}

int64_t AudioSink::head()
{
    if (!playing) return anchorMediaUs;

    const auto timeUs = anchorMediaUs + std::max(monotonic() - anchorMonotonicUs, int64_t(0));
    if (ring.queued == 0) {
        if (ended) return timeUs;
        // a long gap runs on up to the pcm after it, a starving queue holds the clock so the picture never outruns the sound
        return waitPtsUs >= 0 ? std::min(timeUs, std::max(waitPtsUs, anchorMediaUs)) : anchorMediaUs;
    }

    const auto& chunk = ring.chunks[ring.read];
    return std::min(timeUs, chunk.endUs);
}

bool AudioSink::play(bool on)
{
    // the worker calling this is the only one to change playing, so reading it needs no key
    if (playing == on) return true;

    {
        ScopedLock lock(key);
        anchor(head());
        playing = on;
    }
    return !sl.player || SL_OK((*sl.player)->SetPlayState(sl.player, on ? SL_PLAYSTATE_PLAYING : SL_PLAYSTATE_PAUSED));
}

bool AudioSink::seek(int64_t timeUs)
{
    // stop first: the player takes no more buffers while the queue restarts under the key
    if (sl.player && !SL_OK((*sl.player)->SetPlayState(sl.player, SL_PLAYSTATE_STOPPED))) return false;
    tailUs = timeUs;

    ScopedLock lock(key);
    if (sl.queue && !SL_OK((*sl.queue)->Clear(sl.queue))) return false;

    ring.read = ring.queued = 0;
    waitPtsUs = -1;
    anchor(timeUs);
    playing = false;
    ended = !sl.player;
    return true;
}

bool AudioSink::ready(int64_t ptsUs)
{
    if (ptsUs - tailUs <= GAP_THRESHOLD_US) return true;

    ScopedLock lock(key);
    if (waitPtsUs != ptsUs) {
        // a starving clock runs on from where it holds, not from its stale anchor
        if (ring.queued == 0) anchor(head());
        waitPtsUs = ptsUs;
    }
    return ring.queued == 0 && head() >= ptsUs;
}

bool AudioSink::push(const uint8_t* data, size_t size, int64_t ptsUs)
{
    const auto frames = size / frameSize;
    if (frames == 0) return true;

    // queued buffers play back to back: a short gap in the timeline becomes silence, a longer one was waited out in ready()
    const auto gapUs = ptsUs - tailUs;
    const auto silence = (gapUs > 0 && gapUs <= GAP_THRESHOLD_US) ? static_cast<size_t>(gapUs * sampleRate / MICROSEC) * frameSize : 0;
    // the chunk starts with its silence, and the next gap counts from its end
    const auto startUs = silence > 0 ? tailUs : ptsUs;
    tailUs = ptsUs + static_cast<int64_t>(frames) * MICROSEC / sampleRate; // pts + duration

    ScopedLock lock(key);
    // the player reads the pcm in place until the callback retires it: copy it into the free slot, reusing its memory
    auto& chunk = ring.chunks[(ring.read + ring.queued) % CHUNK_COUNT];
    if (chunk.capacity < silence + size) {
        auto buf = tvg::realloc<uint8_t>(chunk.data, silence + size);
        if (!buf) return false;
        chunk.data = buf;
        chunk.capacity = silence + size;
    }
    memset(chunk.data, 0, silence);
    memcpy(chunk.data + silence, data, size);
    chunk.ptsUs = startUs;
    chunk.endUs = tailUs;
    if (!SL_OK((*sl.queue)->Enqueue(sl.queue, chunk.data, static_cast<SLuint32>(silence + size)))) return false;

    // the clock resumes from a starved queue with the chunk that ends the starvation
    if (ring.queued++ == 0 && playing) anchor(std::max(anchorMediaUs, chunk.ptsUs));
    // no long gap is waited out anymore
    waitPtsUs = -1;
    return true;
}

void AudioSink::finish()
{
    ScopedLock lock(key);
    ended = true;
}

bool AudioSink::full()
{
    ScopedLock lock(key);
    return ring.queued == CHUNK_COUNT;
}

bool AudioSink::primed()
{
    ScopedLock lock(key);
    // a long gap starts the clock with no pcm: the pcm after it waits for the clock
    return ended || waitPtsUs >= 0 || ring.queued >= PRIME_COUNT;
}

int64_t AudioSink::time()
{
    ScopedLock lock(key);
    return head();
}

// retire what the player consumed: its queue state, unlike a callback count, stays right across a seek
void AudioSink::callback(SLAndroidSimpleBufferQueueItf queue, void* context)
{
    const auto sink = static_cast<AudioSink*>(context);
    SLAndroidSimpleBufferQueueState state;

    ScopedLock lock(sink->key);
    if (!SL_OK((*queue)->GetState(queue, &state))) return;
    while (sink->ring.queued > state.count) {
        const auto& chunk = sink->ring.chunks[sink->ring.read];
        // the sound is at the end of the consumed chunk: the clock takes it over, but a paused one stays put
        if (sink->playing) sink->anchor(chunk.endUs);
        sink->ring.read = (sink->ring.read + 1) % CHUNK_COUNT;
        --sink->ring.queued;
    }
}
