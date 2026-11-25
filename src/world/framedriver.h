// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <functional>

#include "ui/frame_client.h"

namespace clockapp::world {

// The page's one source of frames: woken by whatever starts to move, it calls
// back every frame with the time since the last one and stops as soon as the
// callback says nothing moves any more.
class FrameDriver : public properdesktop::ui::FrameClient
{
public:
    using properdesktop::ui::FrameClient::FrameClient;

    void wake();
    bool isRunning() const { return isTicking(); }

    // Milliseconds since the previous frame (0 on the first) and the frame's
    // CLOCK_MONOTONIC time; returns whether to keep going.
    std::function<bool(double elapsedMs, uint64_t frameTimeNs)> onFrame;

protected:
    void tick(uint64_t frameTimeNs) override;

private:
    uint64_t m_lastFrameNs = 0;
};

} // namespace clockapp::world
