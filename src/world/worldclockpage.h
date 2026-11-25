// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/signal.h"
#include "page.h"
#include "world/cities.h"
#include "world/settle.h"

namespace properdesktop::ui {
class HeaderBackdrop;
class Menu;
class ScrollView;
}

namespace clockapp::world {

class AddCityDialog;
class CardGrid;
class CityCard;
class CityDatabase;
class EmptyState;
class FrameDriver;

// World Clock: a card for each of the user's cities, kept in clock.conf
// [WorldClock] Cities; the local city and a few others until the user
// changes the list.
class WorldClockPage : public Page
{
public:
    WorldClockPage(PageContext& context, const CityDatabase& database);
    ~WorldClockPage() override;

    std::vector<HeaderAction> headerActions() override;
    std::string newItemText() const override;
    void newItem() override;
    void activated() override;
    void deactivated() override;
    void entering(double progress) override;

    Cities& cities() { return m_cities; }
    CardGrid* grid() const { return m_grid; }
    properdesktop::ui::ScrollView* scrollView() const { return m_scroll; }
    AddCityDialog* addCityDialog() const { return m_addCity.get(); }
    bool isEditing() const { return m_editing; }
    void setEditing(bool editing);
    void showAddCity();
    void removeCity(const std::string& zoneId);

protected:
    void layout() override;
    void paint(properdesktop::ui::Painter& painter) override;

private:
    int64_t nowMilliseconds() const;
    void citiesChanged(bool animated);
    void refresh(bool animated);
    void secondTicked();
    bool frame(double elapsedMs);
    bool animates() const;
    void showMenu(const std::string& zoneId, properdesktop::ui::PointF windowPosition);
    bool cardKey(CityCard* card, const properdesktop::ui::KeyEvent& event);

    PageContext& m_context;
    const CityDatabase& m_database;
    Cities m_cities;
    properdesktop::ui::ScrollView* m_scroll;
    CardGrid* m_grid;
    properdesktop::ui::HeaderBackdrop* m_backdrop;
    EmptyState* m_empty;
    std::unique_ptr<FrameDriver> m_driver;
    std::unique_ptr<AddCityDialog> m_addCity;
    std::unique_ptr<properdesktop::ui::Menu> m_menu;
    properdesktop::base::Subscription m_tick;
    properdesktop::base::Subscription m_citiesChanged;
    properdesktop::base::Subscription m_scrolled;
    std::vector<int> m_shortcuts;
    std::string m_localZoneId;

    bool m_current = false;
    bool m_editing = false;
    SpringState m_edit;
    SpringState m_emptyIn;
    // Since the second hand's last step began; past kSettleMs it rests.
    double m_stepElapsed = kSettleMs;
};

} // namespace clockapp::world
