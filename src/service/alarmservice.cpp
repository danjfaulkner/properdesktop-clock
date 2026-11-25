// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "service/alarmservice.h"
#include "alarms/alarmtime.h"
#include "alarms/toneplayer.h"

#include <base/i18n.h>
#include <base/locale.h>

#include <algorithm>
#include <chrono>

namespace dbus = properdesktop::base::dbus;
using properdesktop::i18n::format;
using properdesktop::i18n::tr;
using properdesktop::i18n::trc;
using properdesktop::i18n::trn;

namespace clockapp {

// Checked at least this often, so a clock set or a wake is noticed soon.
static constexpr int64_t LongestWaitMs = 30 * 1000;
static constexpr int64_t DayMs = 24 * 60 * 60 * 1000;

static int64_t systemNow()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

static std::string escaped(const std::string& text)
{
    std::string result;
    for (char c : text) {
        switch (c) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        default:
            result.push_back(c);
        }
    }
    return result;
}

static std::string durationText(int64_t ms)
{
    const int64_t seconds = (ms + 500) / 1000;
    const int64_t hours = seconds / 3600;
    const int64_t minutes = seconds / 60 % 60;
    const int64_t rest = seconds % 60;
    std::vector<std::string> parts;
    if (hours > 0)
        parts.push_back(trn("{n} hour", "{n} hours", int(hours)));
    if (minutes > 0)
        parts.push_back(trn("{n} minute", "{n} minutes", int(minutes)));
    if (rest > 0 || parts.empty())
        parts.push_back(trn("{n} second", "{n} seconds", int(rest)));
    std::string text;
    for (const std::string& part : parts)
        text += (text.empty() ? "" : " ") + part;
    return text;
}

AlarmService::AlarmService(ClockStore& store, dbus::Connection* bus, TonePlayer& player, Clock clock)
    : m_store(store)
    , m_bus(bus)
    , m_player(player)
    , m_clock(std::move(clock))
    , m_notifications(bus, "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                      "org.freedesktop.Notifications")
{
    m_timer.setSingleShot(true);
    m_timer.onTimeout = [this] { check(); };
    m_changed = m_store.changed.connect([this] { check(); });
    // A ring of the last moments before the service started, as when the
    // application starts it for a timer about to end, still rings.
    m_checkedMs = now() - 5000;

    m_actionInvoked = m_notifications.onSignal("ActionInvoked", [this](const dbus::Message& message) {
        uint32_t id = 0;
        std::string action;
        if (message.read(id, action))
            actionInvoked(id, action);
    });
    m_closed = m_notifications.onSignal("NotificationClosed", [this](const dbus::Message& message) {
        uint32_t id = 0;
        uint32_t reason = 0;
        if (!message.read(id, reason))
            return;
        const auto it = std::find_if(m_ringing.begin(), m_ringing.end(),
                                     [id](const Ring& ring) { return ring.notification == id; });
        // Dismissed on screen: as good as Stop.
        if (it != m_ringing.end())
            stop(it->key);
    });
}

AlarmService::~AlarmService()
{
    m_player.stop();
}

int64_t AlarmService::now() const
{
    return m_clock ? m_clock() : systemNow();
}

void AlarmService::check()
{
    const int64_t nowMs = now();
    // The clock was set back: what lies ahead from here rings again.
    if (nowMs < m_checkedMs)
        m_checkedMs = nowMs;

    const int64_t from = m_checkedMs;
    std::vector<Ring> due;
    for (const Alarm& alarm : m_store.alarms()) {
        const std::optional<int64_t> next = alarmtime::nextRing(alarm, from / 1000);
        if (!next || *next * 1000 > nowMs)
            continue;
        // Over the window a repeating alarm can come round more than once;
        // the latest of its rings is the one that counts.
        int64_t at = *next;
        while (true) {
            const std::optional<int64_t> later = alarmtime::nextRing(alarm, at);
            if (!later || *later * 1000 > nowMs)
                break;
            at = *later;
        }
        const std::string key = "alarm:" + alarm.id;
        if (!m_rung.emplace(key + "@" + std::to_string(at), at * 1000).second)
            continue;
        if (nowMs - at * 1000 > CatchUpMs)
            continue;
        Ring ring;
        ring.kind = Ring::Kind::Alarm;
        ring.key = key;
        ring.id = alarm.id;
        ring.label = alarm.label;
        ring.sound = alarm.sound;
        ring.snooze = alarm.snooze;
        ring.hour = alarm.hour;
        ring.minute = alarm.minute;
        due.push_back(std::move(ring));
    }
    for (const RunningTimer& timer : m_store.timers()) {
        if (!timer.isRunning() || timer.endsAtMs > nowMs || timer.endsAtMs <= from)
            continue;
        const std::string key = "timer:" + timer.id;
        if (!m_rung.emplace(key + "@" + std::to_string(timer.endsAtMs), timer.endsAtMs).second)
            continue;
        if (nowMs - timer.endsAtMs > CatchUpMs)
            continue;
        Ring ring;
        ring.kind = Ring::Kind::Timer;
        ring.key = key;
        ring.id = timer.id;
        ring.label = timer.label;
        ring.sound = timer.sound;
        ring.durationMs = timer.durationMs;
        due.push_back(std::move(ring));
    }

    // A snooze of an alarm since switched off or deleted is dropped.
    std::erase_if(m_snoozed, [this](const Snoozed& snoozed) {
        const Alarm* alarm = m_store.alarm(snoozed.ring.id);
        return !alarm || !alarm->enabled;
    });
    for (auto it = m_snoozed.begin(); it != m_snoozed.end();) {
        if (it->untilMs <= nowMs) {
            due.push_back(it->ring);
            it = m_snoozed.erase(it);
        } else {
            ++it;
        }
    }
    // Switched off or deleted in the application while ringing.
    for (const Ring& ringing : std::vector<Ring>(m_ringing)) {
        if (ringing.kind != Ring::Kind::Alarm)
            continue;
        const Alarm* alarm = m_store.alarm(ringing.id);
        if (!alarm || !alarm->enabled)
            stop(ringing.key);
    }

    m_checkedMs = nowMs;
    std::erase_if(m_rung, [nowMs](const auto& item) { return item.second < nowMs - DayMs; });

    for (Ring& ring : due) {
        ring.startedMs = nowMs;
        this->ring(std::move(ring));
    }
    updateTone();

    int64_t next = nowMs + LongestWaitMs;
    for (const Alarm& alarm : m_store.alarms()) {
        if (const std::optional<int64_t> at = alarmtime::nextRing(alarm, nowMs / 1000))
            next = std::min(next, *at * 1000);
    }
    for (const RunningTimer& timer : m_store.timers()) {
        if (timer.isRunning() && timer.endsAtMs > nowMs)
            next = std::min(next, timer.endsAtMs);
    }
    for (const Snoozed& snoozed : m_snoozed)
        next = std::min(next, snoozed.untilMs);
    for (const Ring& ring : m_ringing) {
        const int64_t cap = ring.kind == Ring::Kind::Alarm ? AlarmToneMs : TimerToneMs;
        if (ring.startedMs + cap > nowMs)
            next = std::min(next, ring.startedMs + cap);
    }
    m_timer.start(std::chrono::milliseconds(std::clamp<int64_t>(next - nowMs, 1, LongestWaitMs)));
}

void AlarmService::ring(Ring ring)
{
    // The same alarm or timer ringing again replaces its notification.
    const auto it = std::find_if(m_ringing.begin(), m_ringing.end(),
                                 [&ring](const Ring& other) { return other.key == ring.key; });
    if (it != m_ringing.end()) {
        ring.notification = it->notification;
        m_ringing.erase(it);
    }
    m_ringing.push_back(std::move(ring));
    notify(m_ringing.back());
}

void AlarmService::notify(Ring& ring)
{
    std::string summary;
    std::string body;
    std::vector<std::string> actions{"default", ""};
    if (ring.kind == Ring::Kind::Alarm) {
        summary = ring.label.empty() ? tr("Alarm") : ring.label;
        body = properdesktop::base::Locale::system().toString(properdesktop::base::ClockTime{ring.hour, ring.minute},
                                                         properdesktop::base::Locale::FormatType::Short);
        if (ring.snooze) {
            actions.push_back("snooze");
            actions.push_back(tr("Snooze"));
        }
        actions.push_back("stop");
        actions.push_back(tr("Stop"));
    } else {
        summary = ring.label.empty() ? tr("Timer") : ring.label;
        // TRANSLATORS: a timer's notification, such as "5 minutes are up".
        body = format(tr("{duration} done"), {{"duration", durationText(ring.durationMs)}});
        actions.push_back("stop");
        actions.push_back(tr("Stop"));
        actions.push_back("repeat");
        actions.push_back(trc("button", "Repeat"));
    }
    // Critical: the banner stays up until answered, also under Do Not Disturb.
    const dbus::VariantMap hints{
        {"urgency", dbus::Variant(uint8_t(2))},
        {"category", dbus::Variant(std::string(ring.kind == Ring::Kind::Alarm ? "alarm" : "timer"))},
        {"desktop-entry", dbus::Variant(std::string("properdesktop-clock"))},
    };

    // The reply brings the id that the actions arrive with.
    std::erase_if(m_pending, [](const Pending& pending) { return *pending.done; });
    auto done = std::make_shared<bool>(false);
    const std::string key = ring.key;
    const uint32_t replaces = ring.notification;
    dbus::PendingCall call = m_notifications.callAsync("Notify", [this, key, done](const dbus::Reply& reply) {
        *done = true;
        const uint32_t id = reply.value<uint32_t>(0).value_or(0);
        const auto it = std::find_if(m_ringing.begin(), m_ringing.end(),
                                     [&key](const Ring& ring) { return ring.key == key; });
        if (it != m_ringing.end())
            it->notification = id;
        else if (id != 0)
            closeNotification(id);
    }, std::string("Clock"), replaces, std::string("properdesktop-clock"), summary, escaped(body), actions, hints,
       int32_t(0));
    m_pending.push_back({std::move(call), done});
}

void AlarmService::closeNotification(uint32_t id)
{
    if (id != 0)
        m_notifications.send("CloseNotification", id);
}

void AlarmService::updateTone()
{
    const int64_t nowMs = now();
    const Ring* loudest = nullptr;
    for (const Ring& ring : m_ringing) {
        const int64_t cap = ring.kind == Ring::Kind::Alarm ? AlarmToneMs : TimerToneMs;
        if (nowMs - ring.startedMs < cap)
            loudest = &ring;
    }
    if (!loudest) {
        m_toneKey.clear();
        m_player.stop();
        return;
    }
    if (loudest->key == m_toneKey && m_player.isPlaying())
        return;
    m_toneKey = loudest->key;
    // An alarm wakes gently; a timer is heard at once.
    TonePlayer::Options options;
    options.rise = loudest->kind == Ring::Kind::Alarm ? 20 : 3;
    m_player.play(loudest->sound, options);
}

void AlarmService::snooze(const std::string& key)
{
    const auto it = std::find_if(m_ringing.begin(), m_ringing.end(),
                                 [&key](const Ring& ring) { return ring.key == key; });
    if (it == m_ringing.end() || it->kind != Ring::Kind::Alarm)
        return;
    Ring ring = *it;
    m_ringing.erase(it);
    closeNotification(ring.notification);
    ring.notification = 0;
    m_snoozed.push_back({ring, now() + alarmtime::SnoozeMinutes * 60 * 1000});
    check();
}

void AlarmService::stop(const std::string& key)
{
    const auto it = std::find_if(m_ringing.begin(), m_ringing.end(),
                                 [&key](const Ring& ring) { return ring.key == key; });
    if (it == m_ringing.end())
        return;
    const uint32_t id = it->notification;
    m_ringing.erase(it);
    closeNotification(id);
    updateTone();
}

void AlarmService::repeatTimer(const std::string& key)
{
    const auto it = std::find_if(m_ringing.begin(), m_ringing.end(),
                                 [&key](const Ring& ring) { return ring.key == key; });
    if (it == m_ringing.end() || it->kind != Ring::Kind::Timer)
        return;
    const std::string id = it->id;
    stop(key);
    // Only the application writes the store; the bus starts it if need be.
    if (m_bus) {
        dbus::Proxy clock(m_bus, "org.properdesktop.Clock", "/Clock", "org.properdesktop.Clock");
        clock.send("RepeatTimer", id);
    }
}

void AlarmService::openClock()
{
    if (!m_bus)
        return;
    dbus::Proxy clock(m_bus, "org.properdesktop.Clock", "/Clock", "org.properdesktop.Clock");
    clock.send("open", std::vector<std::string>(), std::string());
}

void AlarmService::actionInvoked(uint32_t id, const std::string& action)
{
    const auto it = std::find_if(m_ringing.begin(), m_ringing.end(),
                                 [id](const Ring& ring) { return ring.notification == id; });
    if (it == m_ringing.end())
        return;
    const std::string key = it->key;
    if (action == "snooze") {
        snooze(key);
    } else if (action == "repeat") {
        repeatTimer(key);
    } else if (action == "default") {
        stop(key);
        openClock();
    } else {
        stop(key);
    }
}

} // namespace clockapp
