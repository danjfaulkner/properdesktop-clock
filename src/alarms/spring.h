// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/frame_client.h"

#include <cstdint>
#include <functional>

namespace clockapp {

// A value on the window's frame clock that follows its target as a
// critically damped spring: retargeting mid-flight keeps the velocity, so
// the motion bends rather than jumps, and frames stop once it rests.
class Spring : public properdesktop::ui::FrameClient
{
public:
    Spring(properdesktop::ui::Window* window, double settleMs, double value = 0);

    double value() const { return m_value; }
    double target() const { return m_target; }
    double velocity() const { return m_velocity; }
    bool isMoving() const { return isTicking(); }

    void setTarget(double target);
    // Moves with the pointer: no spring, the velocity kept for the release.
    void dragTo(double value, uint64_t timeNs);
    void jumpTo(double value);
    void setSettleTime(double settleMs) { m_settleMs = settleMs; }

    std::function<void()> onChanged;
    std::function<void()> onSettled;

protected:
    void tick(uint64_t frameTimeNs) override;

private:
    double m_settleMs;
    double m_value;
    double m_target;
    double m_velocity = 0;
    uint64_t m_lastFrameNs = 0;
    uint64_t m_lastDragNs = 0;
};

} // namespace clockapp
