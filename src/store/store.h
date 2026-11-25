// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <base/file_watcher.h>
#include <base/signal.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace properdesktop::base::dbus {
class Connection;
}

namespace clockapp {

struct Alarm
{
    std::string id;
    int hour = 7;
    int minute = 0;
    // Bit d for weekday d, 0 Sunday to 6 Saturday; none for a one-time alarm.
    uint8_t repeat = 0;
    std::string label;
    std::string sound;
    bool snooze = true;
    bool enabled = true;
    // Unix seconds when the alarm was last switched on or saved. A one-time
    // alarm rings at the first hour:minute after it and is over once that
    // has passed, so the alarm service never has to write the file.
    int64_t armedAt = 0;

    bool isOneTime() const { return repeat == 0; }
    friend bool operator==(const Alarm&, const Alarm&) = default;
};

struct RunningTimer
{
    std::string id;
    std::string label;
    int64_t durationMs = 0;
    // Unix milliseconds the timer ends at while it runs; 0 while paused.
    int64_t endsAtMs = 0;
    // What is left while paused.
    int64_t remainingMs = 0;
    std::string sound;

    bool isRunning() const { return endsAtMs > 0; }
    friend bool operator==(const RunningTimer&, const RunningTimer&) = default;
};

struct ClockData
{
    std::vector<Alarm> alarms;
    std::vector<RunningTimer> timers;

    friend bool operator==(const ClockData&, const ClockData&) = default;
};

namespace clockstore {

// ~/.local/share/properdesktop/clock/alarms.json
std::string defaultPath();
// alarms.json in the folder.
std::string pathIn(const std::string& folder);

std::string serialize(const ClockData& data);
// Nothing when the text is not a JSON object, as when it was cut short;
// entries without an id or with times out of range are left out.
std::optional<ClockData> parse(std::string_view text);

// The tones a sound name can be; the first is the default.
const std::vector<std::string>& soundNames();

// Asks the session bus to start the alarm service unless it runs.
void ensureAlarmService(properdesktop::base::dbus::Connection* bus);

} // namespace clockstore

// The alarms and running timers. Only properdesktop-clock writes the file, by
// replacing it whole; the alarm service only reads it and follows changes.
class ClockStore
{
public:
    explicit ClockStore(std::string path = clockstore::defaultPath());

    ClockStore(const ClockStore&) = delete;
    ClockStore& operator=(const ClockStore&) = delete;

    // The process's one store over the data file in the folder (the page
    // context's data folder), made on first use, so every page of the
    // window shares it.
    static ClockStore& inFolder(const std::string& folder);

    const std::string& path() const { return m_path; }

    // Reads the file again. A missing file reads as empty; an unreadable or
    // invalid one keeps what was read before. Emits `changed` when the
    // contents differ.
    void load();
    // Reloads whenever the file changes, for the alarm service.
    void watch();

    const ClockData& data() const { return m_data; }
    const std::vector<Alarm>& alarms() const { return m_data.alarms; }
    const std::vector<RunningTimer>& timers() const { return m_data.timers; }
    const Alarm* alarm(const std::string& id) const;
    const RunningTimer* timer(const std::string& id) const;

    // Each change is written at once and emits `changed`. An alarm or timer
    // with an empty id gets a new one; a known id is replaced in place.
    std::string setAlarm(Alarm alarm);
    void removeAlarm(const std::string& id);
    std::string setTimer(RunningTimer timer);
    void removeTimer(const std::string& id);

    properdesktop::base::Signal<> changed;

private:
    void save();

    std::string m_path;
    ClockData m_data;
    properdesktop::base::FileWatch m_watch;
};

} // namespace clockapp
