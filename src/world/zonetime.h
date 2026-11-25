// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "base/locale.h"

namespace clockapp::world {

// The wall clock in a zone at one moment.
struct ZoneTime
{
    // Seconds east of UTC, daylight saving time included.
    int offsetSeconds = 0;
    bool daylightSaving = false;
    // As the database abbreviates it, such as "CEST" or "+08".
    std::string abbreviation;
    properdesktop::base::CalendarDate date;
    properdesktop::base::ClockTime time;

    // UTC for an unknown zone.
    static ZoneTime at(std::string_view zoneId, int64_t utcMilliseconds);
};

// Days from the date `here` to the date `there`: 1 when it is already
// tomorrow there.
int dayDifference(const properdesktop::base::CalendarDate& here, const properdesktop::base::CalendarDate& there);
// "Today", "Tomorrow", "Yesterday"; further days by count.
std::string relativeDayText(int days);
// The gap to the time here, as the card's caption puts it: "+8HRS",
// "-1HR", "+5:30HRS", "+0HRS".
std::string offsetText(int gapSeconds);
// "Today, +8HRS".
std::string captionText(int days, int gapSeconds);
// The clock face is light from 6:00 to 17:59 there.
bool isDaytime(const properdesktop::base::ClockTime& time);
// The time in the locale's short format, which follows the 12 or 24 hour
// choice.
std::string digitalText(const properdesktop::base::ClockTime& time,
                        const properdesktop::base::Locale& locale = properdesktop::base::Locale::system());

// Clockwise from twelve, in degrees. The second hand steps; the minute and
// hour hands move with the seconds.
struct HandAngles
{
    double hour = 0;
    double minute = 0;
    double second = 0;
};
HandAngles handAngles(const properdesktop::base::ClockTime& time);

} // namespace clockapp::world
