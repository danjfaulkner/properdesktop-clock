// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pagefixture.h"

#include "stopwatch/digitdisplay.h"
#include "stopwatch/roundbutton.h"
#include "store/store.h"
#include "timers/composer.h"
#include "timers/timerspage.h"
#include "timers/wheelpicker.h"

#include "ui/keys.h"
#include "ui/test_input.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>

using namespace clockapp;
using namespace properdesktop::ui;
using namespace std::chrono_literals;

class TimersScenario : public testing::Test
{
protected:
    TimersPage& open()
    {
        if (!store)
            store = std::make_unique<ClockStore>(clockstore::pathIn(fixture.scratch.path()));
        auto page = std::make_unique<TimersPage>(fixture.context, store.get());
        TimersPage* made = page.get();
        fixture.show(std::move(page));
        return *made;
    }

    int64_t nowMs() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(fixture.now.time_since_epoch()).count();
    }

    std::string remaining() { return fixture.driver.find<DigitDisplay>("timers:remaining")->text(); }
    std::string title(const std::string& button) { return fixture.driver.find<RoundButton>(button)->title(); }

    // The composer or the cards, once the cross-fade between them is over.
    bool settlesOn(bool composing)
    {
        return TestDriver::waitFor([&] {
            fixture.driver.layout();
            return fixture.driver.find("timers:composer")->isVisible() == composing &&
                   fixture.driver.find("timers:grid")->isVisible() == !composing;
        });
    }

    PageFixture fixture;
    std::unique_ptr<ClockStore> store;
};

TEST_F(TimersScenario, StartPauseResumeCancel)
{
    TimersPage& page = open();
    EXPECT_TRUE(page.isComposing());
    EXPECT_FALSE(fixture.driver.shows("timers:recents"));
    // The wheels start at five minutes.
    EXPECT_EQ(page.composer()->picker()->durationMs(), 5 * 60 * 1000);

    fixture.driver.click("timers:seconds");
    fixture.driver.typeText("30");
    fixture.driver.click("timers:label");
    fixture.driver.typeText("Tea");
    fixture.driver.click("timers:start");

    ASSERT_EQ(store->timers().size(), 1u);
    const RunningTimer started = store->timers().front();
    EXPECT_EQ(started.durationMs, 330000);
    EXPECT_EQ(started.label, "Tea");
    EXPECT_EQ(started.endsAtMs, nowMs() + 330000);
    EXPECT_TRUE(settlesOn(false));
    EXPECT_EQ(remaining(), "5:30");
    EXPECT_EQ(fixture.driver.text("timers:title"), "Tea");

    fixture.advance(30s);
    EXPECT_EQ(remaining(), "5:00");

    fixture.driver.click("timers:pause");
    ASSERT_EQ(store->timers().size(), 1u);
    EXPECT_FALSE(store->timers().front().isRunning());
    EXPECT_EQ(store->timers().front().remainingMs, 300000);
    EXPECT_EQ(title("timers:pause"), "Resume");
    EXPECT_EQ(fixture.driver.text("timers:ends"), "Paused");
    fixture.advance(60s);
    EXPECT_EQ(remaining(), "5:00");

    fixture.driver.click("timers:pause");
    EXPECT_TRUE(store->timers().front().isRunning());
    EXPECT_EQ(store->timers().front().endsAtMs, nowMs() + 300000);
    EXPECT_EQ(title("timers:pause"), "Pause");

    fixture.driver.click("timers:cancel");
    EXPECT_TRUE(store->timers().empty());
    EXPECT_TRUE(settlesOn(true));

    // The timer is a recent one now, started again with a click.
    ASSERT_EQ(page.recents().size(), 1u);
    EXPECT_TRUE(TestDriver::waitFor([&] {
        fixture.driver.layout();
        return fixture.driver.shows("timers:recents");
    }));
    fixture.driver.click("timers:recent-start");
    ASSERT_EQ(store->timers().size(), 1u);
    EXPECT_EQ(store->timers().front().durationMs, 330000);
    EXPECT_EQ(store->timers().front().label, "Tea");
    EXPECT_TRUE(settlesOn(false));
}

TEST_F(TimersScenario, FinishedTimerShowsItAndRepeats)
{
    RunningTimer tea;
    tea.durationMs = 60000;
    tea.endsAtMs = nowMs() + 60000;
    tea.label = "Eggs";
    store = std::make_unique<ClockStore>(clockstore::pathIn(fixture.scratch.path()));
    const std::string id = store->setTimer(tea);

    TimersPage& page = open();
    EXPECT_FALSE(page.isComposing());
    EXPECT_FALSE(fixture.driver.shows("timers:done"));

    fixture.advance(61s);
    EXPECT_TRUE(fixture.driver.shows("timers:done"));
    EXPECT_EQ(title("timers:pause"), "Repeat");
    EXPECT_EQ(title("timers:cancel"), "Dismiss");

    fixture.driver.click("timers:pause");
    EXPECT_EQ(store->timer(id)->endsAtMs, nowMs() + 60000);
    EXPECT_FALSE(fixture.driver.shows("timers:done"));
    EXPECT_EQ(remaining(), "1:00");

    fixture.advance(60s);
    fixture.driver.click("timers:cancel");
    EXPECT_TRUE(store->timers().empty());
}

TEST_F(TimersScenario, SeveralTimersAtOnce)
{
    TimersPage& page = open();
    fixture.driver.click("timers:start");
    EXPECT_TRUE(settlesOn(false));
    page.newItem();
    EXPECT_TRUE(settlesOn(true));
    fixture.driver.click("timers:minutes");
    fixture.driver.typeKey(key::Up);
    fixture.driver.click("timers:start");
    EXPECT_TRUE(settlesOn(false));

    ASSERT_EQ(store->timers().size(), 2u);
    EXPECT_EQ(store->timers()[1].durationMs, 4 * 60 * 1000);
    EXPECT_EQ(fixture.driver.countShown("timers:card"), 2u);

    // New Timer, then Escape, goes back to the cards.
    page.newItem();
    EXPECT_TRUE(settlesOn(true));
    fixture.driver.typeKey(key::Escape);
    EXPECT_TRUE(settlesOn(false));
}

TEST_F(TimersScenario, PickerTurnsUnderThePointerAnywhere)
{
    TimersPage& page = open();
    DurationPicker* picker = page.composer()->picker();
    // A wheel click over the minutes' unit turns the minutes.
    ScrollEvent down;
    down.deltaY = 15;
    down.value120Y = 120;
    fixture.driver.scroll("timers:minutes-unit", down);
    EXPECT_EQ(picker->durationMs(), 6 * 60 * 1000);

    // A drag that starts on the seconds' unit turns the seconds.
    const RectF unit = fixture.driver.frame("timers:seconds-unit");
    const PointF from{unit.x + unit.width / 2, unit.y + unit.height / 2};
    TestInput::press(fixture.window, from);
    for (int step = 1; step <= 4; ++step)
        TestInput::movePointer(fixture.window, {from.x, from.y - step * Wheel::RowHeight / 2});
    TestInput::release(fixture.window, {from.x, from.y - 2 * Wheel::RowHeight});
    EXPECT_TRUE(TestDriver::waitFor([&] {
        fixture.driver.layout();
        return picker->value(2) == 2;
    }));

    // A click in the gap between two columns turns the nearer one: a row
    // under the middle line of the hours.
    const RectF hours = fixture.driver.frame("timers:hours");
    const RectF frame = fixture.driver.frame("timers:picker");
    const double gapX = hours.right() + 2;
    ASSERT_EQ(picker->columnAt(gapX - frame.x), 0u);
    TestInput::click(fixture.window, {gapX, hours.y + hours.height / 2 + Wheel::Radius * std::sin(Wheel::RowAngle)});
    EXPECT_TRUE(TestDriver::waitFor([&] {
        fixture.driver.layout();
        return picker->value(0) == 1;
    }));
}

TEST_F(TimersScenario, PickerKeyHookComesFirst)
{
    TimersPage& page = open();
    DurationPicker* picker = page.composer()->picker();
    std::vector<uint32_t> taken;
    picker->onKey = [&taken](const KeyEvent& event) {
        if (event.text != "7")
            return false;
        taken.push_back(event.key);
        return true;
    };
    fixture.driver.click("timers:minutes");
    fixture.driver.typeText("7");
    EXPECT_EQ(taken.size(), 1u);
    EXPECT_EQ(picker->value(1), 5);
    fixture.driver.typeText("9");
    EXPECT_EQ(picker->value(1), 9);
}
