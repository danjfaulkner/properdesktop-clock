// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "stopwatch/motion.h"
#include "ui/raster_image.h"
#include "ui/view.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace properdesktop::ui {
class Label;
}

namespace clockapp {

// One column of a WheelPicker.
struct WheelColumn
{
    // What each value shows, in order: "0" to "59", "1" to "12", "AM" and
    // "PM". Values that read as numbers can also be typed.
    std::vector<std::string> values;
    // Shown in the band after the column's values; empty for none, and the
    // values are then centred in the column.
    std::string unit;
    // Turns round from the last value to the first; off, the drum stops at
    // its ends, as a two-value AM/PM column does.
    bool wraps = true;
    // The column's share of the picker's width, against the others'.
    double stretch = 1;
    // Its view's name, for tests; the unit's is the name with "-unit".
    std::string name;
};

// A drum of a column's values that turns, seen from the front: the value on
// the middle line is the current one, and the rows above and below shrink
// in height and fade with the cosine of their angle on the drum. Glyphs are
// drawn whole device pixels across and scaled in height only. Up and Down
// step it and typed digits pick a value that reads as their number; the
// WheelPicker around it turns it by the pointer.
class Wheel : public properdesktop::ui::View
{
public:
    static constexpr double RowHeight = 34;
    // The angle between two rows on the drum, in radians.
    static constexpr double RowAngle = 0.5;
    // The drum's radius: a row's height as the arc of its angle.
    static constexpr double Radius = RowHeight / RowAngle;
    static constexpr double Height = 2 * Radius + 4;

    Wheel(properdesktop::ui::Window* window, WheelColumn column);
    ~Wheel() override;

    const WheelColumn& column() const { return m_column; }
    int value() const { return m_value; }
    void setValue(int value, bool animate = false);

    // Turning by the pointer, in the wheel's coordinates.
    void beginDrag(double y);
    void dragTo(double y);
    void endDrag(double y);
    void scrollBy(const properdesktop::ui::ScrollEvent& event);

    // A row's centre below the middle line and its height scale, at a
    // distance in rows from it; a scale of 0 past the drum's edge.
    struct RowPlace
    {
        double centre = 0;
        double scale = 0;
    };
    static RowPlace place(double distance);

    bool acceptsFocus() const override { return true; }

    std::function<void()> onValueChanged;
    // Asked first for each key the focused drum gets; true takes the key.
    std::function<bool(const properdesktop::ui::KeyEvent&)> onKey;

protected:
    void paint(properdesktop::ui::Painter& painter) override;
    bool keyPress(const properdesktop::ui::KeyEvent& event) override;
    void focusIn(properdesktop::ui::FocusReason) override;
    void focusOut() override;

private:
    int count() const { return int(m_column.values.size()); }
    // The value a position shows on the middle line, or -1 off the ends of
    // a column that does not wrap.
    int valueAt(double position) const;
    double clampPosition(double position) const;
    void moveTo(double target);
    void settle();
    void positionChanged();
    // The values drawn once at the window's scale.
    const properdesktop::ui::RasterImage& figure(int value);

    WheelColumn m_column;
    int m_value = 0;
    // In rows, unbounded when the column wraps.
    std::unique_ptr<Glide> m_position;
    std::unique_ptr<properdesktop::ui::Label> m_renderer;
    std::vector<properdesktop::ui::RasterImage> m_figures;
    double m_figureScale = 0;
    uint64_t m_figureGeneration = 0;
    bool m_dragging = false;
    bool m_dragMoved = false;
    double m_dragStartY = 0;
    double m_dragStartPosition = 0;
    std::string m_typed;
};

// Columns of drums in a card, the middle line marked by a band across them
// with the units inside it. The whole card takes the pointer: a press, a
// drag, a click on a row or a scroll turns the column under it, the units
// and the gaps included.
class WheelPicker : public properdesktop::ui::View
{
public:
    static constexpr double BandInset = 6;
    static constexpr double Padding = 14;

    WheelPicker(properdesktop::ui::Window* window, std::vector<WheelColumn> columns);

    size_t columnCount() const { return m_wheels.size(); }
    Wheel* wheel(size_t column) const { return m_wheels[column]; }
    int value(size_t column) const { return m_wheels[column]->value(); }
    void setValue(size_t column, int value, bool animate = false);

    // The band, in the picker's coordinates, and its radius: concentric
    // with the card's corners.
    properdesktop::ui::RectF bandRect() const;
    double bandRadius() const;
    // The column a point across the picker belongs to.
    size_t columnAt(double x) const;
    // Without the card, for a picker set on a card of its own.
    void setDrawsCard(bool draws);

    properdesktop::ui::SizeF sizeHint() const override;
    bool acceptsPointer() const override { return true; }

    // A column's value changed.
    std::function<void(size_t column)> onChanged;
    // Asked first for each key a drum of the picker gets, before the drum's
    // own keys; true takes the key, as for typing a whole time across the
    // drums.
    std::function<bool(const properdesktop::ui::KeyEvent&)> onKey;

protected:
    void layout() override;
    void paint(properdesktop::ui::Painter& painter) override;
    void pointerPress(const properdesktop::ui::PointerEvent& event) override;
    void pointerMove(const properdesktop::ui::PointerEvent& event) override;
    void pointerRelease(const properdesktop::ui::PointerEvent& event) override;
    bool scroll(const properdesktop::ui::ScrollEvent& event) override;
    void hoverMove(const properdesktop::ui::PointerEvent& event) override;

private:
    // Each column's span across the picker.
    std::vector<std::pair<double, double>> spans() const;

    std::vector<Wheel*> m_wheels;
    std::vector<properdesktop::ui::Label*> m_units;
    bool m_drawsCard = true;
    int m_dragColumn = -1;
    double m_hoverX = 0;
};

} // namespace clockapp
