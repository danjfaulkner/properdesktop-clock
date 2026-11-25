// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "windowfixture.h"

#include "ui/header_button_group.h"
#include "ui/image.h"
#include "ui/segmented_control.h"
#include "ui/test_input.h"
#include "ui/window_controls.h"
#include "ui/window_header.h"

#include <algorithm>
#include <cmath>

using namespace properdesktop::ui;
using clockapp::HeaderAction;
using clockapp::HeaderBar;

static RectF windowRect(const View* view)
{
    const PointF topLeft = view->mapToWindow({});
    return {topLeft.x, topLeft.y, view->geometry().width, view->geometry().height};
}

static bool onDevicePixels(double value, double scale)
{
    return std::abs(value * scale - std::round(value * scale)) < 1e-6;
}

static const double kScales[] = {1.0, 1.75, 2.0};

static std::vector<HeaderAction> someActions()
{
    return {
        {.id = "edit", .text = "Edit", .triggered = [] {}},
        {.id = "add", .glyph = "add", .text = "Add", .triggered = [] {}},
    };
}

// The window buttons' middle, from the window's top.
static double buttonsCentre(WindowFixture& fixture)
{
    auto* header = dynamic_cast<WindowHeader*>(fixture.window->headerBar()->parent());
    EXPECT_NE(header, nullptr);
    return header ? header->buttonsTop() + window_controls::kButton / 2 : 0;
}

TEST(HeaderTest, TheSwitchIsCentredOnTheWindowAndTheButtons)
{
    for (const double scale : kScales) {
        SCOPED_TRACE(scale);
        WindowFixture fixture(scale);
        SegmentedControl* tabs = fixture.window->headerBar()->tabSwitch();
        const RectF rect = windowRect(tabs);
        EXPECT_NEAR(rect.x + rect.width / 2, clockapp::style::windowWidth / 2, 1 / scale);
        EXPECT_NEAR(rect.y + rect.height / 2, buttonsCentre(fixture), 0.5 / scale);
        EXPECT_EQ(rect.height, HeaderBar::kSwitchHeight);
        EXPECT_TRUE(onDevicePixels(rect.x, scale));
        EXPECT_TRUE(onDevicePixels(rect.y, scale));
        EXPECT_LE(rect.bottom(), clockapp::style::headerHeight);
    }
}

TEST(HeaderTest, ThePlateIsConcentricWithTheTrack)
{
    for (const double scale : kScales) {
        SCOPED_TRACE(scale);
        WindowFixture fixture(scale);
        SegmentedControl* tabs = fixture.window->headerBar()->tabSwitch();
        const RectF track = tabs->localRect();
        const double segment = (track.width - 2 * tabs->padding()) / double(tabs->count());
        for (size_t i = 0; i < tabs->count(); ++i) {
            tabs->setCurrentIndex(int(i));
            const RectF plate = tabs->plateRect();
            // One gap to the top, the bottom and the end it meets.
            const double gap = plate.y - track.y;
            EXPECT_GT(gap, 0);
            EXPECT_DOUBLE_EQ(track.bottom() - plate.bottom(), gap);
            if (i == 0)
                EXPECT_DOUBLE_EQ(plate.x - track.x, gap);
            if (i + 1 == tabs->count())
                EXPECT_NEAR(track.right() - plate.right(), gap, 1e-9);
            const double middle = track.x + tabs->padding() + segment * (double(i) + 0.5);
            EXPECT_NEAR(plate.x + plate.width / 2, middle, 1e-9);
        }
    }
}

TEST(HeaderTest, ThePageButtonsKeepTheButtonsGapToTheEnd)
{
    for (const double scale : kScales) {
        SCOPED_TRACE(scale);
        WindowFixture fixture(scale);
        fixture.window->headerBar()->setActions(someActions(), false);
        fixture.driver.layout();
        const RectF edit = fixture.driver.frame("header:edit");
        const RectF add = fixture.driver.frame("header:add");
        ASSERT_GT(edit.width, 0);
        ASSERT_GT(add.width, 0);
        const RectF actions = windowRect(fixture.driver.find("header:actions"));
        // As far from the end as from the top.
        EXPECT_NEAR(clockapp::style::windowWidth - actions.right(), actions.y, 1 / scale);
        EXPECT_NEAR(actions.y + actions.height / 2, buttonsCentre(fixture), 0.5 / scale);
        EXPECT_EQ(actions.height, HeaderButtonGroup::kSegment);
        EXPECT_LT(edit.right(), add.x);
        EXPECT_TRUE(onDevicePixels(actions.x, scale));
        EXPECT_TRUE(onDevicePixels(actions.y, scale));
    }
}

TEST(HeaderTest, AtTheNarrowestTheSwitchKeepsClearOfBothEnds)
{
    for (const double scale : kScales) {
        SCOPED_TRACE(scale);
        WindowFixture fixture(scale, {clockapp::style::windowMinimumWidth, clockapp::style::windowMinimumHeight});
        fixture.window->headerBar()->setActions(someActions(), false);
        fixture.driver.layout();
        auto* header = dynamic_cast<WindowHeader*>(fixture.window->headerBar()->parent());
        ASSERT_NE(header, nullptr);
        const RectF tabs = windowRect(fixture.window->headerBar()->tabSwitch());
        const RectF actions = windowRect(fixture.driver.find("header:actions"));
        EXPECT_GE(tabs.x, header->leadingInset() + 12 - 1e-9);
        EXPECT_LE(tabs.right(), actions.x - 12 + 1 / scale);
        EXPECT_GE(tabs.width, 280);
    }
}

static int peakBrightness(const Image& image, const RectF& area)
{
    int peak = 0;
    const int x0 = std::max(0, int(std::ceil(area.x)) + 4);
    const int y0 = std::max(0, int(std::ceil(area.y)) + 4);
    const int x1 = std::min(image.width(), int(std::floor(area.right())) - 4);
    const int y1 = std::min(image.height(), int(std::floor(area.bottom())) - 4);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const Color c = image.pixel(x, y);
            peak = std::max({peak, int(c.r), int(c.g), int(c.b)});
        }
    }
    return peak;
}

TEST(HeaderTest, TheTextButtonDimsWhenTheWindowIsInactive)
{
    WindowFixture fixture(1.0);
    fixture.window->headerBar()->setActions(someActions(), false);
    fixture.driver.setActive(true);
    fixture.driver.layout();
    const RectF edit = fixture.driver.frame("header:edit");
    ASSERT_GT(edit.width, 0);

    const Image active = TestInput::presentFrame(fixture.window->window(), 1);
    const int activePeak = peakBrightness(active, edit);

    fixture.driver.setActive(false);
    const Image inactive = TestInput::presentFrame(fixture.window->window(), 2);
    const int inactivePeak = peakBrightness(inactive, edit);

    fixture.driver.setActive(true);
    const Image restored = TestInput::presentFrame(fixture.window->window(), 3);
    const int restoredPeak = peakBrightness(restored, edit);

    EXPECT_GT(activePeak, 200);
    EXPECT_LT(inactivePeak, activePeak - 50);
    EXPECT_EQ(restoredPeak, activePeak);
}
