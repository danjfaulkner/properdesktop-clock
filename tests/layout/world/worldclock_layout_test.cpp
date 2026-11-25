// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pagefixture.h"

#include <algorithm>
#include <cmath>

#include "ui/bitmap.h"
#include "ui/scroll_bar.h"
#include "ui/scroll_view.h"
#include "ui/test_driver.h"
#include "ui/theme.h"
#include "ui/list_view.h"
#include "world/addcitydialog.h"
#include "world/cardgrid.h"
#include "world/citycard.h"
#include "world/clockface.h"
#include "world/worldclockpage.h"

using namespace properdesktop::ui;
using namespace clockapp::world;
namespace style = clockapp::style;

using List = std::vector<std::string>;

static const List kCities = {"Asia/Tokyo", "Europe/London", "America/New_York", "Asia/Singapore", "Europe/Paris"};

static WorldClockPage* showPage(PageFixture& fixture)
{
    fixture.context.settings().setValue("WorldClock/Cities", kCities);
    fixture.show(clockapp::makeWorldClockPage(fixture.context));
    return static_cast<WorldClockPage*>(fixture.page);
}

static bool onGrid(double value, double scale)
{
    return std::abs(value * scale - std::round(value * scale)) < 1e-6;
}

class WorldClockLayout : public testing::TestWithParam<double>
{
};

TEST_P(WorldClockLayout, CardsSitInEvenRows)
{
    const double scale = GetParam();
    PageFixture fixture(scale);
    WorldClockPage* page = showPage(fixture);
    const auto& cards = page->grid()->cards();
    ASSERT_EQ(cards.size(), kCities.size());

    const int columns = CardGrid::columnsFor(page->grid()->geometry().width);
    ASSERT_GE(columns, 3);
    const RectF first = cards[0]->geometry();
    // Below the header by the page's padding; equal margins left and right.
    EXPECT_EQ(first.y, style::snap(style::headerHeight + style::pagePadding, scale));
    const RectF lastInRow = cards[size_t(columns - 1)]->geometry();
    EXPECT_NEAR(first.x, page->grid()->geometry().width - lastInRow.right(), 1.0);
    EXPECT_GE(first.x, style::pagePadding);

    for (size_t i = 1; i < cards.size(); ++i) {
        const RectF previous = cards[i - 1]->geometry();
        const RectF card = cards[i]->geometry();
        EXPECT_EQ(card.width, first.width);
        EXPECT_EQ(card.height, first.height);
        if (i % size_t(columns) != 0) {
            EXPECT_NEAR(card.x - previous.right(), style::cardGap, 1.0 / scale) << i;
            EXPECT_EQ(card.y, previous.y);
        } else {
            EXPECT_NEAR(card.y - previous.bottom(), style::cardGap, 1.0 / scale) << i;
            EXPECT_EQ(card.x, first.x);
        }
        EXPECT_TRUE(onGrid(card.x, scale)) << card.x;
        EXPECT_TRUE(onGrid(card.y, scale)) << card.y;
    }
}

TEST_P(WorldClockLayout, CardContentIsCentredAndTheRemoveButtonConcentric)
{
    const double scale = GetParam();
    PageFixture fixture(scale);
    WorldClockPage* page = showPage(fixture);
    page->setEditing(true);
    CityCard* card = page->grid()->cards()[0];
    ASSERT_TRUE(TestDriver::waitFor([&] {
        fixture.driver.layout();
        return fixture.driver.shows("world:remove:Asia/Tokyo");
    }));

    const RectF rect = card->localRect();
    const RectF face = card->face()->geometry();
    EXPECT_NEAR(face.x, rect.width - face.right(), 1.0);
    EXPECT_EQ(face.width, face.height);

    // Centred on the corner's arc: one gap to the top and left edges, and
    // its radius the card's less that gap.
    const RectF remove = card->removeButtonRect();
    const double gap = remove.x;
    EXPECT_EQ(remove.y, gap);
    EXPECT_GT(gap, 0);
    EXPECT_DOUBLE_EQ(remove.width / 2, CityCard::kRadius - gap);

    // The name and the time centred under the face.
    const RectF frame = fixture.driver.frame("world:card:Asia/Tokyo");
    const RectF name = fixture.driver.frame("world:name:Asia/Tokyo");
    EXPECT_NEAR(name.x - frame.x, frame.right() - name.right(), 1e-6);
    RectF time = fixture.driver.frame("world:time:Asia/Tokyo");
    if (fixture.driver.shows("world:period:Asia/Tokyo")) {
        const RectF period = fixture.driver.frame("world:period:Asia/Tokyo");
        time = {std::min(time.x, period.x), time.y, std::max(time.right(), period.right()) - std::min(time.x, period.x),
                time.height};
    }
    EXPECT_NEAR(time.x - frame.x, frame.right() - time.right(), 1.0);
}

TEST_P(WorldClockLayout, EmptyStateIsCentred)
{
    const double scale = GetParam();
    PageFixture fixture(scale);
    fixture.context.settings().setValue("WorldClock/Cities", List{});
    fixture.show(clockapp::makeWorldClockPage(fixture.context));
    const RectF empty = fixture.driver.frame("world:empty");
    const RectF button = fixture.driver.frame("world:empty:add");
    EXPECT_NEAR(button.x - empty.x, empty.right() - button.right(), 1.0);
    EXPECT_EQ(empty.y, style::headerHeight);
}

// The handle's ends in the page's coordinates, from its drawn pixels.
static std::pair<double, double> handleEnds(ScrollBar* bar, double scale)
{
    const Bitmap image = bar->grab(scale);
    int first = -1;
    int last = -1;
    const int column = image.width() / 2;
    uint32_t peak = 0;
    for (int y = 0; y < image.height(); ++y)
        peak = std::max(peak, image.pixel(column, y) >> 24);
    // Half coverage marks an antialiased end.
    for (int y = 0; y < image.height(); ++y) {
        if (peak > 0 && (image.pixel(column, y) >> 24) * 2 >= peak) {
            if (first < 0)
                first = y;
            last = y;
        }
    }
    const double top = bar->mapToWindow({0, 0}).y;
    return {top + first / scale, top + (last + 1) / scale};
}

// The handle's right edge in window coordinates, from its drawn pixels.
static double handleRight(ScrollBar* bar, double scale)
{
    const Bitmap image = bar->grab(scale);
    int row = -1;
    for (int y = 0; y < image.height() && row < 0; ++y) {
        if ((image.pixel(image.width() / 2, y) >> 24) > 0x40)
            row = y + int(4 * scale);
    }
    uint32_t peak = 0;
    for (int x = 0; x < image.width(); ++x)
        peak = std::max(peak, image.pixel(x, row) >> 24);
    int last = -1;
    for (int x = 0; x < image.width(); ++x) {
        if (peak > 0 && (image.pixel(x, row) >> 24) * 2 >= peak)
            last = x;
    }
    return bar->mapToWindow({0, 0}).x + (last + 1) / scale;
}

static ScrollBar* verticalBar(View* scroll)
{
    for (const auto& child : scroll->children()) {
        auto* candidate = dynamic_cast<ScrollBar*>(child.get());
        if (candidate && candidate->geometry().height > candidate->geometry().width)
            return candidate;
    }
    return nullptr;
}

static void letBarsFadeIn(PageFixture& fixture)
{
    TestDriver::waitFor([&] {
        fixture.driver.layout();
        return false;
    }, std::chrono::milliseconds(600));
}

TEST_P(WorldClockLayout, TheScrollBarClearsTheHeaderAndTheWindowCorner)
{
    const double scale = GetParam();
    PageFixture fixture(scale);
    fixture.context.settings().setValue("WorldClock/Cities",
                                        List{"Asia/Tokyo", "Europe/London", "America/New_York", "Asia/Singapore",
                                             "Europe/Paris", "Australia/Sydney", "Asia/Dubai", "Europe/Berlin",
                                             "America/Chicago", "America/Los_Angeles", "Asia/Kolkata", "Africa/Cairo"});
    fixture.show(clockapp::makeWorldClockPage(fixture.context));
    auto* page = static_cast<WorldClockPage*>(fixture.page);
    ScrollView* scroll = page->scrollView();
    scroll->setVerticalBarPolicy(ScrollView::BarPolicy::AlwaysOn);
    fixture.driver.layout();

    // The page, and so the scroll view, reaches the window's bottom edge.
    EXPECT_EQ(scroll->mapToWindow({0, scroll->geometry().height}).y, fixture.window.size().height);

    ScrollBar* bar = verticalBar(scroll);
    ASSERT_NE(bar, nullptr);
    ASSERT_TRUE(bar->isNeeded());
    letBarsFadeIn(fixture);

    const double windowHeight = fixture.window.size().height;
    const double corner = ScrollBar::cornerPadding(Theme::current().windowRadius);
    const double tolerance = 1 / scale + 1e-6;

    scroll->setScrollPosition({0, 0});
    fixture.driver.layout();
    const auto [top, topEnd] = handleEnds(bar, scale);
    EXPECT_NEAR(top, style::headerHeight + ScrollBar::inset, tolerance);
    (void)topEnd;

    scroll->setScrollPosition({0, 1e6});
    fixture.driver.layout();
    const auto [bottomStart, bottom] = handleEnds(bar, scale);
    EXPECT_NEAR(windowHeight - bottom, corner, tolerance);
    (void)bottomStart;

    // Against the window's right edge, not inside the cards' margin.
    EXPECT_EQ(scroll->mapToWindow({scroll->geometry().width, 0}).x, fixture.window.size().width);
    EXPECT_NEAR(handleRight(bar, scale), fixture.window.size().width - ScrollBar::inset, tolerance);
}

TEST_P(WorldClockLayout, TheCityListBarSitsAtTheDialogsEdgeAndCorner)
{
    const double scale = GetParam();
    PageFixture fixture(scale);
    fixture.context.settings().setValue("WorldClock/Cities", List{"Asia/Tokyo"});
    fixture.show(clockapp::makeWorldClockPage(fixture.context));
    auto* page = static_cast<WorldClockPage*>(fixture.page);
    page->showAddCity();
    AddCityDialog* dialog = page->addCityDialog();
    ASSERT_NE(dialog, nullptr);
    Window* window = dialog->window();
    window->setScale(scale);
    TestDriver dialogDriver(*window, [](const std::string& message) { ADD_FAILURE() << message; });
    dialogDriver.layout();

    ListView* list = dialog->list();
    list->setVerticalBarPolicy(ScrollView::BarPolicy::AlwaysOn);
    dialogDriver.layout();
    const SizeF size = window->size();
    // The whole width, down to the bottom edge.
    EXPECT_EQ(list->mapToWindow({0, 0}).x, 0);
    EXPECT_EQ(list->mapToWindow({list->geometry().width, list->geometry().height}).x, size.width);
    EXPECT_EQ(list->mapToWindow({0, list->geometry().height}).y, size.height);

    ScrollBar* bar = verticalBar(list);
    ASSERT_NE(bar, nullptr);
    ASSERT_TRUE(bar->isNeeded());
    TestDriver::waitFor([&] {
        dialogDriver.layout();
        return false;
    }, std::chrono::milliseconds(600));

    const double tolerance = 1 / scale + 1e-6;
    list->setScrollPosition({0, 1e7});
    dialogDriver.layout();
    const auto [start, bottom] = handleEnds(bar, scale);
    (void)start;
    EXPECT_NEAR(size.height - bottom, ScrollBar::cornerPadding(Theme::current().windowRadius), tolerance);
    EXPECT_NEAR(handleRight(bar, scale), size.width - ScrollBar::inset, tolerance);
}

TEST(WorldClockLayoutWidths, ColumnsFollowTheWidth)
{
    EXPECT_EQ(CardGrid::columnsFor(style::windowMinimumWidth), 3);
    EXPECT_EQ(CardGrid::columnsFor(style::windowWidth), 4);
    EXPECT_EQ(CardGrid::columnsFor(1600), 8);
    EXPECT_EQ(CardGrid::columnsFor(300), 1);
}

INSTANTIATE_TEST_SUITE_P(Scales, WorldClockLayout, testing::Values(1.0, 1.75, 2.0));
