// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "alarms/spring.h"

#include <algorithm>
#include <cmath>

namespace clockapp {

Spring::Spring(properdesktop::ui::Window* window, double settleMs, double value)
    : FrameClient(window)
    , m_settleMs(settleMs)
    , m_value(value)
    , m_target(value)
{
}

void Spring::setTarget(double target)
{
    m_target = target;
    if (std::abs(m_target - m_value) < 1e-4 && std::abs(m_velocity) < 1e-3) {
        jumpTo(target);
        return;
    }
    if (!isTicking()) {
        m_lastFrameNs = 0;
        startTicking();
    }
}

void Spring::dragTo(double value, uint64_t timeNs)
{
    if (m_lastDragNs != 0 && timeNs > m_lastDragNs) {
        const double seconds = double(timeNs - m_lastDragNs) / 1e9;
        // Smoothed, so one uneven event does not decide the fling.
        m_velocity = 0.6 * ((value - m_value) / seconds) + 0.4 * m_velocity;
    }
    m_lastDragNs = timeNs;
    m_value = value;
    m_target = value;
    stopTicking();
    if (onChanged)
        onChanged();
}

void Spring::jumpTo(double value)
{
    stopTicking();
    m_value = value;
    m_target = value;
    m_velocity = 0;
    m_lastDragNs = 0;
    if (onChanged)
        onChanged();
}

void Spring::tick(uint64_t frameTimeNs)
{
    if (m_lastFrameNs == 0) {
        m_lastFrameNs = frameTimeNs;
        return;
    }
    double seconds = std::clamp(double(frameTimeNs - m_lastFrameNs) / 1e9, 0.0, 0.05);
    m_lastFrameNs = frameTimeNs;
    m_lastDragNs = 0;
    // A critically damped spring covers about 99% in 6.6 / omega.
    const double omega = 6.6 / (m_settleMs / 1000);
    while (seconds > 0) {
        const double step = std::min(seconds, 0.004);
        const double acceleration = -omega * omega * (m_value - m_target) - 2 * omega * m_velocity;
        m_velocity += acceleration * step;
        m_value += m_velocity * step;
        seconds -= step;
    }
    const bool rested = std::abs(m_value - m_target) < 1e-3 && std::abs(m_velocity) < 1e-2;
    if (rested) {
        m_value = m_target;
        m_velocity = 0;
        stopTicking();
    }
    if (onChanged)
        onChanged();
    if (rested && onSettled)
        onSettled();
}

} // namespace clockapp
