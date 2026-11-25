// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "timers/composer.h"

#include "alarms/tones.h"
#include "stopwatch/roundbutton.h"
#include "style.h"
#include "timers/wheelpicker.h"

#include <base/i18n.h>

#include "ui/label.h"
#include "ui/painter.h"
#include "ui/row_combo_box.h"
#include "ui/scroll_view.h"
#include "ui/text_field.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>

using namespace properdesktop::ui;
using properdesktop::i18n::tr;

namespace clockapp {

// The rows of a card, as the alarm editor and Settings have them.
static constexpr double RowHeight = 44;
static constexpr double RowInset = 14;
static constexpr double RecentStartDiameter = 36;

static Corners themeCorners()
{
    return Theme::current().continuousCorners ? Corners::Continuous : Corners::Circular;
}

static ColorRecipe secondaryColor()
{
    return ColorRecipe(std::function<Color(const ColorProvider&)>(
        [](const ColorProvider&) { return style::secondaryText(); }));
}

// A card of rows, a label on the left and its control on the right, a
// hairline between them.
class TimerComposer::OptionsCard : public View
{
public:
    View* addRow(const std::string& text, std::unique_ptr<View> control)
    {
        View* row = addChild<View>();
        Label* label = row->addChild<Label>(text);
        label->setVerticallyCentered(true);
        row->addChild(std::move(control));
        return row->children()[1].get();
    }

    double heightForWidth(double) const override { return RowHeight * double(children().size()); }
    SizeF sizeHint() const override { return {0, heightForWidth(0)}; }

protected:
    void layout() override
    {
        const double width = geometry().width;
        double y = 0;
        for (const auto& row : children()) {
            row->setGeometry({0, y, width, RowHeight});
            y += RowHeight;
            auto* label = static_cast<Label*>(row->children()[0].get());
            View* control = row->children()[1].get();
            const SizeF size = control->sizeHint();
            const double controlWidth = std::ceil(control->layoutHints().preferredWidth > 0
                                                      ? control->layoutHints().preferredWidth
                                                      : size.width);
            const double controlHeight = std::ceil(size.height);
            double overhang = 0;
            if (const auto* box = dynamic_cast<const RowComboBox*>(control))
                overhang = box->trailingOverhang();
            control->setGeometry({width - RowInset - controlWidth + overhang, style::centred(RowHeight, controlHeight),
                                  controlWidth, controlHeight});
            const double labelHeight = std::ceil(label->sizeHint().height);
            label->setGeometry({RowInset, style::centred(RowHeight, labelHeight),
                                std::max(0.0, width - 3 * RowInset - controlWidth + overhang), labelHeight});
        }
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        painter.fillRoundedRect(localRect(), theme.bigRadius, style::card(), themeCorners());
        const double hairline = 1 / painter.scale();
        for (size_t i = 0; i + 1 < children().size(); ++i) {
            const RectF row = children()[i]->geometry();
            painter.fillRoundedRect({RowInset, row.bottom() - hairline, geometry().width - 2 * RowInset, hairline}, 0,
                                    style::separator());
        }
    }
};

// The recent timers' rows: the duration, the label and sound under it, and
// a play button that keeps one gap to the row's end, top and bottom.
class RecentsPanel::RecentList : public View
{
public:
    explicit RecentList(Window* window)
        : m_window(window)
    {
    }

    void setRecents(const std::vector<timermath::Recent>& recents)
    {
        while (m_rows.size() > recents.size()) {
            takeChild(m_rows.back().view);
            m_rows.pop_back();
        }
        while (m_rows.size() < recents.size()) {
            Row row;
            row.view = addChild<View>();
            row.view->setName("timers:recent");
            row.duration = row.view->addChild<Label>();
            row.duration->setName("timers:recent-duration");
            row.duration->setFontSize(20);
            row.duration->setWeight(400);
            row.duration->setFontFeatures({{"tnum", 1}});
            row.detail = row.view->addChild<Label>();
            row.detail->setFontSize(13);
            row.detail->setColor(secondaryColor());
            row.start = row.view->addChild<RoundButton>(m_window, RecentStartDiameter);
            row.start->setName("timers:recent-start");
            row.start->setTitle(tr("Start"));
            row.start->setPlayGlyph(true);
            row.start->setRole(RoundButton::Role::Start);
            const size_t index = m_rows.size();
            row.start->onClicked = [this, index] {
                if (onStart)
                    onStart(index);
            };
            m_rows.push_back(row);
        }
        for (size_t i = 0; i < recents.size(); ++i) {
            const timermath::Recent& recent = recents[i];
            m_rows[i].duration->setText(timermath::formatRemaining(recent.durationMs));
            std::string detail = recent.label.empty() ? timermath::formatDuration(recent.durationMs) : recent.label;
            detail += " · " + tones::displayName(recent.sound);
            m_rows[i].detail->setText(detail);
        }
        requestLayout();
    }

    SizeF sizeHint() const override { return {0, heightForWidth(0)}; }
    double heightForWidth(double) const override { return TimerComposer::RecentRowHeight * double(m_rows.size()); }

    std::function<void(size_t index)> onStart;

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double rowHeight = TimerComposer::RecentRowHeight;
        const double gap = (rowHeight - RecentStartDiameter) / 2;
        for (size_t i = 0; i < m_rows.size(); ++i) {
            Row& row = m_rows[i];
            row.view->setGeometry({0, rowHeight * double(i), width, rowHeight});
            const double durationHeight = std::ceil(row.duration->sizeHint().height);
            const double detailHeight = std::ceil(row.detail->sizeHint().height);
            const double textTop = style::centred(rowHeight, durationHeight + detailHeight);
            const double textWidth = std::max(0.0, width - RowInset - gap - RecentStartDiameter - 8);
            row.duration->setGeometry({RowInset, textTop, textWidth, durationHeight});
            row.detail->setGeometry({RowInset, textTop + durationHeight, textWidth, detailHeight});
            row.start->setGeometry({width - gap - RecentStartDiameter, gap, RecentStartDiameter, RecentStartDiameter});
        }
    }

    void paint(Painter& painter) override
    {
        const double hairline = 1 / painter.scale();
        for (size_t i = 1; i < m_rows.size(); ++i) {
            const double y = TimerComposer::RecentRowHeight * double(i);
            painter.fillRoundedRect({RowInset, y - hairline, geometry().width - 2 * RowInset, hairline}, 0,
                                    style::separator());
        }
    }

private:
    struct Row
    {
        View* view = nullptr;
        Label* duration = nullptr;
        Label* detail = nullptr;
        RoundButton* start = nullptr;
    };

    Window* m_window;
    std::vector<Row> m_rows;
};

RecentsPanel::RecentsPanel(Window* window)
{
    m_title = addChild<Label>(tr("Recent"));
    m_title->setName("timers:recents-title");
    m_title->setFontSize(15);
    m_title->setWeight(600);
    m_view = addChild<ScrollView>();
    m_view->setName("timers:recents");
    m_view->setScrollDirections(false, true);
    m_view->setCornerRadius(Theme::current().bigRadius);
    m_list = m_view->setContent<RecentList>(window);
    m_list->onStart = [this](size_t index) {
        if (onStartRecent)
            onStartRecent(index);
    };
}

RecentsPanel::~RecentsPanel() = default;

void RecentsPanel::setRecents(const std::vector<timermath::Recent>& recents)
{
    m_list->setRecents(recents);
    requestLayout();
    requestPaint();
}

RectF RecentsPanel::cardRect() const
{
    const double titleHeight = std::ceil(m_title->sizeHint().height);
    const double top = titleHeight + 8;
    const double room = std::max(0.0, geometry().height - top);
    const double width = geometry().width;
    const double height = std::min(room, m_list->heightForWidth(width));
    return {0, top, width, height};
}

void RecentsPanel::layout()
{
    const double titleHeight = std::ceil(m_title->sizeHint().height);
    m_title->setGeometry({4, 0, std::max(0.0, geometry().width - 4), titleHeight});
    m_view->setGeometry(cardRect());
}

void RecentsPanel::paint(Painter& painter)
{
    const RectF card = cardRect();
    if (card.height <= 0)
        return;
    const Theme& theme = Theme::current();
    painter.fillRoundedRect(card, theme.bigRadius, style::card(), themeCorners());
}

TimerComposer::TimerComposer(Window* window)
{
    setName("timers:composer");
    m_picker = addChild<DurationPicker>(window);
    m_picker->onChanged = [this](size_t) { updateStart(); };

    m_options = addChild<OptionsCard>();
    auto label = std::make_unique<TextField>();
    m_label = label.get();
    m_label->setName("timers:label");
    m_label->setFrameVisible(false);
    m_label->setRightAligned(true);
    m_label->setPlaceholder(tr("Timer"));
    m_label->setMaximumLength(60);
    m_label->setLayoutHints({.preferredWidth = 200});
    m_options->addRow(tr("Label"), std::move(label));

    std::vector<std::string> sounds;
    for (const std::string& name : clockstore::soundNames())
        sounds.push_back(tones::displayName(name));
    auto sound = std::make_unique<RowComboBox>(sounds);
    m_sound = sound.get();
    m_sound->setName("timers:sound");
    m_sound->setCurrentIndex(0);
    m_options->addRow(tr("Sound"), std::move(sound));

    m_cancel = addChild<RoundButton>(window, StartDiameter);
    m_cancel->setName("timers:composer-cancel");
    m_cancel->setTitle(tr("Cancel"));
    m_cancel->setVisible(false);
    m_cancel->onClicked = [this] {
        if (onCancel)
            onCancel();
    };
    m_start = addChild<RoundButton>(window, StartDiameter);
    m_start->setName("timers:start");
    m_start->setTitle(tr("Start"));
    m_start->setRole(RoundButton::Role::Start);
    m_start->onClicked = [this] {
        if (m_picker->durationMs() > 0 && onStart)
            onStart();
    };
    m_label->onAccepted = [this] { m_start->onClicked(); };

    m_picker->setDurationMs(5 * 60 * 1000);
    updateStart();
}

TimerComposer::~TimerComposer() = default;

std::string TimerComposer::label() const
{
    return m_label->text();
}

std::string TimerComposer::sound() const
{
    const std::vector<std::string>& names = clockstore::soundNames();
    const int index = m_sound->currentIndex();
    return index >= 0 && size_t(index) < names.size() ? names[size_t(index)] : names.front();
}

void TimerComposer::clearLabel()
{
    m_label->setText({});
}

void TimerComposer::setMargins(double top, double left, double right)
{
    if (top == m_topMargin && left == m_leftMargin && right == m_rightMargin)
        return;
    m_topMargin = top;
    m_leftMargin = left;
    m_rightMargin = right;
    requestLayout();
}

void TimerComposer::setCancelVisible(bool visible)
{
    m_cancel->setVisible(visible);
}

void TimerComposer::updateStart()
{
    m_start->setEnabled(m_picker->durationMs() > 0);
}

RectF TimerComposer::groupRect() const
{
    const double room = std::max(0.0, geometry().width - m_leftMargin - m_rightMargin);
    const double column = std::min(ColumnWidth, room);
    const double x = std::round(m_leftMargin + (room - column) / 2);
    const double height = m_picker->sizeHint().height + 12 + m_options->sizeHint().height + 16 + StartDiameter;
    return {x, m_topMargin + 16, column, height};
}

void TimerComposer::layout()
{
    const RectF group = groupRect();
    const double pickerHeight = m_picker->sizeHint().height;
    m_picker->setGeometry({group.x, group.y, group.width, pickerHeight});
    const double optionsTop = group.y + pickerHeight + 12;
    const double optionsHeight = m_options->sizeHint().height;
    m_options->setGeometry({group.x, optionsTop, group.width, optionsHeight});
    const double buttonsTop = optionsTop + optionsHeight + 16;
    m_cancel->setGeometry({group.x, buttonsTop, StartDiameter, StartDiameter});
    // Start sits on the drums' centre line; Cancel, when there is one, at
    // the group's start.
    const double scale = window() ? window()->scale() : 1;
    m_start->setGeometry({style::snap(group.x + (group.width - StartDiameter) / 2, scale), buttonsTop, StartDiameter,
                          StartDiameter});
}

} // namespace clockapp
