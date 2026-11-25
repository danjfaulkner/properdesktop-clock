// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace properdesktop::base {
class Settings;
}

namespace clockapp {

// A moment read from two clocks at once: the boot clock, which counts through
// suspend and never steps, and the wall clock, which outlives a reboot.
struct Reading
{
    int64_t bootNs = 0;
    int64_t wallMs = 0;
    std::string bootId;
};

// Where the time comes from, so tests can step it.
class TimeSource
{
public:
    virtual ~TimeSource() = default;
    virtual Reading now() const = 0;

    // CLOCK_BOOTTIME, CLOCK_REALTIME and the kernel's boot id.
    static const TimeSource& system();
};

// The fastest and slowest of the completed laps, by index (the first lap is
// 0), once there are enough of them to tell apart.
struct LapExtremes
{
    size_t fastest = 0;
    size_t slowest = 0;
};

// A stopwatch that keeps running while nothing looks at it: what it holds is
// when it started, not a count, so it reads right after any pause of the
// program, a suspend or a restart.
class Stopwatch
{
public:
    // Laps marked fastest and slowest from this many completed laps on.
    static constexpr size_t MarkedLapsFrom = 3;

    bool isRunning() const { return m_running; }
    // Not reset: running, or stopped with time on it.
    bool hasTime() const { return m_running || m_accumulatedNs > 0; }

    int64_t elapsedNs(const Reading& now) const;

    void start(const Reading& now);
    void stop(const Reading& now);
    // Ends the current lap at the time; only while running.
    void lap(const Reading& now);
    void reset();

    // Completed laps, the first first.
    std::vector<int64_t> lapDurations() const;
    int64_t currentLapNs(const Reading& now) const;
    size_t lapCount() const { return m_lapEnds.size(); }
    std::optional<LapExtremes> lapExtremes() const;

    // The "Stopwatch/..." keys of the file; load() moves a start made before
    // a reboot onto this boot's clock by the wall clock.
    void save(properdesktop::base::Settings& settings) const;
    void load(const properdesktop::base::Settings& settings, const Reading& now);

private:
    // Time counted before the current run.
    int64_t m_accumulatedNs = 0;
    bool m_running = false;
    Reading m_runStart;
    // Elapsed time at the end of each lap.
    std::vector<int64_t> m_lapEnds;
};

// "05:07.43", "1:05:07.43" from an hour on: hundredths, truncated.
std::string formatElapsed(int64_t ns);

} // namespace clockapp
