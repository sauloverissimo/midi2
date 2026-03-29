# midi2

[![CI](https://github.com/sauloverissimo/midi2/actions/workflows/ci.yml/badge.svg)](https://github.com/sauloverissimo/midi2/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![C99](https://img.shields.io/badge/C-C99-blue.svg)]()
[![Tests](https://img.shields.io/badge/tests-77%20passing-brightgreen.svg)]()

Portable MIDI 2.0 UMP library. C99, zero dependencies, zero allocation.

## What it does

midi2 builds, parses, and processes [UMP](https://www.midi.org/specifications/midi-2-0-specifications) (Universal MIDI Packet) messages. It handles value scaling, SysEx reassembly, group filtering, MIDI-CI responses, and MIDI 1.0 byte stream conversion.

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

## Modules

| Module | Files | State | What it does |
|--------|-------|-------|-------------|
| **midi2_msg** | `.h` | Stateless | UMP construction, parsing, value scaling. All message types. |
| **midi2_proc** | `.h` + `.c` | Caller-allocated | Group filtering, SysEx7 reassembly, group remap. |
| **midi2_ci** | `.h` + `.c` | Caller-allocated | MIDI-CI responder: Discovery, Profiles, Property Exchange. |
| **midi2_conv** | `.h` + `.c` | Caller-allocated | MIDI 1.0 byte stream to UMP (Running Status, SysEx). |

Use only what you need. `midi2_msg` alone is a header include with zero overhead.

## Message types supported

- MIDI 2.0 Channel Voice (MT 0x4): Note On/Off, CC, Program, Pitch Bend, Pressure, RPN/NRPN
- Per-note expression: pitch bend, CC, management per individual note
- System (MT 0x1): timing clock, start, stop, continue, song position, song select
- Flex Data (MT 0xD): tempo, time signature, key signature
- SysEx7 (MT 0x3) and SysEx8 (MT 0x5): construction and reassembly
- Stream (MT 0xF): endpoint discovery/info, device identity, function blocks, stream config
- JR Timestamps (MT 0x0): jitter reduction clock and timestamp
- MIDI 1.0 Channel Voice (MT 0x2): UMP-wrapped legacy messages

## Caller-provided storage

midi2 never allocates memory. You provide the buffers, you control the size.

```c
/* You decide the buffer size */
uint8_t sysex_buf[256];
midi2_proc_state proc;
midi2_proc_init(&proc, sysex_buf, sizeof(sysex_buf));

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

77 tests, zero warnings with `-Wall -Wextra -Wpedantic`.

## Integration

midi2 is designed to be vendored (copied into your project). Drop the `src/` files into your build:

**Minimal (just message construction):**
- Add `midi2_msg.h` to your include path. Done. Header-only.

**With processor and MIDI-CI:**
- Add `midi2_msg.h`, `midi2_proc.h`, `midi2_proc.c`, `midi2_ci.h`, `midi2_ci.c`
- Compile the `.c` files with your project

**With byte stream conversion:**
- Also add `midi2_conv.h`, `midi2_conv.c`

## Design

- **C99** -- any compiler, any platform
- **Zero allocation** -- no malloc, no free, no hidden heap usage
- **Caller owns all memory** -- buffers and arrays passed at init
- **Stateless where possible** -- midi2_msg is 100% stateless
- **Error codes, not silent failures** -- functions report what happened
- **Portable** -- only stdint.h, stdbool.h, string.h. Runs on AVR, ARM, x86, RISC-V

## License

MIT
