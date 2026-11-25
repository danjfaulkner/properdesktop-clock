// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "base/signal.h"

namespace clockapp::world {

class CityDatabase;

// The user's cities, in their order, as zone ids. Unknown and repeated
// zones are dropped on the way in.
class Cities
{
public:
    explicit Cities(const CityDatabase& database);

    // A first run's cities after the local one, spread round the world.
    static const std::vector<std::string>& defaultZoneIds();

    // From clock.conf. Nothing saved yet is a first run: the local city,
    // then the defaults that are not in its zone.
    void load(const std::optional<std::vector<std::string>>& zoneIds, const std::string& localZoneId);
    const std::vector<std::string>& zoneIds() const { return m_zoneIds; }
    size_t size() const { return m_zoneIds.size(); }

    bool contains(const std::string& zoneId) const;
    // At the end; false when it is there already or unknown.
    bool add(const std::string& zoneId);
    void remove(size_t index);
    // `to` is the index the city has afterwards.
    void move(size_t from, size_t to);

    properdesktop::base::Signal<> changed;

private:
    const CityDatabase& m_database;
    std::vector<std::string> m_zoneIds;
};

} // namespace clockapp::world
