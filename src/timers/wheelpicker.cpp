// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "timers/wheelpicker.h"

#include <base/i18n.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace properdesktop::ui;
using properdesktop::i18n::tr;

namespace clockapp {

static WheelColumn numbers(int count, std::string unit, std::string name)
{
    WheelColumn column;
    for (int i = 0; i < count; ++i)
        column.values.push_back(std::to_string(i));
    column.unit = std::move(unit);
    column.name = std::move(name);
    return column;
}

DurationPicker::DurationPicker(Window* window)
    : WheelPicker(window, {numbers(24, tr("hours"), "timers:hours"), numbers(60, tr("min"), "timers:minutes"),
                           numbers(60, tr("sec"), "timers:seconds")})
{
    setName("timers:picker");
}

int64_t DurationPicker::durationMs() const
{
    return (int64_t(value(0)) * 3600 + value(1) * 60 + value(2)) * 1000;
}

void DurationPicker::setDurationMs(int64_t ms, bool animate)
{
    const int64_t total = std::clamp<int64_t>(ms / 1000, 0, 23 * 3600 + 59 * 60 + 59);
    setValue(0, int(total / 3600), animate);
    setValue(1, int(total / 60 % 60), animate);
    setValue(2, int(total % 60), animate);
}

SizeF DurationPicker::sizeHint() const
{
    return {360, WheelPicker::sizeHint().height};
}

} // namespace clockapp
