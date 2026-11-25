// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "world/citycard.h"

#include <algorithm>
#include <cmath>

#include "style.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/path.h"
#include "ui/theme.h"
#include "world/clockface.h"
#include "world/palette.h"

using namespace properdesktop;
using namespace properdesktop::ui;

namespace clockapp::world {

static constexpr double kPadding = 16;
static constexpr double kFaceTop = 18;
static constexpr double kNameGap = 14;
static constexpr double kNameHeight = 20;
static constexpr double kCaptionHeight = 16;
static constexpr double kTimeGap = 6;
static constexpr double kTimeHeight = 32;
static constexpr double kBottom = 16;
static constexpr double kCaptionShift = 5;
static constexpr double kCaptionSettleMs = 520;

static double faceSize(double width)
{
    return std::clamp(std::round(width - 52), 92.0, 148.0);
}

static uint32_t hashOf(const std::string& text)
{
    uint32_t hash = 2166136261u;
    for (unsigned char c : text)
        hash = (hash ^ c) * 16777619u;
    return hash;
}

// The red minus in the card's corner while editing.
class RemoveButton : public View
{
public:
    explicit RemoveButton(std::string name)
    {
        setName(std::move(name));
        setTracksHover(true);
    }

    std::function<void()> onClicked;

    void setProgress(double progress)
    {
        m_progress = progress;
        setVisible(progress > 0.001);
        requestPaint();
    }

    bool acceptsPointer() const override { return m_progress > 0.5; }
    RectF visualRect() const override { return localRect().adjusted(-4, -4, 4, 6); }

protected:
    void paint(Painter& painter) override
    {
        const double scale = 0.6 + 0.4 * m_progress;
        const PointF centre{geometry().width / 2, geometry().height / 2};
        const double radius = geometry().width / 2 * scale;
        painter.save();
        painter.multiplyOpacity(std::clamp(m_progress, 0.0, 1.0));
        const RectF disc{centre.x - radius, centre.y - radius, radius * 2, radius * 2};
        painter.drawShadow(disc, radius, 2, Color{0, 0, 0, 60}, {0, 1});
        Path path;
        path.addEllipse(disc);
        painter.fillPath(path, m_pressed ? style::red().darker(1.15) : style::red());
        const double bar = radius * 0.48;
        painter.drawLine({centre.x - bar, centre.y}, {centre.x + bar, centre.y}, Color::rgb(0xFFFFFF), 2 * painter.scale());
        painter.restore();
    }

    void pointerPress(const PointerEvent& event) override
    {
        if (event.button != MouseButton::Left)
            return;
        m_pressed = true;
        requestPaint();
    }

    void pointerRelease(const PointerEvent& event) override
    {
        m_pressed = false;
        requestPaint();
        if (event.button == MouseButton::Left && localRect().contains(event.position) && onClicked)
            onClicked();
    }

private:
    double m_progress = 0;
    bool m_pressed = false;
};

double CityCard::heightForCardWidth(double width)
{
    return kFaceTop + faceSize(width) + kNameGap + kNameHeight + kCaptionHeight + kTimeGap + kTimeHeight + kBottom;
}

CityCard::CityCard(const City& city)
    : m_city(city)
    , m_seed(hashOf(city.zoneId))
{
    setName("world:card:" + city.zoneId);
    setTracksHover(true);
    m_face = addChild<ClockFace>();

    m_name = addChild<Label>(city.name);
    m_name->setFontSize(style::points(11.5));
    m_name->setWeight(600);
    m_name->setTextAlignment(Label::TextAlignment::Center);
    m_name->setElide(Label::Elide::Right);

    for (Label** caption : {&m_caption, &m_previousCaption}) {
        *caption = addChild<Label>();
        (*caption)->setFontSize(style::points(8.5));
        (*caption)->setWeight(500);
        (*caption)->setLetterSpacing(0.2);
        (*caption)->setFontFeatures({{"tnum", 1}});
        (*caption)->setTextAlignment(Label::TextAlignment::Center);
        (*caption)->setElide(Label::Elide::Right);
    }
    m_previousCaption->setVisible(false);
    m_caption->setName("world:caption:" + city.zoneId);
    m_name->setName("world:name:" + city.zoneId);

    // Large figures read best light; the day period smaller, a step heavier.
    m_time = addChild<Label>();
    m_time->setName("world:time:" + city.zoneId);
    m_time->setFontSize(style::points(20));
    m_time->setWeight(300);
    m_time->setFontFeatures({{"tnum", 1}, {"lnum", 1}});
    m_period = addChild<Label>();
    m_period->setName("world:period:" + city.zoneId);
    m_period->setFontSize(style::points(10));
    m_period->setWeight(500);

    m_remove = addChild<RemoveButton>("world:remove:" + city.zoneId);
    m_remove->setProgress(0);
    m_remove->onClicked = [this] {
        if (onRemove)
            onRemove();
    };

    m_captionFade.jump(1);
    presence.jump(1);
}

std::string CityCard::captionText() const
{
    return m_caption->text();
}

std::string CityCard::timeText() const
{
    return m_period->text().empty() ? m_time->text() : m_time->text() + " " + m_period->text();
}

void CityCard::update(const ZoneTime& here, int64_t utcMilliseconds, double secondStep, bool animated)
{
    m_there = ZoneTime::at(m_city.zoneId, utcMilliseconds);
    m_sky = m_city.hasLocation ? Sky::at(m_city.latitude, m_city.longitude, utcMilliseconds)
                               : Sky::fromHour(m_there.time.hour, m_there.time.minute);
    m_face->setSky(m_sky, m_seed);
    m_face->setTime(m_there.time, secondStep);
    setCaption(world::captionText(dayDifference(here.date, m_there.date), m_there.offsetSeconds - here.offsetSeconds),
               animated);
    setTime(digitalText(m_there.time));
    requestPaint();
}

void CityCard::setSecondStep(double secondStep)
{
    m_face->setTime(m_there.time, secondStep);
}

void CityCard::setCaption(std::string text, bool animated)
{
    if (text == m_caption->text())
        return;
    if (!animated || m_caption->text().empty()) {
        m_caption->setText(std::move(text));
        m_captionFade.jump(1);
        m_previousCaption->setVisible(false);
        requestLayout();
        return;
    }
    // The day turns over: the old words drift up and fade as the new ones
    // rise into place.
    m_previousCaption->setText(m_caption->text());
    m_caption->setText(std::move(text));
    m_captionFade.jump(0);
    m_captionFade.target = 1;
    m_previousCaption->setVisible(true);
    requestLayout();
}

void CityCard::setTime(const std::string& text)
{
    // The day period, in whichever language, is the part that is not
    // figures and separators at either end.
    const base::Locale& locale = base::Locale::system();
    std::string figures = text;
    std::string period;
    for (const std::string& marker : {locale.amText(), locale.pmText()}) {
        if (marker.empty())
            continue;
        const size_t at = text.find(marker);
        if (at == std::string::npos)
            continue;
        period = marker;
        figures = text.substr(0, at) + text.substr(at + marker.size());
        break;
    }
    // Spaces, including the narrow no-break space, around what is left.
    const auto trim = [](std::string& value) {
        const std::string spaces[] = {" ", " ", " "};
        bool trimmed = true;
        while (trimmed && !value.empty()) {
            trimmed = false;
            for (const std::string& space : spaces) {
                if (value.starts_with(space)) {
                    value.erase(0, space.size());
                    trimmed = true;
                }
                if (value.ends_with(space)) {
                    value.erase(value.size() - space.size());
                    trimmed = true;
                }
            }
        }
    };
    trim(figures);
    if (figures == m_time->text() && period == m_period->text())
        return;
    m_periodFirst = !period.empty() && text.find(period) < text.find(figures.substr(0, 1));
    m_time->setText(figures);
    m_period->setText(period);
    m_period->setVisible(!period.empty());
    requestLayout();
}

void CityCard::setEditProgress(double progress)
{
    if (progress == m_editProgress)
        return;
    m_editProgress = progress;
    m_remove->setProgress(progress);
}

bool CityCard::advance(double elapsedMs)
{
    if (m_captionFade.isAtRest() && m_captionFade.value == m_captionFade.target)
        return false;
    m_captionFade.advance(elapsedMs, kCaptionSettleMs);
    if (m_captionFade.isAtRest())
        m_previousCaption->setVisible(false);
    requestLayout();
    return !m_captionFade.isAtRest();
}

RectF CityCard::removeButtonRect() const
{
    // Centred on the corner's arc, so it keeps the same gap to the top and
    // left edges and runs concentric with the corner.
    const double centre = kRadius;
    return {centre - kRemoveDiameter / 2, centre - kRemoveDiameter / 2, kRemoveDiameter, kRemoveDiameter};
}

RectF CityCard::visualRect() const
{
    return localRect().adjusted(-24, -20, 24, 30);
}

void CityCard::layout()
{
    const double width = geometry().width;
    m_remove->setGeometry(removeButtonRect());
    const double face = faceSize(width);
    m_face->setGeometry({style::centred(width, face), kFaceTop, face, face});

    double y = kFaceTop + face + kNameGap;
    m_name->setGeometry({kPadding, y, width - 2 * kPadding, kNameHeight});
    y += kNameHeight;

    const RectF caption{kPadding, y, width - 2 * kPadding, kCaptionHeight};
    const double fade = std::clamp(m_captionFade.value, 0.0, 1.0);
    const double rise = std::round((1 - fade) * kCaptionShift * 2) / 2;
    m_caption->setGeometry(caption.adjusted(0, rise, 0, rise));
    m_caption->setOpacity(fade);
    const double drift = -std::round(fade * kCaptionShift * 2) / 2;
    m_previousCaption->setGeometry(caption.adjusted(0, drift, 0, drift));
    m_previousCaption->setOpacity(1 - fade);
    y += kCaptionHeight + kTimeGap;

    const SizeF time = m_time->sizeHint();
    const SizeF period = m_period->isVisible() ? m_period->sizeHint() : SizeF{};
    const double gap = m_period->isVisible() ? 3 : 0;
    const double total = std::ceil(time.width) + gap + std::ceil(period.width);
    double x = style::centred(width, total);
    const double baseline = y + kTimeHeight - 7;
    const auto place = [&](Label* label, const SizeF& hint) {
        label->setGeometry({x, std::round(baseline - label->baselineOffset()), std::ceil(hint.width), std::ceil(hint.height)});
        x += std::ceil(hint.width) + gap;
    };
    if (m_periodFirst && m_period->isVisible()) {
        place(m_period, period);
        place(m_time, time);
    } else {
        place(m_time, time);
        if (m_period->isVisible())
            place(m_period, period);
    }

    const Color primary = style::text();
    const Color secondary = style::secondaryText();
    m_name->setColor(primary);
    m_caption->setColor(secondary);
    m_previousCaption->setColor(secondary);
    m_time->setColor(primary);
    m_period->setColor(primary);
}

void CityCard::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const RectF rect = localRect();
    const double lifted = std::clamp(lift.value, 0.0, 1.0);

    painter.drawShadow(rect, kRadius, 6 + 10 * lifted, Color{0, 0, 0, uint8_t(80 + 50 * lifted)}, {0, 2 + 6 * lifted});
    painter.fillRoundedRect(rect, kRadius, style::card(), Corners::Continuous);

    // The card's top takes on its city's sky.
    const SkyPalette palette = SkyPalette::of(m_sky);
    LinearGradient glow{{0, 0}, {0, rect.height * 0.62}, {{0, palette.glow}, {1, palette.glow.withAlpha(0)}}};
    painter.fillRoundedRect(rect, kRadius, glow, Corners::Continuous);
    // A hairline lifts the card's edge off the dark page.
    painter.strokeRoundedRect(rect, kRadius, Color{255, 255, 255, 16}, painter.borderWidth(1), Corners::Continuous);

    if (hasVisibleFocus()) {
        const RectF ring = rect.adjusted(-3, -3, 3, 3);
        painter.strokeRoundedRect(ring, kRadius + 3, theme.highlight.withAlpha(0.6), painter.borderWidth(2),
                                  Corners::Continuous);
    }
}

bool CityCard::keyPress(const KeyEvent& event)
{
    if ((event.key == key::Delete || event.key == key::BackSpace) && onRemove) {
        onRemove();
        return true;
    }
    return onKey && onKey(event);
}

void CityCard::focusIn(FocusReason)
{
    requestPaint();
}

void CityCard::focusOut()
{
    requestPaint();
}

} // namespace clockapp::world
