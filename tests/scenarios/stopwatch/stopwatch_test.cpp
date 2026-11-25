// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pagefixture.h"

#include "stopwatch/digitdisplay.h"
#include "stopwatch/lapslist.h"
#include "stopwatch/roundbutton.h"
#include "stopwatch/stopwatchpage.h"

#include <base/settings.h>

#include "ui/keys.h"

#include <gtest/gtest.h>

using namespace clockapp;
using namespace properdesktop::ui;

static constexpr int64_t Millisecond = 1000000;

// A clock the test moves on by hand.
class SteppedTime : public TimeSource
{
public:
    Reading now() const override { return m_now; }
    void advance(int64_t ms)
    {
        m_now.bootNs += ms * Millisecond;
        m_now.wallMs += ms;
    }
    void reboot()
    {
        m_now.bootNs = 5000 * Millisecond;
        m_now.bootId = "second-boot";
    }

private:
    Reading m_now{1000000 * Millisecond, 1795167660000, "first-boot"};
};

class StopwatchScenario : public testing::Test
{
protected:
    StopwatchPage& open()
    {
        auto page = std::make_unique<StopwatchPage>(fixture.context, &time);
        StopwatchPage* made = page.get();
        fixture.show(std::move(page));
        return *made;
    }

    std::string digits() { return fixture.driver.find<DigitDisplay>("stopwatch:digits")->text(); }
    std::string title(const std::string& button) { return fixture.driver.find<RoundButton>(button)->title(); }
    // The laps as the list shows them, the newest first.
    std::vector<std::string> laps() { return fixture.driver.texts("stopwatch:laps"); }

    void step(int64_t ms)
    {
        time.advance(ms);
        fixture.driver.layout();
    }

    SteppedTime time;
    PageFixture fixture;
};

TEST_F(StopwatchScenario, StartLapStopReset)
{
    StopwatchPage& page = open();
    EXPECT_EQ(digits(), "00:00.00");
    EXPECT_EQ(title("stopwatch:start"), "Start");
    EXPECT_EQ(title("stopwatch:lap"), "Lap");
    EXPECT_FALSE(fixture.driver.find("stopwatch:lap")->isEnabled());

    fixture.driver.click("stopwatch:start");
    EXPECT_EQ(title("stopwatch:start"), "Stop");
    // The frames read the clock: no press needed for the figures to move.
    step(3250);
    EXPECT_EQ(digits(), "00:03.25");

    fixture.driver.click("stopwatch:lap");
    step(1500);
    fixture.driver.click("stopwatch:lap");
    step(2000);
    fixture.driver.click("stopwatch:lap");
    step(250);
    EXPECT_EQ(page.stopwatch().lapCount(), 3u);
    EXPECT_EQ(laps(), (std::vector<std::string>{"Lap 4", "00:00.25", "Lap 3", "00:02.00", "Lap 2", "00:01.50",
                                                "Lap 1", "00:03.25"}));

    fixture.driver.click("stopwatch:start");
    EXPECT_EQ(title("stopwatch:start"), "Start");
    EXPECT_EQ(title("stopwatch:lap"), "Reset");
    step(5000);
    EXPECT_EQ(digits(), "00:07.00");

    fixture.driver.click("stopwatch:lap");
    EXPECT_EQ(digits(), "00:00.00");
    EXPECT_EQ(title("stopwatch:lap"), "Lap");
    EXPECT_TRUE(laps().empty());
}

TEST_F(StopwatchScenario, KeysWorkTheButtons)
{
    StopwatchPage& page = open();
    fixture.driver.typeKey(key::Space);
    EXPECT_TRUE(page.stopwatch().isRunning());
    step(1000);
    fixture.driver.typeKey(key::letter('l'));
    EXPECT_EQ(page.stopwatch().lapCount(), 1u);
    // Reset waits for the stopwatch to stop.
    fixture.driver.typeKey(key::letter('r'));
    EXPECT_TRUE(page.stopwatch().hasTime());
    fixture.driver.typeKey(key::Space);
    fixture.driver.typeKey(key::letter('r'));
    EXPECT_FALSE(page.stopwatch().hasTime());
}

TEST_F(StopwatchScenario, RunsOnAfterThePageGoes)
{
    open();
    fixture.driver.click("stopwatch:start");
    step(2000);
    fixture.driver.click("stopwatch:lap");
    fixture.window.setContentView(nullptr);

    // The program quits, the machine reboots, and 30 s of wall time pass.
    time.advance(30000);
    time.reboot();
    open();
    step(0);
    EXPECT_EQ(digits(), "00:32.00");
    EXPECT_EQ(title("stopwatch:start"), "Stop");
    EXPECT_EQ(laps(), (std::vector<std::string>{"Lap 2", "00:30.00", "Lap 1", "00:02.00"}));
}
