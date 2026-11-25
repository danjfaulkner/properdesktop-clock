// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pagecontext.h"
#include "page.h"

#include "base/paths.h"
#include "ui/window.h"

#include <algorithm>

using namespace std::chrono_literals;

namespace clockapp {

PageContext::PageContext(properdesktop::ui::Window& window, Paths paths, properdesktop::base::dbus::Connection* bus)
    : m_window(window)
    , m_paths(std::move(paths))
    , m_bus(bus)
    , m_settings(std::make_unique<properdesktop::base::Settings>(m_paths.config))
    , m_clock([] { return std::chrono::system_clock::now(); })
{
    m_tickTimer.setSingleShot(true);
    m_tickTimer.onTimeout = [this] {
        emitTick();
        scheduleTick();
    };
    scheduleTick();
}

PageContext::~PageContext()
{
    for (int id : m_windowShortcuts)
        m_window.removeShortcut(id);
    m_settings->sync();
}

PageContext::Paths PageContext::defaultPaths()
{
    namespace paths = properdesktop::base::paths;
    return {
        .config = paths::properdesktopConfig("clock"),
        .data = paths::dataHome() + "/properdesktop/clock",
    };
}

void PageContext::saveSettings()
{
    m_settings->sync();
}

std::chrono::system_clock::time_point PageContext::now() const
{
    return m_clock();
}

void PageContext::setClock(Clock clock)
{
    m_clock = std::move(clock);
    emitTick();
}

void PageContext::emitTick()
{
    tick.emit();
}

// Just past the next second of the real clock, so a page reading now()
// shows the second that has begun; a clock set meanwhile is caught a second
// later at most.
void PageContext::scheduleTick()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto intoSecond = now - std::chrono::duration_cast<std::chrono::seconds>(now);
    m_tickTimer.start(1s - intoSecond + 2ms);
}

void PageContext::actionsChanged(Page& page)
{
    if (onActionsChanged)
        onActionsChanged(page);
}

// One window shortcut per key, which runs the current page's action: pages
// may bind the same key, as Space on the stopwatch and the timers.
int PageContext::addShortcut(Page& page, uint32_t modifiers, uint32_t key, std::function<void()> action)
{
    const int id = ++m_lastShortcut;
    const bool bound = std::any_of(m_shortcuts.begin(), m_shortcuts.end(), [&](const Shortcut& shortcut) {
        return shortcut.modifiers == modifiers && shortcut.key == key;
    });
    m_shortcuts.push_back({id, &page, modifiers, key, std::move(action)});
    if (!bound) {
        m_windowShortcuts.push_back(m_window.addShortcut(modifiers, key, [this, modifiers, key] {
            for (const Shortcut& shortcut : m_shortcuts) {
                if (shortcut.modifiers == modifiers && shortcut.key == key && (!isCurrent || isCurrent(*shortcut.page))) {
                    shortcut.action();
                    return;
                }
            }
        }));
    }
    return id;
}

void PageContext::removeShortcut(int id)
{
    std::erase_if(m_shortcuts, [id](const Shortcut& shortcut) { return shortcut.id == id; });
}

} // namespace clockapp
