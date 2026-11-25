// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "stopwatch/digitdisplay.h"

#include "style.h"

#include "ui/label.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>

using namespace properdesktop::ui;

namespace clockapp {

// The characters a time is written with; any other measures as a digit.
static const char MeasuredCharacters[] = "0:.-";

DigitDisplay::DigitDisplay(Window* window, double fontSize, int weight)
    : m_window(window)
    , m_fontSize(fontSize)
    , m_weight(weight)
{
    // A figure rolls through the line's box, not past it.
    m_line = addChild<View>();
    m_line->setClipsChildren(true);
    for (const char* c = MeasuredCharacters; *c; ++c) {
        Label* label = makeLabel(*c);
        label->setVisible(false);
        m_measures.emplace_back(*c, label);
    }
}

DigitDisplay::~DigitDisplay() = default;

Label* DigitDisplay::makeLabel(char character)
{
    Label* label = m_line->addChild<Label>(std::string(1, character));
    label->setFontSize(m_fontSize);
    label->setWeight(m_weight);
    label->setFontFeatures({{"tnum", 1}});
    label->setElide(Label::Elide::None);
    label->setTextAlignment(Label::TextAlignment::Center);
    if (m_hasColor)
        label->setColor(m_color);
    return label;
}

double DigitDisplay::advance(char character) const
{
    const char key = (character >= '0' && character <= '9') ? '0' : character;
    for (const auto& [c, label] : m_measures) {
        if (c == key)
            return std::ceil(label->sizeHint().width);
    }
    return std::ceil(m_measures.front().second->sizeHint().width);
}

double DigitDisplay::lineWidth() const
{
    double width = 0;
    for (char c : m_text)
        width += advance(c);
    return width;
}

double DigitDisplay::lineStart() const
{
    return style::centred(geometry().width, lineWidth());
}

double DigitDisplay::lineHeight() const
{
    return m_measures.front().second->sizeHint().height;
}

SizeF DigitDisplay::sizeHint() const
{
    return {lineWidth(), lineHeight()};
}

void DigitDisplay::setColor(Color color)
{
    m_color = color;
    m_hasColor = true;
    for (Cell& cell : m_cells) {
        cell.current->setColor(color);
        if (cell.outgoing)
            cell.outgoing->setColor(color);
    }
}

void DigitDisplay::setText(const std::string& text)
{
    if (text == m_text)
        return;
    const bool widthChanged = text.size() != m_text.size();
    const bool roll = !m_skipRoll && !widthChanged && isVisible() && window();
    m_skipRoll = false;
    m_text = text;

    if (widthChanged) {
        for (Cell& cell : m_cells) {
            m_line->takeChild(cell.current);
            if (cell.outgoing)
                m_line->takeChild(cell.outgoing);
        }
        m_cells.clear();
        m_cells.resize(text.size());
        for (size_t i = 0; i < text.size(); ++i) {
            Cell& cell = m_cells[i];
            cell.character = text[i];
            cell.current = makeLabel(text[i]);
            cell.roll = std::make_unique<Glide>(m_window, 170, 1);
            cell.roll->setRestDistance(0.01);
            cell.roll->onChanged = [this, i] { placeCell(i); };
        }
        requestLayout();
        return;
    }

    for (size_t i = 0; i < text.size(); ++i) {
        Cell& cell = m_cells[i];
        if (cell.character == text[i])
            continue;
        cell.character = text[i];
        const bool steady = i + m_steadyCells >= text.size();
        if (!roll || steady) {
            cell.current->setText(std::string(1, text[i]));
            if (cell.outgoing) {
                m_line->takeChild(cell.outgoing);
                cell.outgoing = nullptr;
            }
            cell.roll->jumpTo(1);
            continue;
        }
        // A roll caught mid-way drops the oldest figure and carries on from
        // where the newest one is.
        if (cell.outgoing)
            m_line->takeChild(cell.outgoing);
        cell.outgoing = cell.current;
        cell.current = makeLabel(text[i]);
        cell.roll->jumpTo(0);
        cell.roll->setTarget(1);
    }
    requestLayout();
}

void DigitDisplay::placeCell(size_t index)
{
    if (index >= m_cells.size())
        return;
    Cell& cell = m_cells[index];
    double x = 0;
    for (size_t i = 0; i < index; ++i)
        x += advance(m_text[i]);
    const double width = advance(m_text[index]);
    const double height = lineHeight();
    const double y = 0;
    const double progress = cell.roll ? cell.roll->value() : 1;
    const double distance = std::round(m_fontSize * 0.6);
    const double scale = window() ? window()->scale() : 1;
    cell.current->setGeometry({x, style::snap(y + (1 - progress) * distance, scale), width, height});
    cell.current->setOpacity(progress);
    if (cell.outgoing) {
        if (progress >= 1) {
            m_line->takeChild(cell.outgoing);
            cell.outgoing = nullptr;
        } else {
            cell.outgoing->setGeometry({x, style::snap(y - progress * distance, scale), width, height});
            cell.outgoing->setOpacity((1 - progress) * (1 - progress));
        }
    }
}

void DigitDisplay::layout()
{
    const double scale = window() ? window()->scale() : 1;
    m_line->setGeometry({lineStart(), style::snap(style::centred(geometry().height, lineHeight()), scale),
                         lineWidth(), lineHeight()});
    for (size_t i = 0; i < m_cells.size(); ++i)
        placeCell(i);
}

} // namespace clockapp
