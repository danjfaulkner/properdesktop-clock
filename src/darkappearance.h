// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/signal.h"

namespace clockapp {

// Clock is dark whatever the desktop's appearance: from now on, each theme
// the desktop applies is taken with dark colours in place of light ones,
// its accent, fonts, radii and other preferences kept. The dark theme goes
// in at once, before any window shows, and again in the same turn as each
// light one, so the windows lay out and paint once, dark. Keep the
// subscription for as long as the application runs.
[[nodiscard]] properdesktop::base::Subscription pinDarkAppearance();

} // namespace clockapp
