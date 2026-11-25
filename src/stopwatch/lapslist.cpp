// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "stopwatch/lapslist.h"

#include "stopwatch/stopwatch.h"
#include "style.h"

#include <base/i18n.h>

#include "ui/label.h"
#include "ui/painter.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>

using namespace properdesktop::ui;
using properdesktop::i18n::format;
using properdesktop::i18n::tr;

namespace clockapp {

enum Mark {
    NoMark,
    Fastest,
    Slowest,
};

static constexpr double FontSize = 15;

LapsList::LapsList(Window* window)
    : m_window(window)
    , m_slide(std::make_unique<Glide>(window, 300, 0))
{
    m_slide->onChanged = [this] {
        layout();
        requestPaint();
    };
}

LapsList::~LapsList() = default;

LapsList::Row LapsList::makeRow()
{
    Row row;
    row.view = addChild<View>();
    row.view->setName("laps:row");
    row.name = row.view->addChild<Label>();
    row.name->setName("laps:name");
    row.name->setFontSize(FontSize);
    row.name->setFontFeatures({{"tnum", 1}});
    row.time = row.view->addChild<Label>();
    row.time->setName("laps:time");
    row.time->setFontSize(FontSize);
    row.time->setFontFeatures({{"tnum", 1}});
    row.time->setTextAlignment(Label::TextAlignment::Right);
    return row;
}

void LapsList::setRow(Row& row, size_t lapNumber, int64_t ns, int mark)
{
    row.name->setText(format(tr("Lap {n}"), {{"n", std::to_string(lapNumber)}}));
    row.time->setText(formatElapsed(ns));
    std::optional<ColorRecipe> color;
    if (mark == Fastest)
        color = ColorRecipe(std::function<Color(const ColorProvider&)>([](const ColorProvider&) {
            return style::green();
        }));
    else if (mark == Slowest)
        color = ColorRecipe(std::function<Color(const ColorProvider&)>([](const ColorProvider&) {
            return style::red();
        }));
    row.name->setColor(color);
    row.time->setColor(color);
}

void LapsList::setLaps(const std::vector<int64_t>& durations, std::optional<LapExtremes> extremes, int64_t currentNs,
                       bool animate)
{
    const size_t count = durations.size() + (currentNs >= 0 ? 1 : 0);
    const bool added = count == m_rows.size() + 1 && !m_rows.empty();
    while (m_rows.size() < count)
        m_rows.push_back(makeRow());
    while (m_rows.size() > count) {
        takeChild(m_rows.back().view);
        m_rows.pop_back();
    }
    m_hasCurrent = currentNs >= 0;

    size_t index = 0;
    if (m_hasCurrent)
        setRow(m_rows[index++], durations.size() + 1, currentNs, NoMark);
    for (size_t lap = durations.size(); lap-- > 0;) {
        int mark = NoMark;
        if (extremes && extremes->fastest == lap)
            mark = Fastest;
        else if (extremes && extremes->slowest == lap)
            mark = Slowest;
        setRow(m_rows[index++], lap + 1, durations[lap], mark);
    }

    if (added && animate && isVisible() && window()) {
        m_slide->jumpTo(-RowHeight);
        m_slide->setTarget(0);
    } else {
        m_slide->jumpTo(0);
    }
    requestLayout();
}

void LapsList::setCurrent(int64_t currentNs)
{
    if (!m_hasCurrent || m_rows.empty())
        return;
    m_rows.front().time->setText(formatElapsed(currentNs));
}

void LapsList::setInsets(double left, double right)
{
    if (left == m_left && right == m_right)
        return;
    m_left = left;
    m_right = right;
    layout();
    requestPaint();
}

SizeF LapsList::sizeHint() const
{
    return {0, RowHeight * double(m_rows.size())};
}

void LapsList::layout()
{
    const double width = std::max(0.0, geometry().width - m_left - m_right);
    const double offset = m_slide->value();
    const double scale = window() ? window()->scale() : 1;
    for (size_t i = 0; i < m_rows.size(); ++i) {
        Row& row = m_rows[i];
        // The rows slide down together, the newest coming in from above
        // the list's top as it fades in.
        const double y = double(i) * RowHeight + offset;
        row.view->setGeometry({m_left, style::snap(y, scale), width, RowHeight});
        row.view->setOpacity(i == 0 ? 1 + offset / RowHeight : 1);
        const double height = std::ceil(row.name->sizeHint().height);
        const double top = style::centred(RowHeight, height);
        row.name->setGeometry({0, top, width / 2, height});
        row.time->setGeometry({width / 2, top, width / 2, height});
    }
}

void LapsList::paint(Painter& painter)
{
    const double offset = m_slide->value();
    const Color line = style::separator();
    const double hairline = 1 / painter.scale();
    const double width = std::max(0.0, geometry().width - m_left - m_right);
    for (size_t i = 0; i < m_rows.size(); ++i) {
        const double y = double(i) * RowHeight + offset;
        painter.drawHorizontalLine(m_left, style::snap(y, painter.scale()), width, line);
        if (i + 1 == m_rows.size())
            painter.drawHorizontalLine(m_left, style::snap(y + RowHeight - hairline, painter.scale()), width, line);
    }
}

} // namespace clockapp
