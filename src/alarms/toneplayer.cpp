// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "alarms/toneplayer.h"
#include "alarms/tones.h"

#include <algorithm>
#include <cmath>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

namespace clockapp {

static constexpr int Channels = 2;
static constexpr uint32_t Stride = sizeof(float) * Channels;
static constexpr float Volume = 0.55f;
// Seconds of the fade when stopped.
static constexpr double FadeOut = 0.06;

const pw_stream_events& TonePlayer::streamEvents()
{
    static const pw_stream_events events = [] {
        pw_stream_events events{};
        events.version = PW_VERSION_STREAM_EVENTS;
        events.process = &TonePlayer::process;
        return events;
    }();
    return events;
}

TonePlayer::TonePlayer()
{
    m_reaper.onTimeout = [this] {
        if (m_finished) {
            m_reaper.stop();
            close();
        }
    };
}

TonePlayer::~TonePlayer()
{
    close();
}

bool TonePlayer::open()
{
    if (m_stream)
        return true;
    static std::once_flag initialized;
    std::call_once(initialized, [] { pw_init(nullptr, nullptr); });

    m_loop = pw_thread_loop_new("properdesktop-clock-tone", nullptr);
    if (!m_loop)
        return false;
    pw_properties* properties = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Alarm",
        PW_KEY_APP_NAME, "Clock",
        PW_KEY_APP_ID, "properdesktop-clock",
        PW_KEY_APP_ICON_NAME, "properdesktop-clock",
        PW_KEY_NODE_NAME, "properdesktop-clock-tone",
        PW_KEY_NODE_LATENCY, "1024/48000",
        nullptr);
    pw_thread_loop_lock(m_loop);
    m_stream = pw_stream_new_simple(pw_thread_loop_get_loop(m_loop), "Alarm", properties, &streamEvents(), this);
    if (!m_stream) {
        pw_thread_loop_unlock(m_loop);
        close();
        return false;
    }
    uint8_t buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    spa_audio_info_raw info{};
    info.format = SPA_AUDIO_FORMAT_F32;
    info.rate = tones::SampleRate;
    info.channels = Channels;
    info.position[0] = SPA_AUDIO_CHANNEL_FL;
    info.position[1] = SPA_AUDIO_CHANNEL_FR;
    const spa_pod* params[] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};
    const int connected = pw_stream_connect(
        m_stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
        pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
        params, 1);
    pw_thread_loop_unlock(m_loop);
    if (connected < 0 || pw_thread_loop_start(m_loop) < 0) {
        close();
        return false;
    }
    return true;
}

void TonePlayer::close()
{
    if (m_loop)
        pw_thread_loop_stop(m_loop);
    if (m_stream)
        pw_stream_destroy(m_stream);
    if (m_loop)
        pw_thread_loop_destroy(m_loop);
    m_stream = nullptr;
    m_loop = nullptr;
}

void TonePlayer::play(const std::string& sound, Options options)
{
    m_sound = tones::resolved(sound);
    std::vector<float> samples = tones::synthesize(m_sound);
    // A fresh stream each time: the old one may still be fading out.
    close();
    {
        std::lock_guard lock(m_mutex);
        m_samples = std::move(samples);
        m_options = options;
        m_frame = 0;
        m_fade = 1;
        m_stopping = false;
        m_finished = false;
    }
    if (!open()) {
        m_finished = true;
        return;
    }
    m_reaper.start(std::chrono::milliseconds(200));
}

void TonePlayer::stop()
{
    if (!m_finished)
        m_stopping = true;
}

bool TonePlayer::isPlaying() const
{
    return !m_finished && !m_stopping;
}

void TonePlayer::process(void* data)
{
    auto* player = static_cast<TonePlayer*>(data);
    pw_buffer* buffer = pw_stream_dequeue_buffer(player->m_stream);
    if (!buffer)
        return;
    spa_buffer* spa = buffer->buffer;
    auto* out = static_cast<float*>(spa->datas[0].data);
    if (!out) {
        pw_stream_queue_buffer(player->m_stream, buffer);
        return;
    }
    uint32_t frames = spa->datas[0].maxsize / Stride;
    if (buffer->requested)
        frames = std::min<uint32_t>(frames, uint32_t(buffer->requested));

    std::unique_lock lock(player->m_mutex, std::try_to_lock);
    const std::vector<float>& samples = player->m_samples;
    const int64_t period = int64_t(samples.size());
    const Options& options = player->m_options;
    const float fadeStep = float(1.0 / (FadeOut * tones::SampleRate));
    for (uint32_t i = 0; i < frames; ++i) {
        float value = 0;
        const bool over = options.periods > 0 && player->m_frame >= period * options.periods;
        if (lock.owns_lock() && period > 0 && !over && !player->m_finished) {
            if (player->m_stopping)
                player->m_fade = std::max(0.0f, player->m_fade - fadeStep);
            double gain = 1;
            if (options.rise > 0) {
                const double t = std::min(1.0, double(player->m_frame) / (options.rise * tones::SampleRate));
                gain = 0.2 + 0.8 * t * t * (3 - 2 * t);
            }
            value = samples[size_t(player->m_frame % period)] * float(gain) * Volume * player->m_fade;
            ++player->m_frame;
            if (player->m_fade <= 0)
                player->m_finished = true;
        } else if (over) {
            player->m_finished = true;
        }
        out[i * Channels] = value;
        out[i * Channels + 1] = value;
    }
    spa->datas[0].chunk->offset = 0;
    spa->datas[0].chunk->stride = Stride;
    spa->datas[0].chunk->size = frames * Stride;
    pw_stream_queue_buffer(player->m_stream, buffer);
}

} // namespace clockapp
