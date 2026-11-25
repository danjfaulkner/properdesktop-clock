// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "page.h"
#include "pagecontext.h"

#include "base/signal.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace properdesktop::ui {
class AboutDialog;
class Animation;
class Menu;
class Window;
}

namespace clockapp {

class HeaderBar;
class ClockRoot;

// The window: the four pages under a frosted header with the tab switch.
class ClockWindow
{
public:
    struct Options
    {
        PageContext::Paths paths = PageContext::defaultPaths();
        properdesktop::base::dbus::Connection* bus = nullptr;
        // Off in tests, which keep away from the user's clock.conf.
        bool restoresSize = true;
    };

    explicit ClockWindow(Options options);
    ~ClockWindow();

    ClockWindow(const ClockWindow&) = delete;
    ClockWindow& operator=(const ClockWindow&) = delete;

    void show();
    void activate(const std::string& token);

    properdesktop::ui::Window& window() { return *m_window; }
    PageContext& context() { return *m_context; }
    HeaderBar* headerBar() const;
    Page* page(Tab tab) const { return m_pages[size_t(tab)]; }

    Tab tab() const { return m_tab; }
    // Kept in [View] Tab.
    void setTab(Tab tab, bool animated = true);
    bool isSwitching() const;
    // Starts a timer of the store again from its full duration.
    void repeatTimer(const std::string& id);

    properdesktop::base::Signal<> tabChanged;

private:
    void buildMainMenu();
    void installShortcuts();
    void updateActions(bool animated);
    void startTransition(Page* from, Page* to, int direction);
    void finishTransition();
    void openAbout();
    void retranslate();

    std::unique_ptr<properdesktop::ui::Window> m_window;
    std::unique_ptr<PageContext> m_context;
    ClockRoot* m_root = nullptr;
    std::array<Page*, TabCount> m_pages {};
    Tab m_tab = Tab::WorldClock;
    Page* m_outgoing = nullptr;
    Page* m_incoming = nullptr;
    std::unique_ptr<properdesktop::ui::Animation> m_transition;
    std::unique_ptr<properdesktop::ui::Menu> m_mainMenu;
    std::unique_ptr<properdesktop::ui::AboutDialog> m_about;
    properdesktop::base::Subscription m_languageChanged;
};

} // namespace clockapp
