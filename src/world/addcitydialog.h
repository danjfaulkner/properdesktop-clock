// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace properdesktop::ui {
class DialogWindow;
class ListView;
class Window;
}

namespace clockapp::world {

class Cities;
class CityDatabase;
struct City;

// Add City: a search field over every city, the time and the day there on
// each row. Up and Down move through the list, Return adds the current city,
// a click adds the one clicked; cities already on the page are marked and
// cannot be added twice.
class AddCityDialog
{
public:
    AddCityDialog(properdesktop::ui::Window& parent, const CityDatabase& database, const Cities& cities,
                  std::function<int64_t()> now);
    ~AddCityDialog();

    AddCityDialog(const AddCityDialog&) = delete;
    AddCityDialog& operator=(const AddCityDialog&) = delete;

    void show();
    void close();
    properdesktop::ui::Window* window() const;
    // Across the whole dialog, from under the search field to its bottom edge.
    properdesktop::ui::ListView* list() const { return m_list; }

    // The cities listed for the search text, best first.
    const std::vector<const City*>& results() const { return m_results; }
    int currentIndex() const { return m_current; }
    void setSearchText(const std::string& text);

    std::function<void(const std::string& zoneId)> onPicked;
    std::function<void()> onClosed;

private:
    class Content;

    void search(const std::string& text);
    void setCurrent(int index);
    void pick(int index);

    const CityDatabase& m_database;
    const Cities& m_cities;
    std::function<int64_t()> m_now;
    std::unique_ptr<properdesktop::ui::DialogWindow> m_dialog;
    Content* m_content = nullptr;
    properdesktop::ui::ListView* m_list = nullptr;
    std::vector<const City*> m_all;
    std::vector<const City*> m_results;
    int m_current = -1;
};

} // namespace clockapp::world
