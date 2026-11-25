// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "windowfixture.h"

#include "base/settings.h"
#include "store/store.h"
#include "ui/event.h"

#include <chrono>

using clockapp::Tab;
using namespace properdesktop::ui;

static const char* const kPages[] = {"page:world", "page:alarms", "page:stopwatch", "page:timers"};

// Exactly the page of the tab shows, at rest.
static void expectShowing(WindowFixture& fixture, Tab tab)
{
    EXPECT_EQ(fixture.window->tab(), tab);
    for (int i = 0; i < clockapp::TabCount; ++i)
        EXPECT_EQ(fixture.driver.shows(kPages[i]), i == int(tab)) << kPages[i];
    clockapp::Page* page = fixture.window->page(tab);
    EXPECT_EQ(page->opacity(), 1);
    EXPECT_TRUE(page->transform().isIdentity());
    EXPECT_FALSE(page->isLayered());
    EXPECT_EQ(fixture.window->headerBar()->tabSwitch()->currentIndex(), int(tab));
    EXPECT_FALSE(fixture.window->headerBar()->tabSwitch()->isGliding());
}

TEST(WindowTest, OpensOnWorldClock)
{
    WindowFixture fixture;
    expectShowing(fixture, Tab::WorldClock);
}

TEST(WindowTest, ClickingATabSwitchesWithATransition)
{
    WindowFixture fixture;
    ASSERT_TRUE(fixture.driver.chooseSegment("header:tabs", size_t(Tab::Alarms)));
    EXPECT_EQ(fixture.window->tab(), Tab::Alarms);
    EXPECT_TRUE(fixture.window->isSwitching());
    // Both pages show while one gives way to the other.
    EXPECT_TRUE(fixture.driver.shows("page:world"));
    EXPECT_TRUE(fixture.driver.shows("page:alarms"));
    ASSERT_TRUE(fixture.settle());
    expectShowing(fixture, Tab::Alarms);
}

TEST(WindowTest, ClickingTheCurrentTabDoesNothing)
{
    WindowFixture fixture;
    ASSERT_TRUE(fixture.driver.chooseSegment("header:tabs", size_t(Tab::WorldClock)));
    EXPECT_FALSE(fixture.window->isSwitching());
    EXPECT_FALSE(fixture.window->headerBar()->tabSwitch()->isGliding());
    expectShowing(fixture, Tab::WorldClock);
}

TEST(WindowTest, ASwitchDuringASwitchEndsOnTheLastTab)
{
    WindowFixture fixture;
    ASSERT_TRUE(fixture.driver.chooseSegment("header:tabs", size_t(Tab::Alarms)));
    fixture.driver.layout();
    ASSERT_TRUE(fixture.driver.chooseSegment("header:tabs", size_t(Tab::Timers)));
    fixture.driver.layout();
    ASSERT_TRUE(fixture.driver.chooseSegment("header:tabs", size_t(Tab::WorldClock)));
    ASSERT_TRUE(fixture.settle());
    expectShowing(fixture, Tab::WorldClock);
}

TEST(WindowTest, CommandDigitsPickTheTabs)
{
    WindowFixture fixture;
    const Tab tabs[] = {Tab::Stopwatch, Tab::Timers, Tab::Alarms, Tab::WorldClock};
    for (Tab tab : tabs) {
        fixture.driver.typeKey(uint32_t('1' + int(tab)), ControlModifier);
        ASSERT_TRUE(fixture.settle());
        expectShowing(fixture, tab);
    }
}

TEST(WindowTest, TheViewMenuPicksTheTabs)
{
    WindowFixture fixture;
    ASSERT_TRUE(fixture.driver.chooseMenuItem("View", "Stopwatch"));
    ASSERT_TRUE(fixture.settle());
    expectShowing(fixture, Tab::Stopwatch);
    ASSERT_TRUE(fixture.driver.chooseMenuItem("View", "World Clock"));
    ASSERT_TRUE(fixture.settle());
    expectShowing(fixture, Tab::WorldClock);
}

TEST(WindowTest, TheTabIsKeptForTheNextStart)
{
    ScratchDir dir;
    {
        WindowFixture fixture(1, {clockapp::style::windowWidth, clockapp::style::windowHeight}, &dir);
        ASSERT_TRUE(fixture.driver.chooseSegment("header:tabs", size_t(Tab::Timers)));
        ASSERT_TRUE(fixture.settle());
    }
    {
        const properdesktop::base::Settings settings(dir.file("clock.conf"));
        EXPECT_EQ(settings.stringValue("View/Tab"), "timers");
    }
    WindowFixture fixture(1, {clockapp::style::windowWidth, clockapp::style::windowHeight}, &dir);
    EXPECT_FALSE(fixture.window->isSwitching());
    expectShowing(fixture, Tab::Timers);
}

TEST(WindowTest, AnUnknownSavedTabOpensWorldClock)
{
    ScratchDir dir;
    {
        properdesktop::base::Settings settings(dir.file("clock.conf"));
        settings.setValue("View/Tab", "calendar");
    }
    WindowFixture fixture(1, {clockapp::style::windowWidth, clockapp::style::windowHeight}, &dir);
    expectShowing(fixture, Tab::WorldClock);
}

TEST(WindowTest, TheFileMenuClosesTheWindow)
{
    WindowFixture fixture;
    EXPECT_TRUE(fixture.driver.chooseMenuItem("File", "Close Window"));
}

TEST(WindowTest, RepeatTimerStartsAFinishedTimerAgain)
{
    WindowFixture fixture;
    const auto now = std::chrono::system_clock::time_point(std::chrono::seconds(1795167660));
    fixture.window->context().setClock([now] { return now; });
    const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    clockapp::ClockStore& store = clockapp::ClockStore::inFolder(fixture.window->context().paths().data);
    clockapp::RunningTimer finished;
    finished.label = "Tea";
    finished.durationMs = 180000;
    finished.endsAtMs = nowMs - 5000;
    const std::string id = store.setTimer(finished);

    fixture.window->repeatTimer(id);
    const clockapp::RunningTimer* repeated = store.timer(id);
    ASSERT_NE(repeated, nullptr);
    EXPECT_EQ(repeated->endsAtMs, nowMs + 180000);
    EXPECT_EQ(repeated->label, "Tea");

    fixture.window->repeatTimer("no-such-timer");
    EXPECT_EQ(store.timers().size(), 1u);
}
