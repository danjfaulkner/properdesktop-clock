// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/view.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace clockapp {

class PageContext;

enum class Tab {
    WorldClock,
    Alarms,
    Stopwatch,
    Timers,
};

inline constexpr int TabCount = 4;

// A button a page puts at the header's end, named "header:<id>": a round
// glyph button when it has a glyph, else a text button ("Edit", "Done").
// The text is the glyph button's accessible label.
struct HeaderAction
{
    std::string id;
    std::string glyph;
    std::string text;
    bool enabled = true;
    // A text button drawn in the accent colour, as "Done" while editing.
    bool prominent = false;
    std::function<void()> triggered;
};

// A row a page adds to the window's Edit menu, below its standard rows.
struct MenuAction
{
    std::string text;
    // A key equivalent as ui::Menu::setItemKeyEquivalent takes it, shown
    // only; bind the key itself with PageContext::addShortcut.
    std::string key;
    uint32_t modifiers = 0;
    bool enabled = true;
    std::function<void()> triggered;
};

// One tab's content, filling the window below the header. The window owns
// it, shows only the current tab's page and asks it again for its header
// buttons and menu rows whenever it calls PageContext::actionsChanged().
class Page : public properdesktop::ui::View
{
public:
    ~Page() override;

    // The buttons at the header's end, in reading order.
    virtual std::vector<HeaderAction> headerActions() { return {}; }
    // File > "New Alarm" / "New Timer" and Command+N; empty for none.
    virtual std::string newItemText() const { return {}; }
    virtual void newItem() {}
    virtual std::vector<MenuAction> editActions() { return {}; }

    // The page became the current tab, or stopped being it.
    virtual void activated() {}
    virtual void deactivated() {}
    // Each frame of the tab switch bringing the page in, from 0 to 1 (eased),
    // after activated(); 1 at once when the switch is not animated. The window
    // fades and slides the page as a whole; a page may move its own parts on
    // top, as content settling in a beat later. Ends at 1 even when cut short.
    virtual void entering(double progress) { (void)progress; }

    // The space the page leaves free for the frosted header over its top:
    // a page that scrolls starts its content this far down.
    double headerHeight() const { return m_headerHeight; }
    void setHeaderHeight(double height);

private:
    double m_headerHeight = 0;
};

// Each module defines its factory; one not built yet shows a placeholder.
std::unique_ptr<Page> makeWorldClockPage(PageContext& context);
std::unique_ptr<Page> makeAlarmsPage(PageContext& context);
std::unique_ptr<Page> makeStopwatchPage(PageContext& context);
std::unique_ptr<Page> makeTimersPage(PageContext& context);

// The page of a tab, from its module's factory or the placeholder.
std::unique_ptr<Page> makePage(Tab tab, PageContext& context);
// "World Clock", "Alarms", "Stopwatch", "Timers", translated.
std::string tabTitle(Tab tab);
// "world", "alarms", "stopwatch", "timers": the [View] Tab value.
const char* tabKey(Tab tab);

} // namespace clockapp
