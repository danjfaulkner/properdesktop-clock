// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "page.h"
#include "stopwatch/motion.h"
#include "timers/timermath.h"

#include <base/signal.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace properdesktop::ui {
class HeaderBackdrop;
class ScrollView;
}

namespace clockapp {

class ClockStore;
class PageContext;
class RecentsPanel;
class TimerCard;
class TimerComposer;

// The running timers as cards, and the composer that sets a new one, shown
// on its own while no timer runs and over the cards from New Timer. The
// timers themselves live in the alarms' data file, so the alarm service
// rings them whether the window is open or not; the page only shows them.
class TimersPage : public Page
{
public:
    // The store over the context's data folder when none is given.
    TimersPage(PageContext& context, ClockStore* store = nullptr);
    ~TimersPage() override;

    bool isComposing() const { return m_composing; }
    TimerComposer* composer() const { return m_composer; }
    const std::vector<timermath::Recent>& recents() const { return m_recents; }

    // Starts the composer's timer, or a recent one again.
    void startFromComposer();
    void startRecent(size_t index);
    void showComposer(bool show);

    std::vector<HeaderAction> headerActions() override;
    std::string newItemText() const override;
    void newItem() override;
    std::vector<MenuAction> editActions() override;
    void activated() override;
    void deactivated() override;
    void entering(double progress) override;

    // Brings the cards up to the store and the wall clock.
    void refresh();

protected:
    void layout() override;
    void paint(properdesktop::ui::Painter& painter) override;

private:
    class Grid;

    void start(const timermath::Recent& recent);
    void sync();
    void updateRecents();
    void updateTicking();
    int64_t nowMs() const;

    PageContext& m_context;
    ClockStore& m_store;
    properdesktop::base::Subscription m_storeChanged;
    std::vector<timermath::Recent> m_recents;
    bool m_composing = true;
    bool m_current = false;
    // The timer just started, whose ring fills in once its card is placed.
    std::string m_fillIn;
    int m_escapeShortcut = 0;

    properdesktop::ui::ScrollView* m_gridView = nullptr;
    Grid* m_grid = nullptr;
    TimerComposer* m_composer = nullptr;
    RecentsPanel* m_recentsPanel = nullptr;
    properdesktop::ui::HeaderBackdrop* m_backdrop = nullptr;
    // From 0, the cards, to 1, the composer.
    std::unique_ptr<Glide> m_modeFade;
    // From 0, no recents column, to 1, the column in place.
    std::unique_ptr<Glide> m_recentsShown;
    std::unique_ptr<FrameTicker> m_ticker;
};

} // namespace clockapp
