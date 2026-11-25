// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "store/store.h"

#include <cstdint>
#include <string>
#include <vector>

namespace properdesktop::base {
class Settings;
}

// A timer's time, from the store's record of it and the wall clock (Unix
// milliseconds), which is what the alarm service rings it by.
namespace clockapp::timermath {

// The longest duration the picker sets: 23:59:59.
constexpr int64_t MaximumDurationMs = (23 * 3600 + 59 * 60 + 59) * 1000LL;

RunningTimer make(int64_t durationMs, std::string label, std::string sound, int64_t nowMs);

int64_t remainingMs(const RunningTimer& timer, int64_t nowMs);
// From 1 when it starts to 0 when it ends.
double fractionLeft(const RunningTimer& timer, int64_t nowMs);
bool isFinished(const RunningTimer& timer, int64_t nowMs);

void pause(RunningTimer& timer, int64_t nowMs);
void resume(RunningTimer& timer, int64_t nowMs);
// Running again from its full duration.
void restart(RunningTimer& timer, int64_t nowMs);
// The stored timer of the id restarted and written, as Repeat does from the
// page or the alarm service's notification; false for an unknown id.
bool repeatTimer(ClockStore& store, const std::string& id, int64_t nowMs);

// Whole seconds, rounded up so a timer reads 0:00 only once it is over:
// "4:05", "12:00", "1:02:03".
std::string formatRemaining(int64_t ms);
// "5 min", "1 hr 30 min", "45 sec" style, for a recent timer.
std::string formatDuration(int64_t ms);

// A timer started before, to start again with one click.
struct Recent
{
    int64_t durationMs = 0;
    std::string label;
    std::string sound;

    friend bool operator==(const Recent&, const Recent&) = default;
};

constexpr size_t MaximumRecents = 6;

// Puts the timer first, dropping an equal one further down and the oldest
// past the limit.
void remember(std::vector<Recent>& recents, const Recent& recent);
void forget(std::vector<Recent>& recents, size_t index);

// "Timers/Recent", one "duration|sound|label" entry each.
std::vector<Recent> loadRecents(const properdesktop::base::Settings& settings);
void saveRecents(properdesktop::base::Settings& settings, const std::vector<Recent>& recents);

} // namespace clockapp::timermath
