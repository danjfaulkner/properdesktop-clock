// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "world/emptystate.h"

#include <algorithm>
#include <cmath>

#include "base/i18n.h"
#include "style.h"
#include "ui/dialog_window.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/path.h"
#include "world/palette.h"

using namespace properdesktop::ui;
using properdesktop::i18n::tr;

namespace clockapp::world {

static constexpr double kGlobe = 112;
static constexpr double kGlobeGap = 22;
static constexpr double kTitleGap = 6;
static constexpr double kButtonGap = 18;
static constexpr double kTextWidth = 300;

EmptyState::EmptyState()
{
    setName("world:empty");
    m_title = addChild<Label>(tr("No Cities"));
    m_title->setFontSize(style::points(15));
    m_title->setWeight(600);
    m_title->setTextAlignment(Label::TextAlignment::Center);
    m_text = addChild<Label>(tr("Add the cities you keep in touch with to see their time and sky at a glance."));
    m_text->setFontSize(style::points(10));
    m_text->setWrap(true);
    m_text->setTextAlignment(Label::TextAlignment::Center);
    m_add = addChild<DialogButton>(tr("Add City"), true);
    m_add->setName("world:empty:add");
    m_add->onClicked = [this] {
        if (onAdd)
            onAdd();
    };
}

void EmptyState::setProgress(double progress)
{
    if (progress == m_progress)
        return;
    m_progress = progress;
    requestPaint();
}

RectF EmptyState::globeRect() const
{
    const double textHeight = m_text->heightForWidth(kTextWidth);
    const double total = kGlobe + kGlobeGap + m_title->sizeHint().height + kTitleGap + textHeight + kButtonGap
        + m_add->sizeHint().height;
    const double top = std::max(0.0, style::centred(geometry().height, total));
    return {style::centred(geometry().width, kGlobe), top, kGlobe, kGlobe};
}

void EmptyState::layout()
{
    const RectF globe = globeRect();
    const double width = geometry().width;
    double y = globe.bottom() + kGlobeGap;
    const SizeF title = m_title->sizeHint();
    m_title->setGeometry({0, y, width, std::ceil(title.height)});
    y += std::ceil(title.height) + kTitleGap;
    const double textWidth = std::min(kTextWidth, width - 2 * style::pagePadding);
    const double textHeight = std::ceil(m_text->heightForWidth(textWidth));
    m_text->setGeometry({style::centred(width, textWidth), y, textWidth, textHeight});
    y += textHeight + kButtonGap;
    const SizeF button = m_add->sizeHint();
    m_add->setGeometry({style::centred(width, std::ceil(button.width)), y, std::ceil(button.width), button.height});

    m_title->setColor(style::text());
    m_text->setColor(style::secondaryText());
}

void EmptyState::paint(Painter& painter)
{
    const RectF globe = globeRect();
    const double radius = globe.width / 2;
    const PointF centre{globe.x + radius, globe.y + radius};

    // Half in daylight, half at night: the terminator runs down the middle.
    Path disc;
    disc.addEllipse(globe);
    painter.drawShadow(globe, radius, 8, Color{0, 0, 0, 110}, {0, 3});
    painter.fillPath(disc, LinearGradient{{globe.x, centre.y}, {globe.right(), centre.y},
                                          {{0, Color::rgb(0xE6EBF1)},
                                           {0.46, Color::rgb(0xC2CCD8)},
                                           {0.56, Color::rgb(0x2E3658)},
                                           {1, Color::rgb(0x121731)}}});

    const Color line = Color{120, 132, 160, 110};
    const double eased = 1 - std::pow(1 - std::clamp(m_progress, 0.0, 1.0), 3);
    const double turn = (1 - eased) * 30;
    painter.save();
    painter.clipRoundedRect(globe, radius);
    // Meridians: ellipses whose width follows the cosine of their longitude.
    for (int meridian = -75; meridian <= 90; meridian += 30) {
        const double longitude = (meridian + turn) * M_PI / 180;
        if (std::cos(longitude) <= 0)
            continue;
        const double half = radius * std::abs(std::sin(longitude));
        Path path;
        path.addEllipse({centre.x - half, globe.y, half * 2, globe.height});
        painter.strokePath(path, line, 1);
    }
    for (int parallel = -60; parallel <= 60; parallel += 30) {
        const double y = centre.y - radius * std::sin(parallel * M_PI / 180);
        const double half = radius * std::cos(parallel * M_PI / 180);
        painter.drawLine({centre.x - half, y}, {centre.x + half, y}, line, painter.scale());
    }
    painter.restore();
    painter.strokePath(disc, Color{255, 255, 255, 40}, 1);

    // The sun on the terminator, in the second hand's colour.
    Path sun;
    const double dot = 4.5;
    sun.addEllipse({centre.x - dot, globe.y + radius * 0.32 - dot, dot * 2, dot * 2});
    painter.fillPath(sun, kSecondHand);
}

} // namespace clockapp::world
