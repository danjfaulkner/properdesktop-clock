// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/settings.h"
#include "base/signal.h"
#include "base/timer.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace properdesktop::base::dbus {
class Connection;
}

namespace properdesktop::ui {
class Window;
}

namespace clockapp {

class Page;

// What the window gives every page. Tests make one over their own window,
// files and clock.
class PageContext
{
public:
    using Clock = std::function<std::chrono::system_clock::time_point()>;

    struct Paths
    {
        // clock.conf: the window's and pages' preferences and state.
        std::string config;
        // The folder the alarms and timers keep their data file in.
        std::string data;
    };

    PageContext(properdesktop::ui::Window& window, Paths paths, properdesktop::base::dbus::Connection* bus = nullptr);
    ~PageContext();

    PageContext(const PageContext&) = delete;
    PageContext& operator=(const PageContext&) = delete;

    // ~/.config/properdesktop/clock.conf and ~/.local/share/properdesktop/clock.
    static Paths defaultPaths();

    // The window, for dialogs (ui::DialogWindow over it) and menus.
    properdesktop::ui::Window& window() const { return m_window; }
    const Paths& paths() const { return m_paths; }
    // Null without a session bus, as in some tests.
    properdesktop::base::dbus::Connection* bus() const { return m_bus; }

    // clock.conf, read once and shared by the window and every page: a page
    // keeps to its own group ("WorldClock/Cities", "Stopwatch/..."). Write a
    // change out with saveSettings().
    properdesktop::base::Settings& settings() { return *m_settings; }
    void saveSettings();

    // The wall clock, which tests replace.
    std::chrono::system_clock::time_point now() const;
    void setClock(Clock clock);

    // Once a second, just after the wall clock's second changes, and when a
    // test sets the clock: what shows the time of day redraws on it.
    properdesktop::base::Signal<> tick;
    void emitTick();

    // The page's header buttons, its New row or its Edit rows changed.
    void actionsChanged(Page& page);
    // A key bound while the page is the current tab, when no focused view
    // takes it. The id removes it.
    int addShortcut(Page& page, uint32_t modifiers, uint32_t key, std::function<void()> action);
    void removeShortcut(int id);

    // For the window.
    std::function<void(Page&)> onActionsChanged;
    // Whether the page is the current tab.
    std::function<bool(const Page&)> isCurrent;

private:
    void scheduleTick();

    properdesktop::ui::Window& m_window;
    Paths m_paths;
    properdesktop::base::dbus::Connection* m_bus;
    std::unique_ptr<properdesktop::base::Settings> m_settings;
    Clock m_clock;
    properdesktop::base::Timer m_tickTimer;
    struct Shortcut
    {
        int id;
        Page* page;
        uint32_t modifiers;
        uint32_t key;
        std::function<void()> action;
    };
    std::vector<Shortcut> m_shortcuts;
    std::vector<int> m_windowShortcuts;
    int m_lastShortcut = 0;
};

} // namespace clockapp
