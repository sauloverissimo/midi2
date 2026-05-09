# Synthetic downstream consumer used by the cmake-build CI job.
#
# The CI installs midi2 to a prefix, then configures this project with
# CMAKE_PREFIX_PATH pointing at that prefix. find_package(midi2) must
# resolve to the imported midi2::midi2 target with INTERFACE_INCLUDE_DIRECTORIES
# pointing at the installed dist/midi2.h.
#
# Run locally:
#   cmake -S /repo -B /tmp/build && cmake --build /tmp/build --parallel
#   cmake --install /tmp/build --prefix /tmp/install
#   cmake -S /repo/tests/cmake-consumer -B /tmp/consumer-build \
#         -DCMAKE_PREFIX_PATH=/tmp/install
#   cmake --build /tmp/consumer-build
#   /tmp/consumer-build/midi2_consumer

cmake_minimum_required(VERSION 3.14)
project(midi2_consumer LANGUAGES C)

find_package(midi2 0.3.1 REQUIRED CONFIG)

add_executable(midi2_consumer main.c)
target_link_libraries(midi2_consumer PRIVATE midi2::midi2)
