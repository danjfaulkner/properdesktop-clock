// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "alarms/alarmrow.h"

#include "alarms/spring.h"
#include "style.h"

#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/switch.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>

using namespace properdesktop::ui;

namespace clockapp {

static constexpr double Inset = 20;
static constexpr double TimeFontSize = 46;
static constexpr double HalfFontSize = 19;
static constexpr double DetailFontSize = 13.5;
// The opacity of an alarm's text while it is off.
static constexpr double DimmedOpacity = 0.42;
static constexpr std::chrono::milliseconds HintTime{3600};
// The line under the time rises this far as it changes.
static constexpr double HintTravel = 6;

AlarmRow::AlarmRow(Window* window, std::string id)
    : m_id(std::move(id))
    , m_light(std::make_unique<Spring>(window, 320, 0))
    , m_hintMix(std::make_unique<Spring>(window, 360, 0))
{
    setName("alarm:" + m_id);
    setTracksHover(true);

    m_time = addChild<Label>();
    m_time->setName("alarm:time");
    m_time->setFontSize(TimeFontSize);
    // Large figures read best light; tabular so rows line up digit for digit.
    m_time->setWeight(300);
    m_time->setFontFeatures({{"tnum", 1}});
    m_time->setLetterSpacing(-0.6);

    m_half = addChild<Label>();
    m_half->setFontSize(HalfFontSize);
    m_half->setWeight(500);

    m_detail = addChild<Label>();
    m_detail->setName("alarm:detail");
    m_detail->setFontSize(DetailFontSize);
    m_detail->setElide(Label::Elide::Right);
    m_detail->setColor(ColorRecipe(style::secondaryText()));

    m_hint = addChild<Label>();
    m_hint->setName("alarm:hint");
    m_hint->setFontSize(DetailFontSize);
    m_hint->setWeight(500);
    m_hint->setElide(Label::Elide::Right);
    m_hint->setFontFeatures({{"tnum", 1}});
    m_hint->setColor(ColorRecipe(style::accent()));
    m_hint->setVisible(false);

    m_switch = addChild<Switch>();
    m_switch->setName("alarm:switch");
    m_switch->onToggled = [this](bool on) {
        m_active = on;
        m_light->setTarget(on ? 1 : 0);
        if (onToggled)
            onToggled(on);
    };

    m_light->onChanged = [this] { applyDim(); };
    m_hintMix->onChanged = [this] { requestLayout(); };
    m_hintTimer.setSingleShot(true);
    m_hintTimer.onTimeout = [this] { m_hintMix->setTarget(0); };
}

AlarmRow::~AlarmRow() = default;

void AlarmRow::setContent(const Content& content, bool animate)
{
    m_time->setText(content.time);
    m_half->setText(content.half);
    m_half->setVisible(!content.half.empty());
    m_detail->setText(content.detail);
    if (content.active != m_active || m_switch->isChecked() != content.active) {
        m_active = content.active;
        m_switch->setChecked(content.active);
        if (animate) {
            m_light->setTarget(m_active ? 1 : 0);
        } else {
            m_light->jumpTo(m_active ? 1 : 0);
        }
        if (!m_active) {
            m_hintTimer.stop();
            m_hintMix->setTarget(0);
        }
    }
    applyDim();
    requestLayout();
}

void AlarmRow::flashHint(const std::string& text)
{
    m_hint->setText(text);
    m_hintMix->setTarget(1);
    m_hintTimer.start(HintTime);
    requestLayout();
}

void AlarmRow::applyDim()
{
    const double light = m_light->value();
    const double opacity = DimmedOpacity + (1 - DimmedOpacity) * light;
    for (Label* label : {m_time, m_half})
        label->setOpacity(opacity);
    requestLayout();
}

void AlarmRow::layout()
{
    const double width = geometry().width;
    const double scale = window() ? window()->scale() : 1.0;
    const SizeF switchSize = m_switch->sizeHint();
    m_switch->setGeometry({width - Inset - std::ceil(switchSize.width),
                           style::snap((Height - std::ceil(switchSize.height)) / 2, scale),
                           std::ceil(switchSize.width), std::ceil(switchSize.height)});
    const double textWidth = std::max(0.0, width - 3 * Inset - switchSize.width);

    // From the top of the time's figures to the baseline of the line under
    // it is one block centred on the card; descenders hang below.
    const double timeHeight = std::ceil(m_time->sizeHint().height);
    const double detailHeight = std::ceil(m_detail->sizeHint().height);
    const double gap = 0;
    const double figureTop = m_time->baselineOffset() - m_time->capHeight();
    const double block = timeHeight + gap + m_detail->baselineOffset() - figureTop;
    const double top = style::snap((Height - block) / 2 - figureTop, scale);
    const double timeWidth = std::ceil(m_time->sizeHint().width);
    m_time->setGeometry({Inset, top, timeWidth, timeHeight});
    if (m_half->isVisible()) {
        const SizeF half = m_half->sizeHint();
        // On the time's baseline.
        const double baseline = top + m_time->baselineOffset();
        const double y = style::snap(baseline - m_half->baselineOffset(), scale);
        m_half->setGeometry({Inset + timeWidth + 4, y, std::ceil(half.width), std::ceil(half.height)});
    }

    const double mix = m_hintMix->value();
    const double detailTop = style::snap(top + timeHeight + gap, scale);
    m_detail->setVisible(mix < 1);
    m_hint->setVisible(mix > 0);
    m_detail->setGeometry({Inset, style::snap(detailTop - HintTravel * mix, scale), textWidth, detailHeight});
    m_hint->setGeometry({Inset, style::snap(detailTop + HintTravel * (1 - mix), scale), textWidth, detailHeight});
    const double light = DimmedOpacity + (1 - DimmedOpacity) * m_light->value();
    m_detail->setOpacity(light * (1 - mix));
    m_hint->setOpacity(light * mix);
}

void AlarmRow::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const Corners corners = theme.continuousCorners ? Corners::Continuous : Corners::Circular;
    const double radius = theme.hugeRadius;
    // Lifted a little off the page: a soft shadow under it and a faint top light.
    painter.drawOuterShadow(localRect(), radius, corners, 8, Color{0, 0, 0, 40}, {0, 2});
    // A veil of white over the page: the card catches more light than the page.
    const Color fill{255, 255, 255, uint8_t(m_pressed ? 26 : 16)};
    painter.fillRoundedRect(localRect(), radius, fill, corners);
    painter.strokeRoundedRect(localRect(), radius, Color{255, 255, 255, 14}, 1, corners);
    if (hasVisibleFocus())
        painter.strokeRoundedRect(localRect(), radius, style::withAlpha(style::accent(), 0.7), 2, corners);
}

void AlarmRow::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    m_pressed = true;
    requestPaint();
}

void AlarmRow::pointerRelease(const PointerEvent& event)
{
    if (!m_pressed)
        return;
    m_pressed = false;
    requestPaint();
    if (localRect().contains(event.position) && onOpen)
        onOpen();
}

bool AlarmRow::keyPress(const KeyEvent& event)
{
    if (event.modifiers != NoModifier)
        return false;
    if (event.key == key::Return || event.key == key::KeypadEnter) {
        if (onOpen)
            onOpen();
        return true;
    }
    if (event.key == key::Space) {
        m_switch->setChecked(!m_switch->isChecked());
        if (m_switch->onToggled)
            m_switch->onToggled(m_switch->isChecked());
        return true;
    }
    return false;
}

void AlarmRow::focusIn(FocusReason)
{
    requestPaint();
}

void AlarmRow::focusOut()
{
    requestPaint();
}

void AlarmRow::hoverChanged()
{
    requestPaint();
}

} // namespace clockapp
