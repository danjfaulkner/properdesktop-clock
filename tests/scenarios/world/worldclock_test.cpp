// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pagefixture.h"

#include "base/settings.h"
#include "ui/keys.h"
#include "ui/test_driver.h"
#include "ui/test_input.h"
#include "ui/window.h"
#include "world/addcitydialog.h"
#include "world/cardgrid.h"
#include "world/citycard.h"
#include "world/citydatabase.h"
#include "world/worldclockpage.h"

using namespace properdesktop::ui;
using namespace clockapp::world;

using List = std::vector<std::string>;

static WorldClockPage* showPage(PageFixture& fixture)
{
    fixture.show(clockapp::makeWorldClockPage(fixture.context));
    return static_cast<WorldClockPage*>(fixture.page);
}

static void trigger(WorldClockPage* page, const std::string& id)
{
    for (const clockapp::HeaderAction& action : page->headerActions()) {
        if (action.id == id) {
            action.triggered();
            return;
        }
    }
    ADD_FAILURE() << "no header action " << id;
}

// Every card where it belongs and every fade over.
static bool settle(PageFixture& fixture, WorldClockPage* page)
{
    return TestDriver::waitFor([&] {
        fixture.driver.layout();
        for (CityCard* card : page->grid()->cards()) {
            if (card->removing || !card->presence.isAtRest() || !card->x.isAtRest() || !card->y.isAtRest())
                return false;
        }
        return true;
    });
}

static List savedCities(PageFixture& fixture)
{
    properdesktop::base::Settings settings(fixture.scratch.file("clock.conf"));
    return settings.stringListValue("WorldClock/Cities", {"(none)"});
}

TEST(WorldClockScenario, FirstRunShowsTheLocalCityFirstThenTheDefaults)
{
    PageFixture fixture;
    WorldClockPage* page = showPage(fixture);
    ASSERT_GE(page->grid()->cards().size(), 5u);
    EXPECT_EQ(page->grid()->cards()[0]->zoneId(), CityDatabase::localZoneId());
    EXPECT_NE(page->grid()->card("Asia/Tokyo"), nullptr);
    // Nothing is written until the user changes the list.
    EXPECT_EQ(savedCities(fixture), List{"(none)"});
    EXPECT_TRUE(fixture.driver.shows("world:card:" + CityDatabase::localZoneId()));
    EXPECT_FALSE(fixture.driver.shows("world:empty"));
    EXPECT_EQ(page->grid()->cards()[0]->captionText().rfind("Today, ", 0), 0u);
}

TEST(WorldClockScenario, AddACityFromTheDialog)
{
    PageFixture fixture;
    fixture.context.settings().setValue("WorldClock/Cities", List{"Europe/London"});
    WorldClockPage* page = showPage(fixture);
    trigger(page, "world-add");
    AddCityDialog* dialog = page->addCityDialog();
    ASSERT_NE(dialog, nullptr);
    TestDriver dialogDriver(*dialog->window(), [](const std::string& message) { ADD_FAILURE() << message; });
    dialogDriver.layout();
    // Every city before anything is typed.
    EXPECT_GT(dialog->results().size(), 300u);

    dialogDriver.focus("world:search");
    dialogDriver.typeText("tokyo");
    dialogDriver.layout();
    ASSERT_FALSE(dialog->results().empty());
    EXPECT_EQ(dialog->results().front()->zoneId, "Asia/Tokyo");
    dialogDriver.typeKey(key::Return);

    EXPECT_TRUE(page->cities().contains("Asia/Tokyo"));
    ASSERT_TRUE(settle(fixture, page));
    EXPECT_TRUE(fixture.driver.shows("world:card:Asia/Tokyo"));
    EXPECT_EQ(savedCities(fixture), (List{"Europe/London", "Asia/Tokyo"}));

    // 09:41 UTC is 18:41 in Tokyo.
    CityCard* tokyo = page->grid()->card("Asia/Tokyo");
    ASSERT_NE(tokyo, nullptr);
    EXPECT_NE(tokyo->timeText().find("41"), std::string::npos) << tokyo->timeText();
}

TEST(WorldClockScenario, ACityOnThePageCannotBeAddedTwice)
{
    PageFixture fixture;
    fixture.context.settings().setValue("WorldClock/Cities", List{CityDatabase::localZoneId()});
    WorldClockPage* page = showPage(fixture);
    page->showAddCity();
    AddCityDialog* dialog = page->addCityDialog();
    ASSERT_NE(dialog, nullptr);
    TestDriver dialogDriver(*dialog->window(), [](const std::string& message) { ADD_FAILURE() << message; });
    dialog->setSearchText(CityDatabase::system().find(CityDatabase::localZoneId())->name);
    dialogDriver.layout();
    dialogDriver.focus("world:search");
    dialogDriver.typeKey(key::Return);
    EXPECT_EQ(page->cities().size(), 1u);
}

TEST(WorldClockScenario, RemoveACityInEditMode)
{
    PageFixture fixture;
    fixture.context.settings().setValue("WorldClock/Cities", List{"Asia/Tokyo", "Europe/London", "America/New_York"});
    WorldClockPage* page = showPage(fixture);
    ASSERT_EQ(page->grid()->cards().size(), 3u);

    trigger(page, "world-edit");
    EXPECT_TRUE(page->isEditing());
    ASSERT_TRUE(TestDriver::waitFor([&] {
        fixture.driver.layout();
        return page->grid()->card("Europe/London")->isEditing();
    }));
    ASSERT_TRUE(fixture.driver.click("world:remove:Europe/London"));
    EXPECT_EQ(page->cities().zoneIds(), (List{"Asia/Tokyo", "America/New_York"}));
    ASSERT_TRUE(settle(fixture, page));
    EXPECT_FALSE(fixture.driver.shows("world:card:Europe/London"));
    EXPECT_EQ(page->grid()->cards().size(), 2u);
    EXPECT_EQ(savedCities(fixture), (List{"Asia/Tokyo", "America/New_York"}));

    // New York closed the gap.
    EXPECT_EQ(page->grid()->card("America/New_York")->geometry().x,
              page->grid()->card("Asia/Tokyo")->geometry().right() + clockapp::style::cardGap);
}

TEST(WorldClockScenario, RemoveFromTheContextMenu)
{
    PageFixture fixture;
    fixture.context.settings().setValue("WorldClock/Cities", List{"Asia/Tokyo", "Europe/London"});
    WorldClockPage* page = showPage(fixture);
    ASSERT_TRUE(fixture.driver.click("world:card:Asia/Tokyo", MouseButton::Right));
    ASSERT_TRUE(fixture.driver.choosePopupItem("Remove"));
    EXPECT_EQ(page->cities().zoneIds(), List{"Europe/London"});
}

TEST(WorldClockScenario, RemovingTheLastCityShowsTheEmptyStateAndStaysEmpty)
{
    PageFixture fixture;
    fixture.context.settings().setValue("WorldClock/Cities", List{"Asia/Tokyo"});
    WorldClockPage* page = showPage(fixture);
    page->removeCity("Asia/Tokyo");
    ASSERT_TRUE(settle(fixture, page));
    EXPECT_TRUE(fixture.driver.shows("world:empty"));
    EXPECT_TRUE(fixture.driver.shows("world:empty:add"));
    EXPECT_FALSE(page->isEditing());
    EXPECT_EQ(page->headerActions().size(), 1u);

    // An emptied list is not the first run.
    PageFixture again;
    again.context.settings().setValue("WorldClock/Cities", List{});
    WorldClockPage* reopened = showPage(again);
    EXPECT_TRUE(reopened->cities().zoneIds().empty());
    EXPECT_TRUE(again.driver.shows("world:empty"));
}

TEST(WorldClockScenario, KeyboardMovesFocusAndAltArrowReorders)
{
    PageFixture fixture;
    fixture.context.settings().setValue("WorldClock/Cities", List{"Asia/Tokyo", "Europe/London"});
    WorldClockPage* page = showPage(fixture);
    ASSERT_TRUE(fixture.driver.focus("world:card:Asia/Tokyo"));
    fixture.driver.typeKey(key::Right);
    EXPECT_TRUE(fixture.driver.hasFocus("world:card:Europe/London"));
    fixture.driver.typeKey(key::Left, AltModifier);
    EXPECT_EQ(page->cities().zoneIds(), (List{"Europe/London", "Asia/Tokyo"}));
    fixture.driver.typeKey(key::Delete);
    EXPECT_EQ(page->cities().zoneIds(), List{"Asia/Tokyo"});
}

TEST(WorldClockScenario, TheDayLabelTurnsOverAtMidnight)
{
    PageFixture fixture;
    fixture.context.settings().setValue("WorldClock/Cities", List{"Pacific/Kiritimati"});
    // 09:59:59 UTC is 23:59:59 on Kiritimati (+14).
    fixture.now = std::chrono::system_clock::time_point(std::chrono::seconds(1795132800 + 10 * 3600 - 1));
    WorldClockPage* page = showPage(fixture);
    CityCard* card = page->grid()->card("Pacific/Kiritimati");
    const std::string before = card->captionText();
    fixture.advance(std::chrono::seconds(1));
    EXPECT_NE(card->captionText(), before);
}

TEST(WorldClockScenario, DraggingACardLiftsItIntoTheWindowOverlayAndReorders)
{
    PageFixture fixture;
    fixture.context.settings().setValue("WorldClock/Cities", List{"Asia/Tokyo", "Europe/London"});
    WorldClockPage* page = showPage(fixture);
    CityCard* tokyo = page->grid()->card("Asia/Tokyo");
    CityCard* london = page->grid()->card("Europe/London");
    ASSERT_NE(tokyo, nullptr);
    ASSERT_NE(london, nullptr);

    const RectF fromRect = fixture.driver.frame("world:card:Asia/Tokyo");
    const RectF toRect = fixture.driver.frame("world:card:Europe/London");
    const PointF from{fromRect.x + fromRect.width / 2, fromRect.y + fromRect.height / 2};
    const PointF to{toRect.x + toRect.width / 2, toRect.y + toRect.height / 2};

    TestInput::press(fixture.window, from);
    TestInput::movePointer(fixture.window, {to.x, 10});
    fixture.driver.layout();

    // While dragged, the card lives under the window's overlay so it paints above the header.
    EXPECT_EQ(tokyo->parent()->parent(), fixture.window.overlay());
    EXPECT_LT(tokyo->geometry().y, page->headerHeight());

    TestInput::movePointer(fixture.window, to);
    TestInput::release(fixture.window, to);
    EXPECT_EQ(page->cities().zoneIds(), (List{"Europe/London", "Asia/Tokyo"}));

    ASSERT_TRUE(settle(fixture, page));
    EXPECT_EQ(tokyo->parent(), page->grid());
    EXPECT_TRUE(fixture.window.overlay()->children().empty());
}
