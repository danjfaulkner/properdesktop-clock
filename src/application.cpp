// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "application.h"

#include "base/dbus.h"

namespace clockapp {

App::App() = default;

App::~App() = default;

void App::publish(properdesktop::base::dbus::Connection* bus)
{
    if (!bus)
        return;
    m_object = std::make_unique<properdesktop::base::dbus::Object>(bus, kPath, kService);
    m_object->addMethod(
        "open",
        [this](const std::vector<std::string>&, const std::string& activationToken) {
            activateRequested.emit(activationToken);
        },
        properdesktop::base::dbus::ArgumentNames{{"paths", "activationToken"}, {}});
    // Only this process writes the alarms file, so the alarm service asks
    // here to start a finished timer again.
    m_object->addMethod(
        "RepeatTimer", [this](const std::string& id) { repeatTimerRequested.emit(id); },
        properdesktop::base::dbus::ArgumentNames{{"id"}, {}});
    m_object->publish();
}

void App::publishInstance(properdesktop::base::dbus::Connection* bus)
{
    if (!bus)
        return;
    // The dock's view of a running application: it stays in the dock with
    // its window closed, and the dock's Quit (and Command+Q) ends it here
    // rather than closing its window.
    m_instance = std::make_unique<properdesktop::base::dbus::Object>(bus, "/org/properdesktop/Application",
                                                                "org.properdesktop.Application");
    m_instance->addProperty("AppId", [] { return std::string("properdesktop-clock"); });
    m_instance->addProperty("ShowsInDock", [] { return true; });
    m_instance->addMethod(
        "Launch",
        [this](const std::vector<std::string>&, const std::string& activationToken) {
            activateRequested.emit(activationToken);
        },
        properdesktop::base::dbus::ArgumentNames{{"paths", "activationToken"}, {}});
    m_instance->addMethod("Quit", [this] { quitRequested.emit(); });
    m_instance->publish();
    bus->requestName("org.properdesktop.Application.properdesktop_clock");
}

} // namespace clockapp
