// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "stopwatch/motion.h"
#include "ui/view.h"

#include <memory>
#include <string>
#include <vector>

namespace properdesktop::ui {
class Label;
}

namespace clockapp {

// A time in figures of one width, each character in a cell of its own and
// the line centred in the view. A cell whose character changes rolls the new
// one up into place, unless it is one of the last `steadyCells` (hundredths
// change too fast to roll) or the view is not on screen.
class DigitDisplay : public properdesktop::ui::View
{
public:
    DigitDisplay(properdesktop::ui::Window* window, double fontSize, int weight);
    ~DigitDisplay() override;

    void setText(const std::string& text);
    const std::string& text() const { return m_text; }
    void setSteadyCells(size_t count) { m_steadyCells = count; }
    void setColor(properdesktop::ui::Color color);
    // No roll for the next change, as when the page shows again.
    void skipRoll() { m_skipRoll = true; }

    // The line's width, and where it starts in the view.
    double lineWidth() const;
    double lineStart() const;
    // A tight box around the capitals' height, centred on the line.
    double lineHeight() const;

    properdesktop::ui::SizeF sizeHint() const override;

protected:
    void layout() override;

private:
    struct Cell
    {
        char character = 0;
        properdesktop::ui::Label* current = nullptr;
        properdesktop::ui::Label* outgoing = nullptr;
        std::unique_ptr<Glide> roll;
    };

    properdesktop::ui::Label* makeLabel(char character);
    double advance(char character) const;
    void placeCell(size_t index);

    properdesktop::ui::Window* m_window;
    // The line's box, which the cells roll inside.
    properdesktop::ui::View* m_line = nullptr;
    double m_fontSize;
    int m_weight;
    std::string m_text;
    size_t m_steadyCells = 0;
    bool m_skipRoll = true;
    properdesktop::ui::Color m_color;
    bool m_hasColor = false;
    std::vector<Cell> m_cells;
    // One label per character measured, kept hidden.
    mutable std::vector<std::pair<char, properdesktop::ui::Label*>> m_measures;
};

} // namespace clockapp
