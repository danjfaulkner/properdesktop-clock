// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "store/store.h"

#include <base/event_loop.h>
#include <base/file.h>

#include <gtest/gtest.h>

#include <filesystem>

using namespace clockapp;
namespace fs = std::filesystem;

static ClockData sample()
{
    ClockData data;
    Alarm wake;
    wake.id = "a1";
    wake.hour = 6;
    wake.minute = 45;
    wake.repeat = 0b0111110;
    wake.label = "Wake \"up\"";
    wake.sound = "chime";
    wake.snooze = false;
    wake.enabled = true;
    wake.armedAt = 1790000000;
    data.alarms.push_back(wake);
    Alarm once;
    once.id = "a2";
    once.hour = 23;
    once.minute = 0;
    once.enabled = false;
    data.alarms.push_back(once);

    RunningTimer tea;
    tea.id = "t1";
    tea.label = "Tea";
    tea.durationMs = 180000;
    tea.endsAtMs = 1790000123456;
    tea.sound = "pulse";
    data.timers.push_back(tea);
    RunningTimer paused;
    paused.id = "t2";
    paused.durationMs = 600000;
    paused.remainingMs = 250000;
    data.timers.push_back(paused);
    return data;
}

TEST(StoreTest, RoundTrip)
{
    const ClockData data = sample();
    const std::optional<ClockData> read = clockstore::parse(clockstore::serialize(data));
    ASSERT_TRUE(read);
    EXPECT_EQ(*read, data);
    EXPECT_TRUE(read->timers[0].isRunning());
    EXPECT_FALSE(read->timers[1].isRunning());
    EXPECT_FALSE(read->alarms[0].isOneTime());
    EXPECT_TRUE(read->alarms[1].isOneTime());
}

TEST(StoreTest, RejectsCutShortText)
{
    const std::string text = clockstore::serialize(sample());
    for (size_t length : {size_t(0), size_t(1), text.size() / 3, text.size() / 2, text.size() - 3})
        EXPECT_FALSE(clockstore::parse(text.substr(0, length))) << length;
    EXPECT_FALSE(clockstore::parse("[1, 2]"));
    EXPECT_FALSE(clockstore::parse("not json"));
}

TEST(StoreTest, SkipsInvalidEntries)
{
    const std::optional<ClockData> data = clockstore::parse(R"({
        "alarms": [
            {"id": "ok", "hour": 7, "minute": 30, "repeat": [0, 6, 9, -1, "x"]},
            {"id": "", "hour": 7, "minute": 30},
            {"id": "late", "hour": 24, "minute": 0},
            {"id": "minute", "hour": 3, "minute": 60},
            "nonsense"
        ],
        "timers": [
            {"id": "t", "duration": 1000, "remaining": 5000},
            {"id": "zero", "duration": 0},
            {"duration": 1000}
        ],
        "future": {"keys": true}
    })");
    ASSERT_TRUE(data);
    ASSERT_EQ(data->alarms.size(), 1u);
    EXPECT_EQ(data->alarms[0].id, "ok");
    EXPECT_EQ(data->alarms[0].repeat, 0b1000001);
    EXPECT_TRUE(data->alarms[0].snooze);
    EXPECT_TRUE(data->alarms[0].enabled);
    ASSERT_EQ(data->timers.size(), 1u);
    EXPECT_EQ(data->timers[0].remainingMs, 1000);
}

class StoreFileTest : public testing::Test
{
protected:
    void SetUp() override
    {
        std::string pattern = (fs::temp_directory_path() / "clock-store-XXXXXX").string();
        m_folder = mkdtemp(pattern.data());
    }
    void TearDown() override
    {
        std::error_code error;
        fs::remove_all(m_folder, error);
    }

    std::string m_folder;
};

TEST_F(StoreFileTest, WritesAndReadsBack)
{
    properdesktop::base::EventLoop loop;
    ClockStore store(clockstore::pathIn(m_folder + "/clock"));
    EXPECT_TRUE(store.alarms().empty());
    int changes = 0;
    properdesktop::base::Subscription subscription = store.changed.connect([&changes] { ++changes; });

    Alarm alarm;
    alarm.hour = 8;
    const std::string id = store.setAlarm(alarm);
    EXPECT_FALSE(id.empty());
    EXPECT_EQ(changes, 1);
    alarm.id = id;
    alarm.minute = 15;
    EXPECT_EQ(store.setAlarm(alarm), id);
    ASSERT_EQ(store.alarms().size(), 1u);
    EXPECT_EQ(store.alarm(id)->minute, 15);

    RunningTimer timer;
    timer.durationMs = 60000;
    const std::string timerId = store.setTimer(timer);

    ClockStore reader(store.path());
    EXPECT_EQ(reader.data(), store.data());

    store.removeAlarm(id);
    store.removeTimer(timerId);
    reader.load();
    EXPECT_TRUE(reader.alarms().empty());
    EXPECT_TRUE(reader.timers().empty());
}

TEST_F(StoreFileTest, ReaderKeepsLastGoodContents)
{
    properdesktop::base::EventLoop loop;
    const std::string path = clockstore::pathIn(m_folder);
    ASSERT_TRUE(properdesktop::base::file::writeAtomically(path, clockstore::serialize(sample())));
    ClockStore reader(path);
    ASSERT_EQ(reader.alarms().size(), 2u);

    int changes = 0;
    properdesktop::base::Subscription subscription = reader.changed.connect([&changes] { ++changes; });
    const std::string text = clockstore::serialize(sample());
    ASSERT_TRUE(properdesktop::base::file::writeAtomically(path, text.substr(0, text.size() / 2)));
    reader.load();
    EXPECT_EQ(reader.alarms().size(), 2u);
    EXPECT_EQ(changes, 0);

    // Gone altogether: nothing is set any more.
    fs::remove(path);
    reader.load();
    EXPECT_TRUE(reader.alarms().empty());
    EXPECT_EQ(changes, 1);
}

TEST_F(StoreFileTest, WatchFollowsTheWriter)
{
    properdesktop::base::EventLoop loop;
    const std::string path = clockstore::pathIn(m_folder);
    ClockStore reader(path);
    reader.watch();
    ClockStore writer(path);
    properdesktop::base::Subscription subscription = reader.changed.connect([&loop] { loop.quit(); });
    Alarm alarm;
    alarm.hour = 5;
    writer.setAlarm(alarm);
    const int expire = loop.startTimer(std::chrono::seconds(5), false, [&loop] { loop.quit(); });
    loop.run();
    loop.stopTimer(expire);
    ASSERT_EQ(reader.alarms().size(), 1u);
    EXPECT_EQ(reader.alarms()[0].hour, 5);
}

TEST_F(StoreFileTest, SharedPerFolder)
{
    properdesktop::base::EventLoop loop;
    ClockStore& first = ClockStore::inFolder(m_folder);
    EXPECT_EQ(&first, &ClockStore::inFolder(m_folder));
    EXPECT_NE(&first, &ClockStore::inFolder(m_folder + "/other"));
    EXPECT_EQ(first.path(), m_folder + "/alarms.json");
}
