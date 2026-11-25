// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "store/store.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace properdesktop::ui {
class DialogButton;
class Label;
class RowComboBox;
class Switch;
class TextField;
class Window;
}

namespace clockapp {

class DayChips;
class TimePicker;
class TonePlayer;

// The dialog that makes or changes an alarm: its time on wheels, then
// Repeat, Label, Sound (each tone heard when picked) and Snooze, and Add or
// Save, with Delete Alarm for one that exists.
class AlarmEditor
{
public:
    using Clock = std::function<int64_t()>;

    AlarmEditor(properdesktop::ui::Window* parent, ClockStore& store, Alarm alarm, Clock now);
    ~AlarmEditor();

    AlarmEditor(const AlarmEditor&) = delete;
    AlarmEditor& operator=(const AlarmEditor&) = delete;

    void show();
    void close();
    properdesktop::ui::Window* window() const;

    // The alarm saved, by its id.
    std::function<void(const std::string& id)> onSaved;
    std::function<void(const std::string& id)> onDeleted;
    std::function<void()> onClosed;

private:
    bool isNew() const { return m_alarm.id.empty(); }
    void save();
    void remove();
    void updateCountdown();

    ClockStore& m_store;
    Alarm m_alarm;
    Clock m_now;
    std::unique_ptr<properdesktop::ui::Window> m_window;
    std::unique_ptr<TonePlayer> m_preview;
    TimePicker* m_picker = nullptr;
    properdesktop::ui::Label* m_countdown = nullptr;
    DayChips* m_days = nullptr;
    properdesktop::ui::TextField* m_label = nullptr;
    properdesktop::ui::RowComboBox* m_sound = nullptr;
    properdesktop::ui::Switch* m_snooze = nullptr;
};

} // namespace clockapp
