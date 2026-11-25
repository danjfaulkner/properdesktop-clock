// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/signal.h"

#include <memory>
#include <string>
#include <vector>

namespace properdesktop::base::dbus {
class Connection;
class Object;
}

namespace clockapp {

// The running Clock on the session bus: the single instance's object and
// the running-application object the dock follows.
class App
{
public:
    static constexpr const char* kService = "org.properdesktop.Clock";
    static constexpr const char* kPath = "/Clock";

    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // The single instance's object, before its name is asked for.
    void publish(properdesktop::base::dbus::Connection* bus);
    // The dock's object, once the name is ours.
    void publishInstance(properdesktop::base::dbus::Connection* bus);

    // Another launch asked for the window; Clock opens no files.
    properdesktop::base::Signal<const std::string&> activateRequested;
    properdesktop::base::Signal<> quitRequested;
    // The alarm service's Repeat on a finished timer's notification.
    properdesktop::base::Signal<const std::string&> repeatTimerRequested;

private:
    std::unique_ptr<properdesktop::base::dbus::Object> m_object;
    std::unique_ptr<properdesktop::base::dbus::Object> m_instance;
};

} // namespace clockapp
