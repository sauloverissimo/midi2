# 🎹 midi2

[![CI](https://github.com/sauloverissimo/midi2/actions/workflows/ci.yml/badge.svg)](https://github.com/sauloverissimo/midi2/actions/workflows/ci.yml)
[![Tests](https://img.shields.io/badge/tests-252%20passing-brightgreen.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![C99](https://img.shields.io/badge/standard-C99-blue.svg)]()
[![Zero Alloc](https://img.shields.io/badge/allocation-zero-orange.svg)]()
[![UMP 100%](https://img.shields.io/badge/UMP_M2--104-100%25-blueviolet.svg)]()
[![CI 100%](https://img.shields.io/badge/MIDI--CI_M2--101-100%25-blueviolet.svg)]()
[![Platforms](https://img.shields.io/badge/platforms-11_targets-informational.svg)]()
[![Sponsor](https://img.shields.io/badge/sponsor-%E2%9D%A4-pink.svg)](https://github.com/sponsors/sauloverissimo)

🎹 Portable MIDI 2.0 UMP library. C99, zero dependencies, zero allocation. Runs from AVR to desktop.

## What it does

midi2 builds, parses, dispatches, and processes [UMP](https://www.midi.org/specifications/midi-2-0-specifications) (Universal MIDI Packet) messages per the M2-104-UM v1.1.2 specification. It handles value scaling, typed message dispatch, SysEx reassembly, group filtering, MIDI-CI responses, and MIDI 1.0 byte stream conversion.

It sits between the transport layer and your platform wrapper. The layers below (TinyUSB, Teensy USB, PIO-USB, BLE MIDI, Network MIDI, Serial DIN) and above (ESP32, Teensy, Daisy, RP2040, Arduino, Zephyr, bare-metal) are not its concern.

## Quick start

```c
#include "midi2.h"

/* Build a Note On: group 0, channel 0, note 60, velocity 75% */
uint32_t w[2];
midi2_msg_note_on(w, 0, 0, 60, 0xC000, 0);

/* Parse it back */
uint8_t note = midi2_msg_get_note(w);       /* 60 */
uint16_t vel = midi2_msg_get_velocity(w);   /* 0xC000 */

/* Scale a legacy 7-bit value to 16-bit */
uint16_t vel16 = midi2_msg_scale_up_7to16(127);  /* 65535 */
```

### With dispatch (typed callbacks)

```c
#include "midi2.h"

void my_note_on(uint8_t group, uint8_t channel, uint8_t note,
                uint16_t velocity, uint8_t attr_type,
                uint16_t attr_data, void *ctx) {
    printf("Note On: ch=%d note=%d vel=%u\n", channel, note, velocity);
}

void my_cc(uint8_t group, uint8_t channel, uint8_t index,
           uint32_t value, void *ctx) {
    printf("CC: ch=%d index=%d value=%u\n", channel, index, value);
}

midi2_dispatch dp;
midi2_dispatch_init(&dp);
dp.on_note_on = my_note_on;
dp.on_cc = my_cc;

/* Feed a raw UMP -- dispatch calls the right callback */
midi2_dispatch_feed(w, 2, &dp);
```

## Amalgamated

| File | What you get |
|------|-------------|
| **`midi2.h`** + **`midi2.c`** | Everything. All 7 modules. Drop both files into your project, done. |

## Modules

When you need finer control, include individual modules instead:

| Module | Files | State | What it does |
|--------|-------|-------|-------------|
| **midi2_msg** | `.h` | Stateless | UMP construction, parsing, value scaling. All message types. |
| **midi2_dispatch** | `.h` + `.c` | Caller-allocated | Typed UMP dispatch with 42 granular callbacks. Optional MT 0x2->0x4 upscale. |
| **midi2_proc** | `.h` + `.c` | Caller-allocated | Group filtering, SysEx7/8 reassembly, group remap. |
| **midi2_conv** | `.h` + `.c` | Caller-allocated | MIDI 1.0 byte stream to UMP (Running Status, streaming SysEx, MT 0x2->0x4 translation). |
| **midi2_ci_msg** | `.h` | Stateless | MIDI-CI message construction and parsing. All 31 messages. |
| **midi2_ci_dispatch** | `.h` + `.c` | Caller-allocated | Typed CI dispatch with 33 granular callbacks. |
| **midi2_ci** | `.h` + `.c` | Caller-allocated | Convenience CI responder (uses ci_msg + ci_dispatch). |

Use only what you need. `midi2_msg` alone is a header include with zero overhead. See [Integration](#integration) for setup details.

## Message types supported

All message types defined in the UMP specification (M2-104-UM v1.1.2):

- **Utility (MT 0x0):** NOOP, JR Clock, JR Timestamp, Delta Clockstamp TPQ/DC
- **System (MT 0x1):** Timing Clock, Start, Stop, Continue, Song Position, Song Select, Tune Request, Active Sensing, Reset
- **MIDI 1.0 Channel Voice (MT 0x2):** Note On/Off, CC, Program Change, Pressure, Pitch Bend
- **SysEx7 (MT 0x3):** Construction, fragmentation, reassembly, and streaming send
- **MIDI 2.0 Channel Voice (MT 0x4):** Note On/Off, CC, Program Change, Pitch Bend, Channel/Poly Pressure, RPN, NRPN, Relative RPN/NRPN, Registered/Assignable Per-Note Controllers, Per-Note Pitch Bend, Per-Note Management
- **SysEx8 Data 128-bit (MT 0x5):** SysEx8 construction, fragmentation, and reassembly. Mixed Data Set (Header + Payload)
- **Flex Data (MT 0xD):** Tempo, Time Signature, Metronome, Key Signature, Chord Name, Metadata Text (13 subtypes), Performance Text (lyrics, ruby, language)
- **UMP Stream (MT 0xF):** Endpoint Discovery/Info, Device Identity, Endpoint Name, Product Instance ID, Stream Config, Function Block Discovery/Info/Name, Start/End of Clip

## Caller-provided storage

midi2 never allocates memory. You provide the buffers, you control the size.

```c
/* You decide the buffer size */
uint8_t sysex_buf[256];
midi2_proc_state proc;
midi2_proc_init(&proc, sysex_buf, sizeof(sysex_buf), NULL, 0);

/* You decide the capacity */
uint8_t profiles[4][5];
midi2_ci_property props[2];
midi2_ci_state ci;
midi2_ci_init(&ci, seed, profiles, 4, props, 2);
```

On a microcontroller with 2KB RAM, use small buffers. On a desktop, use large ones. Same library, same API.

## Error handling

Functions that can fail return error codes:

```c
int result = midi2_ci_add_profile(&ci, profile_id);
if (result == MIDI2_CI_ERR_FULL) {
    /* storage is full -- caller decides what to do */
}
```

## Build and test

```bash
make test          # gcc by default
make CC=clang test # or clang
```

252 tests across 7 modules, zero warnings with `-Wall -Wextra -Wpedantic`.

CI runs 11 jobs on every push:

| Target | Type | What it verifies |
|--------|------|-----------------|
| gcc (Linux x64) | Compile + run | Primary compiler, all 252 tests |
| clang (Linux x64) | Compile + run | Catches different warnings |
| Apple clang (macOS) | Compile + run | macOS / Darwin compatibility |
| MSVC (Windows) | Compile + run | Microsoft compiler, C11 mode |
| gcc 32-bit (Linux x86) | Compile + run | No 64-bit assumptions |
| gcc + ASan + UBSan | Compile + run | Memory safety, undefined behavior |
| ARM Cortex-M4 | Cross-compile | ESP32, STM32, nRF52, SAMD, Daisy |
| AArch64 (Cortex-A) | Cross-compile | Raspberry Pi, 64-bit ARM |
| RISC-V 64 | Cross-compile | ESP32-C3/C6, future platforms |
| ESP32 (component) | Cross-compile | ESP-IDF component compatibility |
| AVR ATmega328P | Cross-compile | Arduino Uno/Nano (header-only) |

### Hardware validation

| Platform | MCU | Device | Host | Transport |
|----------|-----|--------|------|-----------|
| 🌟 T-Display S3 (ESP-IDF) | ESP32-S3 | ✅ | ✅ | TinyUSB, ESP-NOW, BLE, UART, USB-OTG |
| T-Display S3 Amoled (ESP-IDF) | ESP32-S3 | ✅ | ✅ | TinyUSB, ESP-NOW, BLE, UART, USB-OTG |
| 🌟 Waveshare ESP32-P4 (ESP-IDF) | ESP32-P4 | ✅ | ✅ | TinyUSB, BLE |
| T-PicoC3 (SDK-Pico) | RP2040 | ✅ | -- | TinyUSB |
| T-PicoC3 (ESP-IDF) | ESP32-C3 | ✅ | -- | TinyUSB |
| ESP32-S3 (Arduino) | ESP32-S3 | ✅ | ✅ | TinyUSB, BLE |
| 🌟 Teensy 4.1 | i.MX RT1062 | ✅ | ✅ | Native USB |
| Raspberry Pi Pico | RP2040 | ✅ | -- | TinyUSB |
| Waveshare RP2040-Zero | RP2040 | ✅ | -- | TinyUSB |
| Raspberry Pi Pico 2 | RP2350 | ✅ | ✅ | TinyUSB, PIO-USB |
| 🌟 Daisy Seed | STM32H750 | ✅ | -- | STM32 HAL USB |
| ESP32-C6 | ESP32-C6 | 🔜 | -- | TinyUSB, BLE |
| Nordic nRF52840 | nRF52840 | 🔜 | -- | TinyUSB, BLE |
| 🌟 Adafruit Feather RP2040 Host | RP2040 | 🔜 | -- | TinyUSB, BLE |
| Xiao SAMD21 | SAMD21 | 🔜 | -- | TinyUSB |
| Xiao Renesas RA4M1 | RA4M1 | 🔜 | -- | TinyUSB |
| Windows | x86_64 | ✅ MSVC | -- | -- |
| Linux | x86_64 | ✅ gcc/clang | -- | -- |

## Integration

midi2 is designed to be vendored (copied into your project). There are two ways to integrate:

### Pair (recommended for vendoring)

Copy `src/midi2.h` + `src/midi2.c` into your project. Your build system compiles `midi2.c` alongside your code. No special defines needed.

```c
// In your code -- just include the header
#include "midi2.h"
```

This is the simplest integration path and works naturally with platforms that auto-compile `.c` files (Arduino, Teensy, PlatformIO, ESP-IDF components).

### Single-header (alternative)

If you prefer a single file, use only `src/midi2.h` with the stb-style pattern:

```c
// In any file -- declarations + inline functions
#include "midi2.h"

// In exactly ONE .c file -- generates the implementation
#define MIDI2_IMPLEMENTATION
#include "midi2.h"
```

Both files are auto-generated from the multi-module sources via `tools/amalgamate.sh`.

### Multi-module (for development or selective inclusion)

Drop individual `src/` files into your build for finer control:

**Minimal (just message construction):**
- Add `midi2_msg.h` to your include path. Done. Header-only.

**With typed dispatch:**
- Add `midi2_dispatch.h`, `midi2_dispatch.c`, `midi2_msg.h`
- Compile `midi2_dispatch.c` with your project

**With processor and MIDI-CI:**
- Add `midi2_proc.h/.c`, `midi2_ci.h/.c`

**With byte stream conversion:**
- Also add `midi2_conv.h/.c`

### PlatformIO

`library.json` is included for direct use with PlatformIO.

## Architecture

midi2 is layer 2 in a 4-layer MIDI 2.0 stack:

```
Layer 1: Transport     TinyUSB / USB Host / BLE / Network / Serial / PIO-USB
Layer 2: midi2         THIS LIBRARY -- portable C infrastructure
Layer 3: Platform      ESP32 / Teensy / Daisy / Adafruit / RP2040 wrapper
Layer 4: Application   Your synth, controller, DAW plugin, or embedded device
```

### How the layers work

**Layer 1 (Transport)** moves raw UMP words between hardware and software. It knows nothing about music -- just bytes in, bytes out. Examples: TinyUSB MIDI 2.0 class driver (ESP32-S3, RP2040, STM32), Teensy native USB, ESP-IDF USB Host, PIO-USB (RP2040/RP2350), BLE MIDI service, Network MIDI 2.0 UDP, classic Serial DIN-5/TRS.

**Layer 2 (midi2)** turns raw words into structured music data. It constructs, parses, dispatches, and processes UMP and MIDI-CI messages. It is pure C, portable, and has zero opinion about hardware or application logic. This is where you are.

**Layer 3 (Platform)** bridges midi2 to a specific hardware ecosystem. It provides the developer-friendly API (`onNoteOn`, `onCC`, `setProfile`) and handles platform-specific concerns. Each platform vendorizes midi2 as a dependency. Examples:

| Platform | MCU / Environment | Transport | Notes |
|----------|-------------------|-----------|-------|
| ESP32 | ESP32-S3, ESP32-P4 | TinyUSB, ESP-NOW, BLE, UART, USB-OTG | FreeRTOS tasks, ESP-IDF events |
| Teensy | Teensy 4.x (NXP i.MX RT) | Native USB | USB dispatch built into core |
| Daisy | Daisy Seed (STM32H7) | STM32 HAL USB, SAI | libDaisy integration |
| Adafruit | RP2040, nRF52, SAMD | TinyUSB, BLE | Arduino + Adafruit_TinyUSB |
| RP2040/RP2350 | RP2040, RP2350 | TinyUSB, PIO-USB | Host + Device, dual port |
| Nordic | nRF52840 | TinyUSB, BLE | Zephyr or bare-metal |
| Desktop | Linux, macOS, Windows | ALSA, CoreMIDI, WinRT | Testing, DAW plugins, tools |
| XMOS | xcore.ai | xCORE USB | Real-time multi-core |
| Zephyr | Any Zephyr-supported MCU | USB, BLE | RTOS integration |
| Bare-metal | Any C99 target | Any | No OS, no dependencies |

**Layer 4 (Application)** is the user's code. It only sees the platform API -- never midi2 directly. A synth, a MIDI controller, a lighting bridge, a DAW plugin.

### What each layer owns

| Responsibility | Layer |
|---------------|-------|
| USB descriptors, endpoints, DMA | Transport (1) |
| UMP construction, parsing, scaling | **midi2 (2)** |
| Typed dispatch (42 UMP + 33 CI callbacks) | **midi2 (2)** |
| SysEx reassembly, group filtering | **midi2 (2)** |
| CI message construction and parsing | **midi2 (2)** |
| MIDI 1.0 byte stream to UMP conversion | **midi2 (2)** |
| CI convenience responder (optional) | **midi2 (2)** |
| Arduino-friendly callbacks (`onNoteOn`) | Platform (3) |
| Profile behavior (GM2, MPE, etc.) | Platform (3) or Sketch (4) |
| PE JSON schema interpretation | Platform (3) or Sketch (4) |
| MUID peer management, CI session state | Platform (3) |
| FreeRTOS tasks, event queues | Platform (3) |
| Musical application logic | Sketch (4) |

### Platform integration model

midi2 is designed to be vendorized (copied) into platform wrappers. Each platform decides which midi2 modules to include:

| Platform | Typical midi2 modules used |
|----------|---------------------------|
| **ESP32** (TinyUSB) | All -- transport delivers raw words, midi2 does everything |
| **Teensy** (native USB) | msg + ci_msg + ci_dispatch -- Teensy USB already has typed dispatch |
| **Adafruit** (TinyUSB Arduino) | All -- similar to ESP32 |
| **Daisy** (STM32/TinyUSB) | All -- similar to ESP32 |
| **RP2040/RP2350** (TinyUSB/PIO-USB) | All -- similar to ESP32 |

Platforms that already have typed dispatch at the transport level (Teensy) use fewer midi2 modules. Platforms that receive raw UMP words (TinyUSB-based) use the full stack.

### Optional modules

Not every project needs every module. midi2 is designed for incremental adoption:

```
midi2_msg.h          Always needed. Header-only, zero cost.
  |
  +-- midi2_dispatch   Add if you want typed UMP callbacks.
  +-- midi2_proc       Add if you need SysEx reassembly or group filtering.
  +-- midi2_conv       Add if you receive MIDI 1.0 byte streams (Serial/DIN).
  +-- midi2_ci_msg     Add if you handle MIDI-CI messages.
       |
       +-- midi2_ci_dispatch  Add if you want typed CI callbacks.
       +-- midi2_ci           Add if you want a convenience CI responder.
```

## Spec coverage

midi2 covers the two core MIDI 2.0 specifications that define data formats and protocols:

| Spec | Document | Coverage |
|------|----------|----------|
| **M2-104-UM v1.1.2** | UMP Format & MIDI 2.0 Protocol | 100% -- all message types (MT 0x0-0xF) |
| **M2-101-UM v1.2** | MIDI-CI | 100% -- all 31 message types, 4 categories |
| **M2-115-U v1.0.2** | Bit Scaling and Resolution | 100% -- round-trip safe value scaling |
| **M2-116-U v1.0** | MIDI Clip File | Partial -- Delta Clockstamp, Start/End of Clip |

### What midi2 deliberately does NOT cover

These are responsibilities of other layers, not portable C infrastructure:

- **Transports** (USB-MIDI 2.0, Network MIDI 2.0 UDP, BLE MIDI) -- handled by transport libraries like TinyUSB
- **PE Resource schemas** (M2-105 through M2-112, M2-117) -- JSON content for Property Exchange. midi2 provides the PE message transport; the JSON schemas are application-level
- **Profile behavior** (M2-113, M2-118 through M2-125) -- how a device behaves when a Profile is enabled. midi2 provides the Profile negotiation messages; the behavior is application-level
- **JSON parsing** -- Property Exchange headers use JSON. Parsing JSON is out of scope for a zero-allocation embedded library. Use a JSON parser of your choice at the platform layer

## Design

- **C99** -- any compiler, any platform
- **Zero allocation** -- no malloc, no free, no hidden heap usage
- **Caller owns all memory** -- buffers and arrays passed at init
- **Stateless where possible** -- midi2_msg is 100% stateless
- **Error codes, not silent failures** -- functions report what happened
- **Portable** -- only stdint.h, stdbool.h, string.h. Runs on AVR, ARM, x86, RISC-V
- **Spec-complete** -- covers 100% of core MIDI 2.0 data formats and protocols

## Sponsor

If midi2 is useful to your project, consider [sponsoring](https://github.com/sponsors/sauloverissimo) the development. Sponsorship helps fund hardware for testing, spec access, and continued maintenance across platforms.

## License

MIT -- free for commercial and open-source use.
