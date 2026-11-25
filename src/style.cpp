// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "style.h"

using properdesktop::ui::Color;
using properdesktop::ui::Theme;

namespace clockapp::style {

Color text()
{
    return Theme::current().text;
}

Color secondaryText()
{
    return withAlpha(Theme::current().text, 0.55);
}

Color separator()
{
    return withAlpha(Theme::current().text, 0.08);
}

Color page()
{
    return Theme::current().secondBackground;
}

Color card()
{
    return Theme::current().background;
}

Color controlTint()
{
    return Theme::current().darkMode ? Color{255, 255, 255, 18} : Color{0, 0, 0, 12};
}

Color accent()
{
    return Theme::current().highlight;
}

Color green()
{
    return properdesktop::ui::accent::green;
}

Color red()
{
    return properdesktop::ui::accent::red;
}

} // namespace clockapp::style
