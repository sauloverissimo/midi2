# Design Decisions

Key architectural choices made during midi2 development and their rationale.

## 1. C99, not C++

**Decision**: Pure C99 for the entire library.

**Why**: Infrastructure libraries that succeed in embedded (TinyUSB, FreeRTOS, lwIP, LVGL) are all C. Any language can link with C. The ergonomic C++ API lives in Layer 3 (platform wrappers), not in the infrastructure.

## 2. Zero allocation

**Decision**: No `malloc`, no `free`, no hidden heap usage. Caller provides all buffers.

**Why**: Embedded systems need deterministic memory. A 2KB RAM MCU and a 16GB desktop machine use the same midi2 API -- only the buffer sizes differ.

## 3. Header-only for stateless modules

**Decision**: `midi2_msg.h` and `midi2_ci_msg.h` are header-only (all `static inline`). Stateful modules (proc, dispatch, ci) use `.h` + `.c`.

**Why**: Stateless functions (construct UMP, parse fields, scale values) are tiny and benefit from inlining. Stateful modules have real logic that should compile once.

## 4. Granular dispatch callbacks (not structs)

**Decision**: 49 individual UMP callbacks (`on_note_on`, `on_cc`, etc.) instead of 8 struct-based callbacks.

**Why**: Analyzed against AM_MIDI2.0Lib (granular), ni-midi2 (views), cmidi2 (none), and Teensy (granular). Granular callbacks are semantically correct (each callback has named parameters matching the spec), auto-documented, and what the MIDI 2.0 ecosystem expects.

## 5. MIDI-CI as separate protocol layer

**Decision**: CI split into 3 internal layers: `ci_msg` (construct/parse), `ci_dispatch` (callbacks), `ci` (convenience responder).

**Why**: MIDI-CI is a protocol that rides on SysEx, not part of UMP format. No reference implementation (AM_MIDI2, ni-midi2, cmidi2) bundles auto-response into the parser. The convenience responder is optional for platforms that want it.

## 6. CI convenience responder is simplified

**Decision**: The `midi2_ci` auto-responder does not parse JSON PE headers and always returns the first property.

**Why**: JSON parsing requires either dynamic memory or a JSON library dependency -- both violate midi2's constraints. The responder works for simple devices (1-2 properties). For full PE, use `midi2_ci_dispatch` with your own JSON parser at the platform layer.

## 7. No transport code

**Decision**: midi2 never touches USB descriptors, BLE characteristics, network sockets, or serial ports.

**Why**: Transport is Layer 1. midi2 is Layer 2. Mixing them would make the library hardware-specific and unportable. TinyUSB handles USB, the platform handles everything else.

## 8. SysEx7 status enums use shifted values

**Decision**: `MIDI2_SYSEX7_COMPLETE = 0x00`, `MIDI2_SYSEX7_START = 0x10`, etc. (upper nibble of the status byte).

**Why**: The dispatch and proc modules extract the full upper nibble `(byte >> 16) & 0xF0`. Using the shifted enum values allows direct comparison without additional shifting in application code.

## 9. Spec-complete API, then hardware validation

**Decision**: Cover 100% of UMP and CI message types first, then validate progressively on real hardware.

**Why**: A partial API that gets published and later extended causes breaking changes for early adopters. By stabilizing the full API surface first, hardware validation focuses on integration quality rather than API design. The library has 350 host-side tests covering all message types; hardware validation confirms real-world interoperability.

## 10. Progressive hardware validation

**Decision**: Validate midi2 integration on multiple platforms (ESP32, Teensy, Daisy, RP2040, and others) with documented results before each public release.

**Why**: Different platforms have different transport models (TinyUSB raw words vs Teensy typed dispatch vs BLE MIDI). The API must work naturally on all of them. Each validated platform will be documented with test results, including Device and Host modes where applicable.
