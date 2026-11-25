// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "stopwatch/motion.h"

#include <cmath>

namespace clockapp {

Glide::Glide(properdesktop::ui::Window* window, double settleMs, double value)
    : FrameClient(window)
    , m_settleMs(settleMs)
    , m_value(value)
    , m_target(value)
{
}

void Glide::setTarget(double target)
{
    if (target == m_target && !isTicking() && m_value == target)
        return;
    m_target = target;
    if (!isTicking()) {
        m_lastFrameNs = 0;
        startTicking();
    }
}

void Glide::jumpTo(double value)
{
    m_value = value;
    m_target = value;
    m_velocity = 0;
    stopTicking();
    if (onChanged)
        onChanged();
}

void Glide::tick(uint64_t frameTimeNs)
{
    // The first frame only takes the time: motion starts from where it is.
    if (m_lastFrameNs == 0 || frameTimeNs <= m_lastFrameNs) {
        m_lastFrameNs = frameTimeNs;
        return;
    }
    const double t = std::min(0.1, double(frameTimeNs - m_lastFrameNs) / 1e9);
    m_lastFrameNs = frameTimeNs;

    const double omega = 6.0 / (m_settleMs / 1000.0);
    const double displacement = m_value - m_target;
    const double decay = std::exp(-omega * t);
    const double shared = (m_velocity + omega * displacement) * t;
    const double next = (displacement + shared) * decay;
    m_velocity = (m_velocity - omega * shared) * decay;
    m_value = m_target + next;

    if (std::abs(next) < m_restDistance && std::abs(m_velocity) < m_restDistance * 10) {
        m_value = m_target;
        m_velocity = 0;
        stopTicking();
    }
    if (onChanged)
        onChanged();
}

void FrameTicker::setRunning(bool running)
{
    if (running == isTicking())
        return;
    if (running)
        startTicking();
    else
        stopTicking();
}

} // namespace clockapp
