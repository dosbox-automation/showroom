# This file is part of the dosbox-automation-showroom Project.
# License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
#

cmake_minimum_required(VERSION 3.25)

# Only preset configurations are tested, so a bare cmake invocation gets a
# nudge rather than silently building something nobody verified.
if (NOT IS_PRESET_USED)
  message(WARNING
    "\nUse a platform preset instead of configuring by hand:
     cmake --preset <name>"
  )
  execute_process(
    COMMAND ${CMAKE_COMMAND} --list-presets
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  message("")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL
    "Generate compile_commands.json for use with LSPs")

project(dosbox-automation-showroom
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 6.4 is the baseline the code may rely on. Raising it cuts off older
# distribution Qt builds, so check the build VMs before bumping.
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

if (MSVC)
  add_compile_options("/W4")
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type)
endif()

qt_add_executable(showroom
  src/main.cpp
)

target_link_libraries(showroom PRIVATE
  Qt6::Widgets
  Qt6::Network
)

option(OPT_TESTS "Enable tests" ON)

if (OPT_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
