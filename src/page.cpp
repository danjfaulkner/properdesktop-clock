// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "page.h"

#include "base/i18n.h"

using properdesktop::i18n::tr;

namespace clockapp {

Page::~Page() = default;

void Page::setHeaderHeight(double height)
{
    if (height == m_headerHeight)
        return;
    m_headerHeight = height;
    requestLayout();
}

std::string tabTitle(Tab tab)
{
    switch (tab) {
    case Tab::WorldClock:
        return tr("World Clock");
    case Tab::Alarms:
        return tr("Alarms");
    case Tab::Stopwatch:
        return tr("Stopwatch");
    case Tab::Timers:
        return tr("Timers");
    }
    return {};
}

const char* tabKey(Tab tab)
{
    switch (tab) {
    case Tab::WorldClock:
        return "world";
    case Tab::Alarms:
        return "alarms";
    case Tab::Stopwatch:
        return "stopwatch";
    case Tab::Timers:
        return "timers";
    }
    return "world";
}

} // namespace clockapp
