// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "world/palette.h"

#include <algorithm>
#include <cmath>

using properdesktop::ui::Color;

namespace clockapp::world {

Color mix(Color a, Color b, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    const auto channel = [t](uint8_t x, uint8_t y) { return uint8_t(std::lround(x + (y - x) * t)); };
    return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a)};
}

// The WCAG relative luminance.
static double luminance(Color color)
{
    const auto linear = [](uint8_t channel) {
        const double value = channel / 255.0;
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.r) + 0.7152 * linear(color.g) + 0.0722 * linear(color.b);
}

static double contrast(Color a, Color b)
{
    const double first = luminance(a) + 0.05;
    const double second = luminance(b) + 0.05;
    return std::max(first, second) / std::min(first, second);
}

SkyPalette SkyPalette::of(const Sky& sky)
{
    // Day is a soft paper white that does not glare on the dark page, night a
    // deep indigo just lighter than the card; sunrise warms towards peach
    // and lavender, sunset towards a deep terracotta over violet, both
    // dark enough for one ink to read over the whole dial.
    const Color dayTop = Color::rgb(0xEEF1F5);
    const Color dayBottom = Color::rgb(0xD2D8E1);
    const Color nightTop = Color::rgb(0x27304F);
    const Color nightBottom = Color::rgb(0x0E1328);
    const bool morning = sky.phase == SkyPhase::Dawn;
    const Color warmTop = morning ? Color::rgb(0xF9D9C4) : Color::rgb(0xA85A3C);
    const Color warmBottom = morning ? Color::rgb(0xB9C3E6) : Color::rgb(0x6A5C9A);

    // The glow is strongest at sunrise and sunset, the middle of twilight.
    const double glow = (sky.phase == SkyPhase::Dawn || sky.phase == SkyPhase::Dusk)
        ? std::sin(M_PI * sky.twilight) : 0.0;

    SkyPalette palette;
    palette.dialTop = mix(mix(nightTop, dayTop, sky.daylight), warmTop, glow * 0.8);
    palette.dialBottom = mix(mix(nightBottom, dayBottom, sky.daylight), warmBottom, glow * 0.8);

    // Whichever ink stands out more at the dial's weaker end; a blend of the
    // two would be a grey as light as a mid-toned dial.
    const Color lightInk = Color::rgb(0xF4F6FA);
    const Color darkInk = Color::rgb(0x1C1D21);
    const auto weakest = [&](Color ink) {
        return std::min(contrast(ink, palette.dialTop), contrast(ink, palette.dialBottom));
    };
    const double ink = weakest(darkInk) > weakest(lightInk) ? 1 : 0;
    palette.hands = mix(lightInk, darkInk, ink);
    palette.ticks = mix(Color{244, 246, 250, 150}, Color{28, 29, 33, 120}, ink);
    palette.numerals = mix(Color{244, 246, 250, 230}, Color{28, 29, 33, 235}, ink);
    palette.handShadow = Color{0, 0, 0, uint8_t(std::lround(mix(Color{0, 0, 0, 110}, Color{0, 0, 0, 50}, ink).a))};
    palette.rim = mix(Color{255, 255, 255, 34}, Color{255, 255, 255, 90}, ink);

    const Color daySky = Color{120, 165, 230, 30};
    const Color nightSky = Color{80, 90, 180, 34};
    const Color warmSky = morning ? Color{250, 165, 135, 46} : Color{245, 135, 75, 50};
    palette.glow = mix(mix(nightSky, daySky, sky.daylight), warmSky, glow);
    palette.stars = std::clamp(1 - sky.daylight * 2.5, 0.0, 1.0);
    return palette;
}

} // namespace clockapp::world
