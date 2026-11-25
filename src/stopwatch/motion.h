// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/frame_client.h"

#include <cstdint>
#include <functional>

namespace clockapp {

// A value that follows its target on the window's frames as a critically
// damped spring: a new target mid-flight bends the motion without a jump,
// and the frames stop once it has settled.
class Glide : public properdesktop::ui::FrameClient
{
public:
    // The time the spring takes to cover most of the distance.
    explicit Glide(properdesktop::ui::Window* window, double settleMs = 220, double value = 0);

    double value() const { return m_value; }
    double target() const { return m_target; }
    bool isMoving() const { return isTicking(); }

    void setTarget(double target);
    // No motion: at once, as while hidden or in tests.
    void jumpTo(double value);
    void setSettleTime(double settleMs) { m_settleMs = settleMs; }
    // How near the target it comes to rest there: a fraction of opacity
    // stops sooner than a position in pixels.
    void setRestDistance(double distance) { m_restDistance = distance; }

    std::function<void()> onChanged;

protected:
    void tick(uint64_t frameTimeNs) override;

private:
    double m_settleMs;
    double m_restDistance = 0.002;
    double m_value;
    double m_target;
    double m_velocity = 0;
    uint64_t m_lastFrameNs = 0;
};

// Calls back on every frame while running, for what moves with the time
// itself, such as a running stopwatch or a draining ring.
class FrameTicker : public properdesktop::ui::FrameClient
{
public:
    using properdesktop::ui::FrameClient::FrameClient;

    void setRunning(bool running);
    bool isRunning() const { return isTicking(); }

    std::function<void(uint64_t frameTimeNs)> onFrame;

protected:
    void tick(uint64_t frameTimeNs) override
    {
        if (onFrame)
            onFrame(frameTimeNs);
    }
};

} // namespace clockapp
