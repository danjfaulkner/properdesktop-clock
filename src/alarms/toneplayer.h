// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <base/timer.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct pw_stream;
struct pw_stream_events;
struct pw_thread_loop;

namespace clockapp {

// Plays a tone through PipeWire, looped until stopped. The stream exists
// only while something plays, so an idle player costs nothing.
class TonePlayer
{
public:
    struct Options
    {
        // Seconds over which the loudness rises from a fifth to full.
        double rise = 0;
        // Times the tone's period plays; 0 for until stopped.
        int periods = 0;
    };

    TonePlayer();
    virtual ~TonePlayer();

    TonePlayer(const TonePlayer&) = delete;
    TonePlayer& operator=(const TonePlayer&) = delete;

    virtual void play(const std::string& sound, Options options);
    // Fades out over a few milliseconds, so it ends without a click.
    virtual void stop();
    virtual bool isPlaying() const;
    // The sound last played, for tests.
    const std::string& sound() const { return m_sound; }

private:
    static const pw_stream_events& streamEvents();
    static void process(void* data);
    bool open();
    void close();

    std::string m_sound;
    // Closes the stream once the tone is over.
    properdesktop::base::Timer m_reaper;
    pw_thread_loop* m_loop = nullptr;
    pw_stream* m_stream = nullptr;

    // Shared with PipeWire's thread; the samples are swapped under the lock
    // only while the stream is paused.
    std::mutex m_mutex;
    std::vector<float> m_samples;
    Options m_options;
    int64_t m_frame = 0;
    std::atomic<bool> m_stopping{false};
    std::atomic<bool> m_finished{true};
    float m_fade = 1;
};

} // namespace clockapp
