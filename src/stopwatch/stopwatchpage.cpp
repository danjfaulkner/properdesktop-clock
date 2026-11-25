// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "stopwatch/stopwatchpage.h"

#include "pagecontext.h"
#include "stopwatch/digitdisplay.h"
#include "stopwatch/lapslist.h"
#include "stopwatch/roundbutton.h"
#include "style.h"

#include <base/i18n.h>

#include "ui/keys.h"
#include "ui/painter.h"
#include "ui/scroll_view.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>

using namespace properdesktop::ui;
using properdesktop::i18n::tr;

namespace clockapp {

static constexpr double ButtonDiameter = 76;
static constexpr double ButtonGap = 48;
static constexpr double ColumnWidth = 560;
static constexpr double DigitsHeight = 150;
static constexpr double DigitsFontSize = 84;

StopwatchPage::StopwatchPage(PageContext& context, const TimeSource* time)
    : m_context(context)
    , m_time(time ? *time : TimeSource::system())
    , m_ticker(std::make_unique<FrameTicker>(&context.window()))
{
    setName("page:stopwatch");
    Window* window = &context.window();

    m_digits = addChild<DigitDisplay>(window, DigitsFontSize, 200);
    m_digits->setName("stopwatch:digits");
    m_digits->setSteadyCells(2);

    m_lapsView = addChild<ScrollView>();
    m_lapsView->setName("stopwatch:laps");
    m_lapsView->setScrollDirections(false, true);
    m_lapsView->setBottomCornerRadius(Theme::current().windowRadius);
    m_laps = m_lapsView->setContent<LapsList>(window);

    m_lapButton = addChild<RoundButton>(window, ButtonDiameter);
    m_lapButton->setName("stopwatch:lap");
    m_lapButton->onClicked = [this] { lapOrReset(); };
    m_startButton = addChild<RoundButton>(window, ButtonDiameter);
    m_startButton->setName("stopwatch:start");
    m_startButton->onClicked = [this] { startOrStop(); };

    m_ticker->onFrame = [this](uint64_t) { refresh(); };

    m_stopwatch.load(context.settings(), m_time.now());

    m_shortcuts.push_back(context.addShortcut(*this, NoModifier, key::Space, [this] { startOrStop(); }));
    m_shortcuts.push_back(context.addShortcut(*this, NoModifier, key::letter('l'), [this] {
        if (m_stopwatch.isRunning())
            lapOrReset();
    }));
    m_shortcuts.push_back(context.addShortcut(*this, NoModifier, key::letter('r'), [this] {
        if (!m_stopwatch.isRunning())
            lapOrReset();
    }));

    changed(false);
}

StopwatchPage::~StopwatchPage()
{
    for (int id : m_shortcuts)
        m_context.removeShortcut(id);
}

void StopwatchPage::startOrStop()
{
    const Reading now = m_time.now();
    if (m_stopwatch.isRunning())
        m_stopwatch.stop(now);
    else
        m_stopwatch.start(now);
    save();
    changed(false);
}

void StopwatchPage::lapOrReset()
{
    if (m_stopwatch.isRunning()) {
        m_stopwatch.lap(m_time.now());
        save();
        changed(true);
        return;
    }
    if (!m_stopwatch.hasTime())
        return;
    m_stopwatch.reset();
    save();
    changed(false);
}

void StopwatchPage::activated()
{
    m_current = true;
    m_digits->skipRoll();
    refresh();
    updateTicking();
}

void StopwatchPage::deactivated()
{
    m_current = false;
    updateTicking();
}

void StopwatchPage::entering(double progress)
{
    // The buttons and laps settle in a beat after the figures.
    const double later = std::clamp((progress - 0.3) / 0.7, 0.0, 1.0);
    m_lapButton->setOpacity(later);
    m_startButton->setOpacity(later);
    m_lapsView->setOpacity(later);
}

void StopwatchPage::save()
{
    m_stopwatch.save(m_context.settings());
    m_context.saveSettings();
}

void StopwatchPage::updateTicking()
{
    m_ticker->setRunning(m_current && m_stopwatch.isRunning());
}

void StopwatchPage::changed(bool lapAdded)
{
    const bool running = m_stopwatch.isRunning();
    m_startButton->setTitle(running ? tr("Stop") : tr("Start"));
    m_startButton->setRole(running ? RoundButton::Role::Stop : RoundButton::Role::Start);
    m_lapButton->setTitle(running || !m_stopwatch.hasTime() ? tr("Lap") : tr("Reset"));
    m_lapButton->setEnabled(m_stopwatch.hasTime());

    const Reading now = m_time.now();
    const bool hasLaps = m_stopwatch.hasTime();
    m_laps->setLaps(m_stopwatch.lapDurations(), m_stopwatch.lapExtremes(),
                    hasLaps ? m_stopwatch.currentLapNs(now) : -1, lapAdded);
    if (lapAdded)
        m_lapsView->setScrollPosition({0, 0});
    refresh();
    updateTicking();
    requestLayout();
}

void StopwatchPage::refresh()
{
    const Reading now = m_time.now();
    const int64_t elapsed = m_stopwatch.elapsedNs(now);
    m_digits->setText(formatElapsed(elapsed));
    if (m_stopwatch.hasTime())
        m_laps->setCurrent(m_stopwatch.currentLapNs(now));
}

void StopwatchPage::layout()
{
    const double width = geometry().width;
    const double height = geometry().height;
    const double scale = window() ? window()->scale() : 1;
    const double column = std::min(ColumnWidth, width - 2 * style::pagePadding);
    const double left = style::centred(width, column);
    const double top = headerHeight() + 12;

    m_digits->setGeometry({left, style::snap(top, scale), column, DigitsHeight});

    const double buttonsTop = std::round(top + DigitsHeight + 8);
    const double buttonsWidth = 2 * ButtonDiameter + ButtonGap;
    const double buttonsLeft = style::snap((width - buttonsWidth) / 2, scale);
    m_lapButton->setGeometry({buttonsLeft, buttonsTop, ButtonDiameter, ButtonDiameter});
    m_startButton->setGeometry({buttonsLeft + ButtonDiameter + ButtonGap, buttonsTop, ButtonDiameter, ButtonDiameter});

    const double lapsTop = buttonsTop + ButtonDiameter + 20;
    // The list spans the window, its rows on the column, so its bar sits at
    // the window's edge.
    m_lapsView->setGeometry({0, lapsTop, width, std::max(0.0, height - lapsTop)});
    m_laps->setInsets(left, width - left - column);
}

void StopwatchPage::paint(Painter& painter)
{
    painter.fillRect(localRect(), style::page());
}

std::unique_ptr<Page> makeStopwatchPage(PageContext& context)
{
    return std::make_unique<StopwatchPage>(context);
}

} // namespace clockapp
