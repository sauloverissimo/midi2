# Copyright (c) 2026 Saulo Verissimo
# SPDX-License-Identifier: MIT
#
# midi2 Zephyr example: full-spec USB MIDI 2.0 device showcase on the
# Raspberry Pi Pico (RP2040). Builds out-of-tree against any Zephyr
# workspace that lists this midi2 repo as a module via west.yml.

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rpi_pico_device_zephyr LANGUAGES C)
target_sources(app PRIVATE src/main.c)
