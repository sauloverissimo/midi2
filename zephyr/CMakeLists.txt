# Copyright (c) 2026 Saulo Verissimo
# SPDX-License-Identifier: MIT
#
# Zephyr build integration for midi2. Picked up by the Zephyr build
# system via zephyr/module.yml. Sources live under ../src/; the
# amalgamated dist/ is NOT consumed here so Zephyr builds match the
# CI matrix for other architectures.

if(CONFIG_MIDI2)
  zephyr_library()
  zephyr_library_sources(
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/midi2_proc.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/midi2_dispatch.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/midi2_conv.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/midi2_ci.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/midi2_ci_dispatch.c
  )
  # zephyr_include_directories (NOT zephyr_library_include_directories):
  # exposes midi2_*.h to the application and any other libraries that
  # may want to call into midi2. Internal library sources also see it.
  zephyr_include_directories(
    ${ZEPHYR_CURRENT_MODULE_DIR}/src
  )
endif()
