# This file is part of the dosbox-automation-showroom Project.
# License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
#

find_package(GTest REQUIRED)

add_executable(showroom_tests
  smoke_tests.cpp
  imported/log_tests.cpp
  model/game_definition_tests.cpp
  model/game_catalog_tests.cpp
  model/tile_state_tests.cpp
  model/step_sizer_tests.cpp
)

target_link_libraries(showroom_tests PRIVATE
  showroom_model
  GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(showroom_tests
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Widgets get their own binary, which is what stops the pure-logic tier
# from quietly growing a Qt dependency.
find_package(Qt6 6.4 REQUIRED COMPONENTS Test)

qt_add_executable(showroom_ui_tests
  ui/ui_test_main.cpp
  app/paths_tests.cpp
  ui/legend_bar_tests.cpp
  ui/game_tile_tests.cpp
  ui/main_window_tests.cpp
)

target_link_libraries(showroom_ui_tests PRIVATE
  showroom_ui
  Qt6::Test
  GTest::gtest
)

gtest_discover_tests(showroom_ui_tests
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
