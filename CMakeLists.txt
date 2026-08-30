cmake_minimum_required(VERSION 3.15)
project(sentineld VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ship with debug symbols by default so a production core dump is actually
# analysable; a bare `cmake ..` previously produced an unoptimised, stripped build.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

# Enable standard compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Dependencies via FetchContent
include(FetchContent)

# spdlog for logging
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# nlohmann_json for configuration
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(json)

# CLI11 for argument parsing
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.3.2
)
FetchContent_MakeAvailable(CLI11)

# Main executable
add_executable(sentineld 
    src/main.cpp
    src/core/config.cpp
    src/core/logger.cpp
    src/daemon/daemonizer.cpp
    src/daemon/signal_handler.cpp
    src/features/resource_monitor.cpp
    src/features/file_watcher.cpp
)

target_include_directories(sentineld PRIVATE include)
target_link_libraries(sentineld PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    CLI11::CLI11
    Threads::Threads
)
target_compile_definitions(sentineld PRIVATE SENTINEL_VERSION="${PROJECT_VERSION}")

# Installation
install(TARGETS sentineld DESTINATION bin)
install(FILES config/sentinel.json DESTINATION /etc/sentineld)
install(FILES scripts/sentineld.service DESTINATION /usr/lib/systemd/system)

# CPack Configuration (RPM)
set(CPACK_PACKAGE_NAME "sentineld")
set(CPACK_PACKAGE_VENDOR "Ehteshamoddin")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Linux System Monitor Daemon")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

set(CPACK_GENERATOR "RPM")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "System/Monitoring")
set(CPACK_RPM_PACKAGE_URL "https://github.com/Ehteshamoddin/sentineld")

# Prevent conflict with systemd package by not owning system directories
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/usr/lib/systemd" "/usr/lib/systemd/system" "/etc/sentineld")

# Disable build-id links to keep /usr/lib clean
set(CPACK_RPM_SPEC_MORE_DEFINE "%define _build_id_links none")

# Preserve operator edits to the config across `rpm -U`.
set(CPACK_RPM_USER_FILELIST
    "%config(noreplace) /etc/sentineld/sentinel.json"
    "%attr(644,root,root) /usr/lib/systemd/system/sentineld.service"
)

# Creates the unprivileged service account and runs `systemctl daemon-reload`,
# without which a freshly installed unit is invisible to systemd.
set(CPACK_RPM_PACKAGE_REQUIRES "systemd, shadow-utils")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/postinst")

include(CPack)
