// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "alarms/alarmspage.h"
#include "alarms/timepicker.h"
#include "pagefixture.h"
#include "store/store.h"

#include <base/event_loop.h>

#include "ui/application.h"
#include "ui/switch.h"

#include <gtest/gtest.h>

#include <functional>

using namespace clockapp;
using properdesktop::ui::TestDriver;

static void runPosted()
{
    properdesktop::base::EventLoop& loop = properdesktop::ui::Application::instance()->loop();
    loop.post([&loop] { loop.quit(); });
    loop.run();
}

static std::unique_ptr<TestDriver> dialogOver(TestDriver& driver)
{
    runPosted();
    properdesktop::ui::Window* window = driver.dialog();
    if (!window) {
        ADD_FAILURE() << "no dialog over the window";
        return nullptr;
    }
    auto dialog = std::make_unique<TestDriver>(*window, [](const std::string& message) { ADD_FAILURE() << message; });
    dialog->layout();
    return dialog;
}

// Runs frames, laying out between them, until the condition holds.
static bool settles(TestDriver& driver, const std::function<bool()>& condition)
{
    return TestDriver::waitFor([&] {
        driver.layout();
        return condition();
    });
}

static bool closes(TestDriver& driver)
{
    return TestDriver::waitFor([&driver] { return !driver.dialog(); });
}

TEST(AlarmsScenario, CreateToggleEditDelete)
{
    PageFixture fixture;
    fixture.show(makeAlarmsPage(fixture.context));
    ClockStore& store = ClockStore::inFolder(fixture.context.paths().data);
    TestDriver& driver = fixture.driver;
    EXPECT_TRUE(driver.shows("alarms:empty"));

    // Create: the empty page's button, the time typed on the wheels.
    ASSERT_TRUE(driver.click("alarms:emptyAdd"));
    std::unique_ptr<TestDriver> editor = dialogOver(driver);
    ASSERT_TRUE(editor);
    // A whole time typed on a drum; A picks the morning on a 12-hour clock.
    ASSERT_TRUE(editor->focus("picker:hours"));
    editor->typeText("0730a");
    EXPECT_FALSE(editor->text("editor:countdown").empty());
    ASSERT_TRUE(editor->click("editor:day1"));
    ASSERT_TRUE(editor->click("editor:save"));
    ASSERT_TRUE(closes(driver));
    ASSERT_EQ(store.alarms().size(), 1u);
    const Alarm created = store.alarms().front();
    EXPECT_EQ(created.hour, 7);
    EXPECT_EQ(created.minute, 30);
    EXPECT_EQ(created.repeat, 1 << 1);
    EXPECT_TRUE(created.enabled);
    EXPECT_EQ(created.armedAt, 1795167660);
    driver.layout();
    const std::string row = "alarm:" + created.id;
    ASSERT_TRUE(settles(driver, [&] { return driver.shows(row); }));
    EXPECT_TRUE(settles(driver, [&] { return !driver.shows("alarms:empty"); }));

    // Toggle off and on again.
    ASSERT_TRUE(driver.click("alarm:switch"));
    EXPECT_FALSE(store.alarms().front().enabled);
    EXPECT_FALSE(driver.find<properdesktop::ui::Switch>("alarm:switch")->isChecked());
    fixture.advance(std::chrono::minutes(5));
    ASSERT_TRUE(driver.click("alarm:switch"));
    EXPECT_TRUE(store.alarms().front().enabled);
    EXPECT_EQ(store.alarms().front().armedAt, 1795167660 + 300);
    EXPECT_TRUE(settles(driver, [&] { return driver.shows("alarm:hint"); }));

    // Edit: a label.
    ASSERT_TRUE(driver.click(row));
    editor = dialogOver(driver);
    ASSERT_TRUE(editor);
    ASSERT_TRUE(editor->focus("editor:label"));
    editor->typeText("Gym");
    ASSERT_TRUE(editor->click("editor:save"));
    ASSERT_TRUE(closes(driver));
    ASSERT_EQ(store.alarms().size(), 1u);
    EXPECT_EQ(store.alarms().front().label, "Gym");
    EXPECT_EQ(store.alarms().front().id, created.id);
    driver.layout();
    EXPECT_NE(driver.text("alarm:detail").find("Gym"), std::string::npos) << driver.text("alarm:detail");

    // Delete.
    ASSERT_TRUE(driver.click(row));
    editor = dialogOver(driver);
    ASSERT_TRUE(editor);
    ASSERT_TRUE(editor->click("editor:delete"));
    ASSERT_TRUE(closes(driver));
    EXPECT_TRUE(store.alarms().empty());
    driver.layout();
    EXPECT_FALSE(driver.shows(row));
    EXPECT_TRUE(settles(driver, [&] { return driver.shows("alarms:empty"); }));
}

TEST(AlarmsScenario, CancelClosesEditorWithoutSaving)
{
    PageFixture fixture;
    fixture.show(makeAlarmsPage(fixture.context));
    ClockStore& store = ClockStore::inFolder(fixture.context.paths().data);
    TestDriver& driver = fixture.driver;

    ASSERT_TRUE(driver.click("alarms:emptyAdd"));
    std::unique_ptr<TestDriver> editor = dialogOver(driver);
    ASSERT_TRUE(editor);
    EXPECT_FALSE(editor->showsText("New Alarm"));
    const properdesktop::ui::RectF cancel = editor->frame("editor:cancel");
    const properdesktop::ui::RectF save = editor->frame("editor:save");
    EXPECT_GT(cancel.width, 0);
    EXPECT_LT(cancel.right(), save.x);
    ASSERT_TRUE(editor->click("editor:cancel"));
    ASSERT_TRUE(closes(driver));
    EXPECT_TRUE(store.alarms().empty());
}

TEST(AlarmsScenario, OneTimeAlarmShowsOffOnceRung)
{
    PageFixture fixture;
    ClockStore& store = ClockStore::inFolder(fixture.context.paths().data);
    const time_t now = time_t(1795167660);
    std::tm local{};
    localtime_r(&now, &local);
    Alarm alarm;
    // Two minutes from the fixture's moment.
    alarm.hour = (local.tm_hour + (local.tm_min + 2) / 60) % 24;
    alarm.minute = (local.tm_min + 2) % 60;
    alarm.armedAt = now;
    const std::string id = store.setAlarm(alarm);
    fixture.show(makeAlarmsPage(fixture.context));
    TestDriver& driver = fixture.driver;
    ASSERT_TRUE(driver.shows("alarm:" + id));
    EXPECT_TRUE(driver.find<properdesktop::ui::Switch>("alarm:switch")->isChecked());
    fixture.advance(std::chrono::minutes(3));
    EXPECT_FALSE(driver.find<properdesktop::ui::Switch>("alarm:switch")->isChecked());
    // It stays in the file as it was; only its time has passed.
    EXPECT_TRUE(store.alarms().front().enabled);
}

TEST(TimePickerScenario, TwelveHourHalvesAndLetters)
{
    PageFixture fixture;
    auto made = std::make_unique<TimePicker>(&fixture.window, true);
    TimePicker* picker = made.get();
    fixture.window.setContentView(std::move(made));
    fixture.window.show();
    fixture.driver.layout();
    picker->setTime(19, 5, false);
    EXPECT_EQ(picker->hour(), 19);
    EXPECT_EQ(picker->minute(), 5);
    picker->setTime(0, 0, false);
    EXPECT_EQ(picker->hour(), 0);

    ASSERT_TRUE(fixture.driver.focus("picker:hours"));
    fixture.driver.typeText("9");
    fixture.driver.typeText("p");
    EXPECT_EQ(picker->hour(), 21);
    fixture.driver.typeText("a");
    EXPECT_EQ(picker->hour(), 9);
}

TEST(TimePickerScenario, TwentyFourHours)
{
    PageFixture fixture;
    auto made = std::make_unique<TimePicker>(&fixture.window, false);
    TimePicker* picker = made.get();
    fixture.window.setContentView(std::move(made));
    fixture.window.show();
    fixture.driver.layout();
    // From the minutes drum too, the digits are the whole time.
    ASSERT_TRUE(fixture.driver.focus("picker:minutes"));
    fixture.driver.typeText("1945");
    EXPECT_EQ(picker->hour(), 19);
    EXPECT_EQ(picker->minute(), 45);
}

TEST(TimePickerScenario, ThreeDigitsReadAsHourAndMinutes)
{
    PageFixture fixture;
    auto made = std::make_unique<TimePicker>(&fixture.window, false);
    TimePicker* picker = made.get();
    fixture.window.setContentView(std::move(made));
    fixture.window.show();
    fixture.driver.layout();
    ASSERT_TRUE(fixture.driver.focus("picker:hours"));
    fixture.driver.typeText("730");
    EXPECT_EQ(picker->hour(), 7);
    EXPECT_EQ(picker->minute(), 30);
}
