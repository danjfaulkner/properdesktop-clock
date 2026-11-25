// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "world/zonetime.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

#include "base/i18n.h"

using namespace properdesktop;
using properdesktop::i18n::format;
using properdesktop::i18n::tr;
using properdesktop::i18n::trn;

namespace clockapp::world {

static int64_t floorDiv(int64_t a, int64_t b)
{
    return a / b - ((a % b != 0) && ((a < 0) != (b < 0)));
}

// Days since 1970-01-01 to the civil date, after Howard Hinnant's algorithm.
static base::CalendarDate civilFromDays(int64_t days)
{
    days += 719468;
    const int64_t era = floorDiv(days, 146097);
    const int64_t dayOfEra = days - era * 146097;
    const int64_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const int64_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const int64_t monthPart = (5 * dayOfYear + 2) / 153;
    const int day = int(dayOfYear - (153 * monthPart + 2) / 5 + 1);
    const int month = int(monthPart < 10 ? monthPart + 3 : monthPart - 9);
    const int year = int(yearOfEra + era * 400 + (month <= 2));
    return {year, month, day};
}

static int64_t daysFromCivil(const base::CalendarDate& date)
{
    const int64_t year = date.year - (date.month <= 2);
    const int64_t era = floorDiv(year, 400);
    const int64_t yearOfEra = year - era * 400;
    const int64_t dayOfYear = (153 * (date.month > 2 ? date.month - 3 : date.month + 9) + 2) / 5 + date.day - 1;
    const int64_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

ZoneTime ZoneTime::at(std::string_view zoneId, int64_t utcMilliseconds)
{
    ZoneTime result;
    const std::chrono::sys_seconds seconds{std::chrono::seconds(floorDiv(utcMilliseconds, 1000))};
    try {
        const std::chrono::time_zone* zone = std::chrono::locate_zone(zoneId);
        const std::chrono::sys_info info = zone->get_info(seconds);
        result.offsetSeconds = int(info.offset.count());
        result.daylightSaving = info.save != std::chrono::minutes(0);
        result.abbreviation = info.abbrev;
    } catch (const std::runtime_error&) {
        result.abbreviation = "UTC";
    }

    const int64_t local = utcMilliseconds + int64_t(result.offsetSeconds) * 1000;
    const int64_t day = floorDiv(local, 86400000);
    const int64_t inDay = local - day * 86400000;
    result.date = civilFromDays(day);
    result.time = {int(inDay / 3600000), int(inDay / 60000 % 60), int(inDay / 1000 % 60), int(inDay % 1000)};
    return result;
}

int dayDifference(const base::CalendarDate& here, const base::CalendarDate& there)
{
    return int(daysFromCivil(there) - daysFromCivil(here));
}

std::string relativeDayText(int days)
{
    if (days == 0)
        return tr("Today");
    if (days == 1)
        return tr("Tomorrow");
    if (days == -1)
        return tr("Yesterday");
    return days > 0 ? trn("In {n} day", "In {n} days", uint64_t(days))
                    : trn("{n} day ago", "{n} days ago", uint64_t(-days));
}

std::string offsetText(int gapSeconds)
{
    const std::string sign = gapSeconds < 0 ? "-" : "+";
    const int hours = std::abs(gapSeconds) / 3600;
    const int minutes = std::abs(gapSeconds) % 3600 / 60;
    if (minutes == 0) {
        // The count goes in unsigned, the sign in front of it.
        return sign + trn("{n}HR", "{n}HRS", uint64_t(hours));
    }
    return sign + format(tr("{hours}:{minutes}HRS"),
                         {{"hours", std::to_string(hours)},
                          {"minutes", (minutes < 10 ? "0" : "") + std::to_string(minutes)}});
}

std::string captionText(int days, int gapSeconds)
{
    return format(tr("{day}, {offset}"), {{"day", relativeDayText(days)}, {"offset", offsetText(gapSeconds)}});
}

bool isDaytime(const base::ClockTime& time)
{
    return time.hour >= 6 && time.hour < 18;
}

std::string digitalText(const base::ClockTime& time, const base::Locale& locale)
{
    return locale.toString(base::ClockTime{time.hour, time.minute}, base::Locale::FormatType::Short);
}

HandAngles handAngles(const base::ClockTime& time)
{
    const double minutes = time.minute + time.second / 60.0;
    return {(time.hour % 12 + minutes / 60.0) * 30.0, minutes * 6.0, time.second * 6.0};
}

} // namespace clockapp::world
