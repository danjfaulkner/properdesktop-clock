// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "timers/timerspage.h"

#include "pagecontext.h"
#include "store/store.h"
#include "style.h"
#include "timers/composer.h"
#include "timers/timercard.h"
#include "timers/wheelpicker.h"

#include <base/i18n.h>

#include "ui/header_backdrop.h"
#include "ui/keys.h"
#include "ui/painter.h"
#include "ui/scroll_bar.h"
#include "ui/scroll_view.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace properdesktop::ui;
using properdesktop::i18n::tr;

namespace clockapp {

static constexpr double CardGap = 16;

// The cards in rows, centred; a card glides to its new place when one
// before it goes.
class TimersPage::Grid : public View
{
public:
    explicit Grid(Window* window)
        : m_window(window)
    {
    }

    void setMargins(double top, double left, double right)
    {
        if (top == m_topMargin && left == m_leftMargin && right == m_rightMargin)
            return;
        m_topMargin = top;
        m_leftMargin = left;
        m_rightMargin = right;
        requestLayout();
    }

    TimerCard* card(const std::string& id) const
    {
        for (const Entry& entry : m_entries) {
            if (entry.card->id() == id)
                return entry.card;
        }
        return nullptr;
    }

    std::vector<TimerCard*> cards() const
    {
        std::vector<TimerCard*> cards;
        for (const Entry& entry : m_entries)
            cards.push_back(entry.card);
        return cards;
    }

    // Cards for the timers, in their order; the new ones made by `make`.
    void sync(const std::vector<RunningTimer>& timers, const std::function<TimerCard*(Grid&, const std::string&)>& make)
    {
        std::vector<Entry> entries;
        for (const RunningTimer& timer : timers) {
            auto found = std::find_if(m_entries.begin(), m_entries.end(),
                                      [&timer](const Entry& entry) { return entry.card->id() == timer.id; });
            if (found != m_entries.end()) {
                entries.push_back(std::move(*found));
                m_entries.erase(found);
                continue;
            }
            Entry entry;
            entry.card = make(*this, timer.id);
            entry.x = std::make_unique<Glide>(m_window, 300);
            entry.y = std::make_unique<Glide>(m_window, 300);
            entry.appear = std::make_unique<Glide>(m_window, 260, 1);
            entry.appear->setRestDistance(0.01);
            TimerCard* card = entry.card;
            Glide* x = entry.x.get();
            Glide* y = entry.y.get();
            const auto place = [card, x, y] {
                card->setGeometry({std::round(x->value()), std::round(y->value()), TimerCard::Width, TimerCard::Height});
            };
            entry.x->onChanged = place;
            entry.y->onChanged = place;
            Glide* appear = entry.appear.get();
            entry.appear->onChanged = [card, appear] { card->setOpacity(appear->value()); };
            entries.push_back(std::move(entry));
        }
        // A card can go from its own button's click: it is hidden now and
        // destroyed once that click is over.
        for (Entry& gone : m_entries) {
            gone.card->setVisible(false);
            m_gone.push_back(takeChild(gone.card));
        }
        m_entries = std::move(entries);
        requestLayout();
    }

    SizeF sizeHint() const override { return {0, heightForWidth(geometry().width)}; }

    double heightForWidth(double width) const override
    {
        const int columns = columnCount(width);
        const int rows = (int(m_entries.size()) + columns - 1) / columns;
        return m_topMargin + rows * (TimerCard::Height + CardGap) + 24;
    }

protected:
    void layout() override
    {
        m_gone.clear();
        const double width = geometry().width;
        const int columns = columnCount(width);
        const int used = std::min<int>(columns, std::max<int>(1, int(m_entries.size())));
        const double rowWidth = used * TimerCard::Width + (used - 1) * CardGap;
        const double room = std::max(0.0, width - m_leftMargin - m_rightMargin);
        const double left = m_leftMargin + style::centred(room, rowWidth);
        for (size_t i = 0; i < m_entries.size(); ++i) {
            Entry& entry = m_entries[i];
            const double x = left + double(int(i) % columns) * (TimerCard::Width + CardGap);
            const double y = m_topMargin + double(int(i) / columns) * (TimerCard::Height + CardGap);
            const bool arriving = !entry.placed && m_shown && isVisible();
            if (arriving && i > 0 && int(i) % columns != 0) {
                // A new card joins the row just after the card before it
                // and glides with the row as it re-centres, fading in.
                const Entry& before = m_entries[i - 1];
                entry.placed = true;
                entry.x->jumpTo(before.x->value() + TimerCard::Width + CardGap);
                entry.y->jumpTo(before.y->value());
                entry.x->setTarget(x);
                entry.y->setTarget(y);
                entry.appear->jumpTo(0);
                entry.appear->setTarget(1);
            } else if (!entry.placed || !isVisible()) {
                // The first cards a page shows are there at once; one
                // starting a row fades in where it lands.
                if (arriving) {
                    entry.appear->jumpTo(0);
                    entry.appear->setTarget(1);
                }
                entry.placed = true;
                entry.x->jumpTo(x);
                entry.y->jumpTo(y);
            } else {
                entry.x->setTarget(x);
                entry.y->setTarget(y);
            }
        }
        m_shown = width > 0;
    }

private:
    struct Entry
    {
        TimerCard* card = nullptr;
        std::unique_ptr<Glide> x;
        std::unique_ptr<Glide> y;
        std::unique_ptr<Glide> appear;
        bool placed = false;
    };

    int columnCount(double width) const
    {
        const double room = width - m_leftMargin - m_rightMargin + CardGap;
        return std::max(1, int(room / (TimerCard::Width + CardGap)));
    }

    Window* m_window;
    double m_topMargin = 0;
    double m_leftMargin = 0;
    double m_rightMargin = 0;
    // Laid out once: cards from now on are new to the user.
    bool m_shown = false;
    std::vector<Entry> m_entries;
    std::vector<std::unique_ptr<View>> m_gone;
};

TimersPage::TimersPage(PageContext& context, ClockStore* store)
    : m_context(context)
    , m_store(store ? *store : ClockStore::inFolder(context.paths().data))
    , m_modeFade(std::make_unique<Glide>(&context.window(), 320, 1))
    , m_recentsShown(std::make_unique<Glide>(&context.window(), 380, 0))
    , m_ticker(std::make_unique<FrameTicker>(&context.window()))
{
    setName("page:timers");
    Window* window = &context.window();

    m_gridView = addChild<ScrollView>();
    m_gridView->setName("timers:grid");
    m_gridView->setScrollDirections(false, true);
    m_gridView->setBottomCornerRadius(Theme::current().windowRadius);
    m_grid = m_gridView->setContent<Grid>(window);
    m_backdrop = addChild<HeaderBackdrop>(m_gridView);
    m_backdrop->setSeparatorVisible(false);

    m_composer = addChild<TimerComposer>(window);
    m_composer->onStart = [this] { startFromComposer(); };
    m_composer->onCancel = [this] { showComposer(false); };

    m_recentsPanel = addChild<RecentsPanel>(window);
    m_recentsPanel->onStartRecent = [this](size_t index) { startRecent(index); };
    m_recentsPanel->setVisible(false);

    m_gridView->setLayered(true);
    m_composer->setLayered(true);
    m_modeFade->setRestDistance(0.01);
    m_modeFade->onChanged = [this] {
        requestLayout();
        if (!m_modeFade->isMoving())
            requestPaint();
    };
    m_recentsShown->onChanged = [this] {
        requestLayout();
        requestPaint();
    };
    m_ticker->onFrame = [this](uint64_t) { refresh(); };

    m_recents = timermath::loadRecents(context.settings());
    updateRecents();

    m_storeChanged = m_store.changed.connect([this] { sync(); });
    m_store.load();
    sync();
    m_composing = m_store.timers().empty();
    m_modeFade->jumpTo(m_composing ? 1 : 0);
    m_composer->setCancelVisible(!m_store.timers().empty());

    m_escapeShortcut = context.addShortcut(*this, NoModifier, key::Escape, [this] {
        if (m_composing)
            showComposer(false);
    });
}

TimersPage::~TimersPage()
{
    m_context.removeShortcut(m_escapeShortcut);
}

int64_t TimersPage::nowMs() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(m_context.now().time_since_epoch()).count();
}

void TimersPage::start(const timermath::Recent& recent)
{
    RunningTimer timer = timermath::make(recent.durationMs, recent.label, recent.sound, nowMs());
    timermath::remember(m_recents, {timer.durationMs, timer.label, timer.sound});
    timermath::saveRecents(m_context.settings(), m_recents);
    m_context.saveSettings();
    updateRecents();
    clockstore::ensureAlarmService(m_context.bus());
    m_fillIn = m_store.setTimer(timer);
    showComposer(false);
    if (TimerCard* card = m_grid->card(m_fillIn)) {
        card->fillIn();
        m_fillIn.clear();
    }
}

void TimersPage::startFromComposer()
{
    const int64_t duration = m_composer->picker()->durationMs();
    if (duration <= 0)
        return;
    start({duration, m_composer->label(), m_composer->sound()});
    m_composer->clearLabel();
}

void TimersPage::startRecent(size_t index)
{
    if (index >= m_recents.size())
        return;
    start(m_recents[index]);
}

void TimersPage::showComposer(bool show)
{
    if (!show && m_store.timers().empty())
        show = true;
    m_composer->setCancelVisible(!m_store.timers().empty());
    if (show == m_composing)
        return;
    m_composing = show;
    if (isVisible() && m_current)
        m_modeFade->setTarget(show ? 1 : 0);
    else
        m_modeFade->jumpTo(show ? 1 : 0);
    m_context.actionsChanged(*this);
    updateTicking();
}

void TimersPage::sync()
{
    m_grid->sync(m_store.timers(), [this](Grid& grid, const std::string& id) {
        TimerCard* card = grid.addChild<TimerCard>(&m_context.window(), id);
        card->onCancel = [this, id] { m_store.removeTimer(id); };
        card->onPauseResume = [this, id] {
            const RunningTimer* found = m_store.timer(id);
            if (!found)
                return;
            RunningTimer timer = *found;
            if (timer.isRunning())
                timermath::pause(timer, nowMs());
            else
                timermath::resume(timer, nowMs());
            m_store.setTimer(timer);
        };
        card->onRepeat = [this, id] {
            if (!timermath::repeatTimer(m_store, id, nowMs()))
                return;
            if (TimerCard* repeated = m_grid->card(id))
                repeated->fillIn();
        };
        return card;
    });
    if (m_store.timers().empty() && !m_composing)
        showComposer(true);
    m_composer->setCancelVisible(!m_store.timers().empty());
    refresh();
    updateTicking();
}

void TimersPage::refresh()
{
    const int64_t now = nowMs();
    for (const RunningTimer& timer : m_store.timers()) {
        if (TimerCard* card = m_grid->card(timer.id))
            card->update(timer, now);
    }
    updateTicking();
}

void TimersPage::updateRecents()
{
    if (!m_recents.empty())
        m_recentsPanel->setRecents(m_recents);
    const double shown = m_recents.empty() ? 0 : 1;
    // The list keeps its rows while it slides away, and drops them after.
    if (m_recents.empty() && !m_recentsShown->isMoving() && m_recentsShown->value() == 0)
        m_recentsPanel->setRecents(m_recents);
    if (window() && isVisible() && geometry().width > 0)
        m_recentsShown->setTarget(shown);
    else
        m_recentsShown->jumpTo(shown);
    requestLayout();
}

void TimersPage::updateTicking()
{
    bool running = false;
    const int64_t now = nowMs();
    for (const RunningTimer& timer : m_store.timers())
        running = running || (timer.isRunning() && !timermath::isFinished(timer, now));
    m_ticker->setRunning(m_current && !m_composing && running);
}

std::vector<HeaderAction> TimersPage::headerActions()
{
    if (m_composing)
        return {};
    HeaderAction add;
    add.id = "new";
    add.glyph = "add";
    add.text = tr("New Timer");
    add.triggered = [this] { showComposer(true); };
    return {add};
}

std::string TimersPage::newItemText() const
{
    return tr("New Timer");
}

void TimersPage::newItem()
{
    showComposer(true);
}

std::vector<MenuAction> TimersPage::editActions()
{
    MenuAction clear;
    clear.text = tr("Clear Recent Timers");
    clear.enabled = !m_recents.empty();
    clear.triggered = [this] {
        m_recents.clear();
        timermath::saveRecents(m_context.settings(), m_recents);
        m_context.saveSettings();
        updateRecents();
        m_context.actionsChanged(*this);
    };
    return {clear};
}

void TimersPage::activated()
{
    m_current = true;
    m_store.load();
    refresh();
}

void TimersPage::entering(double progress)
{
    // Each card settles in a little after the one before it.
    const std::vector<TimerCard*> cards = m_grid->cards();
    for (size_t i = 0; i < cards.size(); ++i) {
        const double delay = std::min(0.4, 0.08 * double(i));
        cards[i]->setOpacity(std::clamp((progress - delay) / (1 - delay), 0.0, 1.0));
    }
}

void TimersPage::deactivated()
{
    m_current = false;
    updateTicking();
}

void TimersPage::layout()
{
    const RectF area = localRect();
    const double fade = m_modeFade->value();
    const double shown = m_recentsShown->value();
    const double header = headerHeight();
    const double drift = 12;

    const double contentX = shown > 0
        ? std::round(style::pagePadding + shown * (TimerComposer::RecentsWidth + TimerComposer::RecentsGap))
        : 0;
    const double contentWidth = std::max(0.0, area.width - contentX);
    const double leftMargin = (1 - shown) * style::pagePadding;

    m_grid->setMargins(header + 16, leftMargin, style::pagePadding);
    m_composer->setMargins(header, leftMargin, style::pagePadding);
    // The cards scroll under the frosted header: the bar starts below it.
    m_gridView->setVerticalBarPadding(header + ScrollBar::inset, 0);

    m_gridView->setVisible(fade < 1);
    m_backdrop->setVisible(fade < 1);
    m_gridView->setOpacity(1 - fade);
    m_gridView->setGeometry({contentX, std::round(-drift * fade), contentWidth, area.height});
    m_backdrop->setGeometry({contentX, 0, contentWidth, header});

    m_composer->setVisible(fade > 0);
    m_composer->setOpacity(fade);
    m_composer->setGeometry({contentX, std::round(drift * (1 - fade)), contentWidth, area.height});

    const bool recentsVisible = shown > 0;
    m_recentsPanel->setVisible(recentsVisible);
    m_recentsPanel->setOpacity(shown);
    if (!recentsVisible && !m_recentsShown->isMoving() && m_recents.empty())
        m_recentsPanel->setRecents({});
    const double recentsX =
        std::round(style::pagePadding - (1 - shown) * (TimerComposer::RecentsWidth + style::pagePadding));
    const double recentsTop = header + 16;
    m_recentsPanel->setGeometry({recentsX, recentsTop, TimerComposer::RecentsWidth,
                                 std::max(0.0, area.height - recentsTop - style::pagePadding)});

    if (!m_fillIn.empty()) {
        if (TimerCard* card = m_grid->card(m_fillIn))
            card->fillIn();
        m_fillIn.clear();
    }
}

void TimersPage::paint(Painter& painter)
{
    painter.fillRect(localRect(), style::page());
}

std::unique_ptr<Page> makeTimersPage(PageContext& context)
{
    return std::make_unique<TimersPage>(context);
}

} // namespace clockapp
