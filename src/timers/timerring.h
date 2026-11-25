// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "stopwatch/motion.h"
#include "ui/view.h"

#include <cstdint>
#include <memory>

namespace clockapp {

// A timer's ring: a faint track, and over it the time left as an arc from
// twelve that drains clockwise, lit at its leading end. It fills in when the
// timer starts, dims while paused, and blooms in rings when the time is up.
class TimerRing : public properdesktop::ui::View
{
public:
    static constexpr double StrokeWidth = 7;

    explicit TimerRing(properdesktop::ui::Window* window);
    ~TimerRing() override;

    void setFraction(double fraction);
    double fraction() const { return m_fraction; }
    void setPaused(bool paused);
    void setFinished(bool finished);
    // The arc sweeps in from nothing to the time left.
    void fillIn();

    // The ring's circle, through the middle of the stroke.
    double radius() const;

    // The bloom and the light reach out past the ring.
    properdesktop::ui::RectF visualRect() const override;

protected:
    void paint(properdesktop::ui::Painter& painter) override;

private:
    double m_fraction = 1;
    bool m_finished = false;
    std::unique_ptr<Glide> m_reveal;
    std::unique_ptr<Glide> m_pause;
    std::unique_ptr<FrameTicker> m_bloom;
    uint64_t m_bloomStartNs = 0;
    double m_bloomPhase = -1;
};

} // namespace clockapp
