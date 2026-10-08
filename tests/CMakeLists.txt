find_package(Qt6 6.11 REQUIRED COMPONENTS Test)
find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
qt_add_executable(portal-tests test_portal.cpp)
set_target_properties(portal-tests PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
target_link_libraries(portal-tests PRIVATE portal-presentation Qt6::Test Qt6::QuickControls2)
qt_add_resources(portal-tests chooser PREFIX /qt/qml/HolonightPortal BASE ${PROJECT_SOURCE_DIR}/qml
  FILES ${PROJECT_SOURCE_DIR}/qml/Chooser.qml)
add_test(NAME portal-tests COMMAND ${DBUS_RUN_SESSION} --config-file=${CMAKE_CURRENT_SOURCE_DIR}/dbus-session.conf
  -- $<TARGET_FILE:portal-tests>)
string(REPLACE ";" ":" runtime_qml "${QML_IMPORT_PATH}")
set_tests_properties(portal-tests PROPERTIES TIMEOUT 60 ENVIRONMENT
  "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML_IMPORT_PATH=${runtime_qml}")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME portal-staged-activation COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/check-install.py
  ${PROJECT_BINARY_DIR} $<TARGET_FILE:portal-tests>)
set_tests_properties(portal-staged-activation PROPERTIES TIMEOUT 30 ENVIRONMENT
  "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML_IMPORT_PATH=${runtime_qml};LD_LIBRARY_PATH=$<TARGET_FILE_DIR:HolonightFileBrowser::Quick>")
qt_add_executable(portal-native-probe native_probe.cpp)
set_target_properties(portal-native-probe PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
target_include_directories(portal-native-probe PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries(portal-native-probe PRIVATE portal-presentation Qt6::GuiPrivate Qt6::QuickControls2 PkgConfig::WaylandClient)
qt_add_resources(portal-native-probe chooser PREFIX /qt/qml/HolonightPortal BASE ${PROJECT_SOURCE_DIR}/qml
  FILES ${PROJECT_SOURCE_DIR}/qml/Chooser.qml)
foreach(target portal-tests portal-native-probe)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()
file(GENERATE OUTPUT ${PROJECT_BINARY_DIR}/native-check.sh CONTENT
  "#!/usr/bin/env bash\nset -euo pipefail\nexport QML_IMPORT_PATH='${runtime_qml}'\nexec '${DBUS_RUN_SESSION}' --config-file='${CMAKE_CURRENT_SOURCE_DIR}/dbus-session.conf' -- '$<TARGET_FILE:portal-native-probe>' \"\$@\"\n")
