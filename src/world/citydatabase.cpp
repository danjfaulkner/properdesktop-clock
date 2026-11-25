// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "world/citydatabase.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

#include <unicode/locid.h>
#include <unicode/normalizer2.h>
#include <unicode/tznames.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>

#include "base/file.h"
#include "base/locale.h"

using namespace properdesktop;

namespace clockapp::world {

static bool knownZone(const std::string& id)
{
    try {
        return std::chrono::locate_zone(id) != nullptr;
    } catch (const std::runtime_error&) {
        return false;
    }
}

static std::string toUtf8(const icu::UnicodeString& text)
{
    std::string result;
    text.toUTF8String(result);
    return result;
}

std::string CityDatabase::nameFromZoneId(std::string_view zoneId)
{
    const size_t slash = zoneId.rfind('/');
    std::string city(slash == std::string_view::npos ? zoneId : zoneId.substr(slash + 1));
    std::replace(city.begin(), city.end(), '_', ' ');
    return city;
}

std::string CityDatabase::folded(std::string_view text)
{
    UErrorCode status = U_ZERO_ERROR;
    const icu::Normalizer2* nfd = icu::Normalizer2::getNFDInstance(status);
    icu::UnicodeString source = icu::UnicodeString::fromUTF8(icu::StringPiece(text.data(), int32_t(text.size())));
    icu::UnicodeString decomposed = U_SUCCESS(status) ? nfd->normalize(source, status) : source;
    if (U_FAILURE(status))
        decomposed = source;

    icu::UnicodeString result;
    for (int32_t i = 0; i < decomposed.length();) {
        const UChar32 c = decomposed.char32At(i);
        i += U16_LENGTH(c);
        if (u_charType(c) == U_NON_SPACING_MARK)
            continue;
        // Letters with a stroke have no decomposition.
        switch (c) {
        case U'_':
            result.append(UChar32(U' '));
            break;
        case U'ø':
        case U'Ø':
            result.append(UChar32(U'o'));
            break;
        case U'ł':
        case U'Ł':
            result.append(UChar32(U'l'));
            break;
        case U'đ':
        case U'Đ':
            result.append(UChar32(U'd'));
            break;
        default:
            result.append(c);
        }
    }
    result.foldCase();
    return toUtf8(result);
}

double CityDatabase::iso6709Degrees(std::string_view text, size_t degreeDigits)
{
    if (text.empty() || (text[0] != '+' && text[0] != '-'))
        return 0.0;
    const size_t digits = text.size() - 1;
    if (digits != degreeDigits + 2 && digits != degreeDigits + 4)
        return 0.0;

    const auto field = [text](size_t at, size_t length) {
        int value = 0;
        for (size_t i = at; i < at + length; ++i)
            value = value * 10 + (text[i] >= '0' && text[i] <= '9' ? text[i] - '0' : 0);
        return value;
    };
    const double minutes = field(1 + degreeDigits, 2);
    const double seconds = digits == degreeDigits + 4 ? field(3 + degreeDigits, 2) : 0;
    const double degrees = field(1, degreeDigits) + minutes / 60 + seconds / 3600;
    return text[0] == '-' ? -degrees : degrees;
}

std::string CityDatabase::localZoneId()
{
    char target[512];
    const ssize_t size = readlink("/etc/localtime", target, sizeof(target) - 1);
    if (size > 0) {
        const std::string path(target, size_t(size));
        const size_t at = path.find("zoneinfo/");
        if (at != std::string::npos && knownZone(path.substr(at + 9)))
            return path.substr(at + 9);
    }
    try {
        return std::string(std::chrono::current_zone()->name());
    } catch (const std::runtime_error&) {
        return "Etc/UTC";
    }
}

const CityDatabase& CityDatabase::system()
{
    static const CityDatabase database = [] {
        std::string language = base::Locale::system().name();
        if (language.empty() || language == "C" || language == "POSIX")
            language = "en";
        const char* table = access("/usr/share/zoneinfo/zone.tab", R_OK) == 0 ? "/usr/share/zoneinfo/zone.tab"
                                                                                : "/usr/share/zoneinfo/zone1970.tab";
        return CityDatabase(base::file::read(table).value_or(std::string()), language);
    }();
    return database;
}

CityDatabase::CityDatabase(const std::string& zoneTab, const std::string& language)
    : m_language(language)
{
    UErrorCode status = U_ZERO_ERROR;
    const icu::Locale locale(language.c_str());
    std::unique_ptr<icu::TimeZoneNames> names(icu::TimeZoneNames::createInstance(locale, status));
    if (U_FAILURE(status))
        names.reset();

    std::istringstream lines(zoneTab);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty() || line.starts_with('#'))
            continue;
        std::vector<std::string> parts;
        std::istringstream fields(line);
        std::string field;
        while (std::getline(fields, field, '\t'))
            parts.push_back(field);
        if (parts.size() < 3 || !knownZone(parts[2]))
            continue;

        City city = make(parts[2], parts[0].substr(0, parts[0].find(',')));
        const std::string_view coordinate = parts[1];
        const size_t split = coordinate.find_first_of("+-", 1);
        if (split != std::string_view::npos) {
            city.latitude = iso6709Degrees(coordinate.substr(0, split), 2);
            city.longitude = iso6709Degrees(coordinate.substr(split), 3);
            city.hasLocation = true;
        }
        if (names) {
            icu::UnicodeString exemplar;
            names->getExemplarLocationName(icu::UnicodeString::fromUTF8(city.zoneId), exemplar);
            if (!exemplar.isBogus() && !exemplar.isEmpty())
                city.name = toUtf8(exemplar);
        }
        m_cities.push_back(std::move(city));
    }

    std::sort(m_cities.begin(), m_cities.end(), [](const City& a, const City& b) { return a.zoneId < b.zoneId; });
    for (const City& city : m_cities)
        m_folded.push_back({folded(city.name), folded(city.zoneId), folded(city.country)});
}

City CityDatabase::make(const std::string& zoneId, const std::string& countryCode) const
{
    City city{zoneId, nameFromZoneId(zoneId), countryCode, {}, 0, 0, false};
    if (!countryCode.empty()) {
        icu::UnicodeString name;
        icu::Locale("", countryCode.c_str()).getDisplayCountry(icu::Locale(m_language.c_str()), name);
        city.country = toUtf8(name);
        if (city.country.empty())
            city.country = base::Locale::territoryName(countryCode);
    }
    return city;
}

const City* CityDatabase::find(std::string_view zoneId) const
{
    const auto at = std::lower_bound(m_cities.begin(), m_cities.end(), zoneId,
                                     [](const City& city, std::string_view id) { return city.zoneId < id; });
    if (at != m_cities.end() && at->zoneId == zoneId)
        return &*at;

    for (const City& city : m_extra) {
        if (city.zoneId == zoneId)
            return &city;
    }
    const std::string id(zoneId);
    if (id.empty() || !knownZone(id))
        return nullptr;
    m_extra.push_back(make(id, {}));
    return &m_extra.back();
}

std::vector<const City*> CityDatabase::search(std::string_view text, size_t limit) const
{
    std::string needle = folded(text);
    needle.erase(0, needle.find_first_not_of(' '));
    needle.erase(needle.find_last_not_of(' ') + 1);
    if (needle.empty())
        return {};

    struct Match
    {
        int rank;
        size_t index;
    };
    std::vector<Match> matches;
    for (size_t i = 0; i < m_cities.size(); ++i) {
        const Entry& entry = m_folded[i];
        int rank = -1;
        if (entry.name.starts_with(needle))
            rank = 0;
        else if (entry.id.find("/" + needle) != std::string::npos)
            rank = 1;
        else if (entry.name.find(needle) != std::string::npos)
            rank = 2;
        else if (entry.country.starts_with(needle) || folded(m_cities[i].countryCode) == needle)
            rank = 3;
        else if (entry.country.find(needle) != std::string::npos)
            rank = 4;
        if (rank >= 0)
            matches.push_back({rank, i});
    }
    std::stable_sort(matches.begin(), matches.end(), [this](const Match& a, const Match& b) {
        if (a.rank != b.rank)
            return a.rank < b.rank;
        return m_folded[a.index].name < m_folded[b.index].name;
    });

    std::vector<const City*> result;
    for (const Match& match : matches) {
        if (result.size() >= limit)
            break;
        result.push_back(&m_cities[match.index]);
    }
    return result;
}

} // namespace clockapp::world
