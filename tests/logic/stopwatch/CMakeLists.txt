# The stopwatch's time alone, built from its source: no window.
clock_add_test(clock-stopwatch-logic-tests logic
    SOURCES stopwatch_test.cpp ${PROJECT_SOURCE_DIR}/src/stopwatch/stopwatch.cpp
    LIBRARIES ProperDesktop::ToolkitBase)
target_include_directories(clock-stopwatch-logic-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
