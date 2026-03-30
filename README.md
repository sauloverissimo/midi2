# midi2

[![CI](https://github.com/sauloverissimo/midi2/actions/workflows/ci.yml/badge.svg)](https://github.com/sauloverissimo/midi2/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![C99](https://img.shields.io/badge/C-C99-blue.svg)]()
[![Tests](https://img.shields.io/badge/tests-151%20passing-brightgreen.svg)]()

Portable MIDI 2.0 UMP library. C99, zero dependencies, zero allocation.

## What it does

midi2 builds, parses, dispatches, and processes [UMP](https://www.midi.org/specifications/midi-2-0-specifications) (Universal MIDI Packet) messages per the M2-104-UM v1.1.2 specification. It handles value scaling, typed message dispatch, SysEx reassembly, group filtering, MIDI-CI responses, and MIDI 1.0 byte stream conversion.

It sits between the USB stack and your platform wrapper. The layers below (TinyUSB, hardware) and above (ESP32, Arduino, your application) are not its concern.

## Quick start

```c
#include "midi2_msg.h"

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
#include "midi2_dispatch.h"

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

## Modules

| Module | Files | State | What it does |
|--------|-------|-------|-------------|
| **midi2_msg** | `.h` | Stateless | UMP construction, parsing, value scaling. All message types. |
| **midi2_dispatch** | `.h` + `.c` | Caller-allocated | Typed message dispatch with 42 granular callbacks. |
| **midi2_proc** | `.h` + `.c` | Caller-allocated | Group filtering, SysEx7/8 reassembly, group remap. |
| **midi2_ci** | `.h` + `.c` | Caller-allocated | MIDI-CI responder: Discovery, Profiles, Property Exchange. |
| **midi2_conv** | `.h` + `.c` | Caller-allocated | MIDI 1.0 byte stream to UMP (Running Status, SysEx). |

Use only what you need. `midi2_msg` alone is a header include with zero overhead.

## Message types supported

All message types defined in the UMP specification (M2-104-UM v1.1.2):

- **Utility (MT 0x0):** NOOP, JR Clock, JR Timestamp, Delta Clockstamp TPQ/DC
- **System (MT 0x1):** Timing Clock, Start, Stop, Continue, Song Position, Song Select, Tune Request, Active Sensing, Reset
- **MIDI 1.0 Channel Voice (MT 0x2):** Note On/Off, CC, Program Change, Pressure, Pitch Bend
- **SysEx7 (MT 0x3):** Construction, fragmentation, and reassembly
- **MIDI 2.0 Channel Voice (MT 0x4):** Note On/Off, CC, Program Change, Pitch Bend, Channel/Poly Pressure, RPN, NRPN, Relative RPN/NRPN, Registered/Assignable Per-Note Controllers, Per-Note Pitch Bend, Per-Note Management
- **Data 128-bit (MT 0x5):** SysEx8, Mixed Data Set (Header + Payload)
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

151 tests across 5 modules, zero warnings with `-Wall -Wextra -Wpedantic`.

Tested on: gcc (Linux), clang (Linux/macOS), MSVC (Windows), ARM Cortex-M (cross-compile).

## Integration

midi2 is designed to be vendored (copied into your project). Drop the `src/` files into your build:

**Minimal (just message construction):**
- Add `midi2_msg.h` to your include path. Done. Header-only.

**With typed dispatch:**
- Add `midi2_dispatch.h`, `midi2_dispatch.c`, `midi2_msg.h`
- Compile `midi2_dispatch.c` with your project

**With processor and MIDI-CI:**
- Add `midi2_proc.h/.c`, `midi2_ci.h/.c`

**With byte stream conversion:**
- Also add `midi2_conv.h/.c`

**PlatformIO:**
- `library.json` is included for direct use with PlatformIO.

## Architecture

```
Layer 1: TinyUSB          hardware -> raw UMP words
Layer 2: midi2            words -> parsed music data (THIS library)
Layer 3: Platform          ESP32MIDI / Adafruit / Teensy (C++ wrapper)
Layer 4: Sketch            user application
```

midi2 is layer 2: portable C infrastructure consumed by platform wrappers. The end user never includes `midi2.h` directly -- they include `ESP32MIDI.h` or similar.

## Design

- **C99** -- any compiler, any platform
- **Zero allocation** -- no malloc, no free, no hidden heap usage
- **Caller owns all memory** -- buffers and arrays passed at init
- **Stateless where possible** -- midi2_msg is 100% stateless
- **Error codes, not silent failures** -- functions report what happened
- **Portable** -- only stdint.h, stdbool.h, string.h. Runs on AVR, ARM, x86, RISC-V
- **Spec-complete** -- covers 100% of M2-104-UM v1.1.2 message types

## License

MIT
