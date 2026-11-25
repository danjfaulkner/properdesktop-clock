// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "store/store.h"
#include "ui/view.h"

#include <cstdint>
#include <functional>
#include <string>

namespace properdesktop::ui {
class Label;
}

namespace clockapp {

class DigitDisplay;
class RoundButton;
class TimerRing;

// One running, paused or finished timer: its ring with the time left and
// its label inside, when it ends under that, and two buttons below.
class TimerCard : public properdesktop::ui::View
{
public:
    static constexpr double Width = 220;
    // The ring's and the buttons' gap to the card's edges.
    static constexpr double Inset = 18;
    static constexpr double ButtonDiameter = 56;
    static constexpr double RingDiameter = Width - 2 * Inset;
    static constexpr double Height = Inset + RingDiameter + 12 + ButtonDiameter + Inset;

    TimerCard(properdesktop::ui::Window* window, std::string id);
    ~TimerCard() override;

    const std::string& id() const { return m_id; }
    void update(const RunningTimer& timer, int64_t nowMs);
    void fillIn();
    bool isFinished() const { return m_finished; }

    TimerRing* ring() const { return m_ring; }
    DigitDisplay* remaining() const { return m_remaining; }

    properdesktop::ui::SizeF sizeHint() const override { return {Width, Height}; }
    properdesktop::ui::RectF visualRect() const override { return localRect().adjusted(-30, -30, 30, 30); }

    std::function<void()> onPauseResume;
    std::function<void()> onCancel;
    std::function<void()> onRepeat;

protected:
    void layout() override;
    void paint(properdesktop::ui::Painter& painter) override;

private:
    std::string m_id;
    bool m_finished = false;
    bool m_paused = false;
    int64_t m_endsAtMs = -1;
    TimerRing* m_ring = nullptr;
    properdesktop::ui::Label* m_title = nullptr;
    DigitDisplay* m_remaining = nullptr;
    properdesktop::ui::Label* m_done = nullptr;
    properdesktop::ui::Label* m_ends = nullptr;
    RoundButton* m_cancel = nullptr;
    RoundButton* m_pause = nullptr;
};

} // namespace clockapp
