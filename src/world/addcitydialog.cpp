// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "world/addcitydialog.h"

#include <algorithm>
#include <cmath>

#include "base/collator.h"
#include "base/i18n.h"
#include "style.h"
#include "ui/dialog_window.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/list_view.h"
#include "ui/painter.h"
#include "ui/search_field.h"
#include "ui/theme.h"
#include "ui/window.h"
#include "world/cities.h"
#include "world/citydatabase.h"
#include "world/zonetime.h"

using namespace properdesktop;
using namespace properdesktop::ui;
using properdesktop::i18n::tr;

namespace clockapp::world {

static constexpr double kWidth = 420;
static constexpr double kHeight = 520;
static constexpr double kSearchHeight = 32;
static constexpr double kSearchGap = 12;
static constexpr double kRowHeight = 46;
// The rows line up with the search field, the dialog's content padding in
// from its sides; that is past the dialog's corner radius, so the highlight
// takes a control's own radius.
static constexpr double kRowInset = 16;
static constexpr double kRowRadius = 8;
static constexpr double kRowPadding = 10;

// The toolkit's search field, with Up, Down and Return for the list.
class SearchBox : public SearchField
{
public:
    SearchBox()
        : SearchField(tr("Search city or country"))
    {
        setName("world:search");
    }

    std::function<bool(const KeyEvent& event)> onKey;

protected:
    bool keyPress(const KeyEvent& event) override
    {
        if (onKey && onKey(event))
            return true;
        return SearchField::keyPress(event);
    }
};

// One city: its name and country, and on the right its time and how far
// ahead or behind it is.
class ResultRow : public View
{
public:
    ResultRow()
    {
        setTracksHover(true);
        m_name = addChild<Label>();
        m_name->setWeight(600);
        m_name->setFontSize(style::points(10));
        m_name->setElide(Label::Elide::Right);
        m_country = addChild<Label>();
        m_country->setFontSize(style::points(8.5));
        m_country->setElide(Label::Elide::Right);
        m_time = addChild<Label>();
        m_time->setFontSize(style::points(10));
        m_time->setWeight(500);
        m_time->setFontFeatures({{"tnum", 1}});
        m_time->setTextAlignment(Label::TextAlignment::Right);
        m_detail = addChild<Label>();
        m_detail->setFontSize(style::points(8.5));
        m_detail->setFontFeatures({{"tnum", 1}});
        m_detail->setTextAlignment(Label::TextAlignment::Right);
    }

    void bind(int row, const City& city, std::string time, std::string detail, bool current, bool added)
    {
        index = row;
        setName("world:result:" + city.zoneId);
        m_name->setText(city.name);
        m_country->setText(added ? tr("Added") : city.country);
        m_time->setText(std::move(time));
        m_detail->setText(std::move(detail));
        m_current = current;
        m_added = added;
        requestLayout();
        requestPaint();
    }

    int index = -1;
    std::function<void(int index)> onPicked;
    std::function<void(int index)> onHovered;

    bool acceptsPointer() const override { return true; }

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double right = 120;
        const double textWidth = width - 2 * kRowPadding - right - 8;
        m_name->setGeometry({kRowPadding, 6, textWidth, 19});
        m_country->setGeometry({kRowPadding, 25, textWidth, 15});
        m_time->setGeometry({width - kRowPadding - right, 6, right, 19});
        m_detail->setGeometry({width - kRowPadding - right, 25, right, 15});

        const Theme& theme = Theme::current();
        const Color primary = m_current ? theme.highlightedText : style::text();
        const Color secondary = m_current ? theme.highlightedText.withAlpha(0.8) : style::secondaryText();
        m_name->setColor(primary);
        m_time->setColor(primary);
        m_country->setColor(secondary);
        m_detail->setColor(secondary);
        setOpacity(m_added && !m_current ? 0.55 : 1);
    }

    void paint(Painter& painter) override
    {
        if (m_current)
            painter.fillRoundedRect(localRect(), kRowRadius, Theme::current().highlight);
    }

    void hoverChanged() override
    {
        if (isHovered() && onHovered)
            onHovered(index);
    }

    void pointerRelease(const PointerEvent& event) override
    {
        if (event.button == MouseButton::Left && localRect().contains(event.position) && onPicked)
            onPicked(index);
    }

private:
    Label* m_name;
    Label* m_country;
    Label* m_time;
    Label* m_detail;
    bool m_current = false;
    bool m_added = false;
};

// The search field. The list is the dialog's own child beside it, so it can
// run the dialog's full width with its bar at the right edge; the content
// places it.
class AddCityDialog::Content : public View
{
public:
    explicit Content(AddCityDialog& dialog)
        : m_dialog(dialog)
    {
        setName("world:add-city");
        search = addChild<SearchBox>();
        empty = addChild<Label>(tr("No matching cities"));
        empty->setTextAlignment(Label::TextAlignment::Center);
    }

    SearchBox* search;
    Label* empty;

    SizeF sizeHint() const override { return {kWidth, kHeight}; }

protected:
    void layout() override
    {
        const double width = geometry().width;
        search->setGeometry({0, 0, width, kSearchHeight});
        const double listTop = kSearchHeight + kSearchGap;
        if (ListView* list = m_dialog.m_list; list && parent()) {
            const RectF dialog = parent()->localRect();
            const double top = geometry().y + listTop;
            list->setGeometry({0, top, dialog.width, std::max(0.0, dialog.height - top)});
        }
        const double area = std::max(0.0, geometry().height - listTop);
        const SizeF hint = empty->sizeHint();
        empty->setGeometry({0, listTop + style::centred(area, hint.height), width, std::ceil(hint.height)});
        empty->setColor(style::secondaryText());
        empty->setVisible(m_dialog.m_results.empty());
    }

private:
    AddCityDialog& m_dialog;
};

AddCityDialog::AddCityDialog(Window& parent, const CityDatabase& database, const Cities& cities,
                             std::function<int64_t()> now)
    : m_database(database)
    , m_cities(cities)
    , m_now(std::move(now))
{
    for (const City& city : database.cities())
        m_all.push_back(&city);
    const base::Collator collator;
    std::stable_sort(m_all.begin(), m_all.end(),
                     [&collator](const City* a, const City* b) { return collator.compare(a->name, b->name) < 0; });

    m_dialog = std::make_unique<DialogWindow>(&parent, tr("Add City"), kWidth + 32);
    m_content = m_dialog->setContent<Content>(*this);
    m_list = m_dialog->window()->contentView()->addChild<ListView>();
    m_list->setName("world:results");
    m_list->setRowHeight(kRowHeight);
    m_list->setMargins({kRowInset, 0, kRowInset, kRowInset});
    m_list->setBottomCornerRadius(Theme::current().windowRadius);
    m_list->setElastic(false);
    m_dialog->onClosed = [this] {
        if (onClosed)
            onClosed();
    };

    ListView* list = m_list;
    list->createItem = [this]() -> std::unique_ptr<View> {
        auto row = std::make_unique<ResultRow>();
        row->onPicked = [this](int index) { pick(index); };
        row->onHovered = [this](int index) { setCurrent(index); };
        return row;
    };
    list->bindItem = [this](View* item, size_t row) {
        const City& city = *m_results[row];
        const int64_t now = m_now();
        const ZoneTime here = ZoneTime::at(CityDatabase::localZoneId(), now);
        const ZoneTime there = ZoneTime::at(city.zoneId, now);
        static_cast<ResultRow*>(item)->bind(int(row), city, digitalText(there.time),
                                            captionText(dayDifference(here.date, there.date),
                                                        there.offsetSeconds - here.offsetSeconds),
                                            int(row) == m_current, m_cities.contains(city.zoneId));
    };

    TextField* field = m_content->search;
    field->onTextChanged = [this, field] { search(field->text()); };
    m_content->search->onKey = [this](const KeyEvent& event) {
        const int count = int(m_results.size());
        switch (event.key) {
        case key::Down:
            setCurrent(count == 0 ? -1 : std::min(m_current + 1, count - 1));
            return true;
        case key::Up:
            setCurrent(count == 0 ? -1 : std::max(m_current - 1, 0));
            return true;
        case key::Return:
            pick(m_current);
            return true;
        default:
            return false;
        }
    };
    search({});
}

AddCityDialog::~AddCityDialog() = default;

Window* AddCityDialog::window() const
{
    return m_dialog->window();
}

void AddCityDialog::show()
{
    m_dialog->show();
    m_content->search->setFocus();
}

void AddCityDialog::close()
{
    m_dialog->close();
}

void AddCityDialog::setSearchText(const std::string& text)
{
    m_content->search->setText(text);
    search(text);
}

void AddCityDialog::search(const std::string& text)
{
    m_results = CityDatabase::folded(text).find_first_not_of(' ') == std::string::npos
        ? m_all
        : m_database.search(text, 200);
    m_current = m_results.empty() ? -1 : 0;
    m_list->setRowCount(m_results.size());
    m_list->setScrollPosition({0, 0});
    m_list->reload();
    m_content->requestLayout();
}

void AddCityDialog::setCurrent(int index)
{
    if (index == m_current)
        return;
    m_current = index;
    m_list->reload();
    if (index >= 0)
        m_list->scrollToRow(size_t(index));
}

void AddCityDialog::pick(int index)
{
    if (index < 0 || index >= int(m_results.size()))
        return;
    const City* city = m_results[size_t(index)];
    if (m_cities.contains(city->zoneId))
        return;
    if (onPicked)
        onPicked(city->zoneId);
    close();
}

} // namespace clockapp::world
