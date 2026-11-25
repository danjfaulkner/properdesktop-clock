// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "timers/wheel.h"

#include <cstdint>
#include <functional>

namespace clockapp {

// Hours, minutes and seconds on three drums.
class DurationPicker : public WheelPicker
{
public:
    explicit DurationPicker(properdesktop::ui::Window* window);

    int64_t durationMs() const;
    void setDurationMs(int64_t ms, bool animate = false);

    properdesktop::ui::SizeF sizeHint() const override;
};

} // namespace clockapp
