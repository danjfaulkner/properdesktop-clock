// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "timers/wheel.h"

#include "style.h"

#include "ui/bitmap.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

using namespace properdesktop::ui;

namespace clockapp {

static constexpr double FontSize = 24;
static constexpr double UnitGap = 8;
static constexpr double Pi = 3.14159265358979323846;

static long wrap(long value, long count)
{
    const long wrapped = value % count;
    return wrapped < 0 ? wrapped + count : wrapped;
}

static Corners themeCorners()
{
    return Theme::current().continuousCorners ? Corners::Continuous : Corners::Circular;
}

// The number a value reads as, or -1.
static long numberOf(const std::string& text)
{
    if (text.empty() || text.size() > 4)
        return -1;
    char* end = nullptr;
    const long number = std::strtol(text.c_str(), &end, 10);
    return *end == '\0' ? number : -1;
}

Wheel::Wheel(Window* window, WheelColumn column)
    : m_column(std::move(column))
    , m_position(std::make_unique<Glide>(window, 260, 0))
    , m_renderer(std::make_unique<Label>())
{
    if (!m_column.name.empty())
        setName(m_column.name);
    if (m_column.values.empty())
        m_column.values.push_back({});
    m_renderer->setFontSize(FontSize);
    m_renderer->setWeight(400);
    m_renderer->setFontFeatures({{"tnum", 1}});
    m_renderer->setElide(Label::Elide::None);
    m_position->onChanged = [this] { positionChanged(); };
}

Wheel::~Wheel() = default;

int Wheel::valueAt(double position) const
{
    const long row = std::lround(position);
    if (m_column.wraps)
        return int(wrap(row, count()));
    return int(std::clamp<long>(row, 0, count() - 1));
}

double Wheel::clampPosition(double position) const
{
    return m_column.wraps ? position : std::clamp(position, 0.0, double(count() - 1));
}

Wheel::RowPlace Wheel::place(double distance)
{
    const double angle = distance * RowAngle;
    if (std::abs(angle) >= Pi / 2)
        return {};
    return {Radius * std::sin(angle), std::cos(angle)};
}

void Wheel::moveTo(double target)
{
    m_position->setTarget(clampPosition(target));
    // The value is where the drum is going, from the moment it turns.
    positionChanged();
}

void Wheel::setValue(int value, bool animate)
{
    value = std::clamp(value, 0, count() - 1);
    const double target = m_position->target();
    double next = value;
    if (m_column.wraps) {
        // The shortest way round to the value.
        int step = value - valueAt(target);
        if (step > count() / 2)
            step -= count();
        else if (step < -count() / 2)
            step += count();
        next = std::round(target) + step;
    }
    if (animate && window() && isVisible())
        m_position->setTarget(next);
    else
        m_position->jumpTo(next);
    if (value != m_value) {
        m_value = value;
        if (onValueChanged)
            onValueChanged();
    }
}

void Wheel::settle()
{
    moveTo(std::round(m_position->value()));
}

void Wheel::positionChanged()
{
    const int value = valueAt(m_position->target());
    if (value != m_value) {
        m_value = value;
        if (onValueChanged)
            onValueChanged();
    }
    requestPaint();
}

const RasterImage& Wheel::figure(int value)
{
    const double scale = window() ? window()->scale() : 1;
    if (scale != m_figureScale || measurementGeneration() != m_figureGeneration) {
        m_figures.assign(size_t(count()), RasterImage());
        m_figureScale = scale;
        m_figureGeneration = measurementGeneration();
        m_renderer->setTextLayoutMode(textLayoutMode());
    }
    RasterImage& image = m_figures[size_t(value)];
    if (image.isNull() && !m_column.values[size_t(value)].empty()) {
        m_renderer->setText(m_column.values[size_t(value)]);
        m_renderer->setColor(style::text());
        const SizeF size = m_renderer->sizeHint();
        m_renderer->setGeometry({0, 0, std::ceil(size.width), std::ceil(size.height)});
        const Bitmap bitmap = m_renderer->grab(scale);
        std::vector<uint32_t> pixels(size_t(bitmap.width()) * size_t(bitmap.height()));
        for (int y = 0; y < bitmap.height(); ++y)
            std::memcpy(pixels.data() + size_t(y) * size_t(bitmap.width()),
                        bitmap.constBits() + size_t(y) * bitmap.stride(), size_t(bitmap.width()) * 4);
        image = RasterImage(bitmap.width(), bitmap.height(), std::move(pixels), true);
    }
    return image;
}

void Wheel::paint(Painter& painter)
{
    const double scale = painter.scale();
    const double middle = geometry().height / 2;
    const double width = geometry().width;
    const double position = m_position->value();
    const long first = long(std::floor(position)) - 3;
    for (long row = first; row <= first + 7; ++row) {
        if (!m_column.wraps && (row < 0 || row >= count()))
            continue;
        const double distance = double(row) - position;
        const RowPlace at = place(distance);
        if (at.scale <= 0.05)
            continue;
        const RasterImage& image = figure(int(wrap(row, count())));
        if (image.isNull())
            continue;
        // Whole device pixels across, so the glyphs stay sharp; only the
        // height follows the drum.
        const double imageWidth = image.width() / scale;
        const double height = std::max(1.0, std::round(image.height() * at.scale)) / scale;
        const double x = style::snap(m_column.unit.empty() ? (width - imageWidth) / 2 : width - imageWidth, scale);
        const double y = style::snap(middle + at.centre - height / 2, scale);
        // The value on the middle line is whole; the rest fade with their
        // angle.
        const double nearness = std::clamp(1 - std::abs(distance) * 2, 0.0, 1.0);
        const double opacity = nearness + (1 - nearness) * 0.62 * at.scale * at.scale;
        painter.save();
        painter.multiplyOpacity(opacity);
        painter.drawImage(image.skImage(), RectF{x, y, imageWidth, height});
        painter.restore();
    }
    if (hasVisibleFocus())
        painter.strokeRoundedRect({0, middle - RowHeight / 2, width, RowHeight}, 6, style::accent(),
                                  painter.toDevice(2));
}

void Wheel::focusIn(FocusReason)
{
    m_typed.clear();
    requestPaint();
}

void Wheel::focusOut()
{
    m_typed.clear();
    requestPaint();
}

void Wheel::beginDrag(double y)
{
    m_dragging = true;
    m_dragMoved = false;
    m_dragStartY = y;
    m_dragStartPosition = m_position->value();
}

void Wheel::dragTo(double y)
{
    if (!m_dragging)
        return;
    const double delta = m_dragStartY - y;
    if (!m_dragMoved && std::abs(delta) < 3)
        return;
    m_dragMoved = true;
    m_position->jumpTo(clampPosition(m_dragStartPosition + delta / RowHeight));
}

void Wheel::endDrag(double y)
{
    if (!m_dragging)
        return;
    m_dragging = false;
    if (m_dragMoved) {
        settle();
        return;
    }
    // A click on a row turns the drum to it: the row whose place on the
    // drum is nearest the click.
    const double offset = y - geometry().height / 2;
    long best = 0;
    double bestGap = 1e9;
    for (long step = -3; step <= 3; ++step) {
        const RowPlace at = place(double(step));
        if (at.scale <= 0)
            continue;
        const double gap = std::abs(at.centre - offset);
        if (gap < bestGap) {
            bestGap = gap;
            best = step;
        }
    }
    if (best != 0)
        moveTo(std::round(m_position->target()) + double(best));
}

void Wheel::scrollBy(const ScrollEvent& event)
{
    if (event.precise) {
        if (event.stop) {
            settle();
            return;
        }
        m_position->jumpTo(clampPosition(m_position->value() + event.deltaY / RowHeight));
        return;
    }
    if (event.deltaY == 0 && event.value120Y == 0)
        return;
    const int clicks = event.value120Y != 0 ? (event.value120Y > 0 ? 1 : -1) : (event.deltaY > 0 ? 1 : -1);
    moveTo(std::round(m_position->target()) + clicks);
}

bool Wheel::keyPress(const KeyEvent& event)
{
    if (onKey && onKey(event))
        return true;
    if (event.key == key::Up || event.key == key::Down) {
        m_typed.clear();
        moveTo(std::round(m_position->target()) + (event.key == key::Down ? 1 : -1));
        return true;
    }
    if (event.text.size() != 1 || event.text[0] < '0' || event.text[0] > '9' || (event.modifiers & ControlModifier))
        return false;
    const auto find = [this](const std::string& typed) {
        const long number = numberOf(typed);
        for (int i = 0; i < count(); ++i) {
            if (numberOf(m_column.values[size_t(i)]) == number)
                return i;
        }
        return -1;
    };
    m_typed += event.text;
    int index = find(m_typed);
    if (index < 0) {
        m_typed = event.text;
        index = find(m_typed);
    }
    if (index >= 0)
        setValue(index, true);
    if (m_typed.size() >= 2)
        m_typed.clear();
    return true;
}

WheelPicker::WheelPicker(Window* window, std::vector<WheelColumn> columns)
{
    setTracksHover(true);
    for (WheelColumn& column : columns) {
        const std::string unit = column.unit;
        const std::string name = column.name;
        Wheel* wheel = addChild<Wheel>(window, std::move(column));
        const size_t index = m_wheels.size();
        wheel->onValueChanged = [this, index] {
            if (onChanged)
                onChanged(index);
        };
        wheel->onKey = [this](const KeyEvent& event) { return onKey && onKey(event); };
        m_wheels.push_back(wheel);
        Label* label = nullptr;
        if (!unit.empty()) {
            label = addChild<Label>(unit);
            if (!name.empty())
                label->setName(name + "-unit");
            label->setFontSize(15);
            label->setWeight(600);
        }
        m_units.push_back(label);
    }
}

void WheelPicker::setValue(size_t column, int value, bool animate)
{
    if (column < m_wheels.size())
        m_wheels[column]->setValue(value, animate);
}

void WheelPicker::setDrawsCard(bool draws)
{
    m_drawsCard = draws;
    requestPaint();
}

SizeF WheelPicker::sizeHint() const
{
    return {120 * double(m_wheels.size()), Wheel::Height + 2 * Padding};
}

RectF WheelPicker::bandRect() const
{
    const double middle = geometry().height / 2;
    return {BandInset, std::round(middle - Wheel::RowHeight / 2), geometry().width - 2 * BandInset,
            Wheel::RowHeight};
}

double WheelPicker::bandRadius() const
{
    return std::max(0.0, Theme::current().bigRadius - BandInset);
}

std::vector<std::pair<double, double>> WheelPicker::spans() const
{
    double total = 0;
    for (const Wheel* wheel : m_wheels)
        total += std::max(0.0, wheel->column().stretch);
    std::vector<std::pair<double, double>> spans;
    const double width = geometry().width;
    double x = 0;
    for (const Wheel* wheel : m_wheels) {
        const double share = total > 0 ? std::max(0.0, wheel->column().stretch) / total : 1.0 / m_wheels.size();
        const double next = std::round(x + share * width);
        spans.emplace_back(x, next);
        x = next;
    }
    return spans;
}

size_t WheelPicker::columnAt(double x) const
{
    const std::vector<std::pair<double, double>> columns = spans();
    for (size_t i = 0; i < columns.size(); ++i) {
        if (x < columns[i].second)
            return i;
    }
    return columns.empty() ? 0 : columns.size() - 1;
}

void WheelPicker::layout()
{
    const std::vector<std::pair<double, double>> columns = spans();
    const double top = style::centred(geometry().height, Wheel::Height);
    const double middle = top + Wheel::Height / 2;
    for (size_t i = 0; i < m_wheels.size(); ++i) {
        const auto [left, right] = columns[i];
        Label* unit = m_units[i];
        if (!unit) {
            m_wheels[i]->setGeometry({left, top, right - left, Wheel::Height});
            continue;
        }
        // The value ends a little past the column's middle, its unit after.
        const double valuesEnd = std::round((left + right) / 2) + 4;
        m_wheels[i]->setGeometry({left, top, valuesEnd - left, Wheel::Height});
        const SizeF size = unit->sizeHint();
        unit->setGeometry({valuesEnd + UnitGap, std::round(middle - size.height / 2), std::ceil(size.width),
                           std::ceil(size.height)});
    }
}

void WheelPicker::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    if (m_drawsCard)
        painter.fillRoundedRect(localRect(), theme.bigRadius, style::card(), themeCorners());
    painter.fillRoundedRect(bandRect(), bandRadius(), style::withAlpha(style::text(), 0.08), themeCorners());
}

void WheelPicker::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left || m_wheels.empty())
        return;
    m_dragColumn = int(columnAt(event.position.x));
    Wheel* wheel = m_wheels[size_t(m_dragColumn)];
    wheel->setFocus(FocusReason::Mouse);
    wheel->beginDrag(event.position.y - wheel->geometry().y);
}

void WheelPicker::pointerMove(const PointerEvent& event)
{
    if (m_dragColumn < 0)
        return;
    Wheel* wheel = m_wheels[size_t(m_dragColumn)];
    wheel->dragTo(event.position.y - wheel->geometry().y);
}

void WheelPicker::pointerRelease(const PointerEvent& event)
{
    if (m_dragColumn < 0)
        return;
    Wheel* wheel = m_wheels[size_t(m_dragColumn)];
    m_dragColumn = -1;
    wheel->endDrag(event.position.y - wheel->geometry().y);
}

bool WheelPicker::scroll(const ScrollEvent& event)
{
    if (m_wheels.empty())
        return false;
    m_wheels[m_dragColumn >= 0 ? size_t(m_dragColumn) : columnAt(m_hoverX)]->scrollBy(event);
    return true;
}

void WheelPicker::hoverMove(const PointerEvent& event)
{
    m_hoverX = event.position.x;
}

} // namespace clockapp
