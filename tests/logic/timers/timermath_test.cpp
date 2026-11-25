// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "timers/timermath.h"

#include <base/settings.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

using namespace clockapp;

static constexpr int64_t Start = 1795167660000;

TEST(TimerMath, RemainingRunsDownToZero)
{
    RunningTimer timer = timermath::make(5 * 60 * 1000, "Tea", "chime", Start);
    EXPECT_TRUE(timer.isRunning());
    EXPECT_EQ(timer.endsAtMs, Start + 300000);
    EXPECT_EQ(timermath::remainingMs(timer, Start), 300000);
    EXPECT_DOUBLE_EQ(timermath::fractionLeft(timer, Start), 1);
    EXPECT_EQ(timermath::remainingMs(timer, Start + 120000), 180000);
    EXPECT_DOUBLE_EQ(timermath::fractionLeft(timer, Start + 150000), 0.5);
    EXPECT_FALSE(timermath::isFinished(timer, Start + 299999));
    EXPECT_TRUE(timermath::isFinished(timer, Start + 300000));
    EXPECT_EQ(timermath::remainingMs(timer, Start + 400000), 0);
    // A clock set back before the start shows no more than the duration.
    EXPECT_EQ(timermath::remainingMs(timer, Start - 60000), 300000);
}

TEST(TimerMath, PauseKeepsWhatIsLeft)
{
    RunningTimer timer = timermath::make(60000, {}, {}, Start);
    timermath::pause(timer, Start + 20000);
    EXPECT_FALSE(timer.isRunning());
    EXPECT_EQ(timer.remainingMs, 40000);
    // Paused, time passing changes nothing, and it never finishes.
    EXPECT_EQ(timermath::remainingMs(timer, Start + 500000), 40000);
    EXPECT_FALSE(timermath::isFinished(timer, Start + 500000));

    timermath::resume(timer, Start + 100000);
    EXPECT_TRUE(timer.isRunning());
    EXPECT_EQ(timer.endsAtMs, Start + 140000);
    EXPECT_EQ(timermath::remainingMs(timer, Start + 110000), 30000);

    // Pausing twice, or a finished timer, keeps it as it is.
    timermath::pause(timer, Start + 120000);
    timermath::pause(timer, Start + 130000);
    EXPECT_EQ(timer.remainingMs, 20000);
    RunningTimer over = timermath::make(1000, {}, {}, Start);
    timermath::pause(over, Start + 5000);
    EXPECT_TRUE(over.isRunning());

    timermath::restart(over, Start + 9000);
    EXPECT_EQ(timermath::remainingMs(over, Start + 9000), 1000);
}

TEST(TimerMath, DurationsAreClamped)
{
    EXPECT_EQ(timermath::make(0, {}, {}, Start).durationMs, 1000);
    EXPECT_EQ(timermath::make(100LL * 3600 * 1000, {}, {}, Start).durationMs, timermath::MaximumDurationMs);
}

TEST(TimerMath, FormatsTheTimeLeft)
{
    EXPECT_EQ(timermath::formatRemaining(0), "0:00");
    // Rounded up: a timer shows 0:00 only once it is over.
    EXPECT_EQ(timermath::formatRemaining(1), "0:01");
    EXPECT_EQ(timermath::formatRemaining(4200), "0:05");
    EXPECT_EQ(timermath::formatRemaining(12 * 60 * 1000), "12:00");
    EXPECT_EQ(timermath::formatRemaining((3600 + 120 + 3) * 1000), "1:02:03");

    EXPECT_EQ(timermath::formatDuration(5 * 60 * 1000), "5 min");
    EXPECT_EQ(timermath::formatDuration(45 * 1000), "45 sec");
    EXPECT_EQ(timermath::formatDuration((3600 + 30 * 60) * 1000), "1 hr 30 min");
    EXPECT_EQ(timermath::formatDuration((7200 + 5) * 1000), "2 hr 5 sec");
}

TEST(TimerMath, RecentsKeepTheNewestFirst)
{
    std::vector<timermath::Recent> recents;
    timermath::remember(recents, {60000, "Eggs", "dawn"});
    timermath::remember(recents, {300000, "Tea", "chime"});
    timermath::remember(recents, {60000, "Eggs", "dawn"});
    ASSERT_EQ(recents.size(), 2u);
    EXPECT_EQ(recents[0].label, "Eggs");

    for (int i = 1; i <= 10; ++i)
        timermath::remember(recents, {i * 1000LL, {}, "dawn"});
    EXPECT_EQ(recents.size(), timermath::MaximumRecents);
    EXPECT_EQ(recents.front().durationMs, 10000);

    timermath::forget(recents, 0);
    EXPECT_EQ(recents.front().durationMs, 9000);
    timermath::forget(recents, 99);
    EXPECT_EQ(recents.size(), timermath::MaximumRecents - 1);
}

TEST(TimerMath, RecentsRoundTripTheFile)
{
    std::string pattern = (std::filesystem::temp_directory_path() / "clock-timers-XXXXXX").string();
    const std::string dir = mkdtemp(pattern.data());
    const std::string path = dir + "/clock.conf";
    const std::vector<timermath::Recent> recents{{300000, "Tea, green | strong", "chime"}, {90000, {}, "dawn"}};
    {
        properdesktop::base::Settings settings(path);
        timermath::saveRecents(settings, recents);
        settings.setValue("Timers/Other", 1);
        settings.sync();
    }
    {
        properdesktop::base::Settings settings(path);
        EXPECT_EQ(timermath::loadRecents(settings), recents);
        timermath::saveRecents(settings, {});
        settings.sync();
    }
    properdesktop::base::Settings settings(path);
    EXPECT_TRUE(timermath::loadRecents(settings).empty());
    EXPECT_TRUE(settings.contains("Timers/Other"));
    std::error_code error;
    std::filesystem::remove_all(dir, error);
}

TEST(TimerMath, RepeatRestartsTheStoredTimer)
{
    std::string pattern = (std::filesystem::temp_directory_path() / "clock-timers-XXXXXX").string();
    const std::string dir = mkdtemp(pattern.data());
    {
        ClockStore store(clockstore::pathIn(dir));
        const std::string id = store.setTimer(timermath::make(90000, "Tea", "chime", Start));
        EXPECT_FALSE(timermath::repeatTimer(store, "unknown", Start + 100000));
        EXPECT_TRUE(timermath::repeatTimer(store, id, Start + 100000));
        ASSERT_TRUE(store.timer(id));
        EXPECT_EQ(store.timer(id)->endsAtMs, Start + 190000);
        EXPECT_EQ(store.timer(id)->label, "Tea");

        // A paused timer starts over too.
        RunningTimer paused = *store.timer(id);
        timermath::pause(paused, Start + 120000);
        store.setTimer(paused);
        EXPECT_TRUE(timermath::repeatTimer(store, id, Start + 200000));
        EXPECT_TRUE(store.timer(id)->isRunning());
        EXPECT_EQ(timermath::remainingMs(*store.timer(id), Start + 200000), 90000);
    }
    std::error_code error;
    std::filesystem::remove_all(dir, error);
}
