// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "world/clockface.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "base/transform.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/path.h"
#include "world/zonetime.h"

using namespace properdesktop;
using namespace properdesktop::ui;

namespace clockapp::world {

static constexpr double kRadians = M_PI / 180.0;

static PointF polar(PointF centre, double radius, double degrees)
{
    return {centre.x + radius * std::sin(degrees * kRadians), centre.y - radius * std::cos(degrees * kRadians)};
}

// A hand pointing up from the centre: wide at the root, narrowing to a
// rounded tip, with a short tail behind the centre.
static Path handPath(PointF centre, double length, double tail, double root, double tip)
{
    Path path;
    path.moveTo(centre.x - root / 2, centre.y + tail);
    path.lineTo(centre.x - tip / 2, centre.y - length + tip / 2);
    path.quadTo(centre.x, centre.y - length - tip / 2, centre.x + tip / 2, centre.y - length + tip / 2);
    path.lineTo(centre.x + root / 2, centre.y + tail);
    path.quadTo(centre.x, centre.y + tail + root / 2, centre.x - root / 2, centre.y + tail);
    path.close();
    return path;
}

static Path rotated(const Path& path, PointF centre, double degrees)
{
    const double c = std::cos(degrees * kRadians);
    const double s = std::sin(degrees * kRadians);
    return path.transformed(base::AffineTransform(c, s, -s, c, centre.x - c * centre.x + s * centre.y,
                                                  centre.y - s * centre.x - c * centre.y));
}

static Path circle(PointF centre, double radius)
{
    Path path;
    path.addEllipse({centre.x - radius, centre.y - radius, radius * 2, radius * 2});
    return path;
}

// A small deterministic generator, so a city's stars stay where they are.
static uint32_t nextRandom(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

ClockFace::ClockFace()
{
    for (int hour = 1; hour <= 12; ++hour) {
        Label* numeral = addChild<Label>(std::to_string(hour));
        numeral->setFontFeatures({{"tnum", 1}, {"lnum", 1}});
        numeral->setTextAlignment(Label::TextAlignment::Center);
        m_numerals[size_t(hour - 1)] = numeral;
    }
    updatePalette();
}

void ClockFace::setSky(const Sky& sky, uint32_t seed)
{
    const bool changed = std::abs(sky.daylight - m_sky.daylight) > 0.002 || sky.phase != m_sky.phase
        || std::abs(sky.twilight - m_sky.twilight) > 0.002 || seed != m_seed;
    m_sky = sky;
    m_seed = seed;
    if (changed) {
        updatePalette();
        requestPaint();
    }
}

void ClockFace::setTime(const properdesktop::base::ClockTime& time, double secondStep)
{
    if (time == m_time && secondStep == m_secondStep)
        return;
    m_time = time;
    m_secondStep = secondStep;
    requestPaint();
}

void ClockFace::updatePalette()
{
    m_palette = SkyPalette::of(m_sky);
    for (Label* numeral : m_numerals)
        numeral->setColor(m_palette.numerals);
}

void ClockFace::layout()
{
    const double size = std::min(geometry().width, geometry().height);
    const double radius = size / 2;
    const PointF centre{geometry().width / 2, geometry().height / 2};
    // Numerals in proportion to the face: lighter as they grow.
    const double fontSize = std::clamp(std::round(radius * 0.2), 9.0, 15.0);
    for (size_t i = 0; i < m_numerals.size(); ++i) {
        Label* numeral = m_numerals[i];
        numeral->setFontSize(fontSize);
        numeral->setWeight(fontSize >= 13 ? 500 : 600);
        const SizeF hint = numeral->sizeHint();
        const PointF at = polar(centre, radius * 0.72, double(i + 1) * 30);
        numeral->setGeometry({std::round(at.x - hint.width / 2), std::round(at.y - hint.height / 2), std::ceil(hint.width),
                              std::ceil(hint.height)});
    }
}

void ClockFace::paint(Painter& painter)
{
    const double size = std::min(geometry().width, geometry().height);
    const double radius = size / 2;
    const PointF centre{geometry().width / 2, geometry().height / 2};
    const RectF dial{centre.x - radius, centre.y - radius, size, size};
    const SkyPalette& palette = m_palette;

    // Depth: the face sits a little above the card.
    painter.drawShadow(dial, radius, std::max(2.0, radius * 0.06), Color{0, 0, 0, 110},
                       {0, std::max(1.0, radius * 0.025)});
    painter.fillPath(circle(centre, radius), LinearGradient::vertical(dial, palette.dialTop, palette.dialBottom));

    if (palette.stars > 0.01) {
        uint32_t state = m_seed | 1;
        for (int i = 0; i < 26; ++i) {
            const double angle = double(nextRandom(state) % 3600) / 10.0;
            const double distance = radius * (0.18 + 0.7 * double(nextRandom(state) % 1000) / 1000.0);
            const double twinkle = 0.35 + 0.65 * double(nextRandom(state) % 100) / 100.0;
            const double dot = radius * (0.008 + 0.012 * double(nextRandom(state) % 100) / 100.0);
            const PointF at = polar(centre, distance, angle);
            painter.fillPath(circle(at, std::max(dot, 0.5)),
                             Color{255, 255, 255, uint8_t(std::lround(170 * twinkle * palette.stars))});
        }
    }

    // A fine inner rim catches the light at the top.
    painter.strokePath(circle(centre, radius - 0.5), palette.rim, 1);

    for (int minute = 0; minute < 60; ++minute) {
        const bool hour = minute % 5 == 0;
        const double outer = radius * 0.94;
        const double inner = radius * (hour ? 0.86 : 0.905);
        painter.drawLine(polar(centre, inner, minute * 6), polar(centre, outer, minute * 6), palette.ticks,
                         (hour ? 1.6 : 0.9) * painter.scale());
    }

    const HandAngles angles = handAngles(m_time);
    const PointF shadowOffset{0, std::max(0.8, radius * 0.02)};
    const Path hourHand = rotated(handPath(centre, radius * 0.5, radius * 0.08, radius * 0.075, radius * 0.05),
                                  centre, angles.hour);
    const Path minuteHand = rotated(handPath(centre, radius * 0.8, radius * 0.08, radius * 0.06, radius * 0.035),
                                    centre, angles.minute);
    painter.fillPath(hourHand.translated(shadowOffset.x, shadowOffset.y), palette.handShadow);
    painter.fillPath(minuteHand.translated(shadowOffset.x, shadowOffset.y), palette.handShadow);
    painter.fillPath(hourHand, palette.hands);
    painter.fillPath(minuteHand, palette.hands);
    painter.fillPath(circle(centre, radius * 0.06), palette.hands);

    // The second hand steps from the last second to this one.
    const double second = angles.second - 6 * (1 - m_secondStep);
    const double width = std::max(1.0, radius * 0.018);
    Path secondHand;
    secondHand.moveTo(centre.x, centre.y + radius * 0.2);
    secondHand.lineTo(centre.x, centre.y - radius * 0.88);
    const Path turned = rotated(secondHand, centre, second);
    painter.strokePath(turned.translated(shadowOffset.x, shadowOffset.y), palette.handShadow, width, LineCap::Round);
    painter.strokePath(turned, kSecondHand, width, LineCap::Round);
    const PointF weight = polar(centre, -radius * 0.16, second);
    painter.fillPath(circle(weight, radius * 0.04), kSecondHand);
    painter.fillPath(circle(centre, radius * 0.045), kSecondHand);
    painter.fillPath(circle(centre, radius * 0.018), palette.dialBottom);
}

} // namespace clockapp::world
