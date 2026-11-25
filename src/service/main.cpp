// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "alarms/toneplayer.h"
#include "service/alarmservice.h"
#include "store/store.h"

#include <base/dbus.h>
#include <base/event_loop.h>
#include <base/i18n.h>
#include <base/locale.h>
#include <base/session_language.h>

#include <string>
#include <vector>

namespace dbus = properdesktop::base::dbus;

// properdesktop-clock-alarms [data folder]: the folder is for tests and trial runs.
int main(int argc, char** argv)
{
    properdesktop::base::Locale::setProcessLocale();
    properdesktop::i18n::setDomain("properdesktop-clock");
    properdesktop::base::EventLoop loop;
    dbus::Connection* bus = dbus::Connection::session(&loop);
    if (!bus || !bus->requestName("org.properdesktop.ClockAlarms"))
        return 0;

    properdesktop::base::SessionLanguage language(bus);
    clockapp::ClockStore store(argc > 1 ? clockapp::clockstore::pathIn(argv[1]) : clockapp::clockstore::defaultPath());
    store.watch();
    clockapp::TonePlayer player;
    clockapp::AlarmService service(store, bus, player);

    // A wake from suspend checks at once rather than when the timer, which
    // stood still meanwhile, runs out.
    dbus::Match sleep;
    if (dbus::Connection* system = dbus::Connection::system(&loop)) {
        dbus::Proxy login(system, "org.freedesktop.login1", "/org/freedesktop/login1",
                          "org.freedesktop.login1.Manager");
        sleep = login.onSignal("PrepareForSleep", [&service](const dbus::Message& message) {
            bool sleeping = true;
            if (message.read(sleeping) && !sleeping)
                service.check();
        });
    }
    service.check();
    return loop.run();
}
