// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "alarms/alarmspage.h"

#include "alarms/alarmeditor.h"
#include "alarms/alarmrow.h"
#include "alarms/alarmtime.h"
#include "alarms/spring.h"
#include "pagecontext.h"
#include "style.h"

#include <base/event_loop.h>
#include <base/i18n.h>
#include <base/locale.h>

#include "ui/dialog_window.h"
#include "ui/header_backdrop.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/scroll_bar.h"
#include "ui/scroll_view.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <numbers>

using namespace properdesktop::ui;
using properdesktop::i18n::tr;

namespace clockapp {

static constexpr double RowGap = 12;
static constexpr double ListWidth = 560;
static constexpr double ListTop = 12;
static constexpr double ListBottom = 24;
static constexpr double DialSize = 112;

// The rows, one under another; a row glides to its new place when one
// before it comes or goes, and a new one rises in.
class AlarmList : public View
{
public:
    explicit AlarmList(Window* window)
        : m_window(window)
    {
    }

    void setTopMargin(double margin)
    {
        if (margin == m_topMargin)
            return;
        m_topMargin = margin;
        requestLayout();
    }

    AlarmRow* row(const std::string& id) const
    {
        for (const Entry& entry : m_entries) {
            if (entry.row->id() == id)
                return entry.row;
        }
        return nullptr;
    }

    size_t count() const { return m_entries.size(); }

    // Rows for the ids in order; new ones made by `make`, gone ones removed.
    void sync(const std::vector<std::string>& ids, const std::function<AlarmRow*(AlarmList&, const std::string&)>& make,
              bool animate)
    {
        std::vector<Entry> entries;
        for (const std::string& id : ids) {
            auto found = std::find_if(m_entries.begin(), m_entries.end(),
                                      [&id](const Entry& entry) { return entry.row->id() == id; });
            if (found != m_entries.end()) {
                entries.push_back(std::move(*found));
                m_entries.erase(found);
                continue;
            }
            Entry entry;
            entry.row = make(*this, id);
            entry.y = std::make_unique<Spring>(m_window, 340);
            entry.appear = std::make_unique<Spring>(m_window, 380, animate ? 0 : 1);
            entry.fresh = true;
            entry.y->onChanged = [this] { requestLayout(); };
            entry.appear->onChanged = [this] { requestLayout(); };
            if (animate)
                entry.appear->setTarget(1);
            entries.push_back(std::move(entry));
        }
        for (Entry& gone : m_entries)
            takeChild(gone.row);
        m_entries = std::move(entries);
        m_animate = animate;
        requestLayout();
    }

    double heightForWidth(double) const override
    {
        if (m_entries.empty())
            return 0;
        return m_topMargin + ListTop + double(m_entries.size()) * (AlarmRow::Height + RowGap) - RowGap + ListBottom;
    }
    SizeF sizeHint() const override { return {0, heightForWidth(0)}; }

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double rowWidth = std::min(ListWidth, width - 2 * style::pagePadding);
        const double x = style::centred(width, rowWidth);
        const double scale = m_window->scale();
        double y = m_topMargin + ListTop;
        for (Entry& entry : m_entries) {
            if (entry.fresh || !m_animate) {
                entry.y->jumpTo(y);
                entry.fresh = false;
            } else {
                entry.y->setTarget(y);
            }
            const double appear = entry.appear->value();
            // Rising in from a little below as it fades in.
            double top = entry.y->value() + 10 * (1 - appear);
            if (!entry.y->isMoving() && !entry.appear->isMoving())
                top = style::snap(top, scale);
            entry.row->setOpacity(appear);
            entry.row->setGeometry({x, top, rowWidth, AlarmRow::Height});
            y += AlarmRow::Height + RowGap;
        }
    }

private:
    struct Entry
    {
        AlarmRow* row = nullptr;
        std::unique_ptr<Spring> y;
        std::unique_ptr<Spring> appear;
        bool fresh = true;
    };

    Window* m_window;
    std::vector<Entry> m_entries;
    double m_topMargin = 0;
    bool m_animate = false;
};

// A clock face with its hands at seven, which swing into place when it
// shows; nothing moves after that.
class DialArt : public View
{
public:
    explicit DialArt(Window* window)
        : m_swing(std::make_unique<Spring>(window, 900, 1))
    {
        m_swing->onChanged = [this] { requestPaint(); };
    }

    void swingIn()
    {
        m_swing->jumpTo(0);
        m_swing->setTarget(1);
    }

    SizeF sizeHint() const override { return {DialSize, DialSize}; }

protected:
    void paint(Painter& painter) override
    {
        const RectF face = localRect();
        const PointF centre{face.width / 2, face.height / 2};
        const double radius = face.width / 2;
        painter.drawShadow(face, radius, 10, Color{0, 0, 0, 70}, {0, 4});
        // Lit from above: the face a shade lighter at the top.
        const Color bottom = style::card();
        const Color top{uint8_t(std::min(255, bottom.r + 18)), uint8_t(std::min(255, bottom.g + 18)),
                        uint8_t(std::min(255, bottom.b + 18)), 255};
        painter.fillRoundedRect(face, radius, LinearGradient::vertical(face, top, bottom));
        painter.strokeRoundedRect(face, radius, style::separator(), 1);

        const double px = painter.scale();
        for (int i = 0; i < 12; ++i) {
            const double angle = i * std::numbers::pi / 6;
            const double outer = radius - 9;
            const double inner = outer - (i % 3 == 0 ? 8 : 4);
            const PointF a{centre.x + std::sin(angle) * inner, centre.y - std::cos(angle) * inner};
            const PointF b{centre.x + std::sin(angle) * outer, centre.y - std::cos(angle) * outer};
            painter.drawLine(a, b, style::withAlpha(style::text(), i % 3 == 0 ? 0.5 : 0.22), (i % 3 == 0 ? 2.0 : 1.5) * px);
        }

        // From twelve to seven o'clock, overshooting a hair as a spring settles.
        const double t = m_swing->value();
        const double hourAngle = (7.0 / 12 * 2 * std::numbers::pi) * t;
        const double minuteAngle = 2 * std::numbers::pi * t;
        const auto hand = [&](double angle, double length, double width, Color color) {
            const PointF tip{centre.x + std::sin(angle) * length, centre.y - std::cos(angle) * length};
            const PointF tail{centre.x - std::sin(angle) * 6, centre.y + std::cos(angle) * 6};
            painter.drawLine(tail, tip, color, width * px);
        };
        hand(hourAngle, radius * 0.48, 3.5, style::text());
        hand(minuteAngle, radius * 0.7, 2.5, style::text());
        const double dot = 7;
        painter.fillEllipse({centre.x - dot / 2, centre.y - dot / 2, dot, dot}, style::accent());
    }

private:
    std::unique_ptr<Spring> m_swing;
};

// What the page shows before there is any alarm.
class AlarmsEmptyState : public View
{
public:
    explicit AlarmsEmptyState(Window* window)
    {
        setName("alarms:empty");
        m_dial = addChild<DialArt>(window);
        m_title = addChild<Label>(tr("No Alarms"));
        m_title->setFontSize(style::points(15));
        m_title->setWeight(600);
        m_title->setTextAlignment(Label::TextAlignment::Center);
        m_text = addChild<Label>(tr("Alarms ring even when Clock is closed."));
        m_text->setColor(ColorRecipe(style::secondaryText()));
        m_text->setTextAlignment(Label::TextAlignment::Center);
        m_text->setWrap(true);
        m_button = addChild<DialogButton>(tr("New Alarm"), true);
        m_button->setName("alarms:emptyAdd");
        m_button->onClicked = [this] {
            if (onAdd)
                onAdd();
        };
    }

    void swingIn() { m_dial->swingIn(); }

    std::function<void()> onAdd;

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double textWidth = std::min(300.0, width - 2 * style::pagePadding);
        const double titleHeight = std::ceil(m_title->sizeHint().height);
        const double textHeight = std::ceil(m_text->heightForWidth(textWidth));
        const SizeF button = m_button->sizeHint();
        const double buttonWidth = std::ceil(std::max(button.width, 132.0));
        const double total = DialSize + 24 + titleHeight + 4 + textHeight + 20 + button.height;
        // A little above the middle, where the eye rests.
        double y = std::round((m_area - total) * 0.44) + m_top;
        m_dial->setGeometry({style::centred(width, DialSize), y, DialSize, DialSize});
        y += DialSize + 24;
        m_title->setGeometry({style::centred(width, textWidth), y, textWidth, titleHeight});
        y += titleHeight + 4;
        m_text->setGeometry({style::centred(width, textWidth), y, textWidth, textHeight});
        y += textHeight + 20;
        m_button->setGeometry({style::centred(width, buttonWidth), y, buttonWidth, std::ceil(button.height)});
    }

public:
    // The part of the page under the header that the group centres in.
    void setArea(double top, double height)
    {
        m_top = top;
        m_area = height;
        requestLayout();
    }

private:
    DialArt* m_dial;
    Label* m_title;
    Label* m_text;
    DialogButton* m_button;
    double m_top = 0;
    double m_area = 0;
};

AlarmsPage::AlarmsPage(PageContext& context)
    : m_context(context)
    , m_store(ClockStore::inFolder(context.paths().data))
    , m_emptyFade(std::make_unique<Spring>(&context.window(), 300, 0))
{
    setName("page:alarms");
    Window* window = &context.window();

    m_scroll = addChild<ScrollView>();
    m_scroll->setName("alarms:list");
    m_scroll->setScrollDirections(false, true);
    m_scroll->setBottomCornerRadius(Theme::current().windowRadius);
    m_list = m_scroll->setContent<AlarmList>(window);
    m_backdrop = addChild<HeaderBackdrop>(m_scroll);
    m_backdrop->setSeparatorVisible(false);

    m_empty = addChild<AlarmsEmptyState>(window);
    m_empty->onAdd = [this] { newItem(); };
    m_emptyFade->onChanged = [this] { requestLayout(); };

    m_changed = m_store.changed.connect([this] { sync(true); });
    // A one-time alarm that has rung shows as off from then on.
    m_tick = context.tick.connect([this] {
        const int64_t now = nowMs() / 1000;
        for (const Alarm& alarm : m_store.alarms()) {
            const AlarmRow* row = m_list->row(alarm.id);
            if (row && row->isActive() != alarmtime::isActive(alarm, now)) {
                sync(true);
                return;
            }
        }
    });
    sync(false);
    m_emptyFade->jumpTo(m_store.alarms().empty() ? 1 : 0);
}

AlarmsPage::~AlarmsPage() = default;

int64_t AlarmsPage::nowMs() const
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(m_context.now().time_since_epoch()).count();
}

std::vector<HeaderAction> AlarmsPage::headerActions()
{
    HeaderAction add;
    add.id = "add";
    add.glyph = "add";
    add.text = tr("New Alarm");
    add.triggered = [this] { newItem(); };
    return {add};
}

std::string AlarmsPage::newItemText() const
{
    return tr("New Alarm");
}

void AlarmsPage::newItem()
{
    // The time it is now, to the next five minutes, as a start to turn from.
    const time_t now = time_t(nowMs() / 1000);
    std::tm local{};
    localtime_r(&now, &local);
    int minutes = (local.tm_hour * 60 + local.tm_min + 4) / 5 * 5;
    minutes %= 24 * 60;
    Alarm alarm;
    alarm.hour = minutes / 60;
    alarm.minute = minutes % 60;
    alarm.sound = clockstore::soundNames().front();
    openEditor(alarm);
}

void AlarmsPage::edit(const std::string& id)
{
    if (const Alarm* alarm = m_store.alarm(id))
        openEditor(*alarm);
}

void AlarmsPage::openEditor(Alarm alarm)
{
    if (m_editor && m_editor->window()) {
        m_editor->window()->activate();
        return;
    }
    m_editor = std::make_unique<AlarmEditor>(&m_context.window(), m_store, std::move(alarm),
                                             [this] { return nowMs(); });
    m_editor->onSaved = [this](const std::string& id) {
        clockstore::ensureAlarmService(m_context.bus());
        const Alarm* saved = m_store.alarm(id);
        AlarmRow* row = m_list->row(id);
        if (!saved || !row)
            return;
        const int64_t now = nowMs() / 1000;
        if (const std::optional<int64_t> ring = alarmtime::nextRing(*saved, now))
            row->flashHint(alarmtime::untilText(*ring - now));
    };
    m_editor->onClosed = [this] {
        // Not from inside the dialog's own callback.
        properdesktop::base::EventLoop::current()->post([this] { m_editor.reset(); });
    };
    m_editor->show();
}

void AlarmsPage::activated()
{
    if (m_store.alarms().empty())
        m_empty->swingIn();
}

void AlarmsPage::toggled(const std::string& id, bool on)
{
    const Alarm* found = m_store.alarm(id);
    if (!found)
        return;
    Alarm alarm = *found;
    alarm.enabled = on;
    const int64_t now = nowMs() / 1000;
    if (on)
        alarm.armedAt = now;
    m_store.setAlarm(alarm);
    if (!on)
        return;
    clockstore::ensureAlarmService(m_context.bus());
    if (AlarmRow* row = m_list->row(id)) {
        if (const std::optional<int64_t> ring = alarmtime::nextRing(alarm, now))
            row->flashHint(alarmtime::untilText(*ring - now));
    }
}

static AlarmRow::Content rowContent(const Alarm& alarm, int64_t now)
{
    const properdesktop::base::Locale& locale = properdesktop::base::Locale::system();
    const properdesktop::base::ClockTime time{alarm.hour, alarm.minute};
    AlarmRow::Content content;
    if (locale.usesTwentyFourHourTime()) {
        content.time = locale.toString(time, "HH:mm");
    } else {
        content.time = locale.toString(time, "h:mm");
        content.half = alarm.hour < 12 ? locale.amText() : locale.pmText();
    }
    std::string detail = alarm.label.empty() ? std::string(tr("Alarm")) : alarm.label;
    const std::string repeat = alarmtime::repeatSummary(alarm.repeat);
    if (!repeat.empty())
        // TRANSLATORS: an alarm's label and when it repeats, such as "Wake Up, Weekdays".
        detail = properdesktop::i18n::format(tr("{label}, {repeat}"), {{"label", detail}, {"repeat", repeat}});
    content.detail = detail;
    content.active = alarmtime::isActive(alarm, now);
    return content;
}

void AlarmsPage::sync(bool animate)
{
    std::vector<Alarm> alarms = m_store.alarms();
    std::stable_sort(alarms.begin(), alarms.end(), [](const Alarm& a, const Alarm& b) {
        return a.hour * 60 + a.minute < b.hour * 60 + b.minute;
    });
    std::vector<std::string> ids;
    for (const Alarm& alarm : alarms)
        ids.push_back(alarm.id);
    Window* window = &m_context.window();
    m_list->sync(ids, [this, window](AlarmList& list, const std::string& id) {
        AlarmRow* row = list.addChild<AlarmRow>(window, id);
        row->onOpen = [this, id] { edit(id); };
        row->onToggled = [this, id](bool on) { toggled(id, on); };
        return row;
    }, animate && isVisible());
    const int64_t now = nowMs() / 1000;
    for (const Alarm& alarm : alarms) {
        if (AlarmRow* row = m_list->row(alarm.id))
            row->setContent(rowContent(alarm, now), animate);
    }

    const bool empty = alarms.empty();
    if (empty && m_emptyFade->target() < 1 && animate)
        m_empty->swingIn();
    if (animate)
        m_emptyFade->setTarget(empty ? 1 : 0);
    else
        m_emptyFade->jumpTo(empty ? 1 : 0);
    requestLayout();
}

void AlarmsPage::layout()
{
    const RectF area = localRect();
    const double header = headerHeight();
    m_list->setTopMargin(header);
    // The track starts one inset below the frosted header, as the handle
    // keeps from the window's side; its bottom end keeps clear of the corner.
    m_scroll->setVerticalBarPadding(header + ScrollBar::inset, 0);
    m_scroll->setGeometry(area);
    m_backdrop->setGeometry({0, 0, area.width, header});

    const double fade = m_emptyFade->value();
    m_empty->setVisible(fade > 0);
    m_empty->setOpacity(fade);
    m_empty->setGeometry({0, 0, area.width, area.height});
    m_empty->setArea(header, area.height - header);
    m_scroll->setOpacity(1 - fade * (m_list->count() == 0 ? 1 : 0));
}

void AlarmsPage::paint(Painter& painter)
{
    painter.fillRect(localRect(), style::page());
}

std::unique_ptr<Page> makeAlarmsPage(PageContext& context)
{
    return std::make_unique<AlarmsPage>(context);
}

} // namespace clockapp
