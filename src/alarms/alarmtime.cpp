// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "alarms/alarmtime.h"

#include <base/i18n.h>
#include <base/locale.h>

#include <algorithm>
#include <ctime>

using properdesktop::i18n::format;
using properdesktop::i18n::tr;
using properdesktop::i18n::trn;

namespace clockapp::alarmtime {

static int minuteOfDay(const std::tm& tm)
{
    return tm.tm_hour * 60 + tm.tm_min;
}

static bool sameDay(const std::tm& a, const std::tm& b)
{
    return a.tm_year == b.tm_year && a.tm_mon == b.tm_mon && a.tm_mday == b.tm_mday;
}

int64_t wallTime(int64_t around, int daysAhead, int hour, int minute)
{
    const time_t base = time_t(around);
    std::tm day{};
    localtime_r(&base, &day);
    // Noon keeps the day arithmetic clear of any daylight saving change.
    day.tm_mday += daysAhead;
    day.tm_hour = 12;
    day.tm_min = 0;
    day.tm_sec = 0;
    day.tm_isdst = -1;
    mktime(&day);

    std::optional<int64_t> best;
    for (int isdst : {1, 0}) {
        std::tm wanted = day;
        wanted.tm_hour = hour;
        wanted.tm_min = minute;
        wanted.tm_sec = 0;
        wanted.tm_isdst = isdst;
        const time_t t = mktime(&wanted);
        if (t == time_t(-1))
            continue;
        std::tm shown{};
        localtime_r(&t, &shown);
        if (sameDay(shown, day) && shown.tm_hour == hour && shown.tm_min == minute && (!best || t < *best))
            best = int64_t(t);
    }
    if (best)
        return *best;

    // Skipped: the clock went from before the time straight to after it.
    std::tm wanted = day;
    wanted.tm_hour = hour;
    wanted.tm_min = minute;
    wanted.tm_sec = 0;
    wanted.tm_isdst = -1;
    time_t t = mktime(&wanted);
    const int target = hour * 60 + minute;
    for (int step = 0; step < 24 * 60; ++step) {
        const time_t earlier = t - 60;
        std::tm shown{};
        localtime_r(&earlier, &shown);
        if (!sameDay(shown, day) || minuteOfDay(shown) < target)
            break;
        t = earlier;
    }
    return int64_t(t);
}

int64_t oneTimeRing(const Alarm& alarm)
{
    for (int days = 0; days < 3; ++days) {
        const int64_t t = wallTime(alarm.armedAt, days, alarm.hour, alarm.minute);
        if (t > alarm.armedAt)
            return t;
    }
    return alarm.armedAt;
}

bool isOver(const Alarm& alarm, int64_t now)
{
    return alarm.isOneTime() && oneTimeRing(alarm) <= now;
}

bool isActive(const Alarm& alarm, int64_t now)
{
    return alarm.enabled && !isOver(alarm, now);
}

std::optional<int64_t> nextRing(const Alarm& alarm, int64_t after)
{
    if (!alarm.enabled)
        return std::nullopt;
    if (alarm.isOneTime()) {
        const int64_t t = oneTimeRing(alarm);
        return t > after ? std::optional<int64_t>(t) : std::nullopt;
    }
    for (int days = 0; days <= 7; ++days) {
        const int64_t t = wallTime(after, days, alarm.hour, alarm.minute);
        const time_t local = time_t(t);
        std::tm shown{};
        localtime_r(&local, &shown);
        if ((alarm.repeat & (1 << shown.tm_wday)) && t > after)
            return t;
    }
    return std::nullopt;
}

std::string repeatSummary(uint8_t repeat)
{
    repeat &= EveryDay;
    if (repeat == 0)
        return {};
    if (repeat == EveryDay)
        return tr("Every Day");
    if (repeat == Weekdays)
        return tr("Weekdays");
    if (repeat == Weekends)
        return tr("Weekends");

    const properdesktop::base::Locale& locale = properdesktop::base::Locale::system();
    const int first = locale.firstDayOfWeek();
    std::string text;
    for (int i = 0; i < 7; ++i) {
        // 1 for Monday to 7 for Sunday, as the locale counts.
        const int day = (first - 1 + i) % 7 + 1;
        if (!(repeat & (1 << (day % 7))))
            continue;
        if (!text.empty())
            text += ' ';
        text += locale.standaloneDayName(day, properdesktop::base::Locale::FormatType::Short);
    }
    return text;
}

std::string untilText(int64_t seconds)
{
    // Rounded up: a ring 59 seconds away is a minute away.
    const int64_t minutes = (std::max<int64_t>(0, seconds) + 59) / 60;
    if (minutes < 1)
        return tr("in less than a minute");
    const int64_t days = minutes / (24 * 60);
    const int hours = int(minutes / 60 % 24);
    const int rest = int(minutes % 60);
    if (days > 0) {
        if (hours == 0)
            return trn("in {n} day", "in {n} days", int(days));
        // TRANSLATORS: "in 1 day 3 hours"; {days} and {hours} as below.
        return format(tr("in {days} {hours}"), {{"days", trn("{n} day", "{n} days", int(days))},
                                                {"hours", trn("{n} hour", "{n} hours", hours)}});
    }
    if (hours == 0)
        return trn("in {n} minute", "in {n} minutes", rest);
    if (rest == 0)
        return trn("in {n} hour", "in {n} hours", hours);
    // TRANSLATORS: "in 7 hours 20 minutes".
    return format(tr("in {hours} {minutes}"), {{"hours", trn("{n} hour", "{n} hours", hours)},
                                               {"minutes", trn("{n} minute", "{n} minutes", rest)}});
}

} // namespace clockapp::alarmtime
