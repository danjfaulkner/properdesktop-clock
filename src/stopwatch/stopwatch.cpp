// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "stopwatch/stopwatch.h"

#include <base/settings.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>

namespace clockapp {

static int64_t readClock(clockid_t id)
{
    timespec ts{};
    clock_gettime(id, &ts);
    return int64_t(ts.tv_sec) * 1000000000 + ts.tv_nsec;
}

static std::string readBootId()
{
    std::ifstream file("/proc/sys/kernel/random/boot_id");
    std::string id;
    std::getline(file, id);
    return id;
}

namespace {

class SystemTimeSource : public TimeSource
{
public:
    Reading now() const override
    {
        Reading reading;
        reading.bootNs = readClock(CLOCK_BOOTTIME);
        reading.wallMs = readClock(CLOCK_REALTIME) / 1000000;
        reading.bootId = m_bootId;
        return reading;
    }

private:
    std::string m_bootId = readBootId();
};

}

const TimeSource& TimeSource::system()
{
    static const SystemTimeSource source;
    return source;
}

int64_t Stopwatch::elapsedNs(const Reading& now) const
{
    if (!m_running)
        return m_accumulatedNs;
    return m_accumulatedNs + std::max<int64_t>(0, now.bootNs - m_runStart.bootNs);
}

void Stopwatch::start(const Reading& now)
{
    if (m_running)
        return;
    m_runStart = now;
    m_running = true;
}

void Stopwatch::stop(const Reading& now)
{
    if (!m_running)
        return;
    m_accumulatedNs = elapsedNs(now);
    m_running = false;
}

void Stopwatch::lap(const Reading& now)
{
    if (!m_running)
        return;
    m_lapEnds.push_back(elapsedNs(now));
}

void Stopwatch::reset()
{
    m_running = false;
    m_accumulatedNs = 0;
    m_runStart = {};
    m_lapEnds.clear();
}

std::vector<int64_t> Stopwatch::lapDurations() const
{
    std::vector<int64_t> durations;
    durations.reserve(m_lapEnds.size());
    int64_t previous = 0;
    for (int64_t end : m_lapEnds) {
        durations.push_back(end - previous);
        previous = end;
    }
    return durations;
}

int64_t Stopwatch::currentLapNs(const Reading& now) const
{
    return elapsedNs(now) - (m_lapEnds.empty() ? 0 : m_lapEnds.back());
}

std::optional<LapExtremes> Stopwatch::lapExtremes() const
{
    if (m_lapEnds.size() < MarkedLapsFrom)
        return std::nullopt;
    const std::vector<int64_t> durations = lapDurations();
    LapExtremes extremes;
    for (size_t i = 1; i < durations.size(); ++i) {
        if (durations[i] < durations[extremes.fastest])
            extremes.fastest = i;
        if (durations[i] > durations[extremes.slowest])
            extremes.slowest = i;
    }
    if (extremes.fastest == extremes.slowest)
        return std::nullopt;
    return extremes;
}

void Stopwatch::save(properdesktop::base::Settings& settings) const
{
    if (!hasTime()) {
        for (const char* key : {"Running", "Accumulated", "StartBoot", "StartWall", "BootId", "Laps"})
            settings.remove(std::string("Stopwatch/") + key);
        return;
    }
    settings.setValue("Stopwatch/Running", m_running);
    settings.setValue("Stopwatch/Accumulated", m_accumulatedNs);
    if (m_running) {
        settings.setValue("Stopwatch/StartBoot", m_runStart.bootNs);
        settings.setValue("Stopwatch/StartWall", m_runStart.wallMs);
        settings.setValue("Stopwatch/BootId", m_runStart.bootId);
    } else {
        settings.remove("Stopwatch/StartBoot");
        settings.remove("Stopwatch/StartWall");
        settings.remove("Stopwatch/BootId");
    }
    std::vector<std::string> laps;
    laps.reserve(m_lapEnds.size());
    for (int64_t end : m_lapEnds)
        laps.push_back(std::to_string(end));
    settings.setValue("Stopwatch/Laps", laps);
}

void Stopwatch::load(const properdesktop::base::Settings& settings, const Reading& now)
{
    reset();
    m_accumulatedNs = std::max<int64_t>(0, settings.intValue("Stopwatch/Accumulated"));
    m_running = settings.boolValue("Stopwatch/Running");
    if (m_running) {
        m_runStart.bootNs = settings.intValue("Stopwatch/StartBoot");
        m_runStart.wallMs = settings.intValue("Stopwatch/StartWall");
        m_runStart.bootId = settings.stringValue("Stopwatch/BootId");
        if (m_runStart.bootId != now.bootId || m_runStart.bootNs > now.bootNs) {
            // The boot clock started again: the wall clock says how long ago
            // the run began, which a clock set backwards makes no less than 0.
            const int64_t sinceMs = std::max<int64_t>(0, now.wallMs - m_runStart.wallMs);
            m_runStart.bootNs = now.bootNs - sinceMs * 1000000;
            m_runStart.bootId = now.bootId;
        }
    }
    int64_t previous = 0;
    for (const std::string& value : settings.stringListValue("Stopwatch/Laps")) {
        char* end = nullptr;
        const long long lapEnd = std::strtoll(value.c_str(), &end, 10);
        if (end == value.c_str() || lapEnd < previous)
            break;
        m_lapEnds.push_back(lapEnd);
        previous = lapEnd;
    }
    if (!m_running && m_accumulatedNs == 0)
        m_lapEnds.clear();
}

std::string formatElapsed(int64_t ns)
{
    const int64_t centis = std::max<int64_t>(0, ns) / 10000000;
    const int64_t hundredths = centis % 100;
    const int64_t seconds = centis / 100 % 60;
    const int64_t minutes = centis / 6000 % 60;
    const int64_t hours = centis / 360000;
    char text[48];
    if (hours > 0)
        std::snprintf(text, sizeof text, "%lld:%02lld:%02lld.%02lld", (long long)hours, (long long)minutes,
                      (long long)seconds, (long long)hundredths);
    else
        std::snprintf(text, sizeof text, "%02lld:%02lld.%02lld", (long long)minutes, (long long)seconds,
                      (long long)hundredths);
    return text;
}

} // namespace clockapp
