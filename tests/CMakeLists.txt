# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

find_package(GTest REQUIRED)
include(GoogleTest)

set(CLOCK_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# The headless application with the glyphs, for scenario and layout tests.
add_library(clock-test-main STATIC common/main.cpp)
target_link_libraries(clock-test-main PUBLIC clock-page GTest::gtest)

# clock_add_test(<target> <logic|scenario|layout> SOURCES ... LIBRARIES ...)
# A logic test links no toolkit window and gets GoogleTest's own main; a
# scenario or layout test runs on the headless platform, one at a time.
function(clock_add_test target layer)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES" ${ARGN})
    add_executable(${target} ${ARG_SOURCES})
    target_include_directories(${target} PRIVATE ${CLOCK_TESTS_DIR}/common)
    target_compile_definitions(${target} PRIVATE CLOCK_TEST_DATA="${CLOCK_TESTS_DIR}/data")
    if(layer STREQUAL "logic")
        target_link_libraries(${target} PRIVATE ${ARG_LIBRARIES} GTest::gtest_main)
        gtest_discover_tests(${target} PROPERTIES LABELS logic)
    else()
        target_link_libraries(${target} PRIVATE ${ARG_LIBRARIES} clock-test-main)
        gtest_discover_tests(${target} PROPERTIES LABELS ${layer} RESOURCE_LOCK clock-ui)
    endif()
endfunction()

# The window's own tests.
clock_add_test(clock-scenario-tests scenario
    SOURCES scenarios/dark_test.cpp scenarios/window_test.cpp
    LIBRARIES clock-ui)
clock_add_test(clock-layout-tests layout
    SOURCES layout/header_test.cpp
    LIBRARIES clock-ui)

# Each module's tests: tests/<layer>/<module>/CMakeLists.txt, calling
# clock_add_test.
foreach(layer logic scenarios layout)
    foreach(module world store alarms service stopwatch timers)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${layer}/${module}/CMakeLists.txt)
            add_subdirectory(${layer}/${module})
        endif()
    endforeach()
endforeach()

# Translated text in its places, in every catalog the build makes.
clock_add_test(clock-fit-tests layout
    SOURCES layout/fit_test.cpp
    LIBRARIES clock-ui)
target_compile_definitions(clock-fit-tests PRIVATE CLOCK_CATALOGS="${PROJECT_BINARY_DIR}/locale")
if(TARGET properdesktop-clock-translations)
    add_dependencies(clock-fit-tests properdesktop-clock-translations)
endif()
