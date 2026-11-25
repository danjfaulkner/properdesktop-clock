// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "store/store.h"

#include <base/dbus.h>
#include <base/signal.h>
#include <base/timer.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace clockapp {

class TonePlayer;

// Rings the alarms and finished timers of the store: a notification with
// its actions and a looping tone, until stopped or for a capped time.
class AlarmService
{
public:
    // Unix milliseconds; tests set their own.
    using Clock = std::function<int64_t()>;

    // Rings missed by no more than this, as over a suspend, still ring.
    static constexpr int64_t CatchUpMs = 60 * 60 * 1000;
    // The tone of an alarm stops after this long, a timer's sooner.
    static constexpr int64_t AlarmToneMs = 10 * 60 * 1000;
    static constexpr int64_t TimerToneMs = 2 * 60 * 1000;

    AlarmService(ClockStore& store, properdesktop::base::dbus::Connection* bus, TonePlayer& player, Clock clock = {});
    ~AlarmService();

    AlarmService(const AlarmService&) = delete;
    AlarmService& operator=(const AlarmService&) = delete;

    // Rings what is due and sets the timer for the next; also after the
    // machine wakes or the clock is set.
    void check();

    // What rings now, for tests.
    size_t ringingCount() const { return m_ringing.size(); }

    // For the notification's actions and tests.
    void snooze(const std::string& key);
    void stop(const std::string& key);
    void repeatTimer(const std::string& key);

private:
    struct Ring
    {
        enum class Kind {
            Alarm,
            Timer,
        };
        Kind kind = Kind::Alarm;
        // "alarm:<id>" or "timer:<id>".
        std::string key;
        std::string id;
        std::string label;
        std::string sound;
        bool snooze = false;
        int hour = 0;
        int minute = 0;
        int64_t durationMs = 0;
        int64_t startedMs = 0;
        uint32_t notification = 0;
    };

    struct Snoozed
    {
        Ring ring;
        int64_t untilMs;
    };

    int64_t now() const;
    void ring(Ring ring);
    void notify(Ring& ring);
    void closeNotification(uint32_t id);
    void updateTone();
    void actionInvoked(uint32_t id, const std::string& action);
    void openClock();

    ClockStore& m_store;
    properdesktop::base::dbus::Connection* m_bus;
    TonePlayer& m_player;
    Clock m_clock;
    properdesktop::base::dbus::Proxy m_notifications;
    properdesktop::base::Timer m_timer;
    int64_t m_checkedMs = 0;
    // Rings already rung, "<key>@<time>", kept a day against a clock set back.
    std::map<std::string, int64_t> m_rung;
    std::vector<Ring> m_ringing;
    std::vector<Snoozed> m_snoozed;
    std::string m_toneKey;
    struct Pending
    {
        properdesktop::base::dbus::PendingCall call;
        std::shared_ptr<bool> done;
    };
    std::vector<Pending> m_pending;
    properdesktop::base::dbus::Match m_actionInvoked;
    properdesktop::base::dbus::Match m_closed;
    properdesktop::base::Subscription m_changed;
};

} // namespace clockapp
