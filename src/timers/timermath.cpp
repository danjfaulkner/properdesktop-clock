// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "timers/timermath.h"

#include <base/i18n.h>
#include <base/settings.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

using properdesktop::i18n::format;
using properdesktop::i18n::tr;

namespace clockapp::timermath {

RunningTimer make(int64_t durationMs, std::string label, std::string sound, int64_t nowMs)
{
    RunningTimer timer;
    timer.durationMs = std::clamp<int64_t>(durationMs, 1000, MaximumDurationMs);
    timer.label = std::move(label);
    timer.sound = std::move(sound);
    timer.endsAtMs = nowMs + timer.durationMs;
    return timer;
}

int64_t remainingMs(const RunningTimer& timer, int64_t nowMs)
{
    if (!timer.isRunning())
        return std::clamp<int64_t>(timer.remainingMs, 0, timer.durationMs);
    return std::clamp<int64_t>(timer.endsAtMs - nowMs, 0, timer.durationMs);
}

double fractionLeft(const RunningTimer& timer, int64_t nowMs)
{
    if (timer.durationMs <= 0)
        return 0;
    return double(remainingMs(timer, nowMs)) / double(timer.durationMs);
}

bool isFinished(const RunningTimer& timer, int64_t nowMs)
{
    return timer.isRunning() && nowMs >= timer.endsAtMs;
}

void pause(RunningTimer& timer, int64_t nowMs)
{
    if (!timer.isRunning() || isFinished(timer, nowMs))
        return;
    timer.remainingMs = remainingMs(timer, nowMs);
    timer.endsAtMs = 0;
}

void resume(RunningTimer& timer, int64_t nowMs)
{
    if (timer.isRunning())
        return;
    timer.endsAtMs = nowMs + std::max<int64_t>(1, remainingMs(timer, nowMs));
    timer.remainingMs = 0;
}

void restart(RunningTimer& timer, int64_t nowMs)
{
    timer.endsAtMs = nowMs + timer.durationMs;
    timer.remainingMs = 0;
}

bool repeatTimer(ClockStore& store, const std::string& id, int64_t nowMs)
{
    const RunningTimer* found = store.timer(id);
    if (!found)
        return false;
    RunningTimer timer = *found;
    restart(timer, nowMs);
    store.setTimer(timer);
    return true;
}

std::string formatRemaining(int64_t ms)
{
    const int64_t total = (std::max<int64_t>(0, ms) + 999) / 1000;
    const int64_t hours = total / 3600;
    const int64_t minutes = total / 60 % 60;
    const int64_t seconds = total % 60;
    char text[40];
    if (hours > 0)
        std::snprintf(text, sizeof text, "%lld:%02lld:%02lld", (long long)hours, (long long)minutes,
                      (long long)seconds);
    else
        std::snprintf(text, sizeof text, "%lld:%02lld", (long long)minutes, (long long)seconds);
    return text;
}

std::string formatDuration(int64_t ms)
{
    const int64_t total = std::max<int64_t>(0, ms) / 1000;
    const std::string hours = std::to_string(total / 3600);
    const std::string minutes = std::to_string(total / 60 % 60);
    const std::string seconds = std::to_string(total % 60);
    std::string text;
    const auto add = [&text](const std::string& part) {
        if (!text.empty())
            text += ' ';
        text += part;
    };
    if (total >= 3600)
        add(format(tr("{n} hr"), {{"n", hours}}));
    if (total % 3600 >= 60)
        add(format(tr("{n} min"), {{"n", minutes}}));
    if (total % 60 > 0 || total == 0)
        add(format(tr("{n} sec"), {{"n", seconds}}));
    return text;
}

void remember(std::vector<Recent>& recents, const Recent& recent)
{
    recents.erase(std::remove(recents.begin(), recents.end(), recent), recents.end());
    recents.insert(recents.begin(), recent);
    if (recents.size() > MaximumRecents)
        recents.resize(MaximumRecents);
}

void forget(std::vector<Recent>& recents, size_t index)
{
    if (index < recents.size())
        recents.erase(recents.begin() + long(index));
}

std::vector<Recent> loadRecents(const properdesktop::base::Settings& settings)
{
    std::vector<Recent> recents;
    for (const std::string& entry : settings.stringListValue("Timers/Recent")) {
        const size_t first = entry.find('|');
        const size_t second = first == std::string::npos ? first : entry.find('|', first + 1);
        if (second == std::string::npos)
            continue;
        char* end = nullptr;
        const long long duration = std::strtoll(entry.c_str(), &end, 10);
        if (end != entry.c_str() + first || duration < 1000 || duration > MaximumDurationMs)
            continue;
        Recent recent;
        recent.durationMs = duration;
        recent.sound = entry.substr(first + 1, second - first - 1);
        recent.label = entry.substr(second + 1);
        if (std::find(recents.begin(), recents.end(), recent) == recents.end())
            recents.push_back(std::move(recent));
        if (recents.size() == MaximumRecents)
            break;
    }
    return recents;
}

void saveRecents(properdesktop::base::Settings& settings, const std::vector<Recent>& recents)
{
    std::vector<std::string> entries;
    entries.reserve(recents.size());
    for (const Recent& recent : recents)
        entries.push_back(std::to_string(recent.durationMs) + '|' + recent.sound + '|' + recent.label);
    if (entries.empty())
        settings.remove("Timers/Recent");
    else
        settings.setValue("Timers/Recent", entries);
}

} // namespace clockapp::timermath
