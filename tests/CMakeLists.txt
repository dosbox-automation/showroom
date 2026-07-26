# This file is part of the dosbox-automation-showroom Project.
# License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
#

find_package(GTest REQUIRED)

add_executable(showroom_tests
  smoke_tests.cpp
  imported/log_tests.cpp
  model/game_definition_tests.cpp
  model/game_catalog_tests.cpp
)

target_link_libraries(showroom_tests PRIVATE
  showroom_model
  GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(showroom_tests
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
