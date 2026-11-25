add_library(clock-store STATIC store.cpp)
target_include_directories(clock-store PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(clock-store PUBLIC ProperDesktop::ToolkitBase)
