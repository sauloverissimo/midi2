# [midi2](..) examples

Pedagogical sketches and flash-ready board recipes that exercise the midi2 C99 API. Pedagogical examples teach individual surfaces of the library; recipes are full applications with their own README, hardware notes, and validation log.

## Pedagogical sketches

Single-file demos that introduce one or two API surfaces on a host or microcontroller. Compile against `dist/midi2.h` + `dist/midi2.c`, or the modular `src/midi2_*` set when finer control is needed.

| Example | Language | Build | What it demonstrates |
|---------|----------|-------|----------------------|
| [`basic_usage.c`](basic_usage.c) | C99 | `gcc -std=c99 -I../src basic_usage.c ../src/midi2_*.c -o basic_usage` | UMP construction, value scaling, typed dispatch, `midi2_proc` chained into dispatch, MIDI-CI Discovery auto-reply |
| [`BasicUsage/`](BasicUsage) | Arduino | Arduino IDE / arduino-cli | Same four surfaces in an `.ino` sketch. Prints decoded UMP over `Serial`. Validated on Teensy 4.x and host g++ via the same source tree |
| [`CIDiscovery/`](CIDiscovery) | Arduino | Arduino IDE / arduino-cli | Standalone MIDI-CI Discovery responder. Registers one custom Profile, feeds a Discovery Inquiry, dumps the auto-reply UMP words over `Serial`. No USB transport required |

## Hardware recipes

Flash-ready applications targeting a specific board. Each recipe ships its own README with build, flash, validation, and a per-scene UMP log.

| Recipe | Board | MCU | Transport | What it ships |
|--------|-------|-----|-----------|---------------|
| [`zephyr/rpi_pico_device_midi2_showcase/`](zephyr/rpi_pico_device_midi2_showcase) | Raspberry Pi Pico | RP2040 (Cortex-M0+) | Zephyr `usbd_midi2` | Full-spec USB MIDI 2.0 device, 14-scene UMP showcase (Flex Data, Per-Note expression, RPN/NRPN, Note Attribute, multi-fragment SysEx7, System Common / Real-Time, Delta Clockstamp, JR Heartbeat) + MIDI-CI responder with 1 Profile and 3 Property Exchange properties. Flash 67 KB, RAM 14 KB on RP2040. Hardware validated 2026-05-27 |

## What lives elsewhere

- **C++ wrapper recipes** (m2device, m2host, m2bridge, m2ci) under [`midi2cpp/examples/`](https://github.com/sauloverissimo/midi2cpp/tree/main/examples): 23 boards across Arduino, PlatformIO, Pico SDK, native TinyUSB CMake and ESP-IDF build paths.
- **Python and MicroPython** integrations: see the [`midi2`](https://github.com/sauloverissimo/midi2) ecosystem README.

## License

MIT. Same as the parent [`midi2`](../) library.
