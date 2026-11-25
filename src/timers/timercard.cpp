// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "timers/timercard.h"

#include "stopwatch/digitdisplay.h"
#include "stopwatch/roundbutton.h"
#include "style.h"
#include "timers/timermath.h"
#include "timers/timerring.h"

#include <base/i18n.h>
#include <base/locale.h>

#include "ui/label.h"
#include "ui/painter.h"
#include "ui/path.h"
#include "ui/window.h"

#include <cmath>
#include <ctime>

using namespace properdesktop::ui;
using properdesktop::i18n::format;
using properdesktop::i18n::tr;
using properdesktop::i18n::trc;

namespace clockapp {

static ColorRecipe secondaryColor()
{
    return ColorRecipe(std::function<Color(const ColorProvider&)>(
        [](const ColorProvider&) { return style::secondaryText(); }));
}

// The local time of day of a Unix millisecond, as the region writes it.
static std::string timeOfDay(int64_t ms)
{
    const time_t seconds = time_t(ms / 1000);
    tm local{};
    localtime_r(&seconds, &local);
    properdesktop::base::ClockTime time;
    time.hour = local.tm_hour;
    time.minute = local.tm_min;
    return properdesktop::base::Locale::system().toString(time, properdesktop::base::Locale::FormatType::Short);
}

TimerCard::TimerCard(Window* window, std::string id)
    : m_id(std::move(id))
{
    setName("timers:card");
    m_ring = addChild<TimerRing>(window);
    m_ring->setName("timers:ring");

    m_title = addChild<Label>();
    m_title->setName("timers:title");
    m_title->setFontSize(14);
    m_title->setWeight(500);
    m_title->setTextAlignment(Label::TextAlignment::Center);
    m_title->setColor(secondaryColor());

    m_remaining = addChild<DigitDisplay>(window, 38, 300);
    m_remaining->setName("timers:remaining");

    m_done = addChild<Label>(tr("Done"));
    m_done->setName("timers:done");
    m_done->setFontSize(34);
    m_done->setWeight(300);
    m_done->setTextAlignment(Label::TextAlignment::Center);
    m_done->setVisible(false);

    m_ends = addChild<Label>();
    m_ends->setName("timers:ends");
    m_ends->setFontSize(13);
    m_ends->setFontFeatures({{"tnum", 1}});
    m_ends->setTextAlignment(Label::TextAlignment::Center);
    m_ends->setColor(secondaryColor());

    m_cancel = addChild<RoundButton>(window, ButtonDiameter);
    m_cancel->setName("timers:cancel");
    m_cancel->setTitle(tr("Cancel"));
    m_cancel->onClicked = [this] {
        if (onCancel)
            onCancel();
    };
    m_pause = addChild<RoundButton>(window, ButtonDiameter);
    m_pause->setName("timers:pause");
    m_pause->onClicked = [this] {
        if (m_finished) {
            if (onRepeat)
                onRepeat();
        } else if (onPauseResume) {
            onPauseResume();
        }
    };
}

TimerCard::~TimerCard() = default;

void TimerCard::update(const RunningTimer& timer, int64_t nowMs)
{
    const bool finished = timermath::isFinished(timer, nowMs);
    const bool paused = !timer.isRunning();
    m_title->setText(timer.label.empty() ? timermath::formatDuration(timer.durationMs) : timer.label);
    m_ring->setFraction(timermath::fractionLeft(timer, nowMs));
    m_remaining->setText(timermath::formatRemaining(timermath::remainingMs(timer, nowMs)));

    if (finished != m_finished || paused != m_paused || timer.endsAtMs != m_endsAtMs) {
        m_finished = finished;
        m_paused = paused;
        m_endsAtMs = timer.endsAtMs;
        m_ring->setPaused(paused);
        m_ring->setFinished(finished);
        m_remaining->setVisible(!finished);
        m_done->setVisible(finished);
        if (finished)
            m_ends->setText(format(tr("Ended {time}"), {{"time", timeOfDay(timer.endsAtMs)}}));
        else if (paused)
            m_ends->setText(tr("Paused"));
        else
            m_ends->setText(format(tr("Ends {time}"), {{"time", timeOfDay(timer.endsAtMs)}}));
        m_cancel->setTitle(finished ? tr("Dismiss") : tr("Cancel"));
        if (finished) {
            m_pause->setTitle(trc("button", "Repeat"));
            m_pause->setRole(RoundButton::Role::Start);
        } else if (paused) {
            m_pause->setTitle(tr("Resume"));
            m_pause->setRole(RoundButton::Role::Start);
        } else {
            m_pause->setTitle(tr("Pause"));
            m_pause->setRole(RoundButton::Role::Pause);
        }
        requestLayout();
    }
}

void TimerCard::fillIn()
{
    m_ring->fillIn();
}

void TimerCard::layout()
{
    const double width = geometry().width;
    const double scale = window() ? window()->scale() : 1;
    const RectF ring{Inset, Inset, RingDiameter, RingDiameter};
    m_ring->setGeometry(ring);

    const PointF centre{ring.x + ring.width / 2, ring.y + ring.height / 2};
    const double figuresHeight = std::ceil(m_remaining->lineHeight());
    // The time left sits on the ring's centre; the title above and the end
    // time below keep the same distance from it.
    const double figuresTop = style::snap(centre.y - figuresHeight / 2, scale);
    const double inner = RingDiameter - 2 * TimerRing::StrokeWidth - 24;
    const double innerLeft = style::centred(width, inner);
    m_remaining->setGeometry({innerLeft, figuresTop, inner, figuresHeight});
    const double doneHeight = std::ceil(m_done->sizeHint().height);
    m_done->setGeometry({innerLeft, style::snap(centre.y - doneHeight / 2, scale), inner, doneHeight});

    const double titleHeight = std::ceil(m_title->sizeHint().height);
    m_title->setGeometry({innerLeft + 12, figuresTop - 6 - titleHeight, inner - 24, titleHeight});
    const double endsHeight = std::ceil(m_ends->sizeHint().height);
    m_ends->setGeometry({innerLeft, figuresTop + figuresHeight + 6, inner, endsHeight});

    const double buttonsTop = ring.bottom() + 12;
    m_cancel->setGeometry({Inset, buttonsTop, ButtonDiameter, ButtonDiameter});
    m_pause->setGeometry({width - Inset - ButtonDiameter, buttonsTop, ButtonDiameter, ButtonDiameter});
}

void TimerCard::paint(Painter& painter)
{
    const RectF card = localRect();
    painter.drawShadow(card, style::cardRadius, 12, Color{0, 0, 0, 90}, {0, 4});
    painter.fillRoundedRect(card, style::cardRadius,
                            LinearGradient::vertical(card, style::card().lighter(1.08), style::card()));
    // A hairline of light round the edge, as a dark card catches it.
    painter.strokeRoundedRect(card, style::cardRadius, Color{255, 255, 255, 16});
}

} // namespace clockapp
