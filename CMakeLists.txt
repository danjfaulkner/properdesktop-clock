# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)

project(properdesktop-clock VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CTest)

find_package(ProperDesktopToolkit REQUIRED)
find_package(Threads REQUIRED)

# The glyphs, built in: icons/*.svg and every module's icons/<module>/*.svg,
# each by its file name without the suffix.
file(GLOB_RECURSE CLOCK_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/icons/*.svg)
set(CLOCK_EMBEDDED ${CMAKE_CURRENT_BINARY_DIR}/icons.cpp)
# A list would be split into separate arguments on the way to the script.
string(REPLACE ";" "|" CLOCK_ICON_LIST "${CLOCK_ICONS}")
add_custom_command(OUTPUT ${CLOCK_EMBEDDED}
    COMMAND ${CMAKE_COMMAND} -DOUTPUT=${CLOCK_EMBEDDED}
            -DFILES=${CLOCK_ICON_LIST} -P ${PROJECT_SOURCE_DIR}/cmake/EmbedIcons.cmake
    DEPENDS ${CLOCK_ICONS} ${PROJECT_SOURCE_DIR}/cmake/EmbedIcons.cmake
    VERBATIM)

# What every page builds on: the page contract, its context and the shared style.
add_library(clock-page STATIC
    src/darkappearance.cpp
    src/page.cpp
    src/pagecontext.cpp
    src/style.cpp
    ${CLOCK_EMBEDDED}
)
target_include_directories(clock-page PUBLIC src)
target_link_libraries(clock-page PUBLIC ProperDesktop::Toolkit Threads::Threads)

# The modules, each a static library of its own when its directory has a
# CMakeLists.txt: clock-world, clock-store, clock-alarms, clock-service,
# clock-stopwatch, clock-timers.
set(CLOCK_MODULES world store alarms service stopwatch timers)
foreach(module ${CLOCK_MODULES})
    if(EXISTS ${PROJECT_SOURCE_DIR}/src/${module}/CMakeLists.txt)
        add_subdirectory(src/${module})
    endif()
endforeach()

add_library(clock-ui STATIC
    src/application.cpp
    src/clockwindow.cpp
    src/headerbar.cpp
    src/pages.cpp
)
target_include_directories(clock-ui PUBLIC src)
target_link_libraries(clock-ui PUBLIC clock-page)
# A page whose module is not built yet shows as a placeholder.
foreach(module world alarms stopwatch timers)
    if(TARGET clock-${module})
        string(TOUPPER ${module} upper)
        target_link_libraries(clock-ui PUBLIC clock-${module})
        target_compile_definitions(clock-ui PRIVATE CLOCK_HAS_${upper})
    endif()
endforeach()

add_executable(${PROJECT_NAME} src/main.cpp)
target_compile_definitions(${PROJECT_NAME} PRIVATE CLOCK_VERSION="${PROJECT_VERSION}")
target_link_libraries(${PROJECT_NAME} PRIVATE clock-ui)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# The alarm service calls the application (RepeatTimer, open) and starts it if need be.
configure_file(org.properdesktop.Clock.service.in ${CMAKE_CURRENT_BINARY_DIR}/org.properdesktop.Clock.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.properdesktop.Clock.service DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)

properdesktop_add_translations(properdesktop-clock SOURCES src
    DESKTOP_FILES properdesktop-clock.desktop src/service/properdesktop-clock-alarms.desktop)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/properdesktop-clock.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
