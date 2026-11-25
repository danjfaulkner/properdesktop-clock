// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include "world/zonetime.h"

using namespace clockapp::world;
using properdesktop::base::CalendarDate;
using properdesktop::base::ClockTime;

// 2026-11-20 00:00:00 UTC.
static constexpr int64_t kNovember20 = 1795132800000;

static int64_t utc(int64_t dayStart, int hour, int minute = 0, int second = 0)
{
    return dayStart + (int64_t(hour) * 3600 + minute * 60 + second) * 1000;
}

TEST(ZoneTime, ReadsTheWallClockOfTheZone)
{
    const ZoneTime singapore = ZoneTime::at("Asia/Singapore", utc(kNovember20, 20, 15, 30));
    EXPECT_EQ(singapore.offsetSeconds, 8 * 3600);
    EXPECT_FALSE(singapore.daylightSaving);
    EXPECT_EQ(singapore.date, (CalendarDate{2026, 11, 21}));
    EXPECT_EQ(singapore.time, (ClockTime{4, 15, 30, 0}));

    const ZoneTime honolulu = ZoneTime::at("Pacific/Honolulu", utc(kNovember20, 5));
    EXPECT_EQ(honolulu.offsetSeconds, -10 * 3600);
    EXPECT_EQ(honolulu.date, (CalendarDate{2026, 11, 19}));
    EXPECT_EQ(honolulu.time.hour, 19);
}

TEST(ZoneTime, FollowsDaylightSavingAcrossTheChange)
{
    // London goes forward at 01:00 UTC on 2026-03-29.
    const int64_t march29 = kNovember20 - int64_t(236) * 86400000;
    const ZoneTime before = ZoneTime::at("Europe/London", utc(march29, 0, 59, 59));
    const ZoneTime after = ZoneTime::at("Europe/London", utc(march29, 1));
    EXPECT_EQ(before.date, (CalendarDate{2026, 3, 29}));
    EXPECT_EQ(before.offsetSeconds, 0);
    EXPECT_FALSE(before.daylightSaving);
    EXPECT_EQ(before.time, (ClockTime{0, 59, 59, 0}));
    EXPECT_EQ(after.offsetSeconds, 3600);
    EXPECT_TRUE(after.daylightSaving);
    EXPECT_EQ(after.abbreviation, "BST");
    EXPECT_EQ(after.time, (ClockTime{2, 0, 0, 0}));

    // New York goes back at 06:00 UTC on 2026-11-01.
    const int64_t november1 = kNovember20 - int64_t(19) * 86400000;
    EXPECT_EQ(ZoneTime::at("America/New_York", utc(november1, 5, 59)).offsetSeconds, -4 * 3600);
    EXPECT_EQ(ZoneTime::at("America/New_York", utc(november1, 6)).offsetSeconds, -5 * 3600);

    // The southern summer.
    const ZoneTime sydney = ZoneTime::at("Australia/Sydney", kNovember20);
    EXPECT_EQ(sydney.offsetSeconds, 11 * 3600);
    EXPECT_TRUE(sydney.daylightSaving);
}

TEST(ZoneTime, UnknownZoneIsUtc)
{
    const ZoneTime time = ZoneTime::at("Nowhere/Atlantis", utc(kNovember20, 9, 30));
    EXPECT_EQ(time.offsetSeconds, 0);
    EXPECT_EQ(time.time.hour, 9);
}

TEST(ZoneTime, BeforeTheEpochCountsDown)
{
    const ZoneTime time = ZoneTime::at("Etc/UTC", -1000);
    EXPECT_EQ(time.date, (CalendarDate{1969, 12, 31}));
    EXPECT_EQ(time.time, (ClockTime{23, 59, 59, 0}));
}

TEST(ZoneTime, DayDifference)
{
    EXPECT_EQ(dayDifference({2026, 11, 20}, {2026, 11, 20}), 0);
    EXPECT_EQ(dayDifference({2026, 11, 20}, {2026, 11, 21}), 1);
    EXPECT_EQ(dayDifference({2026, 11, 20}, {2026, 11, 19}), -1);
    EXPECT_EQ(dayDifference({2026, 12, 31}, {2027, 1, 1}), 1);
    EXPECT_EQ(dayDifference({2028, 3, 1}, {2028, 2, 29}), -1);
}

TEST(ZoneTime, RelativeDayText)
{
    EXPECT_EQ(relativeDayText(0), "Today");
    EXPECT_EQ(relativeDayText(1), "Tomorrow");
    EXPECT_EQ(relativeDayText(-1), "Yesterday");
}

TEST(ZoneTime, OffsetText)
{
    EXPECT_EQ(offsetText(8 * 3600), "+8HRS");
    EXPECT_EQ(offsetText(-3600), "-1HR");
    EXPECT_EQ(offsetText(3600), "+1HR");
    EXPECT_EQ(offsetText(0), "+0HRS");
    EXPECT_EQ(offsetText(5 * 3600 + 1800), "+5:30HRS");
    EXPECT_EQ(offsetText(-(2 * 3600 + 1800)), "-2:30HRS");
    EXPECT_EQ(offsetText(-1800), "-0:30HRS");
    EXPECT_EQ(offsetText(12 * 3600 + 45 * 60 - 13 * 3600), "-0:15HRS");
}

TEST(ZoneTime, CaptionText)
{
    EXPECT_EQ(captionText(0, 8 * 3600), "Today, +8HRS");
    EXPECT_EQ(captionText(-1, -16 * 3600), "Yesterday, -16HRS");
}

TEST(ZoneTime, Daytime)
{
    EXPECT_FALSE(isDaytime({5, 59}));
    EXPECT_TRUE(isDaytime({6, 0}));
    EXPECT_TRUE(isDaytime({17, 59}));
    EXPECT_FALSE(isDaytime({18, 0}));
}

TEST(ZoneTime, DigitalTextFollowsTheHourCycle)
{
    properdesktop::base::RegionalPreferences preferences;
    preferences.region = "GB";
    preferences.twentyFourHour = true;
    const auto twentyFour = properdesktop::base::Locale::regional("en", preferences);
    EXPECT_EQ(digitalText({13, 5, 42}, twentyFour), "13:05");

    preferences.twentyFourHour = false;
    const auto twelve = properdesktop::base::Locale::regional("en", preferences);
    const std::string text = digitalText({13, 5, 42}, twelve);
    EXPECT_EQ(text.find("13"), std::string::npos) << text;
    EXPECT_NE(text.find("1:05"), std::string::npos) << text;
}

TEST(ZoneTime, HandAngles)
{
    const HandAngles noon = handAngles({12, 0, 0});
    EXPECT_DOUBLE_EQ(noon.hour, 0);
    EXPECT_DOUBLE_EQ(noon.minute, 0);

    const HandAngles angles = handAngles({15, 30, 15});
    EXPECT_DOUBLE_EQ(angles.second, 90);
    EXPECT_DOUBLE_EQ(angles.minute, 30.25 * 6);
    EXPECT_DOUBLE_EQ(angles.hour, (3 + 30.25 / 60) * 30);
}
