// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "windowfixture.h"

#include "ui/theme.h"

using namespace properdesktop::ui;

// The desktop in light mode with an orange accent and larger text.
static Theme lightDesktop()
{
    Theme theme = Theme::light();
    theme.highlight = accent::orange;
    theme.fontPointSize = 11;
    theme.smallRadius = 7;
    return theme;
}

TEST(DarkTest, TheWindowAndItsDialogsAreDarkOnALightDesktop)
{
    WindowFixture fixture;
    Theme::setCurrent(lightDesktop());
    fixture.driver.layout();

    const Theme& theme = Theme::current();
    EXPECT_TRUE(theme.darkMode);
    EXPECT_EQ(theme.secondBackground, Theme::dark().secondBackground);
    EXPECT_EQ(theme.text, Theme::dark().text);
    // Only light and dark are pinned.
    EXPECT_EQ(theme.highlight, accent::orange);
    EXPECT_EQ(theme.fontPointSize, 11);
    EXPECT_EQ(theme.smallRadius, 7);
    EXPECT_TRUE(fixture.window->window().headerDarkMode());
    EXPECT_EQ(clockapp::style::page(), Theme::dark().secondBackground);

    ASSERT_TRUE(fixture.driver.chooseMenuItem("Clock", "About Clock"));
    Window* dialog = fixture.driver.dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_TRUE(dialog->headerDarkMode());
}

TEST(DarkTest, ADarkDesktopKeepsItsOwnTheme)
{
    WindowFixture fixture;
    Theme desktop = Theme::dark();
    desktop.highlight = accent::green;
    Theme::setCurrent(desktop);
    EXPECT_TRUE(Theme::current().darkMode);
    EXPECT_EQ(Theme::current().highlight, accent::green);
}
