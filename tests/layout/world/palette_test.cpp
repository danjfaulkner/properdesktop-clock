// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "world/palette.h"
#include "world/sky.h"

using properdesktop::ui::Color;
using namespace clockapp::world;

static double luminance(Color color)
{
    const auto linear = [](uint8_t channel) {
        const double value = channel / 255.0;
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.r) + 0.7152 * linear(color.g) + 0.0722 * linear(color.b);
}

// The ink composited over the dial, against the dial.
static double contrast(Color ink, Color dial)
{
    const Color over = mix(dial, Color{ink.r, ink.g, ink.b, 255}, ink.a / 255.0);
    const double a = luminance(over) + 0.05;
    const double b = luminance(dial) + 0.05;
    return std::max(a, b) / std::min(a, b);
}

TEST(SkyPalette, NumeralsAndHandsReadOverTheWholeDialThroughTwilight)
{
    // Dawn from 5:00 to 7:00 and dusk from 17:00 to 19:00, minute by minute.
    for (const int startHour : {5, 17}) {
        for (int minutes = 0; minutes <= 120; ++minutes) {
            const Sky sky = Sky::fromHour(startHour + minutes / 60, minutes % 60);
            const SkyPalette palette = SkyPalette::of(sky);
            SCOPED_TRACE(testing::Message() << startHour + minutes / 60 << ":" << minutes % 60);
            for (const Color dial : {palette.dialTop, palette.dialBottom}) {
                EXPECT_GE(contrast(palette.numerals, dial), 3.0);
                EXPECT_GE(contrast(palette.hands, dial), 3.0);
            }
        }
    }
}

TEST(SkyPalette, DayAndNightKeepTheirInk)
{
    EXPECT_EQ(SkyPalette::of(Sky::fromHour(12, 0)).hands, Color::rgb(0x1C1D21));
    EXPECT_EQ(SkyPalette::of(Sky::fromHour(0, 0)).hands, Color::rgb(0xF4F6FA));
}
