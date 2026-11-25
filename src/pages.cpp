// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "page.h"
#include "pagecontext.h"
#include "style.h"

#include "ui/label.h"
#include "ui/painter.h"

namespace clockapp {

// A tab whose module is not built in yet.
class PlaceholderPage : public Page
{
public:
    explicit PlaceholderPage(Tab tab)
        : m_title(addChild<properdesktop::ui::Label>(tabTitle(tab)))
    {
        setName(std::string("page:") + tabKey(tab));
        m_title->setFontSize(style::points(15));
        m_title->setWeight(600);
    }

protected:
    void layout() override
    {
        const properdesktop::ui::SizeF size = m_title->sizeHint();
        m_title->setColor(style::secondaryText());
        m_title->setGeometry({style::centred(geometry().width, size.width),
                              style::centred(geometry().height + headerHeight(), size.height), size.width,
                              size.height});
    }

    void paint(properdesktop::ui::Painter& painter) override
    {
        painter.fillRect(localRect(), style::page());
    }

private:
    properdesktop::ui::Label* m_title;
};

std::unique_ptr<Page> makePage(Tab tab, PageContext& context)
{
    (void)context;
    switch (tab) {
    case Tab::WorldClock:
#ifdef CLOCK_HAS_WORLD
        return makeWorldClockPage(context);
#else
        break;
#endif
    case Tab::Alarms:
#ifdef CLOCK_HAS_ALARMS
        return makeAlarmsPage(context);
#else
        break;
#endif
    case Tab::Stopwatch:
#ifdef CLOCK_HAS_STOPWATCH
        return makeStopwatchPage(context);
#else
        break;
#endif
    case Tab::Timers:
#ifdef CLOCK_HAS_TIMERS
        return makeTimersPage(context);
#else
        break;
#endif
    }
    return std::make_unique<PlaceholderPage>(tab);
}

} // namespace clockapp
