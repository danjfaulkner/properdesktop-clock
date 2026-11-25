// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "page.h"
#include "stopwatch/motion.h"
#include "stopwatch/stopwatch.h"

#include <memory>

namespace properdesktop::ui {
class ScrollView;
}

namespace clockapp {

class DigitDisplay;
class LapsList;
class PageContext;
class RoundButton;

// The figures, the two round buttons under them and the laps. Its state lives
// in clock.conf [Stopwatch], written at every press, so the stopwatch runs on
// while the window is closed or the program quits.
class StopwatchPage : public Page
{
public:
    // A time source of the test's own; the system's when null.
    StopwatchPage(PageContext& context, const TimeSource* time = nullptr);
    ~StopwatchPage() override;

    const Stopwatch& stopwatch() const { return m_stopwatch; }

    void startOrStop();
    void lapOrReset();

    void activated() override;
    void deactivated() override;
    void entering(double progress) override;

    // Every frame while it runs, and after each press.
    void refresh();

protected:
    void layout() override;
    void paint(properdesktop::ui::Painter& painter) override;

private:
    void changed(bool lapAdded);
    void updateTicking();
    void save();

    PageContext& m_context;
    const TimeSource& m_time;
    Stopwatch m_stopwatch;
    bool m_current = false;

    DigitDisplay* m_digits = nullptr;
    RoundButton* m_lapButton = nullptr;
    RoundButton* m_startButton = nullptr;
    properdesktop::ui::ScrollView* m_lapsView = nullptr;
    LapsList* m_laps = nullptr;
    std::unique_ptr<FrameTicker> m_ticker;
    std::vector<int> m_shortcuts;
};

} // namespace clockapp
