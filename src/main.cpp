// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "application.h"
#include "clockwindow.h"
#include "darkappearance.h"
#include "icons.h"

#include "base/command_line.h"
#include "base/dbus.h"
#include "base/i18n.h"
#include "ui/application.h"
#include "ui/icon.h"

#include <chrono>

using namespace properdesktop;
using namespace std::chrono_literals;

int main(int argc, char** argv)
{
    base::CommandLineParser parser;
    parser.setApplicationDescription("Proper Desktop Clock");
    parser.setApplicationVersion(CLOCK_VERSION);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(argc, argv);

    ui::Application app(argc, argv);
    app.setApplicationId("properdesktop-clock");
    i18n::setDomain("properdesktop-clock");
    auto dark = clockapp::pinDarkAppearance();
    for (const auto& [name, svg] : embeddedIcons())
        ui::Icon::registerGlyph(std::string(name), std::string(svg));

    // The object goes up before the name, so a second launch that finds the
    // name always finds someone to raise.
    clockapp::App application;
    base::dbus::Connection* bus = app.sessionBus();
    if (bus) {
        application.publish(bus);
        if (!bus->requestName(clockapp::App::kService)) {
            base::dbus::Proxy running(bus, clockapp::App::kService, clockapp::App::kPath, clockapp::App::kService);
            running.setTimeout(3s);
            if (!running.call("open", std::vector<std::string>(), app.takeActivationToken()).isError())
                return 0;
        }
        application.publishInstance(bus);
    }

    clockapp::ClockWindow window({.bus = bus});
    auto activate = application.activateRequested.connect([&window](const std::string& token) { window.activate(token); });
    auto quit = application.quitRequested.connect([&app] { app.quit(); });
    auto repeat = application.repeatTimerRequested.connect([&window](const std::string& id) { window.repeatTimer(id); });
    window.show();
    return app.run();
}
