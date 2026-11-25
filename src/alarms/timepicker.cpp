// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "alarms/timepicker.h"

#include "timers/wheel.h"

#include <base/locale.h>

#include "ui/window.h"

#include <cctype>
#include <chrono>
#include <utility>
#include <string>
#include <vector>

using namespace properdesktop::ui;

namespace clockapp {

static constexpr size_t Hours = 0;
static constexpr size_t Minutes = 1;
static constexpr size_t Half = 2;

// Digits typed this long apart start a new time.
static constexpr uint64_t TypingPauseNs = 1500000000;

static uint64_t monotonicNs()
{
    using namespace std::chrono;
    return uint64_t(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

static std::string twoDigits(int value)
{
    return (value < 10 ? "0" : "") + std::to_string(value);
}

TimePicker::TimePicker(Window* window, bool twelveHour)
    : m_twelveHour(twelveHour)
{
    WheelColumn hours;
    hours.name = "picker:hours";
    for (int hour = 0; hour < (twelveHour ? 12 : 24); ++hour)
        hours.values.push_back(twelveHour ? std::to_string(hour == 0 ? 12 : hour) : twoDigits(hour));

    WheelColumn minutes;
    minutes.name = "picker:minutes";
    for (int minute = 0; minute < 60; ++minute)
        minutes.values.push_back(twoDigits(minute));

    std::vector<WheelColumn> columns{hours, minutes};
    if (twelveHour) {
        const properdesktop::base::Locale& locale = properdesktop::base::Locale::system();
        WheelColumn half;
        half.name = "picker:half";
        half.values = {locale.amText(), locale.pmText()};
        half.wraps = false;
        columns.push_back(half);
    }

    m_wheels = addChild<WheelPicker>(window, std::move(columns));
    m_wheels->setName("picker:wheels");
    m_wheels->onChanged = [this](size_t) {
        if (onChanged)
            onChanged();
    };
    m_wheels->onKey = [this](const KeyEvent& event) { return typed(event); };
}

TimePicker::~TimePicker() = default;

int TimePicker::hour() const
{
    const int hour = m_wheels->value(Hours);
    if (!m_twelveHour)
        return hour;
    return hour + (m_wheels->value(Half) == 1 ? 12 : 0);
}

int TimePicker::minute() const
{
    return m_wheels->value(Minutes);
}

void TimePicker::setTime(int hour, int minute, bool animate)
{
    if (m_twelveHour) {
        m_wheels->setValue(Hours, hour % 12, animate);
        m_wheels->setValue(Half, hour >= 12 ? 1 : 0, animate);
    } else {
        m_wheels->setValue(Hours, hour, animate);
    }
    m_wheels->setValue(Minutes, minute, animate);
}

bool TimePicker::typed(const KeyEvent& event)
{
    if (event.text.size() != 1 || (event.modifiers & ~ShiftModifier))
        return false;
    const char c = char(std::tolower(static_cast<unsigned char>(event.text[0])));
    if (m_twelveHour && (c == 'a' || c == 'p')) {
        m_wheels->setValue(Half, c == 'p' ? 1 : 0, true);
        if (onChanged)
            onChanged();
        return true;
    }
    if (c < '0' || c > '9')
        return false;

    const uint64_t now = monotonicNs();
    if (now - m_lastDigitNs > TypingPauseNs || m_digits.size() >= 4)
        m_digits.clear();
    m_lastDigitNs = now;
    m_digits.push_back(c);

    // The readings the digits so far can have, the likeliest first: "730"
    // is 7:30 and "073" 07:3 on its way to 07:30.
    const int keep = minute();
    const auto number = [this](size_t from, size_t length) { return std::stoi(m_digits.substr(from, length)); };
    std::vector<std::pair<int, int>> readings;
    if (m_digits.size() == 1) {
        readings.push_back({number(0, 1), keep});
    } else if (m_digits.size() == 2) {
        // "19" is 19:00; "73" is 7:3 on its way to 7:30.
        readings.push_back({number(0, 2), keep});
        readings.push_back({number(0, 1), number(1, 1)});
    } else if (m_digits.size() == 3) {
        if (m_digits[0] != '0')
            readings.push_back({number(0, 1), number(1, 2)});
        readings.push_back({number(0, 2), number(2, 1)});
    } else {
        readings.push_back({number(0, 2), number(2, 2)});
    }
    int hour = -1;
    int minute = keep;
    for (const auto& [h, m] : readings) {
        if (h <= 23 && m <= 59) {
            hour = h;
            minute = m;
            break;
        }
    }
    if (hour < 0) {
        // Not a time: this digit starts the next one.
        m_digits = std::string(1, c);
        hour = c - '0';
        minute = keep;
    }
    // A 12-hour hour keeps its half of the day; past 12 it is the afternoon.
    if (m_twelveHour && hour >= 1 && hour <= 12)
        hour = hour % 12 + (m_wheels->value(Half) == 1 ? 12 : 0);
    setTime(hour, minute, true);
    if (onChanged)
        onChanged();
    return true;
}

SizeF TimePicker::sizeHint() const
{
    return m_wheels->sizeHint();
}

void TimePicker::layout()
{
    m_wheels->setGeometry(localRect());
}

} // namespace clockapp
