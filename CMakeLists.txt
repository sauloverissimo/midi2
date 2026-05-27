# midi2 - Portable MIDI 2.0 UMP library, C99, zero dependencies.
#
# This CMake build coexists with the existing Makefile (which still drives
# the modular src/ test suite). It exposes the amalgamated dist/midi2.c as
# a single static library so external consumers (midi2_cpp, third-party
# CMake projects, FetchContent users, system installs) get one target
# without having to vendor the source.
#
# Consumer usage:
#
#   # 1. Already provided by parent project? Skip.
#   if(NOT TARGET midi2)
#     # 2. Installed on the system? Use it.
#     find_package(midi2 QUIET CONFIG)
#     if(NOT midi2_FOUND)
#       # 3. Pull from GitHub.
#       include(FetchContent)
#       FetchContent_Declare(midi2
#         GIT_REPOSITORY https://github.com/sauloverissimo/midi2.git
#         GIT_TAG        v0.5.0)
#       FetchContent_MakeAvailable(midi2)
#     endif()
#   endif()
#   target_link_libraries(my_target PRIVATE midi2::midi2)

cmake_minimum_required(VERSION 3.14)

# ESP-IDF Component Manager path. When IDF resolves this directory as
# a component (clone under managed_components/midi2 + idf_component.yml
# at the root), it sets ESP_PLATFORM before processing this file. We
# route to idf_component_register and skip the rest, leaving the
# native CMake build (project / install / export) untouched for every
# other consumer.
#
# The Component Manager fetches the dependency as a tarball that does
# NOT include dist/ (filtered as a build-output convention), so the
# ESP-IDF gate consumes the modular src/midi2_*.c sources directly.
# midi2_msg.h, midi2.h and midi2_ci_msg.h are header-only and need no
# entry in SRCS.
if(ESP_PLATFORM)
    idf_component_register(
        SRCS
            src/midi2_proc.c
            src/midi2_dispatch.c
            src/midi2_conv.c
            src/midi2_ci.c
            src/midi2_ci_dispatch.c
        INCLUDE_DIRS src
    )
    return()
endif()

project(midi2 VERSION 0.5.0 LANGUAGES C)

# GNUInstallDirs has to come before target_include_directories so that
# $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> evaluates to the real
# "include" relative path inside the exported config — without it, the
# generator expression collapses to empty and downstream
# find_package(midi2) consumers see a target with no include path.
include(GNUInstallDirs)

# The amalgam is the canonical single-translation-unit form. Modular
# src/*.c stays around for the Makefile's per-module unit tests.
add_library(midi2 STATIC dist/midi2.c)
add_library(midi2::midi2 ALIAS midi2)

target_include_directories(midi2 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dist>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(midi2 PUBLIC c_std_99)

# Warning parity with the Makefile build (-Wall -Wextra -Wpedantic).
# PRIVATE: only when building midi2 itself; downstream consumers pick
# their own warning policy.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(midi2 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# find_package(midi2 CONFIG) support for downstream package managers
# (vcpkg, conan, system installs). Skipped when midi2 is consumed via
# add_subdirectory or FetchContent (where install would pollute the
# parent's install rules unless the parent opts in).
if(PROJECT_IS_TOP_LEVEL)
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/midi2-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(TARGETS midi2 EXPORT midi2-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES dist/midi2.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT midi2-targets
            NAMESPACE midi2::
            FILE midi2-config.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/midi2)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/midi2-config-version.cmake"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/midi2)
endif()
