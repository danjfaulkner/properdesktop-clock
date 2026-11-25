// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "timers/timerring.h"

#include "stopwatch/palette.h"
#include "style.h"

#include "ui/painter.h"
#include "ui/path.h"
#include "ui/theme.h"

#include <algorithm>
#include <cmath>

using namespace properdesktop::ui;

namespace clockapp {

static constexpr double Pi = 3.14159265358979323846;
// The bloom: three rings, one every BloomPeriod seconds.
static constexpr double BloomPeriod = 0.8;
static constexpr int BloomCount = 3;
static constexpr double BloomReach = 22;

static Color timerColor()
{
    return palette::orange;
}

static PointF onCircle(PointF centre, double radius, double turn)
{
    const double angle = turn * 2 * Pi;
    return {centre.x + radius * std::sin(angle), centre.y - radius * std::cos(angle)};
}

TimerRing::TimerRing(Window* window)
    : m_reveal(std::make_unique<Glide>(window, 650, 1))
    , m_pause(std::make_unique<Glide>(window, 240, 0))
    , m_bloom(std::make_unique<FrameTicker>(window))
{
    m_reveal->setRestDistance(0.004);
    m_pause->setRestDistance(0.01);
    m_reveal->onChanged = [this] { requestPaint(); };
    m_pause->onChanged = [this] { requestPaint(); };
    m_bloom->onFrame = [this](uint64_t frameTimeNs) {
        if (m_bloomStartNs == 0)
            m_bloomStartNs = frameTimeNs;
        m_bloomPhase = double(frameTimeNs - m_bloomStartNs) / 1e9;
        if (m_bloomPhase >= BloomPeriod * BloomCount) {
            m_bloomPhase = -1;
            m_bloom->setRunning(false);
        }
        requestPaint();
    };
}

TimerRing::~TimerRing() = default;

double TimerRing::radius() const
{
    return (std::min(geometry().width, geometry().height) - StrokeWidth) / 2;
}

RectF TimerRing::visualRect() const
{
    const double reach = BloomReach + StrokeWidth * 2;
    return localRect().adjusted(-reach, -reach, reach, reach);
}

void TimerRing::setFraction(double fraction)
{
    fraction = std::clamp(fraction, 0.0, 1.0);
    if (fraction == m_fraction)
        return;
    m_fraction = fraction;
    requestPaint();
}

void TimerRing::setPaused(bool paused)
{
    if (isVisible() && window())
        m_pause->setTarget(paused ? 1 : 0);
    else
        m_pause->jumpTo(paused ? 1 : 0);
}

void TimerRing::setFinished(bool finished)
{
    if (finished == m_finished)
        return;
    m_finished = finished;
    m_bloomStartNs = 0;
    m_bloomPhase = finished ? 0 : -1;
    m_bloom->setRunning(finished && isVisible() && window());
    requestPaint();
}

void TimerRing::fillIn()
{
    if (!isVisible() || !window())
        return;
    m_reveal->jumpTo(0);
    m_reveal->setTarget(1);
}

void TimerRing::paint(Painter& painter)
{
    const double r = radius();
    const PointF centre{geometry().width / 2, geometry().height / 2};
    const RectF circle{centre.x - r, centre.y - r, 2 * r, 2 * r};
    const Color tint = timerColor();

    Path track;
    track.addEllipse(circle);
    painter.strokePath(track, style::withAlpha(style::text(), 0.12), StrokeWidth);

    if (m_finished) {
        // At rest after the bloom, the ring stays softly lit all round.
        painter.strokePath(track, style::withAlpha(tint, 0.38), StrokeWidth);
        if (m_bloomPhase >= 0) {
            for (int i = 0; i < BloomCount; ++i) {
                const double p = (m_bloomPhase - i * BloomPeriod) / (BloomPeriod * 1.6);
                if (p <= 0 || p >= 1)
                    continue;
                const double eased = 1 - std::pow(1 - p, 3);
                const double ringRadius = r + BloomReach * eased;
                Path halo;
                halo.addEllipse({centre.x - ringRadius, centre.y - ringRadius, 2 * ringRadius, 2 * ringRadius});
                painter.strokePath(halo, style::withAlpha(tint, 0.55 * (1 - p) * (1 - p)), StrokeWidth * (1 - 0.7 * p));
            }
        }
        return;
    }

    const double shown = m_fraction * m_reveal->value();
    if (shown <= 0)
        return;
    const double pause = m_pause->value();
    const Color arcColor = style::withAlpha(tint, 1 - 0.45 * pause);

    Path arc;
    if (shown >= 0.9999) {
        arc.addEllipse(circle);
    } else {
        arc.arcTo(circle, 90, -shown * 360);
    }
    painter.strokePath(arc, arcColor, StrokeWidth);

    // The light at the leading end: soft discs fading outwards, then a
    // brighter core on the stroke.
    const double glow = 1 - pause;
    if (glow > 0.01) {
        const PointF head = onCircle(centre, r, shown);
        for (int i = 4; i >= 1; --i) {
            const double size = StrokeWidth * (1 + 0.55 * i);
            painter.fillEllipse({head.x - size / 2, head.y - size / 2, size, size},
                                style::withAlpha(tint, 0.13 * glow));
        }
        const double core = StrokeWidth * 0.62;
        painter.fillEllipse({head.x - core / 2, head.y - core / 2, core, core},
                            style::withAlpha(tint.lighter(1.35), 0.9 * glow));
    }
}

} // namespace clockapp
