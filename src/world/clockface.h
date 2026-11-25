// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <array>
#include <cstdint>

#include "base/locale.h"
#include "ui/view.h"
#include "world/palette.h"
#include "world/sky.h"

namespace properdesktop::ui {
class Label;
}

namespace clockapp::world {

// A drawn analog face: a dial in the colours of its city's sky, with stars by
// night, twelve numerals, minute ticks, tapered hour and minute hands over a
// soft shadow, and an orange second hand with a counterweight that steps
// each second with a slight settle.
class ClockFace : public properdesktop::ui::View
{
public:
    ClockFace();

    // `seed` scatters the stars, so each city keeps its own sky.
    void setSky(const Sky& sky, uint32_t seed);
    // The wall clock there; the second hand stands at `secondStep` of the
    // way from the previous second to this one (1 at rest).
    void setTime(const properdesktop::base::ClockTime& time, double secondStep);
    const SkyPalette& palette() const { return m_palette; }

protected:
    void layout() override;
    void paint(properdesktop::ui::Painter& painter) override;

private:
    void updatePalette();

    std::array<properdesktop::ui::Label*, 12> m_numerals{};
    Sky m_sky;
    uint32_t m_seed = 0;
    SkyPalette m_palette;
    properdesktop::base::ClockTime m_time;
    double m_secondStep = 1;
};

} // namespace clockapp::world
