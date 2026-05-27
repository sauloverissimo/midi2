# Copyright (c) 2026 Saulo Verissimo
# SPDX-License-Identifier: MIT
#
# Smoke consumer: builds a tiny Zephyr application that pulls midi2 in
# as a west module and exercises the message + dispatch APIs on
# native_sim. Used by CI to catch regressions in the Zephyr integration
# without standing up real hardware.

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(midi2_zephyr_smoke LANGUAGES C)
target_sources(app PRIVATE src/main.c)
