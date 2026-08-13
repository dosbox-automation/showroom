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
find_package(tomlplusplus CONFIG REQUIRED)
find_package(LibArchive REQUIRED)

qt_standard_project_setup()

if (MSVC)
  add_compile_options("/W4")
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type)
endif()

# Pure logic and infrastructure, no Qt: this carries the test weight and
# both the application and the test binary link it.
add_library(showroom_model STATIC
  src/engine/conf_writer.cpp
  src/imported/log.cpp
  src/model/game_definition.cpp
  src/model/install_check.cpp
  src/model/game_catalog.cpp
  src/model/tile_state.cpp
  src/model/step_sizer.cpp
)

target_include_directories(showroom_model PUBLIC src)

target_link_libraries(showroom_model PUBLIC
  tomlplusplus::tomlplusplus
)

# Child-process tier: needs Qt Core but neither widgets nor the model's
# purity, so it sits between showroom_model and showroom_ui.
qt_add_library(showroom_engine STATIC
  src/engine/api_client.cpp
  src/net/archive_extractor.cpp
  src/net/connectivity.cpp
  src/net/download_plan.cpp
  src/net/downloader.cpp
  src/net/file_verifier.cpp
  src/engine/engine_process.cpp
  src/engine/game_launcher.cpp
  src/engine/install_runner.cpp
)

target_link_libraries(showroom_engine PUBLIC
  showroom_model
  Qt6::Core
  Qt6::Network
)

# PRIVATE: archive.h stays out of the engine's public surface.
target_link_libraries(showroom_engine PRIVATE
  LibArchive::LibArchive
)

# The sidebar badge, set here so the release train has one place to
# change rather than a literal going stale in a widget.
set(SHOWROOM_ENGINE_VERSION "0.84-da4" CACHE STRING
    "Version string of the bundled dosbox-automation")

# A library rather than part of the executable, so the UI tests can link
# it and showroom_model stays provably Qt-free.
qt_add_library(showroom_ui STATIC
  src/app/paths.cpp
  src/app/settings.cpp
  src/ui/about_dialog.cpp
  src/ui/game_tile.cpp
  src/ui/legend_bar.cpp
  src/ui/main_window.cpp
  src/ui/sidebar.cpp
  src/ui/theme.cpp
  src/ui/tile_grid.cpp
)

target_link_libraries(showroom_ui PUBLIC
  showroom_model
  showroom_engine
  Qt6::Widgets
  Qt6::Network
)

target_compile_definitions(showroom_ui PUBLIC
  SHOWROOM_VERSION="${PROJECT_VERSION}"
  SHOWROOM_ENGINE_VERSION="${SHOWROOM_ENGINE_VERSION}"
  # Last resort for a developer build run straight out of the build
  # directory, where nothing has been installed beside the binary yet.
  SHOWROOM_SOURCE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)

qt_add_executable(showroom
  src/main.cpp
)

target_link_libraries(showroom PRIVATE
  showroom_ui
)

option(OPT_TESTS "Enable tests" ON)

if (OPT_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
