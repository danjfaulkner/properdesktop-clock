// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "alarms/alarmspage.h"
#include "pagefixture.h"
#include "store/store.h"

#include <base/event_loop.h>

#include "ui/application.h"
#include "ui/bitmap.h"
#include "ui/label.h"
#include "ui/scroll_bar.h"
#include "ui/scroll_view.h"
#include "ui/theme.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace clockapp;
using properdesktop::ui::Label;
using properdesktop::ui::RectF;
using properdesktop::ui::TestDriver;
using properdesktop::ui::View;

static RectF inRow(View* view, View* row)
{
    RectF rect = view->geometry();
    for (View* parent = view->parent(); parent && parent != row; parent = parent->parent()) {
        rect.x += parent->geometry().x;
        rect.y += parent->geometry().y;
    }
    return rect;
}

class AlarmsLayout : public testing::TestWithParam<double>
{
};

TEST_P(AlarmsLayout, RowIsCentred)
{
    PageFixture fixture(GetParam());
    ClockStore& store = ClockStore::inFolder(fixture.context.paths().data);
    Alarm alarm;
    alarm.hour = 6;
    alarm.minute = 45;
    alarm.repeat = 0b0111110;
    alarm.label = "Wake";
    alarm.armedAt = 1795167660;
    const std::string id = store.setAlarm(alarm);
    fixture.show(makeAlarmsPage(fixture.context));
    TestDriver& driver = fixture.driver;

    View* row = driver.find("alarm:" + id);
    ASSERT_TRUE(row);
    const RectF card = row->geometry();
    // The card is centred on the page.
    EXPECT_NEAR(card.x, fixture.window.size().width - card.right(), 1.0);

    // The switch centred on the card's height, its inset the time's.
    const RectF toggle = inRow(driver.find("alarm:switch"), row);
    EXPECT_NEAR(toggle.y + toggle.height / 2, card.height / 2, 0.5);
    auto* time = driver.find<Label>("alarm:time");
    auto* detail = driver.find<Label>("alarm:detail");
    const RectF timeRect = inRow(time, row);
    EXPECT_NEAR(card.width - toggle.right(), timeRect.x, 0.5);

    // From the top of the figures to the baseline under them, one block in
    // the middle.
    const RectF detailRect = inRow(detail, row);
    const double top = timeRect.y + time->baselineOffset() - time->capHeight();
    const double bottom = detailRect.y + detail->baselineOffset();
    EXPECT_NEAR(top, card.height - bottom, 1.0 / GetParam() + 0.01) << top << " " << bottom;

    // Every edge on the device grid.
    const double scale = GetParam();
    for (const RectF& rect : {timeRect, detailRect, toggle})
        EXPECT_NEAR(rect.y * scale, std::round(rect.y * scale), 0.01);
}

TEST_P(AlarmsLayout, EmptyStateCentred)
{
    PageFixture fixture(GetParam());
    fixture.show(makeAlarmsPage(fixture.context));
    TestDriver& driver = fixture.driver;
    const RectF button = driver.frame("alarms:emptyAdd");
    const double width = fixture.window.size().width;
    EXPECT_NEAR(button.x, width - button.right(), 1.0);
}

// The bar's track starts below the frosted header and ends clear of the
// window's bottom corner: the handle's ink at either end of the scroll.
TEST(AlarmsBar, KeepsClearOfTheHeaderAndTheCorner)
{
    PageFixture fixture;
    ClockStore& store = ClockStore::inFolder(fixture.context.paths().data);
    for (int i = 0; i < 12; ++i) {
        Alarm alarm;
        alarm.hour = 6 + i;
        alarm.armedAt = 1795167660;
        store.setAlarm(alarm);
    }
    fixture.show(makeAlarmsPage(fixture.context));
    auto* scroll = fixture.driver.find<properdesktop::ui::ScrollView>("alarms:list");
    ASSERT_TRUE(scroll);
    scroll->setVerticalBarPolicy(properdesktop::ui::ScrollView::BarPolicy::AlwaysOn);
    const int height = int(fixture.window.size().height);
    const int x = int(fixture.window.size().width) - int(properdesktop::ui::ScrollBar::thickness / 2);

    const auto handleRows = [&](int& first, int& last) {
        fixture.driver.layout();
        // Shown once the content is known to overflow, then faded in on frames.
        scroll->setVerticalBarPolicy(properdesktop::ui::ScrollView::BarPolicy::AsNeeded);
        scroll->setVerticalBarPolicy(properdesktop::ui::ScrollView::BarPolicy::AlwaysOn);
        for (int i = 0; i < 25; ++i) {
            fixture.driver.layout();
            TestDriver::waitFor([] { return false; }, std::chrono::milliseconds(20));
        }
        const properdesktop::ui::Bitmap shot = fixture.page->grab(1);
        const uint32_t page = shot.pixel(x - 6, height / 2);
        first = -1;
        last = -1;
        for (int y = 0; y < height; ++y) {
            if (shot.pixel(x, y) != page) {
                if (first < 0)
                    first = y;
                last = y;
            }
        }
    };

    int first = 0;
    int last = 0;
    scroll->setScrollPosition({0, 0});
    handleRows(first, last);
    ASSERT_GE(first, 0) << "no handle drawn";
    EXPECT_GE(first, int(style::headerHeight)) << "the handle runs under the header";

    // Against the window's right edge, one bar inset in, not inside the page's margins.
    {
        const properdesktop::ui::Bitmap shot = fixture.page->grab(1);
        const int width = int(fixture.window.size().width);
        const int y = first + 8;
        const uint32_t page = shot.pixel(x - 6, y);
        int right = -1;
        for (int column = width - 12; column < width; ++column) {
            if (shot.pixel(column, y) != page)
                right = column;
        }
        ASSERT_GE(right, 0);
        EXPECT_NEAR(right + 1, width - properdesktop::ui::ScrollBar::inset, 1.0);
    }

    scroll->setScrollPosition({0, 1e6});
    handleRows(first, last);
    ASSERT_GE(last, 0);
    const double corner = properdesktop::ui::ScrollBar::cornerPadding(properdesktop::ui::Theme::current().windowRadius);
    EXPECT_LE(last, height - int(corner)) << "the handle runs into the window's corner";
}

INSTANTIATE_TEST_SUITE_P(Scales, AlarmsLayout, testing::Values(1.0, 1.75, 2.0));
