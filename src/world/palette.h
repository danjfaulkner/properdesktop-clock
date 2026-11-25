// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/geometry.h"
#include "world/sky.h"

namespace clockapp::world {

// The colours of a city's face and card at its sky, blended continuously
// from night through twilight to day, for the window's dark page.
struct SkyPalette
{
    // The face's dial, top to bottom.
    properdesktop::ui::Color dialTop;
    properdesktop::ui::Color dialBottom;
    properdesktop::ui::Color rim;
    properdesktop::ui::Color ticks;
    properdesktop::ui::Color numerals;
    properdesktop::ui::Color hands;
    properdesktop::ui::Color handShadow;
    // Over the card's top, fading into the card.
    properdesktop::ui::Color glow;
    // Stars over the dial, 0 by day.
    double stars = 0;

    static SkyPalette of(const Sky& sky);
};

properdesktop::ui::Color mix(properdesktop::ui::Color a, properdesktop::ui::Color b, double t);

// The second hand, the same in every sky.
inline constexpr properdesktop::ui::Color kSecondHand = properdesktop::ui::Color::rgb(0xFF7A1A);

} // namespace clockapp::world
