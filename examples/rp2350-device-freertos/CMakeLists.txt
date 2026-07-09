cmake_minimum_required(VERSION 3.14)  # FetchContent_MakeAvailable

# rp2350-device-freertos: beefy USB MIDI 2.0 device example for the Raspberry
# Pi Pico 2 (RP2350). Pure C99 (midi2 core, no C++ wrapper) over FreeRTOS-Kernel
# + TinyUSB upstream. Lives at midi2/examples/rp2350-device-freertos and consumes
# the parent midi2 C99 core directly from ../../src (header-only usage).
#
# This is the canonical Tier 2f recipe: midi2 C99 + Pico SDK + FreeRTOS + TinyUSB.

set(PICO_BOARD pico2 CACHE STRING "Board type")

# FreeRTOS-Kernel location. Provide it with -DFREERTOS_KERNEL_PATH=... or the
# FREERTOS_KERNEL_PATH env var (V11.2.0+, which carries the RP2350 port in the
# Community-Supported-Ports submodule; run `git submodule update --init` there).
# Resolve into a CMake variable: the include() below uses ${FREERTOS_KERNEL_PATH},
# not the env var, so an env-only value must be propagated first (Tier 2f gotcha).
if(NOT FREERTOS_KERNEL_PATH AND DEFINED ENV{FREERTOS_KERNEL_PATH})
    set(FREERTOS_KERNEL_PATH $ENV{FREERTOS_KERNEL_PATH})
endif()
if(NOT FREERTOS_KERNEL_PATH)
    message(FATAL_ERROR
        "FREERTOS_KERNEL_PATH is not set. Point it at a FreeRTOS-Kernel v11.2.0+ "
        "checkout with the Community-Supported-Ports submodule initialized "
        "(cmake -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel ... or export the env var).")
endif()

# TinyUSB upstream (merged MIDI 2.0 device drivers, tud_midi2_*). Prefer a local
# checkout via -DPICO_TINYUSB_PATH=... ; otherwise fetch the merged commit.
include(FetchContent)
if(NOT DEFINED PICO_TINYUSB_PATH AND NOT DEFINED ENV{PICO_TINYUSB_PATH})
    FetchContent_Declare(
        tinyusb
        GIT_REPOSITORY https://github.com/hathach/tinyusb.git
        GIT_TAG        536157cb98fbc40329c9695281506fe7f04e526f
        GIT_SHALLOW    FALSE
    )
    FetchContent_MakeAvailable(tinyusb)
    set(PICO_TINYUSB_PATH "${tinyusb_SOURCE_DIR}" CACHE PATH "TinyUSB source")
endif()

include(pico_sdk_import.cmake)

# CXX must be enabled even for pure-C firmware: the Pico SDK pulls in C++
# components (Tier 2f gotcha).
project(rp2350-device-freertos C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

# FreeRTOSConfig.h carrier (this directory). The kernel port links it.
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# FreeRTOS-Kernel (RP2040/RP2350 SMP-capable port; single-core selected in config).
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# midi2 C99 core: catalog uses midi2_msg.h (header-only); the MIDI-CI responder
# and inbound processing compile the modular core sources directly.
set(MIDI2_CORE "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

add_executable(rp2350-device-freertos
    src/main.c
    src/pipeline.c
    src/catalog.c
    src/ci_responder.c
    src/usb_descriptors.c
    src/freertos_hooks.c
    ${MIDI2_CORE}/midi2_proc.c
    ${MIDI2_CORE}/midi2_dispatch.c
    ${MIDI2_CORE}/midi2_conv.c
    ${MIDI2_CORE}/midi2_ci.c
    ${MIDI2_CORE}/midi2_ci_dispatch.c
)
target_include_directories(rp2350-device-freertos PRIVATE
    src
    ${CMAKE_CURRENT_SOURCE_DIR}    # FreeRTOSConfig.h
    ${MIDI2_CORE}
)
target_link_libraries(rp2350-device-freertos
    pico_stdlib
    pico_rand              # get_rand_32 for the MIDI-CI MUID
    tinyusb_device
    tinyusb_board
    FreeRTOS-Kernel
)

# USB is dedicated to MIDI 2.0: no USB CDC stdio. UART on GP0/GP1 @ 115200.
pico_enable_stdio_usb(rp2350-device-freertos 0)
pico_enable_stdio_uart(rp2350-device-freertos 1)

pico_add_extra_outputs(rp2350-device-freertos)
