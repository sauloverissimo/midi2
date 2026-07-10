# midi2

## Portable MIDI 2.0 infrastructure. Build MIDI 2.0 platforms in any language, for any framework.

![midi2](https://raw.githubusercontent.com/sauloverissimo/midi2/main/logo_midi2.png)

*C99, portable, zero-allocation, MIT.* From 8-bit MCUs to 64-bit hosts.

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![C99](https://img.shields.io/badge/standard-C99-blue.svg)]()
[![Zero Alloc](https://img.shields.io/badge/allocation-zero-orange.svg)]()
[![MIDI 2.0](https://img.shields.io/badge/MIDI-2.0-blueviolet.svg)](https://midi.org/specifications/midi-2-0-specifications)
[![Compliant with MIDI 2.0 Workbench](https://img.shields.io/badge/MIDI%202.0%20Workbench-compliant-0d9488?labelColor=17151f)](https://github.com/midi2-dev/MIDI2.0Workbench)
[![ESP Component Registry](https://components.espressif.com/components/sauloverissimo/midi2/badge.svg)](https://components.espressif.com/components/sauloverissimo/midi2)
[![Platform](https://img.shields.io/badge/Platform-Arduino%20%7C%20PlatformIO%20%7C%20ESP--IDF%20%7C%20Zephyr%20%7C%20FreeRTOS%20%7C%20CMake-E8B838.svg)](#integration)
[![Sponsor](https://img.shields.io/badge/sponsor-%E2%9D%A4-pink.svg)](https://github.com/sponsors/sauloverissimo)

---

## The library

midi2 is a small piece of infrastructure for boards that already have too much to do. Sound chips, displays, USB stacks, sensors, network. MIDI 2.0 arrives at the transport layer as 32-bit words; midi2 turns those words into structured events and stays out of the way the rest of the time.

The library implements 100% of the UMP format (M2-104-UM v1.1.2) and 100% of MIDI-CI (M2-101-UM v1.2). The application owns memory and decides which modules to link. The transport (USB, BLE, network, serial) lives somewhere else, by design.

## Wrappers

midi2 is the C99 core. Higher-level wrappers build on it and add language-specific ergonomics for each platform target.

The first published wrapper is [**midi2cpp**](https://github.com/sauloverissimo/midi2cpp): a C++17 Arduino-friendly wrapper with `m2device`, `m2host`, `m2bridge` and `m2ci` classes, distributed through the Arduino Library Manager, the PlatformIO Registry and the [ESP Component Registry](https://components.espressif.com/components/sauloverissimo/midi2cpp). It ships **20 platform recipes** under [`midi2cpp/examples/`](https://github.com/sauloverissimo/midi2cpp/tree/main/examples), one per role (device, host, bridge), covering RP2040, RP2350, ESP32-S3, ESP32-P4, ESP32-C6, nRF52840 and SAMD21 boards via Pico SDK / TinyUSB native CMake / ESP-IDF / PlatformIO build paths.

## Contents

- [midi2](#midi2)
  - [The library](#the-library)
  - [Wrappers](#wrappers)
  - [Contents](#contents)
  - [Spec coverage](#spec-coverage)
  - [Quick start](#quick-start)
  - [Modules](#modules)
  - [Memory](#memory)
  - [Error handling](#error-handling)
  - [Continuous integration](#continuous-integration)
  - [Build and test locally](#build-and-test-locally)
  - [Integration](#integration)
    - [Arduino IDE / arduino-cli](#arduino-ide--arduino-cli)
    - [PlatformIO](#platformio)
    - [ESP-IDF (Component Manager)](#esp-idf-component-manager)
    - [Zephyr (west module)](#zephyr-west-module)
    - [Single-header (stb-style)](#single-header-stb-style)
    - [Manual vendor (amalgam pair)](#manual-vendor-amalgam-pair)
    - [Module by module](#module-by-module)
  - [Examples](#examples)
  - [Architecture](#architecture)
  - [Design goals](#design-goals)
  - [What this library is not](#what-this-library-is-not)
  - [Sponsor](#sponsor)
  - [About](#about)
  - [Specifications and trademarks](#specifications-and-trademarks)
  - [License](#license)

## Spec coverage

| Spec | Document | Coverage |
|------|----------|----------|
| **M2-104-UM v1.1.2** | UMP Format and MIDI 2.0 Protocol | 100% (all message types MT 0x0 through 0xF) |
| **M2-101-UM v1.2** | MIDI-CI | 100% (all 31 messages, 4 categories, Appendix E responder) |
| **M2-115-U v1.0.2** | Bit Scaling and Resolution | 100% (round-trip safe 7/14/16/32 bit) |
| **M2-116-U v1.0** | MIDI Clip File | Partial (Delta Clockstamp, Start/End of Clip) |

## Quick start

```c
#include "midi2.h"

uint32_t w[2];
midi2_msg_note_on(w, /*group*/ 0, /*channel*/ 0,
                  /*note*/ 60, /*velocity*/ 0xC000,
                  /*attr_type*/ 0, /*attr_data*/ 0);

uint8_t  note = midi2_msg_get_note(w);
uint16_t vel  = midi2_msg_get_velocity(w);

uint16_t vel16 = midi2_msg_scale_up_7to16(127);
```

With typed dispatch:

```c
#include "midi2.h"

void on_note_on(uint8_t group, uint8_t channel, uint8_t note,
                uint16_t velocity, uint8_t attr_type,
                uint16_t attr_data, void *ctx) {
    /* application logic */
}

midi2_dispatch dp;
midi2_dispatch_init(&dp);
dp.on_note_on = on_note_on;
midi2_dispatch_feed(w, 2, &dp);
```

## Modules

The amalgam (`dist/midi2.h` + `dist/midi2.c`) is the recommended path. For finer control, modules can be picked individually:

| Module | Files | State | Purpose |
|--------|-------|-------|---------|
| `midi2_msg` | `.h` | Stateless | UMP construction, parsing, value scaling. All MTs. MT 0x2 / MT 0x4 protocol translation. USB MIDI 1.0 cable event to UMP. |
| `midi2_dispatch` | `.h` `.c` | Caller-allocated | Typed UMP dispatch (49 callbacks). Optional MT 0x2 to MT 0x4 upscale. |
| `midi2_proc` | `.h` `.c` | Caller-allocated | Group filtering and remap, SysEx7/8 reassembly, UMP Stream and SysEx8 fragmenting senders. |
| `midi2_conv` | `.h` `.c` | Caller-allocated | MIDI 1.0 byte stream to UMP (Running Status, streaming SysEx, MT 0x2 to MT 0x4 translation). |
| `midi2_ci_msg` | `.h` | Stateless | MIDI-CI message construction and parsing. All 32 messages. |
| `midi2_ci_dispatch` | `.h` `.c` | Caller-allocated | Typed CI dispatch (33 callbacks). |
| `midi2_ci` | `.h` `.c` | Caller-allocated | Convenience CI responder: Discovery, Profiles, PE (Subscribe/Notify), Process Inquiry, MUID collision detection, optional NAK-on-unknown. Appendix E complete. |

`midi2_msg` alone is a header include with zero overhead.

## Memory

Memory belongs to the application. midi2 does not call `malloc`, does not keep a hidden heap, and does not bake buffer sizes into headers. A 16 KB board declares small buffers. A desktop tool declares large ones. Same source, same API.

```c
uint8_t sysex_buf[256];
midi2_proc_state proc;
midi2_proc_init(&proc, sysex_buf, sizeof(sysex_buf), NULL, 0);

uint8_t profiles[4][5];
midi2_ci_property props[2];
midi2_ci_state ci;
midi2_ci_init(&ci, seed, profiles, 4, props, 2);
```

## Error handling

Functions that can fail return error codes. There is no global errno, no asserts that fire in release builds, no silent truncation.

```c
int rc = midi2_ci_add_profile(&ci, profile_id);
if (rc == MIDI2_CI_ERR_FULL) {
    /* the application decides what to do */
}
```

## Continuous integration

350 assertions across 8 suites compile clean with `-Wall -Wextra -Wpedantic`. CI runs 12 jobs on every push:

| Target | Type |
|--------|------|
| gcc Linux x64 | Compile and run |
| clang Linux x64 | Compile and run |
| Apple clang macOS | Compile and run |
| MSVC Windows | Compile and run |
| gcc 32-bit Linux x86 | Compile and run |
| gcc + ASan + UBSan | Compile and run |
| ARM Cortex-M4 | Cross-compile |
| AArch64 Cortex-A | Cross-compile |
| RISC-V 64 | Cross-compile |
| ESP32 (ESP-IDF component) | Cross-compile |
| AVR ATmega328P | Cross-compile (header-only) |
| Zephyr (native_sim/native/64) | Compile and run |

## Build and test locally

```bash
make test
make CC=clang test
```

## Integration

midi2 ships in four shapes. Pick the one that matches your build flow.

### Arduino IDE / arduino-cli

Install via Library Manager (search `midi2`, click Install) or manually drop the repo into `~/Arduino/libraries/midi2/`. The `library.properties` declares the library; Arduino IDE compiles the modular `src/*.c` files automatically. Sketches use the umbrella header:

```cpp
#include <midi2.h>
```

A reference sketch under `examples/teensy-device-midi2/` appears in the IDE's File > Examples menu after install: a complete USB MIDI 2.0 device for Teensy 4.x, validated against the official MIDI 2.0 Workbench.

### PlatformIO

`platformio.ini`:

```ini
lib_deps = sauloverissimo/midi2 @ ^0.7.0
```

Library Manager pulls the same `src/` modular layout via `library.json` (`srcDir = src`).

### ESP-IDF (Component Manager)

Published on the [ESP Component Registry](https://components.espressif.com/components/sauloverissimo/midi2). Add to your project's `main/idf_component.yml`:

```yaml
dependencies:
  idf: ">=5.0"
  sauloverissimo/midi2: ">=0.7.0"
```

`idf.py reconfigure` drops the component into `managed_components/midi2/`. The `if(ESP_PLATFORM)` gate in `CMakeLists.txt` routes ESP-IDF builds to `idf_component_register` with the modular `src/midi2_*.c` set, so the same source serves IDF, Arduino, PlatformIO, and native CMake without forks.

### Zephyr (west module)

midi2 ships a `zephyr/` module manifest. Add it to your application's `west.yml`:

```yaml
manifest:
  projects:
    - name: midi2
      url: https://github.com/sauloverissimo/midi2
      revision: v0.7.0
      path: modules/lib/midi2
```

Then enable in `prj.conf`:

```
CONFIG_MIDI2=y
```

`west build` picks up the module via `zephyr/module.yml` and compiles the modular `src/midi2_*.c` set under Zephyr's CMake. midi2 stays at the message layer; pair with `CONFIG_USBD_MIDI2` for USB UMP I/O, or with the Network MIDI 2.0 stack for IP-based transport.

A ready-to-flash Zephyr example for the Raspberry Pi Pico (RP2040) lives under [`examples/rpi-pico-device-zephyr/`](examples/rpi-pico-device-zephyr): full-spec USB MIDI 2.0 device with a 14-scene UMP showcase plus a MIDI-CI responder.

### Single-header (stb-style)

Drop `dist/midi2.h` only. In exactly one `.c` file:

```c
#define MIDI2_IMPLEMENTATION
#include "midi2.h"
```

### Manual vendor (amalgam pair)

Download `dist/midi2.h` and `dist/midi2.c` from the release page. Drop both into the tree. Compile `midi2.c` alongside the project.

```c
#include "midi2.h"
```

Works with Arduino, PlatformIO, ESP-IDF, CMake, Make, plain `gcc`.

### Module by module

For finer control, copy individual files from `src/`:

```
midi2_msg.h          Always needed. Header-only.
  +- midi2_dispatch  Typed UMP callbacks.
  +- midi2_proc      SysEx reassembly, group filtering.
  +- midi2_conv      MIDI 1.0 byte stream to UMP.
  +- midi2_ci_msg    MIDI-CI message construction and parsing.
       +- midi2_ci_dispatch  Typed CI callbacks.
       +- midi2_ci           Convenience CI responder.
```

## Examples

Three real USB MIDI 2.0 devices live under [`examples/`](examples), each wiring the same C99 core to a different runtime and USB stack.

- [`examples/teensy-device-midi2/`](examples/teensy-device-midi2): Arduino sketch for Teensy 4.x over the core's native `usbMIDI2` endpoint. A bare `loop()`, no RTOS.
- [`examples/rp2350-device-freertos/`](examples/rp2350-device-freertos): Raspberry Pi Pico 2 (RP2350) on FreeRTOS-Kernel and TinyUSB upstream, with a 58-entry UMP catalog covering every defined message-type category, a MIDI-CI responder, and a stress loopback mode.
- [`examples/rpi-pico-device-zephyr/`](examples/rpi-pico-device-zephyr): Raspberry Pi Pico (RP2040) on Zephyr's native `usbd_midi2` class.

All three are validated end to end against the official [MIDI 2.0 Workbench](https://github.com/midi2-dev/MIDI2.0Workbench) on hardware: MIDI-CI Discovery, Profile Configuration, and Property Exchange answer with zero errors and zero warnings.

## Architecture

midi2: infrastructure layer 2 of a 4-layer MIDI 2.0 stack:

![midi2](https://raw.githubusercontent.com/sauloverissimo/midi2/main/architecture.png)

What each layer owns:

| Concern | Layer |
|---------|-------|
| USB descriptors, endpoints, DMA | Transport |
| UMP construction, parsing, scaling | midi2 |
| Typed dispatch (49 UMP + 33 CI callbacks) | midi2 |
| SysEx reassembly, group filtering | midi2 |
| CI construction and parsing | midi2 |
| MIDI 1.0 byte stream conversion | midi2 |
| CI convenience responder (optional) | midi2 |
| Arduino-style callbacks (`onNoteOn`) | Platform |
| Profile behavior (GM2, MPE) | Platform or App |
| PE JSON schema interpretation | Platform or App |
| FreeRTOS tasks, event queues | Platform |
| Musical application logic | App |

## Design goals

- **Embedded first.** The same source compiles for AVR ATmega328P, Cortex-M0, RISC-V 64, ESP32 Xtensa, and 64-bit desktops. The smallest target dictates the design.
- **Zero allocation.** No `malloc`, no hidden heap, no per-call buffers. The application sizes everything at init.
- **Stateless where possible.** `midi2_msg` keeps no state; the same word array round-trips through any number of helpers without coupling.
- **Three system headers.** `<stdint.h>`, `<stdbool.h>`, `<string.h>`. That is the dependency list.
- **Errors are codes, not exceptions.** Functions report what happened; the application decides.
- **Spec-complete on its own scope.** 100% of UMP. 100% of MIDI-CI. Anything outside those documents lives elsewhere.

## What this library is not

The boundary is drawn so the core stays portable. A few things deliberately do not belong here.

- **Not a transport.** USB-MIDI 2.0, BLE MIDI, Network MIDI 2.0, Serial DIN: each has its own driver. midi2 reads and writes UMP words; the wire is somebody else's responsibility.
- **Not a JSON parser.** Property Exchange headers are JSON, but parsing JSON in a zero-allocation embedded library is a poor fit. The application chooses a JSON parser at the platform layer.
- **Not a Profile implementation.** GM2, MPE, and the M2-118 through M2-125 profiles describe device behavior. midi2 carries the negotiation messages; the behavior lives in the application.
- **Not the fastest UMP library.** Speed was not a primary goal. Correctness, portability, and predictable memory were. On a Cortex-M4 at 168 MHz, midi2 dispatches faster than any USB transport can deliver words anyway.

## Sponsor

You can sponsor midi2 at [GitHub Sponsors](https://github.com/sponsors/sauloverissimo). Sponsorship funds spec access, hardware for cross-platform validation, and continued maintenance.

## About

midi2 is created and maintained by [Saulo Veríssimo](https://github.com/sauloverissimo).

## Specifications and trademarks

The MIDI 2.0 specifications referenced here are copyright of the [MIDI Association](https://midi.org) and available at https://midi.org/midi-2-0.

"MIDI" is a registered trademark of the MIDI Manufacturers Association (now MIDI Association). "MIDI 2.0", "MIDI-CI", and "UMP" are terms defined by the MIDI Association in the public specifications.

## License

MIT. Free for commercial and open-source use.
