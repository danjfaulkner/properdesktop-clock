// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "store/store.h"

#include <base/dbus.h>
#include <base/file.h>
#include <base/json.h>
#include <base/paths.h>
#include <base/uuid.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>

namespace json = properdesktop::base::json;
namespace dbus = properdesktop::base::dbus;

namespace clockapp {

static const char* const ServiceName = "org.properdesktop.ClockAlarms";

std::string clockstore::defaultPath()
{
    return pathIn(properdesktop::base::paths::dataHome() + "/properdesktop/clock");
}

std::string clockstore::pathIn(const std::string& folder)
{
    return folder + "/alarms.json";
}

const std::vector<std::string>& clockstore::soundNames()
{
    static const std::vector<std::string> names{"dawn", "chime", "pulse", "ripple", "glow"};
    return names;
}

static json::Value repeatValue(uint8_t repeat)
{
    json::Value days{json::Value::Array()};
    for (int day = 0; day < 7; ++day) {
        if (repeat & (1 << day))
            days.append(day);
    }
    return days;
}

std::string clockstore::serialize(const ClockData& data)
{
    json::Value alarms{json::Value::Array()};
    for (const Alarm& alarm : data.alarms) {
        alarms.append(json::Value::object({
            {"id", alarm.id},
            {"hour", alarm.hour},
            {"minute", alarm.minute},
            {"repeat", repeatValue(alarm.repeat)},
            {"label", alarm.label},
            {"sound", alarm.sound},
            {"snooze", alarm.snooze},
            {"enabled", alarm.enabled},
            {"armedAt", alarm.armedAt},
        }));
    }

    json::Value timers{json::Value::Array()};
    for (const RunningTimer& timer : data.timers) {
        timers.append(json::Value::object({
            {"id", timer.id},
            {"label", timer.label},
            {"duration", timer.durationMs},
            {"endsAt", timer.endsAtMs},
            {"remaining", timer.remainingMs},
            {"sound", timer.sound},
        }));
    }

    json::Value root = json::Value::object({{"version", 1}, {"alarms", alarms}, {"timers", timers}});
    return root.serialize(false) + "\n";
}

std::optional<ClockData> clockstore::parse(std::string_view text)
{
    const std::optional<json::Value> root = json::Value::parse(text);
    if (!root || !root->isObject())
        return std::nullopt;

    ClockData data;
    const json::Value& alarms = (*root)["alarms"];
    if (alarms.isArray()) {
        for (const json::Value& item : alarms.toArray()) {
            Alarm alarm;
            alarm.id = item["id"].toString();
            alarm.hour = int(item["hour"].toInt(-1));
            alarm.minute = int(item["minute"].toInt(-1));
            if (alarm.id.empty() || alarm.hour < 0 || alarm.hour > 23 || alarm.minute < 0 || alarm.minute > 59)
                continue;
            if (item["repeat"].isArray()) {
                for (const json::Value& day : item["repeat"].toArray()) {
                    const int64_t d = day.toInt(-1);
                    if (d >= 0 && d < 7)
                        alarm.repeat |= uint8_t(1 << d);
                }
            }
            alarm.label = item["label"].toString();
            alarm.sound = item["sound"].toString();
            alarm.snooze = item["snooze"].toBool(true);
            alarm.enabled = item["enabled"].toBool(true);
            alarm.armedAt = item["armedAt"].toInt(0);
            data.alarms.push_back(std::move(alarm));
        }
    }

    const json::Value& timers = (*root)["timers"];
    if (timers.isArray()) {
        for (const json::Value& item : timers.toArray()) {
            RunningTimer timer;
            timer.id = item["id"].toString();
            timer.durationMs = item["duration"].toInt(0);
            if (timer.id.empty() || timer.durationMs <= 0)
                continue;
            timer.label = item["label"].toString();
            timer.endsAtMs = std::max<int64_t>(0, item["endsAt"].toInt(0));
            timer.remainingMs = std::clamp<int64_t>(item["remaining"].toInt(0), 0, timer.durationMs);
            timer.sound = item["sound"].toString();
            data.timers.push_back(std::move(timer));
        }
    }
    return data;
}

void clockstore::ensureAlarmService(dbus::Connection* bus)
{
    if (!bus)
        return;
    dbus::Proxy daemon(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus");
    daemon.send("StartServiceByName", std::string(ServiceName), uint32_t(0));
}

ClockStore::ClockStore(std::string path)
    : m_path(std::move(path))
{
    load();
}

ClockStore& ClockStore::inFolder(const std::string& folder)
{
    static std::map<std::string, std::unique_ptr<ClockStore>> stores;
    std::unique_ptr<ClockStore>& store = stores[folder];
    if (!store)
        store = std::make_unique<ClockStore>(clockstore::pathIn(folder));
    return *store;
}

void ClockStore::load()
{
    ClockData data;
    std::error_code error;
    if (std::filesystem::exists(m_path, error)) {
        const std::optional<std::string> text = properdesktop::base::file::read(m_path);
        std::optional<ClockData> parsed = text ? clockstore::parse(*text) : std::nullopt;
        if (!parsed)
            return;
        data = std::move(*parsed);
    }
    if (data == m_data)
        return;
    m_data = std::move(data);
    changed.emit();
}

void ClockStore::watch()
{
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(m_path).parent_path(), error);
    m_watch = properdesktop::base::FileWatch::file(m_path, [this] { load(); });
}

const Alarm* ClockStore::alarm(const std::string& id) const
{
    const auto it = std::find_if(m_data.alarms.begin(), m_data.alarms.end(),
                                 [&id](const Alarm& alarm) { return alarm.id == id; });
    return it == m_data.alarms.end() ? nullptr : &*it;
}

const RunningTimer* ClockStore::timer(const std::string& id) const
{
    const auto it = std::find_if(m_data.timers.begin(), m_data.timers.end(),
                                 [&id](const RunningTimer& timer) { return timer.id == id; });
    return it == m_data.timers.end() ? nullptr : &*it;
}

std::string ClockStore::setAlarm(Alarm alarm)
{
    if (alarm.id.empty())
        alarm.id = properdesktop::base::Uuid::generate().toString();
    const std::string id = alarm.id;
    auto it = std::find_if(m_data.alarms.begin(), m_data.alarms.end(),
                           [&id](const Alarm& other) { return other.id == id; });
    if (it == m_data.alarms.end())
        m_data.alarms.push_back(std::move(alarm));
    else
        *it = std::move(alarm);
    save();
    return id;
}

void ClockStore::removeAlarm(const std::string& id)
{
    if (std::erase_if(m_data.alarms, [&id](const Alarm& alarm) { return alarm.id == id; }) > 0)
        save();
}

std::string ClockStore::setTimer(RunningTimer timer)
{
    if (timer.id.empty())
        timer.id = properdesktop::base::Uuid::generate().toString();
    const std::string id = timer.id;
    auto it = std::find_if(m_data.timers.begin(), m_data.timers.end(),
                           [&id](const RunningTimer& other) { return other.id == id; });
    if (it == m_data.timers.end())
        m_data.timers.push_back(std::move(timer));
    else
        *it = std::move(timer);
    save();
    return id;
}

void ClockStore::removeTimer(const std::string& id)
{
    if (std::erase_if(m_data.timers, [&id](const RunningTimer& timer) { return timer.id == id; }) > 0)
        save();
}

void ClockStore::save()
{
    properdesktop::base::file::writeAtomically(m_path, clockstore::serialize(m_data));
    changed.emit();
}

} // namespace clockapp
