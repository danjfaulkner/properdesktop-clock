// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/geometry.h"
#include "ui/theme.h"

#include <cmath>

// Sizes and colours every page shares, in logical pixels.
namespace clockapp::style {

inline constexpr double windowWidth = 760;
inline constexpr double windowHeight = 560;
inline constexpr double windowMinimumWidth = 600;
inline constexpr double windowMinimumHeight = 440;

// The frosted header: the window buttons' line, the tab switch and the
// page's buttons.
inline constexpr double headerHeight = 46;
inline constexpr double headerButtonSize = 32;

// A page's side margin and the cards' radius and gap.
inline constexpr double pagePadding = 20;
inline constexpr double cardRadius = 12;
inline constexpr double cardGap = 12;

// Point sizes in logical pixels, four thirds of a pixel a point.
inline double points(double size)
{
    return size * 4 / 3;
}

inline properdesktop::ui::Color withAlpha(properdesktop::ui::Color color, double alpha)
{
    return color.withAlpha(alpha * color.a / 255.0);
}

properdesktop::ui::Color text();
properdesktop::ui::Color secondaryText();
properdesktop::ui::Color separator();
// The page under the header and the cards on it.
properdesktop::ui::Color page();
properdesktop::ui::Color card();
// The track a control on the page or header gets.
properdesktop::ui::Color controlTint();
properdesktop::ui::Color accent();
// Start buttons and running states.
properdesktop::ui::Color green();
// Stop, cancel and delete.
properdesktop::ui::Color red();

// On the device pixel grid of the scale.
inline double snap(double value, double scale)
{
    return std::round(value * scale) / scale;
}

// Where an item of this length is centred in its parent: each half rounded
// on its own, so odd lengths lean the same way everywhere.
inline double centred(double parent, double item)
{
    return std::round(parent / 2) - std::round(item / 2);
}

} // namespace clockapp::style
