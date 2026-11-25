// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "windowfixture.h"

#include "page.h"
#include "stopwatch/roundbutton.h"
#include "store/store.h"
#include "timers/timermath.h"
#include "timers/wheel.h"

#include "base/i18n.h"
#include "base/settings.h"
#include "ui/application.h"
#include "ui/combo_box.h"
#include "ui/dialog_window.h"
#include "ui/label.h"
#include "ui/row_combo_box.h"

#include <libintl.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace clockapp;
using namespace properdesktop;
using namespace properdesktop::ui;
using namespace std::chrono_literals;

// Every catalog in po/, and English for the source text.
static const char* const kLanguages[] = {"en", "cs", "de", "es", "fr", "id", "it", "ja", "ko",
                                         "ms", "pl", "pt_BR", "ru", "vi", "zh_CN", "zh_HK", "zh_TW"};

// Zones a day apart and at a quarter and a half hour, so the cards show
// every kind of caption from anywhere.
static const std::vector<std::string> kCities = {"Pacific/Kiritimati", "Pacific/Pago_Pago", "Asia/Kathmandu",
                                                 "America/St_Johns", "Pacific/Chatham"};

// gettext wants <dir>/<language>/LC_MESSAGES/<domain>.mo; the build writes
// <dir>/<language>/<domain>.mo.
class BuiltCatalogs
{
public:
    BuiltCatalogs()
    {
        for (const char* language : kLanguages) {
            const std::filesystem::path built = std::filesystem::path(CLOCK_CATALOGS) / language / "properdesktop-clock.mo";
            if (!std::filesystem::exists(built))
                continue;
            const std::filesystem::path folder = std::filesystem::path(m_dir.path()) / language / "LC_MESSAGES";
            std::filesystem::create_directories(folder);
            std::filesystem::create_symlink(built, folder / "properdesktop-clock.mo");
        }
        i18n::setDomain("properdesktop-clock");
        bindtextdomain("properdesktop-clock", m_dir.path().c_str());
    }

private:
    ScratchDir m_dir;
};

static std::string describe(const RectF& rect)
{
    char text[96];
    std::snprintf(text, sizeof text, "%.1f,%.1f %.1fx%.1f", rect.x, rect.y, rect.width, rect.height);
    return text;
}

// Texts wider or taller than their place or cut short, a round button's
// title past the ring inside its edge at its smallest size, and controls
// narrower than they ask.
static void checkFit(const std::string& state, const View* view, std::vector<std::string>& problems)
{
    if (!view->isVisible())
        return;
    const RectF slot = view->geometry();
    if (const auto* label = dynamic_cast<const Label*>(view); label && !label->text().empty()) {
        const SizeF natural = label->sizeHint();
        const double height = label->heightForWidth(slot.width);
        // One line of another script can be a little taller than the slot
        // without wrapping; only wrapping past it is a problem.
        const bool oneLine = height <= natural.height + 0.5;
        double width = slot.width;
        if (const auto* button = dynamic_cast<const RoundButton*>(view->parent())) {
            // The title is measured at the size it shrank to.
            width = std::min(width, button->titleWidth());
        }
        if ((!oneLine && height > slot.height + 0.5) || (oneLine && natural.width > width + 0.5) || label->isElided())
            problems.push_back(state + ": \"" + label->text() + "\" needs " + std::to_string(int(std::ceil(natural.width)))
                               + "x" + std::to_string(int(std::ceil(height))) + " in " + std::to_string(int(width))
                               + " of " + describe(slot));
        return;
    }
    if (const auto* picker = dynamic_cast<const WheelPicker*>(view)) {
        // A unit stays clear of the next column's values and inside the picker.
        const auto& children = picker->children();
        for (size_t i = 0; i < children.size(); ++i) {
            const auto* unit = dynamic_cast<const Label*>(children[i].get());
            if (!unit || !unit->isVisible())
                continue;
            const double end = i + 1 < children.size() ? children[i + 1]->geometry().x : slot.width;
            if (unit->geometry().right() > end + 0.5)
                problems.push_back(state + ": unit \"" + unit->text() + "\" reaches " + describe(unit->geometry())
                                   + " past " + std::to_string(int(end)));
        }
    }
    const bool control = dynamic_cast<const DialogButton*>(view) || dynamic_cast<const ComboBox*>(view)
            || dynamic_cast<const RowComboBox*>(view);
    if (control && view->sizeHint().width > slot.width + 0.5)
        problems.push_back(state + ": control " + view->name() + " needs "
                           + std::to_string(int(std::ceil(view->sizeHint().width))) + " in " + describe(slot));
    for (const auto& child : view->children())
        checkFit(state, child.get(), problems);
}

class FitTest : public testing::TestWithParam<const char*>
{
protected:
    void SetUp() override
    {
        static BuiltCatalogs catalogs;
        Application::instance()->setLanguageEnvironment({{"LANG", "en_US.UTF-8"}, {"LANGUAGE", GetParam()}});
        if (std::string(GetParam()) != "en")
            ASSERT_STRNE(i18n::tr("World Clock"), "World Clock") << "no catalog in use; is en_US.UTF-8 missing?";
    }

    void TearDown() override
    {
        EXPECT_TRUE(problems.empty()) << [this] {
            std::string all;
            for (const std::string& problem : problems)
                all += problem + "\n";
            return all;
        }();
    }

    void check(const std::string& state, Window& window)
    {
        TestDriver(window, [](const std::string& message) { ADD_FAILURE() << message; }).layout();
        checkFit(state, window.contentView(), problems);
    }

    void check(const std::string& state, WindowFixture& fixture)
    {
        fixture.settle();
        check(state, fixture.window->window());
    }

    // Checks the dialog a click opened, and closes it.
    void checkDialog(const std::string& state, WindowFixture& fixture)
    {
        Window* dialog = nullptr;
        ASSERT_TRUE(TestDriver::waitFor([&] { return (dialog = fixture.driver.dialog()) != nullptr; })) << state;
        check(state, *dialog);
        ASSERT_TRUE(dialog->onCloseRequested);
        dialog->onCloseRequested();
        EXPECT_TRUE(TestDriver::waitFor([&] { return !fixture.driver.dialog(); })) << state;
    }

    void showTab(WindowFixture& fixture, Tab tab)
    {
        fixture.window->setTab(tab, false);
        fixture.settle();
    }

    std::vector<std::string> problems;
};

TEST_P(FitTest, EveryPageWithContent)
{
    ScratchDir dir;
    const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    const int64_t nowMs = now.time_since_epoch().count();
    {
        base::Settings settings(dir.file("clock.conf"));
        settings.setValue("WorldClock/Cities", kCities);
    }
    ClockStore& store = ClockStore::inFolder(dir.file("data"));
    Alarm alarm;
    alarm.hour = 6;
    alarm.minute = 45;
    alarm.repeat = 0b0111110;
    alarm.label = "Wake";
    const std::string alarmId = store.setAlarm(alarm);
    RunningTimer running;
    running.label = "Tea";
    running.durationMs = 330000;
    running.endsAtMs = nowMs + 300000;
    store.setTimer(running);
    RunningTimer paused;
    paused.label = "Eggs";
    paused.durationMs = 600000;
    paused.remainingMs = 420000;
    store.setTimer(paused);
    RunningTimer finished;
    finished.label = "Bread";
    finished.durationMs = 60000;
    finished.endsAtMs = nowMs - 5000;
    store.setTimer(finished);

    WindowFixture fixture(1, {style::windowWidth, style::windowHeight}, &dir);
    auto time = now;
    fixture.window->context().setClock([&time] { return time; });

    showTab(fixture, Tab::WorldClock);
    // Every relative day, wherever the test runs.
    for (int step = 0; step < 8; ++step) {
        time = now + std::chrono::hours(3 * step);
        fixture.window->context().emitTick();
        check("world at +" + std::to_string(3 * step) + "h", fixture);
    }
    time = now;
    fixture.window->context().emitTick();
    ASSERT_TRUE(fixture.driver.click("header:world-edit"));
    check("world editing", fixture);
    ASSERT_TRUE(fixture.driver.click("header:world-edit"));
    fixture.settle();
    ASSERT_TRUE(fixture.driver.click("header:world-add"));
    checkDialog("add city", fixture);

    showTab(fixture, Tab::Alarms);
    check("alarms", fixture);
    ASSERT_TRUE(fixture.driver.click("alarm:" + alarmId));
    checkDialog("alarm editor", fixture);

    showTab(fixture, Tab::Stopwatch);
    check("stopwatch idle", fixture);
    ASSERT_TRUE(fixture.driver.click("stopwatch:start"));
    ASSERT_TRUE(fixture.driver.click("stopwatch:lap"));
    ASSERT_TRUE(fixture.driver.click("stopwatch:lap"));
    ASSERT_TRUE(fixture.driver.click("stopwatch:lap"));
    check("stopwatch running", fixture);
    ASSERT_TRUE(fixture.driver.click("stopwatch:start"));
    check("stopwatch stopped", fixture);
    ASSERT_TRUE(fixture.driver.click("stopwatch:lap"));

    showTab(fixture, Tab::Timers);
    check("timers", fixture);
    fixture.window->page(Tab::Timers)->newItem();
    EXPECT_TRUE(TestDriver::waitFor([&] {
        fixture.driver.layout();
        return fixture.driver.shows("timers:composer") && !fixture.driver.shows("timers:grid");
    }));
    check("timer composer", fixture);
}

TEST_P(FitTest, EveryPageEmpty)
{
    ScratchDir dir;
    {
        base::Settings settings(dir.file("clock.conf"));
        timermath::saveRecents(settings, {{330000, "Tea", {}}, {5400000, {}, {}}});
    }
    WindowFixture fixture(1, {style::windowWidth, style::windowHeight}, &dir);
    for (const Tab tab : {Tab::WorldClock, Tab::Alarms, Tab::Stopwatch, Tab::Timers}) {
        showTab(fixture, tab);
        check("empty tab " + std::to_string(int(tab)), fixture);
    }
    showTab(fixture, Tab::Alarms);
    ASSERT_TRUE(fixture.driver.click("alarms:emptyAdd"));
    checkDialog("new alarm", fixture);
}

INSTANTIATE_TEST_SUITE_P(Languages, FitTest, testing::ValuesIn(kLanguages),
                         [](const testing::TestParamInfo<const char*>& info) { return std::string(info.param); });
