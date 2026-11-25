// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "world/worldclockpage.h"

#include <algorithm>
#include <chrono>

#include "base/i18n.h"
#include "pagecontext.h"
#include "style.h"
#include "ui/header_backdrop.h"
#include "ui/keys.h"
#include "ui/menu.h"
#include "ui/painter.h"
#include "ui/scroll_bar.h"
#include "ui/scroll_view.h"
#include "ui/theme.h"
#include "ui/window.h"
#include "world/addcitydialog.h"
#include "world/cardgrid.h"
#include "world/citycard.h"
#include "world/citydatabase.h"
#include "world/emptystate.h"
#include "world/framedriver.h"
#include "world/zonetime.h"

using namespace properdesktop;
using namespace properdesktop::ui;
using properdesktop::i18n::tr;
using properdesktop::i18n::trc;

namespace clockapp {

std::unique_ptr<Page> makeWorldClockPage(PageContext& context)
{
    return std::make_unique<world::WorldClockPage>(context, world::CityDatabase::system());
}

} // namespace clockapp

namespace clockapp::world {

static constexpr const char* kCitiesKey = "WorldClock/Cities";
static constexpr double kEditSettleMs = 260;
static constexpr double kEmptySettleMs = 900;

WorldClockPage::WorldClockPage(PageContext& context, const CityDatabase& database)
    : m_context(context)
    , m_database(database)
    , m_cities(database)
    , m_driver(std::make_unique<FrameDriver>(&context.window()))
{
    setName("page:world");
    m_localZoneId = CityDatabase::localZoneId();

    m_scroll = addChild<ScrollView>();
    m_scroll->setName("world:scroll");
    m_scroll->setScrollDirections(false, true);
    m_scroll->setBottomCornerRadius(Theme::current().windowRadius);
    m_grid = m_scroll->setContent<CardGrid>();
    m_scrolled = m_scroll->scrolled.connect([this] { m_grid->updateFloating(); });
    m_backdrop = addChild<HeaderBackdrop>(m_scroll);
    m_backdrop->setSeparatorVisible(false);
    m_empty = addChild<EmptyState>();
    m_empty->onAdd = [this] { showAddCity(); };

    m_grid->onMotion = [this] { m_driver->wake(); };
    m_grid->onRemove = [this](const std::string& zoneId) { removeCity(zoneId); };
    m_grid->onContextMenu = [this](const std::string& zoneId, PointF at) { showMenu(zoneId, at); };
    m_grid->onMoved = [this](size_t from, size_t to) { m_cities.move(from, to); };
    m_grid->onKey = [this](CityCard* card, const KeyEvent& event) { return cardKey(card, event); };
    m_grid->onCardMade = [this](CityCard* card) {
        const int64_t now = nowMilliseconds();
        card->update(ZoneTime::at(m_localZoneId, now), now, 1, false);
    };

    m_driver->onFrame = [this](double elapsedMs, uint64_t) { return frame(elapsedMs); };
    m_tick = context.tick.connect([this] { secondTicked(); });

    const base::Settings& settings = context.settings();
    std::optional<std::vector<std::string>> saved;
    if (settings.contains(kCitiesKey))
        saved = settings.stringListValue(kCitiesKey);
    m_cities.load(saved, m_localZoneId);
    m_citiesChanged = m_cities.changed.connect([this] {
        m_context.settings().setValue(kCitiesKey, m_cities.zoneIds());
        m_context.saveSettings();
        citiesChanged(true);
    });
    m_edit.jump(0);
    m_emptyIn.jump(1);
    citiesChanged(false);

    m_shortcuts.push_back(context.addShortcut(*this, NoModifier, key::Escape, [this] {
        if (m_editing)
            setEditing(false);
    }));
}

WorldClockPage::~WorldClockPage()
{
    for (int id : m_shortcuts)
        m_context.removeShortcut(id);
}

int64_t WorldClockPage::nowMilliseconds() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(m_context.now().time_since_epoch()).count();
}

bool WorldClockPage::animates() const
{
    return m_current && isVisible() && window() && !window()->isHidden();
}

std::vector<HeaderAction> WorldClockPage::headerActions()
{
    std::vector<HeaderAction> actions;
    if (m_cities.size() > 0 || m_editing) {
        actions.push_back({"world-edit", {}, m_editing ? trc("button", "Done") : trc("button", "Edit"), true, m_editing,
                           [this] { setEditing(!m_editing); }});
    }
    actions.push_back({"world-add", "add", tr("Add City"), true, false, [this] { showAddCity(); }});
    return actions;
}

std::string WorldClockPage::newItemText() const
{
    return tr("Add City…");
}

void WorldClockPage::newItem()
{
    showAddCity();
}

void WorldClockPage::activated()
{
    m_current = true;
    m_localZoneId = CityDatabase::localZoneId();
    refresh(false);
}

void WorldClockPage::deactivated()
{
    m_current = false;
    m_grid->cancelDrag();
    setEditing(false);
}

void WorldClockPage::entering(double progress)
{
    (void)progress;
}

void WorldClockPage::setEditing(bool editing)
{
    if (editing == m_editing)
        return;
    m_editing = editing;
    m_edit.target = editing ? 1 : 0;
    if (animates())
        m_driver->wake();
    else {
        m_edit.jump(m_edit.target);
        m_grid->setEditProgress(m_edit.value);
    }
    m_context.actionsChanged(*this);
}

void WorldClockPage::showAddCity()
{
    if (m_addCity) {
        m_addCity->show();
        return;
    }
    m_addCity = std::make_unique<AddCityDialog>(m_context.window(), m_database, m_cities,
                                                [this] { return nowMilliseconds(); });
    m_addCity->onPicked = [this](const std::string& zoneId) { m_cities.add(zoneId); };
    m_addCity->onClosed = [this] { m_addCity.reset(); };
    m_addCity->show();
}

void WorldClockPage::removeCity(const std::string& zoneId)
{
    const auto& ids = m_cities.zoneIds();
    const auto at = std::find(ids.begin(), ids.end(), zoneId);
    if (at != ids.end())
        m_cities.remove(size_t(at - ids.begin()));
}

void WorldClockPage::citiesChanged(bool animated)
{
    std::vector<const City*> cities;
    for (const std::string& id : m_cities.zoneIds()) {
        if (const City* city = m_database.find(id))
            cities.push_back(city);
    }
    const bool wasEmpty = m_empty->isVisible();
    m_grid->setCities(cities, animated && animates());
    const bool empty = cities.empty();
    if (empty && m_editing)
        setEditing(false);
    m_empty->setVisible(empty);
    if (empty && !wasEmpty) {
        m_emptyIn.jump(animated && animates() ? 0 : 1);
        m_emptyIn.target = 1;
        m_empty->setProgress(m_emptyIn.value);
        m_driver->wake();
    }
    m_scroll->requestLayout();
    requestLayout();
    m_context.actionsChanged(*this);
}

void WorldClockPage::refresh(bool animated)
{
    const int64_t now = nowMilliseconds();
    const ZoneTime here = ZoneTime::at(m_localZoneId, now);
    const double step = secondHandStep(m_stepElapsed);
    for (CityCard* card : m_grid->cards())
        card->update(here, now, step, animated);
}

void WorldClockPage::secondTicked()
{
    m_localZoneId = CityDatabase::localZoneId();
    if (!animates()) {
        m_stepElapsed = kSettleMs;
        if (isVisible())
            refresh(false);
        return;
    }
    m_stepElapsed = 0;
    refresh(true);
    m_driver->wake();
}

bool WorldClockPage::frame(double elapsedMs)
{
    if (!animates()) {
        m_stepElapsed = kSettleMs;
        m_edit.jump(m_edit.target);
        m_emptyIn.jump(m_emptyIn.target);
    }
    bool moving = false;
    if (m_stepElapsed < kSettleMs) {
        m_stepElapsed = std::min(m_stepElapsed + elapsedMs, kSettleMs);
        const double step = secondHandStep(m_stepElapsed);
        for (CityCard* card : m_grid->cards())
            card->setSecondStep(step);
        moving = m_stepElapsed < kSettleMs;
    }
    if (!(m_edit.isAtRest() && m_edit.value == m_edit.target)) {
        m_edit.advance(elapsedMs, kEditSettleMs);
        moving = moving || !m_edit.isAtRest();
    }
    m_grid->setEditProgress(std::clamp(m_edit.value, 0.0, 1.0));
    if (!(m_emptyIn.isAtRest() && m_emptyIn.value == m_emptyIn.target)) {
        m_emptyIn.advance(elapsedMs, kEmptySettleMs);
        m_empty->setProgress(m_emptyIn.value);
        moving = moving || !m_emptyIn.isAtRest();
    }
    if (!animates())
        m_grid->settleNow();
    else if (m_grid->advance(elapsedMs))
        moving = true;
    return moving;
}

void WorldClockPage::showMenu(const std::string& zoneId, PointF windowPosition)
{
    m_menu = std::make_unique<Menu>();
    m_menu->addItem(tr("Remove"), [this, zoneId] { removeCity(zoneId); });
    m_menu->addSeparator();
    m_menu->addItem(m_editing ? tr("Done Editing") : tr("Edit Cities"), [this] { setEditing(!m_editing); });
    m_menu->popup(&m_context.window(), windowPosition);
}

bool WorldClockPage::cardKey(CityCard* card, const KeyEvent& event)
{
    const std::vector<CityCard*>& cards = m_grid->cards();
    const auto at = std::find(cards.begin(), cards.end(), card);
    if (at == cards.end())
        return false;
    const int index = int(at - cards.begin());
    const int columns = CardGrid::columnsFor(m_grid->geometry().width);
    int target = index;
    switch (event.key) {
    case key::Left:
        target = index - 1;
        break;
    case key::Right:
        target = index + 1;
        break;
    case key::Up:
        target = index - columns;
        break;
    case key::Down:
        target = index + columns;
        break;
    default:
        return false;
    }
    if (target < 0 || target >= int(m_cities.size()))
        return true;
    // Alt with an arrow moves the card itself.
    if (event.modifiers & AltModifier) {
        m_cities.move(size_t(index), size_t(target));
        card->setFocus(FocusReason::Keyboard);
        return true;
    }
    cards[size_t(target)]->setFocus(FocusReason::Keyboard);
    m_scroll->ensureVisible(cards[size_t(target)]->geometry());
    return true;
}

void WorldClockPage::layout()
{
    const RectF rect = localRect();
    m_scroll->setGeometry(rect);
    // The bar starts one inset below the floating header, as it would below a
    // flat edge; its bottom end keeps clear of the window's corner.
    m_scroll->setVerticalBarPadding(headerHeight() + ScrollBar::inset, ScrollBar::inset);
    m_grid->setTopInset(headerHeight());
    m_backdrop->setGeometry({0, 0, rect.width, headerHeight()});
    m_empty->setGeometry({0, headerHeight(), rect.width, rect.height - headerHeight()});
}

void WorldClockPage::paint(Painter& painter)
{
    painter.fillRect(localRect(), style::page());
}

} // namespace clockapp::world
