// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "clockwindow.h"
#include "headerbar.h"
#include "style.h"

#include "base/i18n.h"
#include "ui/about_dialog.h"
#include "ui/animation.h"
#include "ui/application.h"
#include "ui/keys.h"
#include "ui/menu.h"
#include "ui/painter.h"
#include "ui/segmented_control.h"
#include "ui/window.h"
#include "ui/window_header.h"

#ifdef CLOCK_HAS_TIMERS
#include "store/store.h"
#include "timers/timermath.h"
#endif

#include <algorithm>
#include <cmath>

using namespace properdesktop::ui;
using properdesktop::i18n::tr;

namespace clockapp {

static constexpr std::chrono::milliseconds kTransitionDuration{300};
// How far the incoming page slides in from the side of travel, and the
// outgoing one drifts the other way.
static constexpr double kSlideIn = 18;
static constexpr double kDriftOut = 8;
static constexpr const char* kTabKey = "View/Tab";

static Tab tabFromKey(const std::string& key)
{
    for (int i = 0; i < TabCount; ++i) {
        if (key == tabKey(Tab(i)))
            return Tab(i);
    }
    return Tab::WorldClock;
}

// The pages, each over the whole window, and the header over their tops.
class ClockRoot : public View
{
public:
    ClockRoot()
        : header(addChild<WindowHeader>(style::headerHeight))
    {
        bar = header->setContent<HeaderBar>();
        header->setContentUnderButtons(true);
    }

    void addPage(std::unique_ptr<Page> page)
    {
        page->setHeaderHeight(style::headerHeight);
        pages.push_back(page.get());
        addChild(std::move(page));
        // The header stays on top.
        std::unique_ptr<View> top = takeChild(header);
        addChild(std::move(top));
    }

    WindowHeader* header;
    HeaderBar* bar;
    std::vector<Page*> pages;

protected:
    void layout() override
    {
        const RectF bounds = localRect();
        for (Page* page : pages)
            page->setGeometry(bounds);
        header->setGeometry({0, 0, bounds.width, style::headerHeight});
    }

    void paint(Painter& painter) override
    {
        painter.fillRect(localRect(), style::page());
    }
};

ClockWindow::ClockWindow(Options options)
    : m_window(std::make_unique<Window>())
{
    m_window->setTitle(tr("Clock"));
    m_window->setMinimumSize({style::windowMinimumWidth, style::windowMinimumHeight});
    m_window->resize({style::windowWidth, style::windowHeight});
    if (options.restoresSize)
        m_window->setRestoresSize("Main");
    m_window->onCloseRequested = [this] { m_window->hideApplication(); };

    m_context = std::make_unique<PageContext>(*m_window, std::move(options.paths), options.bus);
    m_context->isCurrent = [this](const Page& page) { return &page == m_pages[size_t(m_tab)]; };
    m_context->onActionsChanged = [this](Page& page) {
        if (&page == m_pages[size_t(m_tab)])
            updateActions(false);
    };

    auto root = std::make_unique<ClockRoot>();
    m_root = root.get();
    m_window->setContentView(std::move(root));
    m_window->onActiveChanged = [this] { m_root->bar->updateActive(); };
    for (int i = 0; i < TabCount; ++i) {
        std::unique_ptr<Page> page = makePage(Tab(i), *m_context);
        m_pages[size_t(i)] = page.get();
        page->setVisible(false);
        m_root->addPage(std::move(page));
    }

    SegmentedControl* tabs = m_root->bar->tabSwitch();
    std::vector<std::string> titles;
    for (int i = 0; i < TabCount; ++i)
        titles.push_back(tabTitle(Tab(i)));
    tabs->setItems(titles);
    tabs->onCurrentChanged = [this](int index) { setTab(Tab(index)); };

    m_tab = tabFromKey(m_context->settings().stringValue(kTabKey));
    tabs->setCurrentIndex(int(m_tab), false);
    Page* current = m_pages[size_t(m_tab)];
    current->setVisible(true);
    current->activated();
    current->entering(1);
    updateActions(false);

    m_languageChanged = Application::instance()->languageChanged.connect([this] { retranslate(); });
    installShortcuts();
    buildMainMenu();
}

ClockWindow::~ClockWindow()
{
    // The pages go with the window's views, before the context they hold on to.
    m_transition.reset();
    m_window->setMainMenu(nullptr);
    m_window->setContentView(nullptr);
    m_context.reset();
}

void ClockWindow::show()
{
    m_window->show();
}

void ClockWindow::activate(const std::string& token)
{
    m_window->activate(token);
}

HeaderBar* ClockWindow::headerBar() const
{
    return m_root->bar;
}

bool ClockWindow::isSwitching() const
{
    return m_transition && m_transition->isRunning();
}

void ClockWindow::setTab(Tab tab, bool animated)
{
    m_root->bar->tabSwitch()->setCurrentIndex(int(tab), animated);
    if (tab == m_tab)
        return;
    Page* from = m_pages[size_t(m_tab)];
    Page* to = m_pages[size_t(tab)];
    const int direction = int(tab) > int(m_tab) ? 1 : -1;
    m_tab = tab;

    from->deactivated();
    to->activated();
    if (animated) {
        startTransition(from, to, direction);
    } else {
        finishTransition();
        from->setVisible(false);
        to->setVisible(true);
        to->entering(1);
    }
    updateActions(animated);

    m_context->settings().setValue(kTabKey, tabKey(tab));
    m_context->saveSettings();
    tabChanged.emit();
}

// The outgoing page fades out quickly and drifts against the direction of
// travel; the incoming one fades in over the whole time and slides in from
// the side of travel. Both are layered, so a frame repaints neither.
void ClockWindow::repeatTimer(const std::string& id)
{
#ifdef CLOCK_HAS_TIMERS
    const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                              m_context->now().time_since_epoch()).count();
    timermath::repeatTimer(ClockStore::inFolder(m_context->paths().data), id, nowMs);
#else
    (void)id;
#endif
}

void ClockWindow::startTransition(Page* from, Page* to, int direction)
{
    // A switch during a switch: the page leaving now is the one coming in,
    // from where it got to; the one that was leaving goes at once.
    double fromOpacity = 1;
    if (isSwitching()) {
        m_transition->stop();
        if (m_outgoing && m_outgoing != to) {
            m_outgoing->setVisible(false);
            m_outgoing->setOpacity(1);
            m_outgoing->setTransform({});
            m_outgoing->setLayered(false);
        }
        if (m_incoming == from)
            fromOpacity = from->opacity();
    }
    m_outgoing = from;
    m_incoming = to;
    const double toOpacity = to->isVisible() ? to->opacity() : 0;
    for (Page* page : {from, to}) {
        page->setVisible(true);
        page->setLayered(true);
    }

    if (!m_transition) {
        m_transition = std::make_unique<Animation>(m_window.get());
        m_transition->setDuration(kTransitionDuration);
        m_transition->setEasing(Animation::Easing::OutCubic);
    }
    m_transition->onUpdate = [this, from, to, direction, fromOpacity, toOpacity](double t) {
        const double out = std::min(1.0, t * 1.7);
        from->setOpacity(fromOpacity * (1 - out));
        from->setTransform({{-direction * kDriftOut * t, 0}, 1});
        to->setOpacity(toOpacity + (1 - toOpacity) * t);
        to->setTransform({{direction * kSlideIn * (1 - t), 0}, 1});
        to->entering(t);
    };
    m_transition->onFinished = [this] { finishTransition(); };
    m_transition->start();
}

void ClockWindow::finishTransition()
{
    if (m_transition)
        m_transition->stop();
    if (m_outgoing) {
        m_outgoing->setVisible(false);
        m_outgoing->setOpacity(1);
        m_outgoing->setTransform({});
        m_outgoing->setLayered(false);
    }
    if (m_incoming) {
        m_incoming->setOpacity(1);
        m_incoming->setTransform({});
        m_incoming->setLayered(false);
        m_incoming->entering(1);
    }
    m_outgoing = nullptr;
    m_incoming = nullptr;
}

void ClockWindow::updateActions(bool animated)
{
    m_root->bar->setActions(m_pages[size_t(m_tab)]->headerActions(), animated);
}

void ClockWindow::installShortcuts()
{
    Window& window = *m_window;
    for (int i = 0; i < TabCount; ++i)
        window.addShortcut(ControlModifier, uint32_t('1' + i), [this, i] { setTab(Tab(i)); });
    window.addShortcut(ControlModifier, key::letter('n'), [this] {
        Page* page = m_pages[size_t(m_tab)];
        if (!page->newItemText().empty())
            page->newItem();
    });
    window.addShortcut(ControlModifier, key::letter('w'), [this] { m_window->hideApplication(); });
    window.addShortcut(ControlModifier, key::letter('q'), [] { Application::instance()->quit(); });
}

void ClockWindow::openAbout()
{
    if (!m_about) {
        m_about = std::make_unique<AboutDialog>(m_window.get());
        m_about->setName(tr("Clock"));
        m_about->setDescription(tr("The time around the world, alarms, a stopwatch and timers."));
        m_about->setIconSource("properdesktop-clock");
    }
    m_about->show();
}

void ClockWindow::buildMainMenu()
{
    m_window->setMainMenu(nullptr);
    m_mainMenu = std::make_unique<Menu>();
    const uint32_t ctrl = ControlModifier;

    Menu* application = m_mainMenu->addSubmenu(tr("Clock"));
    application->setRole(MenuRole::Application);
    application->addItem(tr("About Clock"), [this] { openAbout(); });
    application->addSeparator();
    application->addItem(tr("Hide Clock"), [this] { m_window->hideApplication(); });
    application->addSeparator();
    const size_t quit = application->addItem(tr("Quit Clock"), [] { Application::instance()->quit(); });
    application->setItemKeyEquivalent(quit, "q", ctrl);

    Menu* file = m_mainMenu->addSubmenu(tr("File"));
    const size_t newItem = file->addItem(tr("New"), [this] {
        Page* page = m_pages[size_t(m_tab)];
        if (!page->newItemText().empty())
            page->newItem();
    });
    file->setItemKeyEquivalent(newItem, "n", ctrl);
    const size_t separator = newItem + 1;
    file->addSeparator();
    const size_t close = file->addItem(tr("Close Window"), [this] { m_window->hideApplication(); });
    file->setItemKeyEquivalent(close, "w", ctrl);
    file->onAboutToShow = [this, file, newItem, separator] {
        const std::string text = m_pages[size_t(m_tab)]->newItemText();
        file->setItemText(newItem, text);
        file->setItemVisible(newItem, !text.empty());
        file->setItemVisible(separator, !text.empty());
    };

    // The page's own rows, made again each time the menu opens.
    Menu* edit = m_mainMenu->addSubmenu(tr("Edit"));
    edit->onAboutToShow = [this, edit] {
        edit->clear();
        const std::vector<MenuAction> actions = m_pages[size_t(m_tab)]->editActions();
        if (actions.empty()) {
            edit->setItemEnabled(edit->addItem(tr("Undo"), [] {}), false);
            return;
        }
        for (const MenuAction& action : actions) {
            const size_t row = edit->addItem(action.text, action.triggered);
            if (!action.key.empty())
                edit->setItemKeyEquivalent(row, action.key, action.modifiers);
            edit->setItemEnabled(row, action.enabled);
        }
    };

    Menu* view = m_mainMenu->addSubmenu(tr("View"));
    for (int i = 0; i < TabCount; ++i) {
        const size_t row = view->addItem(tabTitle(Tab(i)), [this, i] { setTab(Tab(i)); });
        view->setItemKeyEquivalent(row, std::string(1, char('1' + i)), ctrl);
    }
    view->onAboutToShow = [this, view] {
        for (int i = 0; i < TabCount; ++i)
            view->setItemChecked(size_t(i), i == int(m_tab));
    };

    Menu* windowMenu = m_mainMenu->addSubmenu(tr("Window"));
    windowMenu->setRole(MenuRole::Windows);
    windowMenu->addItem(tr("Minimize"), [this] { m_window->minimize(); });
    windowMenu->addItem(tr("Zoom"), [this] { m_window->setMaximized(!m_window->states().maximized); });

    m_window->setMainMenu(m_mainMenu.get());
}

void ClockWindow::retranslate()
{
    m_window->setTitle(tr("Clock"));
    std::vector<std::string> titles;
    for (int i = 0; i < TabCount; ++i)
        titles.push_back(tabTitle(Tab(i)));
    SegmentedControl* tabs = m_root->bar->tabSwitch();
    tabs->setItems(titles);
    tabs->setCurrentIndex(int(m_tab), false);
    m_about.reset();
    updateActions(false);
    buildMainMenu();
}

} // namespace clockapp
