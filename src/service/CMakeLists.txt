add_library(clock-service STATIC alarmservice.cpp)
target_link_libraries(clock-service PUBLIC clock-alarm-logic)

add_executable(properdesktop-clock-alarms main.cpp)
target_link_libraries(properdesktop-clock-alarms PRIVATE clock-service)
install(TARGETS properdesktop-clock-alarms RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# The translated copy, which the top level's properdesktop_add_translations makes.
install(FILES ${CMAKE_BINARY_DIR}/properdesktop-clock-alarms.desktop DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart)
configure_file(org.properdesktop.ClockAlarms.service.in ${CMAKE_CURRENT_BINARY_DIR}/org.properdesktop.ClockAlarms.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.properdesktop.ClockAlarms.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
