// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "stopwatch/stopwatch.h"

#include <base/settings.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

using namespace clockapp;

static constexpr int64_t Second = 1000000000;

static Reading at(int64_t bootSeconds, int64_t wallSeconds, const std::string& boot = "boot-a")
{
    return {bootSeconds * Second, wallSeconds * 1000, boot};
}

class StopwatchFile : public testing::Test
{
protected:
    void SetUp() override
    {
        std::string pattern = (std::filesystem::temp_directory_path() / "clock-stopwatch-XXXXXX").string();
        m_dir = mkdtemp(pattern.data());
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(m_dir, error);
    }

    std::string path() const { return m_dir + "/clock.conf"; }

    // Saved to the file and read back by a new program.
    Stopwatch roundTrip(const Stopwatch& stopwatch, const Reading& now)
    {
        {
            properdesktop::base::Settings settings(path());
            stopwatch.save(settings);
            settings.sync();
        }
        properdesktop::base::Settings settings(path());
        Stopwatch loaded;
        loaded.load(settings, now);
        return loaded;
    }

    std::string m_dir;
};

TEST(Stopwatch, CountsOnlyWhileRunning)
{
    Stopwatch stopwatch;
    EXPECT_FALSE(stopwatch.hasTime());
    EXPECT_EQ(stopwatch.elapsedNs(at(100, 1000)), 0);

    stopwatch.start(at(100, 1000));
    EXPECT_EQ(stopwatch.elapsedNs(at(105, 1005)), 5 * Second);
    stopwatch.stop(at(110, 1010));
    EXPECT_EQ(stopwatch.elapsedNs(at(200, 1100)), 10 * Second);

    // On again: the pause does not count.
    stopwatch.start(at(300, 1200));
    EXPECT_EQ(stopwatch.elapsedNs(at(302, 1202)), 12 * Second);
    stopwatch.reset();
    EXPECT_FALSE(stopwatch.hasTime());
    EXPECT_EQ(stopwatch.lapCount(), 0u);
}

TEST(Stopwatch, LapsSplitTheTime)
{
    Stopwatch stopwatch;
    stopwatch.start(at(0, 0));
    stopwatch.lap(at(3, 3));
    stopwatch.lap(at(10, 10));
    EXPECT_EQ(stopwatch.lapDurations(), (std::vector<int64_t>{3 * Second, 7 * Second}));
    EXPECT_EQ(stopwatch.currentLapNs(at(12, 12)), 2 * Second);

    // A lap needs the stopwatch running.
    stopwatch.stop(at(12, 12));
    stopwatch.lap(at(20, 20));
    EXPECT_EQ(stopwatch.lapCount(), 2u);
}

TEST(Stopwatch, FastestAndSlowestFromThreeLaps)
{
    Stopwatch stopwatch;
    stopwatch.start(at(0, 0));
    stopwatch.lap(at(5, 5));
    stopwatch.lap(at(7, 7));
    EXPECT_FALSE(stopwatch.lapExtremes());

    stopwatch.lap(at(16, 16));
    std::optional<LapExtremes> extremes = stopwatch.lapExtremes();
    ASSERT_TRUE(extremes);
    EXPECT_EQ(extremes->fastest, 1u);
    EXPECT_EQ(extremes->slowest, 2u);

    // Laps all alike have nothing to mark.
    Stopwatch even;
    even.start(at(0, 0));
    for (int i = 1; i <= 3; ++i)
        even.lap(at(i * 4, i * 4));
    EXPECT_FALSE(even.lapExtremes());
}

TEST_F(StopwatchFile, RunsOnAcrossARestart)
{
    Stopwatch stopwatch;
    stopwatch.start(at(100, 5000));
    stopwatch.lap(at(130, 5030));

    // The program comes back later in the same boot, the wall clock set an
    // hour back meanwhile: the boot clock still counts right.
    Stopwatch loaded = roundTrip(stopwatch, at(160, 1400));
    EXPECT_TRUE(loaded.isRunning());
    EXPECT_EQ(loaded.elapsedNs(at(160, 1400)), 60 * Second);
    EXPECT_EQ(loaded.lapDurations(), (std::vector<int64_t>{30 * Second}));
    EXPECT_EQ(loaded.currentLapNs(at(160, 1400)), 30 * Second);
}

TEST_F(StopwatchFile, RunsOnAcrossAReboot)
{
    Stopwatch stopwatch;
    stopwatch.start(at(500, 10000));
    stopwatch.stop(at(520, 10020));
    stopwatch.start(at(530, 10030));

    // A new boot: its clock starts from nothing, the wall clock says 70 s
    // passed since the run began.
    const Reading after = at(12, 10100, "boot-b");
    Stopwatch loaded = roundTrip(stopwatch, after);
    EXPECT_TRUE(loaded.isRunning());
    EXPECT_EQ(loaded.elapsedNs(after), (20 + 70) * Second);
    EXPECT_EQ(loaded.elapsedNs(at(22, 10110, "boot-b")), (20 + 80) * Second);

    // A wall clock set back before the start counts no time, not less.
    Stopwatch backwards = roundTrip(stopwatch, at(12, 9000, "boot-b"));
    EXPECT_EQ(backwards.elapsedNs(at(12, 9000, "boot-b")), 20 * Second);
}

TEST_F(StopwatchFile, KeepsAStoppedTimeAndForgetsAReset)
{
    Stopwatch stopwatch;
    stopwatch.start(at(0, 0));
    stopwatch.lap(at(4, 4));
    stopwatch.stop(at(9, 9));
    Stopwatch loaded = roundTrip(stopwatch, at(1000, 1000, "boot-c"));
    EXPECT_FALSE(loaded.isRunning());
    EXPECT_EQ(loaded.elapsedNs(at(1000, 1000)), 9 * Second);
    EXPECT_EQ(loaded.lapCount(), 1u);

    {
        properdesktop::base::Settings settings(path());
        settings.setValue("Stopwatch/Face", "dial");
        settings.sync();
    }
    stopwatch.reset();
    loaded = roundTrip(stopwatch, at(1000, 1000));
    EXPECT_FALSE(loaded.hasTime());
    properdesktop::base::Settings settings(path());
    EXPECT_FALSE(settings.contains("Stopwatch/Running"));
    EXPECT_EQ(settings.stringValue("Stopwatch/Face"), "dial");
}

TEST(Stopwatch, FormatsHundredths)
{
    EXPECT_EQ(formatElapsed(0), "00:00.00");
    EXPECT_EQ(formatElapsed(5 * Second + 439000000), "00:05.43");
    EXPECT_EQ(formatElapsed(65 * Second + 999999999), "01:05.99");
    EXPECT_EQ(formatElapsed(3600 * Second + 7 * Second), "1:00:07.00");
    EXPECT_EQ(formatElapsed(-5), "00:00.00");
}
