cmake_minimum_required(VERSION 3.14)
project(rp2350_catalog_test C)
set(CMAKE_C_STANDARD 99)
# midi2 C99 core is header-first; point at the repo src (three `..` up from test/).
set(MIDI2_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../../src)
add_executable(test_catalog test_catalog.c ../catalog.c)
target_include_directories(test_catalog PRIVATE ${MIDI2_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/..)
enable_testing()
add_test(NAME catalog COMMAND test_catalog)
