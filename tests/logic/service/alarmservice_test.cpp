// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "alarms/alarmtime.h"
#include "alarms/toneplayer.h"
#include "service/alarmservice.h"
#include "store/store.h"

#include <base/dbus.h>
#include <base/event_loop.h>

#include <gtest/gtest.h>

#include <ctime>
#include <filesystem>

using namespace clockapp;
namespace fs = std::filesystem;
namespace base = properdesktop::base;
namespace dbus = properdesktop::base::dbus;

namespace {

struct Shown
{
    std::string application;
    uint32_t replaces = 0;
    std::string summary;
    std::string body;
    std::vector<std::string> actions;
    dbus::VariantMap hints;
    int32_t timeout = -1;
};

class FakeTonePlayer : public TonePlayer
{
public:
    void play(const std::string& sound, Options options) override
    {
        playing = true;
        lastSound = sound;
        lastRise = options.rise;
        ++plays;
    }
    void stop() override { playing = false; }
    bool isPlaying() const override { return playing; }

    bool playing = false;
    std::string lastSound;
    double lastRise = 0;
    int plays = 0;
};

}

static bool runUntil(base::EventLoop& loop, const std::function<bool()>& condition,
                     std::chrono::milliseconds timeout = std::chrono::seconds(5))
{
    if (condition())
        return true;
    const int check = loop.startTimer(std::chrono::milliseconds(10), true, [&] {
        if (condition())
            loop.quit();
    });
    const int expire = loop.startTimer(timeout, false, [&] { loop.quit(); });
    loop.run();
    loop.stopTimer(check);
    loop.stopTimer(expire);
    return condition();
}

static int64_t todayAt(int hour, int minute)
{
    const time_t now = time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    return int64_t(mktime(&tm));
}

// Runs on a private bus: a notification server of its own on it.
class AlarmServiceTest : public testing::Test
{
protected:
    void SetUp() override
    {
        bus = dbus::Connection::session(&loop);
        ASSERT_TRUE(bus);
        server = dbus::Connection::open(dbus::Connection::Bus::Session, &loop);
        ASSERT_TRUE(server);
        ASSERT_TRUE(server->requestName("org.freedesktop.Notifications"));
        notifications = std::make_unique<dbus::Object>(server.get(), "/org/freedesktop/Notifications",
                                                       "org.freedesktop.Notifications");
        notifications->addMethod("Notify", [this](const std::string& application, uint32_t replaces,
                                                  const std::string&, const std::string& summary,
                                                  const std::string& body, const std::vector<std::string>& actions,
                                                  const dbus::VariantMap& hints, int32_t timeout) {
            shown.push_back({application, replaces, summary, body, actions, hints, timeout});
            return uint32_t(shown.size());
        });
        notifications->addMethod("CloseNotification", [this](uint32_t id) { closed.push_back(id); });
        notifications->addSignal("ActionInvoked", "us");
        notifications->addSignal("NotificationClosed", "uu");
        ASSERT_TRUE(notifications->publish());

        std::string pattern = (fs::temp_directory_path() / "clock-service-XXXXXX").string();
        folder = mkdtemp(pattern.data());
        writer = std::make_unique<ClockStore>(clockstore::pathIn(folder));
        reader = std::make_unique<ClockStore>(writer->path());
    }

    void TearDown() override
    {
        service.reset();
        std::error_code error;
        fs::remove_all(folder, error);
    }

    void start()
    {
        service = std::make_unique<AlarmService>(*reader, bus, player, [this] { return nowMs; });
        service->check();
    }

    void invoke(uint32_t id, const std::string& action)
    {
        notifications->emitSignal("ActionInvoked", id, action);
    }

    base::EventLoop loop;
    dbus::Connection* bus = nullptr;
    std::unique_ptr<dbus::Connection> server;
    std::unique_ptr<dbus::Object> notifications;
    std::vector<Shown> shown;
    std::vector<uint32_t> closed;
    std::string folder;
    std::unique_ptr<ClockStore> writer;
    std::unique_ptr<ClockStore> reader;
    FakeTonePlayer player;
    int64_t nowMs = 0;
    std::unique_ptr<AlarmService> service;
};

TEST_F(AlarmServiceTest, DueAlarmNotifiesAndSnoozes)
{
    const int64_t ring = todayAt(10, 0);
    nowMs = (ring - 30) * 1000;
    Alarm alarm;
    alarm.hour = 10;
    alarm.minute = 0;
    alarm.repeat = alarmtime::EveryDay;
    alarm.label = "Stand <up>";
    alarm.sound = "chime";
    writer->setAlarm(alarm);
    reader->load();
    start();
    runUntil(loop, [] { return false; }, std::chrono::milliseconds(100));
    EXPECT_TRUE(shown.empty());

    nowMs = (ring + 1) * 1000;
    service->check();
    ASSERT_TRUE(runUntil(loop, [this] { return !shown.empty(); }));
    const Shown& notification = shown.front();
    EXPECT_EQ(notification.application, "Clock");
    EXPECT_EQ(notification.summary, "Stand <up>");
    EXPECT_EQ(notification.actions, (std::vector<std::string>{"default", "", "snooze", "Snooze", "stop", "Stop"}));
    EXPECT_EQ(notification.hints.at("urgency").get<uint8_t>().value_or(0), 2);
    EXPECT_EQ(notification.hints.at("desktop-entry").get<std::string>().value_or(""), "properdesktop-clock");
    EXPECT_EQ(notification.timeout, 0);
    EXPECT_TRUE(player.playing);
    EXPECT_EQ(player.lastSound, "chime");
    EXPECT_GT(player.lastRise, 0);
    EXPECT_EQ(service->ringingCount(), 1u);

    // Once, however often the file changes.
    reader->changed.emit();
    service->check();
    runUntil(loop, [] { return false; }, std::chrono::milliseconds(100));
    EXPECT_EQ(shown.size(), 1u);

    invoke(1, "snooze");
    ASSERT_TRUE(runUntil(loop, [this] { return !closed.empty(); }));
    EXPECT_EQ(closed.front(), 1u);
    EXPECT_FALSE(player.playing);
    EXPECT_EQ(service->ringingCount(), 0u);

    nowMs += (alarmtime::SnoozeMinutes * 60 - 1) * 1000;
    service->check();
    runUntil(loop, [] { return false; }, std::chrono::milliseconds(100));
    EXPECT_EQ(shown.size(), 1u);
    nowMs += 2000;
    service->check();
    ASSERT_TRUE(runUntil(loop, [this] { return shown.size() == 2; }));
    EXPECT_TRUE(player.playing);

    invoke(2, "stop");
    ASSERT_TRUE(runUntil(loop, [this] { return closed.size() == 2; }));
    EXPECT_FALSE(player.playing);
}

TEST_F(AlarmServiceTest, FinishedTimerNotifies)
{
    nowMs = 1790000000000;
    RunningTimer timer;
    timer.label = "Tea";
    timer.durationMs = 3 * 60 * 1000;
    timer.endsAtMs = nowMs + 2000;
    timer.sound = "pulse";
    writer->setTimer(timer);
    RunningTimer paused;
    paused.durationMs = 60000;
    paused.remainingMs = 1000;
    writer->setTimer(paused);
    reader->load();
    start();

    nowMs += 2500;
    service->check();
    ASSERT_TRUE(runUntil(loop, [this] { return !shown.empty(); }));
    EXPECT_EQ(shown.front().summary, "Tea");
    EXPECT_EQ(shown.front().body, "3 minutes done");
    EXPECT_EQ(shown.front().actions, (std::vector<std::string>{"default", "", "stop", "Stop", "repeat", "Repeat"}));
    EXPECT_EQ(player.lastSound, "pulse");
    runUntil(loop, [] { return false; }, std::chrono::milliseconds(100));
    EXPECT_EQ(shown.size(), 1u);
}

TEST_F(AlarmServiceTest, OneTimeAlarmRingsOnceAndMissedOnesStayQuiet)
{
    const int64_t ring = todayAt(10, 0);
    Alarm once;
    once.hour = 10;
    once.minute = 0;
    once.armedAt = ring - 3600;
    writer->setAlarm(once);
    // Missed by more than the catch-up: over, but not rung.
    Alarm missed;
    missed.hour = 7;
    missed.minute = 0;
    missed.armedAt = todayAt(6, 0);
    writer->setAlarm(missed);
    reader->load();

    nowMs = (ring - 10) * 1000;
    start();
    nowMs = (ring + 2) * 1000;
    service->check();
    ASSERT_TRUE(runUntil(loop, [this] { return !shown.empty(); }));
    EXPECT_EQ(shown.front().summary, "Alarm");

    // A wake hours later rings nothing new.
    nowMs += 5 * 3600 * 1000;
    service->check();
    runUntil(loop, [] { return false; }, std::chrono::milliseconds(100));
    EXPECT_EQ(shown.size(), 1u);
}

TEST_F(AlarmServiceTest, SwitchedOffWhileRingingStops)
{
    const int64_t ring = todayAt(10, 0);
    nowMs = (ring - 10) * 1000;
    Alarm alarm;
    alarm.hour = 10;
    alarm.minute = 0;
    alarm.repeat = alarmtime::EveryDay;
    const std::string id = writer->setAlarm(alarm);
    reader->load();
    start();
    nowMs = (ring + 1) * 1000;
    service->check();
    ASSERT_TRUE(runUntil(loop, [this] { return !shown.empty(); }));
    ASSERT_TRUE(player.playing);

    alarm.id = id;
    alarm.enabled = false;
    writer->setAlarm(alarm);
    reader->load();
    ASSERT_TRUE(runUntil(loop, [this] { return !closed.empty(); }));
    EXPECT_FALSE(player.playing);
    EXPECT_EQ(service->ringingCount(), 0u);
}

TEST_F(AlarmServiceTest, FinishedTimerRingsOnceUntilRepeated)
{
    nowMs = 1790000000000;
    RunningTimer timer;
    timer.durationMs = 60 * 1000;
    timer.endsAtMs = nowMs + 1000;
    const std::string id = writer->setTimer(timer);
    timer.id = id;
    reader->load();
    start();

    nowMs += 1500;
    service->check();
    ASSERT_TRUE(runUntil(loop, [this] { return shown.size() == 1; }));

    // Still in the store, finished: later checks, a wake from a long
    // suspend and a restarted service ring nothing more.
    nowMs += 20 * 1000;
    service->check();
    nowMs += 3 * 3600 * 1000;
    service->check();
    service.reset();
    start();
    nowMs += 1000;
    service->check();
    runUntil(loop, [] { return false; }, std::chrono::milliseconds(150));
    EXPECT_EQ(shown.size(), 1u);

    // Repeated: a new end, which rings again.
    timer.endsAtMs = nowMs + 1000;
    writer->setTimer(timer);
    reader->load();
    nowMs += 1500;
    service->check();
    ASSERT_TRUE(runUntil(loop, [this] { return shown.size() == 2; }));
    EXPECT_EQ(shown.back().actions, (std::vector<std::string>{"default", "", "stop", "Stop", "repeat", "Repeat"}));
}
