// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pagefixture.h"

#include "stopwatch/digitdisplay.h"
#include "store/store.h"
#include "timers/composer.h"
#include "timers/timercard.h"
#include "timers/timermath.h"
#include "timers/timerring.h"
#include "timers/timerspage.h"
#include "timers/wheelpicker.h"

#include "ui/bitmap.h"
#include "ui/row_combo_box.h"
#include "ui/scroll_bar.h"
#include "ui/scroll_view.h"
#include "ui/theme.h"

#include <gtest/gtest.h>

#include <chrono>

using namespace clockapp;
using namespace properdesktop::ui;

// Where the vertical bar's handle reaches in the bar, top and bottom, in
// logical pixels, with the bar always shown: the track's ends at the top
// and the bottom of the content.
static std::pair<double, double> handleEnds(PageFixture& fixture, ScrollView* view, bool atBottom)
{
    view->setScrollPosition({0, atBottom ? 1e9 : 0});
    fixture.driver.layout();
    // Shown once the content is known to overflow, then faded in on frames.
    view->setVerticalBarPolicy(ScrollView::BarPolicy::AsNeeded);
    view->setVerticalBarPolicy(ScrollView::BarPolicy::AlwaysOn);
    for (int i = 0; i < 25; ++i) {
        fixture.driver.layout();
        TestDriver::waitFor([] { return false; }, std::chrono::milliseconds(20));
    }
    ScrollBar* bar = nullptr;
    for (const auto& child : view->children()) {
        auto* candidate = dynamic_cast<ScrollBar*>(child.get());
        if (candidate && candidate->geometry().height > candidate->geometry().width)
            bar = candidate;
    }
    if (!bar)
        return {-1, -1};
    const double scale = fixture.window.scale();
    Bitmap image = bar->grab(scale);
    const int x = image.width() / 2;
    int first = -1;
    int last = -1;
    for (int y = 0; y < image.height(); ++y) {
        const uint8_t alpha = image.constBits()[size_t(y) * image.stride() + size_t(x) * 4 + 3];
        if (alpha > 24) {
            if (first < 0)
                first = y;
            last = y + 1;
        }
    }
    return {first / scale, last / scale};
}

class TimersLayout : public testing::TestWithParam<double>
{
protected:
    TimersLayout()
        : fixture(GetParam())
        , store(clockstore::pathIn(fixture.scratch.path()))
    {
    }

    TimersPage& open()
    {
        auto page = std::make_unique<TimersPage>(fixture.context, &store);
        TimersPage* made = page.get();
        fixture.show(std::move(page));
        TestDriver::waitFor([&] {
            fixture.driver.layout();
            return fixture.driver.find("timers:composer")->isVisible() !=
                   fixture.driver.find("timers:grid")->isVisible();
        });
        return *made;
    }

    void addTimer(int64_t durationMs)
    {
        const int64_t now =
            std::chrono::duration_cast<std::chrono::milliseconds>(fixture.now.time_since_epoch()).count();
        RunningTimer timer;
        timer.durationMs = durationMs;
        timer.endsAtMs = now + durationMs;
        store.setTimer(timer);
    }

    double tolerance() const { return 0.5 / GetParam(); }

    PageFixture fixture;
    ClockStore store;
};

TEST_P(TimersLayout, RingKeepsOneGapToTheCard)
{
    addTimer(10 * 60 * 1000);
    open();
    const RectF card = fixture.driver.frame("timers:card");
    const RectF ring = fixture.driver.frame("timers:ring");
    const double gap = TimerCard::Inset;
    EXPECT_NEAR(ring.x - card.x, gap, tolerance());
    EXPECT_NEAR(card.right() - ring.right(), gap, tolerance());
    EXPECT_NEAR(ring.y - card.y, gap, tolerance());
    EXPECT_EQ(ring.width, ring.height);

    // The buttons keep the same gap to the sides and the bottom.
    const RectF cancel = fixture.driver.frame("timers:cancel");
    const RectF pause = fixture.driver.frame("timers:pause");
    EXPECT_NEAR(cancel.x - card.x, gap, tolerance());
    EXPECT_NEAR(card.right() - pause.right(), gap, tolerance());
    EXPECT_NEAR(card.bottom() - cancel.bottom(), gap, tolerance());

    // The time left sits on the ring's centre.
    DigitDisplay* remaining = fixture.driver.find<DigitDisplay>("timers:remaining");
    const RectF figures = fixture.driver.frame("timers:remaining");
    const double middle = figures.x + remaining->lineStart() + remaining->lineWidth() / 2;
    EXPECT_NEAR(middle, ring.x + ring.width / 2, 1);
    EXPECT_NEAR(figures.y + figures.height / 2, ring.y + ring.height / 2, 1);
}

TEST_P(TimersLayout, CardsAreCentredInRows)
{
    addTimer(60000);
    addTimer(120000);
    open();
    std::vector<TimerCard*> cards = fixture.driver.findAll<TimerCard>("timers:card");
    ASSERT_EQ(cards.size(), 2u);
    const double left = cards[0]->mapToWindow({0, 0}).x;
    const double right = cards[1]->mapToWindow({TimerCard::Width, 0}).x;
    EXPECT_NEAR((left + right) / 2, clockapp::style::windowWidth / 2, 1);
    EXPECT_EQ(cards[0]->mapToWindow({0, 0}).y, cards[1]->mapToWindow({0, 0}).y);
}

TEST_P(TimersLayout, PickerBandIsConcentric)
{
    open();
    DurationPicker* picker = fixture.driver.find<DurationPicker>("timers:picker");
    ASSERT_TRUE(picker);
    const RectF band = picker->bandRect();
    const double width = picker->geometry().width;
    EXPECT_NEAR(band.x, DurationPicker::BandInset, tolerance());
    EXPECT_NEAR(width - band.right(), DurationPicker::BandInset, tolerance());
    EXPECT_DOUBLE_EQ(picker->bandRadius(), Theme::current().bigRadius - DurationPicker::BandInset);
    // The band on the wheels' middle row.
    const RectF minutes = fixture.driver.frame("timers:minutes");
    const RectF pickerFrame = fixture.driver.frame("timers:picker");
    EXPECT_NEAR(pickerFrame.y + band.y + band.height / 2, minutes.y + minutes.height / 2, 1);
}

TEST_P(TimersLayout, RecentStartButtonKeepsOneGap)
{
    open();
    auto* page = static_cast<TimersPage*>(fixture.page);
    page->startFromComposer();
    page->showComposer(true);
    TestDriver::waitFor([&] {
        fixture.driver.layout();
        return !fixture.driver.find("timers:grid")->isVisible();
    });
    const RectF row = fixture.driver.frame("timers:recent");
    const RectF button = fixture.driver.frame("timers:recent-start");
    EXPECT_NEAR(row.right() - button.right(), button.y - row.y, tolerance());
    EXPECT_NEAR(row.bottom() - button.bottom(), button.y - row.y, tolerance());
}

TEST_P(TimersLayout, CardsBarStartsBelowTheHeader)
{
    for (int i = 1; i <= 8; ++i)
        addTimer(i * 60000);
    open();
    auto* view = fixture.driver.find<ScrollView>("timers:grid");
    ASSERT_TRUE(view);
    // The cards' list spans the window: its bar sits at the window's edge.
    const RectF frame = fixture.driver.frame("timers:grid");
    EXPECT_NEAR(frame.right(), clockapp::style::windowWidth, tolerance());
    const double height = view->geometry().height;
    EXPECT_NEAR(handleEnds(fixture, view, false).first, clockapp::style::headerHeight + ScrollBar::inset,
                tolerance() * 2);
    EXPECT_NEAR(height - handleEnds(fixture, view, true).second,
                ScrollBar::cornerPadding(Theme::current().windowRadius), tolerance() * 2);
}

TEST_P(TimersLayout, RecentsBarClearsTheCardCorners)
{
    std::vector<timermath::Recent> recents;
    for (int i = 1; i <= int(timermath::MaximumRecents); ++i)
        recents.push_back({i * 60000LL, "Recent " + std::to_string(i), "chime"});
    timermath::saveRecents(fixture.context.settings(), recents);
    fixture.window.resize({clockapp::style::windowWidth, clockapp::style::windowMinimumHeight});
    open();
    auto* view = fixture.driver.find<ScrollView>("timers:recents");
    ASSERT_TRUE(view);
    // Not at the window's edge: the bar stays at the column's own end.
    EXPECT_LT(fixture.driver.frame("timers:recents").right(), clockapp::style::windowWidth / 2);
    // The list fills a rounded card: the track keeps clear of its corners
    // at both ends.
    const double padding = ScrollBar::cornerPadding(Theme::current().bigRadius);
    const double height = view->geometry().height;
    EXPECT_NEAR(handleEnds(fixture, view, false).first, padding, tolerance() * 2);
    EXPECT_NEAR(height - handleEnds(fixture, view, true).second, padding, tolerance() * 2);
}

TEST_P(TimersLayout, NoRecentsShowNothing)
{
    open();
    EXPECT_FALSE(fixture.driver.shows("timers:recents"));
    EXPECT_FALSE(fixture.driver.shows("timers:recents-title"));
    // The picker group sits on the page's middle.
    const RectF picker = fixture.driver.frame("timers:picker");
    EXPECT_NEAR(picker.x + picker.width / 2, clockapp::style::windowWidth / 2, 1);
}

TEST_P(TimersLayout, RecentsStandAtTheStart)
{
    timermath::saveRecents(fixture.context.settings(), {{300000, "Tea", "chime"}});
    open();
    ASSERT_TRUE(fixture.driver.shows("timers:recents"));
    const RectF recents = fixture.driver.frame("timers:recents");
    const RectF picker = fixture.driver.frame("timers:picker");
    EXPECT_NEAR(recents.x, clockapp::style::pagePadding, tolerance());
    EXPECT_LE(recents.right() + TimerComposer::RecentsGap, picker.x + tolerance());
    // The group is centred in the space the column leaves.
    const double left = recents.right() + TimerComposer::RecentsGap;
    const double right = clockapp::style::windowWidth - clockapp::style::pagePadding;
    EXPECT_NEAR(picker.x + picker.width / 2, (left + right) / 2, 1);
    // The sound row's chevron ends one row inset from the card's edge.
    auto* soundBox = fixture.driver.find<properdesktop::ui::RowComboBox>("timers:sound");
    ASSERT_NE(soundBox, nullptr);
    const RectF sound = fixture.driver.frame("timers:sound");
    EXPECT_NEAR(picker.right() - (sound.right() - soundBox->trailingOverhang()), 14, tolerance());
}

TEST_P(TimersLayout, StartIsCentredOnThePicker)
{
    for (const bool withRecents : {false, true}) {
        if (withRecents)
            timermath::saveRecents(fixture.context.settings(), {{300000, "Tea", "chime"}});
        open();
        const RectF picker = fixture.driver.frame("timers:picker");
        const RectF start = fixture.driver.frame("timers:start");
        EXPECT_NEAR(start.x + start.width / 2, picker.x + picker.width / 2, tolerance()) << withRecents;
        if (!withRecents)
            EXPECT_NEAR(start.x + start.width / 2, clockapp::style::windowWidth / 2, tolerance());
        fixture.window.setContentView(nullptr);
    }
}

TEST_P(TimersLayout, RunningCardsStayBesideRecents)
{
    timermath::saveRecents(fixture.context.settings(), {{300000, "Tea", "chime"}});
    addTimer(300000);
    open();
    ASSERT_TRUE(fixture.driver.shows("timers:recents"));
    ASSERT_TRUE(fixture.driver.shows("timers:card"));
    const RectF recents = fixture.driver.frame("timers:recents");
    const RectF card = fixture.driver.frame("timers:card");
    EXPECT_NEAR(recents.x, clockapp::style::pagePadding, tolerance());
    EXPECT_LE(recents.right() + TimerComposer::RecentsGap, card.x + tolerance());
    const double left = recents.right() + TimerComposer::RecentsGap;
    const double right = clockapp::style::windowWidth - clockapp::style::pagePadding;
    EXPECT_NEAR(card.x + card.width / 2, (left + right) / 2, 1);
}

INSTANTIATE_TEST_SUITE_P(Scales, TimersLayout, testing::Values(1.0, 1.75, 2.0));
