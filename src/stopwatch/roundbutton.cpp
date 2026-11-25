// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "stopwatch/roundbutton.h"

#include "stopwatch/palette.h"
#include "style.h"

#include "ui/color_provider.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/path.h"
#include "ui/theme.h"

#include <algorithm>
#include <cmath>

using namespace properdesktop::ui;

namespace clockapp {

static Color mix(Color a, Color b, double t)
{
    const auto channel = [t](uint8_t x, uint8_t y) { return uint8_t(std::lround(x + (y - x) * t)); };
    return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a)};
}

static Color tintOf(RoundButton::Role role)
{
    switch (role) {
    case RoundButton::Role::Start:
        return style::green();
    case RoundButton::Role::Stop:
        return style::red();
    case RoundButton::Role::Pause:
        return palette::orange;
    case RoundButton::Role::Neutral:
        break;
    }
    return style::text();
}

// The disc's fill at rest, and the title's colour.
static Color fillOf(RoundButton::Role role)
{
    if (role == RoundButton::Role::Neutral)
        return style::withAlpha(style::text(), 0.16);
    return style::withAlpha(tintOf(role), 0.26);
}

static double ringInsetOf(double diameter)
{
    return std::max(2.0, std::round(diameter * 0.04));
}

static double normalTitleSize(double diameter)
{
    return std::round(diameter * 0.2);
}

static Color titleOf(RoundButton::Role role)
{
    if (role == RoundButton::Role::Neutral)
        return style::text();
    return tintOf(role);
}

RoundButton::RoundButton(Window* window, double diameter)
    : m_diameter(diameter)
    , m_state(std::make_unique<Glide>(window, 140))
    , m_roleFade(std::make_unique<Glide>(window, 260, 1))
{
    m_label = addChild<Label>();
    m_label->setTextAlignment(Label::TextAlignment::Center);
    m_label->setVerticallyCentered(true);
    m_label->setFontSize(normalTitleSize(diameter));
    m_titleSize = normalTitleSize(diameter);
    m_label->setWeight(500);
    m_label->setFadesWhenDisabled(false);
    m_state->setRestDistance(0.01);
    m_roleFade->setRestDistance(0.01);
    m_state->onChanged = [this] { requestPaint(); };
    m_roleFade->onChanged = [this] {
        m_label->setColor(ColorRecipe([this](const ColorProvider&) {
            return mix(titleOf(m_previousRole), titleOf(m_role), m_roleFade->value());
        }));
        requestPaint();
    };
    m_roleFade->onChanged();
}

RoundButton::~RoundButton() = default;

void RoundButton::setTitle(const std::string& title)
{
    if (title == m_title)
        return;
    m_title = title;
    m_label->setText(title);
    fitTitle();
}

double RoundButton::titleWidth() const
{
    return m_diameter - 4 * ringInsetOf(m_diameter);
}

double RoundButton::minimumTitleSize() const
{
    return std::ceil(normalTitleSize(m_diameter) * 0.8);
}

void RoundButton::fitTitle()
{
    double size = normalTitleSize(m_diameter);
    m_label->setFontSize(size);
    while (size > minimumTitleSize() && m_label->sizeHint().width > titleWidth()) {
        size -= 1;
        m_label->setFontSize(size);
    }
    m_titleSize = size;
}

void RoundButton::setPlayGlyph(bool play)
{
    m_playGlyph = play;
    m_label->setVisible(!play);
    requestPaint();
}

void RoundButton::setRole(Role role)
{
    if (role == m_role)
        return;
    m_previousRole = m_role;
    m_role = role;
    m_roleFade->jumpTo(0);
    if (isVisible() && window())
        m_roleFade->setTarget(1);
    else
        m_roleFade->jumpTo(1);
}

void RoundButton::layout()
{
    m_label->setGeometry(localRect());
}

void RoundButton::paint(Painter& painter)
{
    const RectF disc = localRect();
    const double state = m_state->value();
    const double fade = m_roleFade->value();
    Color fill = mix(fillOf(m_previousRole), fillOf(m_role), fade);
    // Hover and press deepen the tint.
    const double deepen = std::clamp(state, 0.0, 2.0) * 0.5;
    fill = fill.withAlpha(std::min(1.0, fill.a / 255.0 * (1 + deepen * 0.7)));
    if (!isEnabled())
        fill = fill.withAlpha(fill.a / 255.0 * 0.5);
    painter.fillEllipse(disc, fill);

    // Light from above: a soft sheen over the upper half.
    const double sheen = 0.07;
    Path upper;
    upper.addEllipse(disc);
    painter.fillPath(upper, LinearGradient::vertical(disc, Color{255, 255, 255, uint8_t(sheen * 255)},
                                                     Color{255, 255, 255, 0}));

    // The ring a little inside the edge, in the page's colour.
    const double ringInset = ringInsetOf(m_diameter);
    Path ring;
    ring.addEllipse(disc.adjusted(ringInset, ringInset, -ringInset, -ringInset));
    painter.strokePath(ring, style::withAlpha(style::page(), 0.9), 1.5);

    if (m_playGlyph) {
        // Centred on its area, not its box, so it does not look pushed left.
        const double side = std::round(m_diameter * 0.34);
        const double height = side;
        const double width = side * 0.866;
        const double x = disc.width / 2 - width / 3;
        const double y = disc.height / 2 - height / 2;
        Path triangle;
        triangle.moveTo(PointF{x, y});
        triangle.lineTo(PointF{x + width, y + height / 2});
        triangle.lineTo(PointF{x, y + height});
        triangle.close();
        painter.fillPath(triangle, mix(titleOf(m_previousRole), titleOf(m_role), fade));
    }

    if (hasVisibleFocus()) {
        Path focus;
        focus.addEllipse(disc.adjusted(-2, -2, 2, 2));
        painter.strokePath(focus, style::accent(), 2);
    }
}

void RoundButton::enabledChanged()
{
    m_label->setOpacity(isEnabled() ? 1 : 0.45);
    requestPaint();
}

void RoundButton::updateState()
{
    m_state->setTarget(m_pressed ? 2 : m_hovered ? 1 : 0);
}

void RoundButton::pointerEnter()
{
    m_hovered = true;
    updateState();
}

void RoundButton::pointerLeave()
{
    m_hovered = false;
    updateState();
}

void RoundButton::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    m_pressed = true;
    updateState();
}

void RoundButton::pointerRelease(const PointerEvent& event)
{
    if (!m_pressed)
        return;
    m_pressed = false;
    updateState();
    const RectF rect = localRect();
    const double dx = event.position.x - rect.width / 2;
    const double dy = event.position.y - rect.height / 2;
    if (dx * dx + dy * dy <= rect.width * rect.width / 4 && onClicked)
        onClicked();
}

bool RoundButton::keyPress(const KeyEvent& event)
{
    if ((event.key == key::Space || event.key == key::Return) && !event.repeat) {
        if (onClicked)
            onClicked();
        return true;
    }
    return false;
}

} // namespace clockapp
