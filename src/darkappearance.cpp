// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "darkappearance.h"

#include "ui/theme.h"

using properdesktop::ui::Theme;

namespace clockapp {

static void applyDark()
{
    const Theme& current = Theme::current();
    if (current.darkMode)
        return;
    const Theme dark = Theme::dark();
    Theme theme = current;
    theme.darkMode = true;
    theme.background = dark.background;
    theme.secondBackground = dark.secondBackground;
    theme.alternateBackground = dark.alternateBackground;
    theme.text = dark.text;
    theme.disabledText = dark.disabledText;
    theme.highlightedText = dark.highlightedText;
    Theme::setCurrent(theme);
}

properdesktop::base::Subscription pinDarkAppearance()
{
    applyDark();
    return Theme::changed().connect([] { applyDark(); });
}

} // namespace clockapp
