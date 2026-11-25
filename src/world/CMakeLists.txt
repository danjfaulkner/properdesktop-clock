# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

find_package(PkgConfig REQUIRED)
pkg_check_modules(ClockWorldIcu REQUIRED IMPORTED_TARGET icu-uc icu-i18n)

# The cities, their times and the user's list: no window.
add_library(clock-world-logic STATIC
    cities.cpp
    citydatabase.cpp
    settle.cpp
    sky.cpp
    zonetime.cpp
)
target_include_directories(clock-world-logic PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(clock-world-logic
    PUBLIC ProperDesktop::ToolkitBase
    PRIVATE PkgConfig::ClockWorldIcu)

# The page.
add_library(clock-world STATIC
    addcitydialog.cpp
    cardgrid.cpp
    citycard.cpp
    clockface.cpp
    emptystate.cpp
    framedriver.cpp
    palette.cpp
    worldclockpage.cpp
)
target_link_libraries(clock-world PUBLIC clock-world-logic clock-page)
