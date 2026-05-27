# Changelog

## [0.5.0]

Zephyr west module integration and an internal pass on input
validation and lookup tables. No public API signature changes.

### Added

- **Zephyr module support**. `zephyr/module.yml`, `zephyr/CMakeLists.txt`
  and `zephyr/Kconfig` let Zephyr applications pull midi2 in via west
  manifest and enable it with `CONFIG_MIDI2=y`. The same `src/midi2_*.c`
  set serves IDF, Arduino, PlatformIO, native CMake and Zephyr without
  forks. Pair with the Zephyr `usbd_midi2` device class for USB UMP or
  with the Network MIDI 2.0 stack for IP transport.
- **Zephyr example** at `examples/rpi-pico-device-zephyr/`:
  full-spec USB MIDI 2.0 device on Raspberry Pi Pico (RP2040). 14-scene
  cycle covering Flex Data, Per-Note expression, RPN/NRPN, Note Attribute,
  multi-fragment SysEx7, System Common/Real-Time, Delta Clockstamp, JR
  Heartbeat plus a MIDI-CI responder. Layered on Zephyr `usbd_midi2`
  with midi2 driving every UMP beyond what the Zephyr Stream responder
  emits. Flash ~67 KB, RAM ~14 KB on RP2040.
- **CI: Zephyr smoke job** on `native_sim/native/64` builds and runs
  the consumer under `tests/zephyr-consumer/` on every push.

### Changed

- **Input validation** hardened across module entry points. Public
  functions treat NULL pointer arguments as no-ops or zero returns
  rather than relying on caller discipline. API signatures unchanged.
- **`midi2_msg_word_count`** covers all 16 message types using their
  spec-defined lengths (M2-104-UM v1.1.2 §2.1.4). Reserved MTs advance
  by the length the spec assigns them, so a router that walks an
  unknown buffer still steps the correct number of words.
- **Cable-event to UMP conversion** uses a lookup table instead of a
  switch; semantics preserved (USB MIDI 1.0 class spec table 4-1).
- Internal helpers reorganized for clarity (`midi2_proc_stream_form`
  for SysEx form selection, `midi2_dispatch_sign_extend_4` for Flex
  Data 4-bit signed nibbles).

### Tests

- 350 assertions across 8 suites, gcc and clang clean with
  `-Wall -Wextra -Wpedantic`, ASan + UBSan clean.
- Cross-compiles for ARM Cortex-M, AArch64, RISC-V 64, ESP32 (Xtensa),
  AVR ATmega328P (header-only). Adds Zephyr native_sim/native/64.

## [0.4.0]

Breaking signature changes across UMP Stream, Channel Voice,
Utility, and Flex Data builders.

### Breaking — UMP Stream (MT 0xF)

- **`midi2_msg_stream_config_request(w, protocol)` →
  `midi2_msg_stream_config_request(w, protocol, rx_jr_enable, tx_jr_enable)`**.
  Status 0x05 (M2-104-UM §7.1.4). Bits [1:0] of word 0 carry
  `JR_TX_ENABLE` (bit 0) / `JR_RX_ENABLE` (bit 1). Used host→device to
  request a session configuration; the dispatcher already decoded these
  bits, the builder did not emit them.

- **`midi2_msg_stream_config_notify(w, protocol)` →
  `midi2_msg_stream_config_notify(w, protocol, rx_jr_enable, tx_jr_enable)`**.
  Status 0x06 (M2-104-UM §7.1.5). Same bit layout as Config Request.
  Without these, devices running a JR Timestamp heartbeat (e.g. enabled
  through `midi2_proc_enable_jr_heartbeat`) showed `Sending JR
  Timestamps: False` upstream even though the wire traffic was correct.

- **`midi2_msg_stream_fb_info(w, active, fb_num, direction, first_group,
  num_groups, midi_ci_ver, sysex8, protocol)` →
  `midi2_msg_stream_fb_info(w, active, fb_num, direction, ui_hint,
  first_group, num_groups, midi_ci_ver, max_sysex8_streams, protocol)`**.
  Status 0x11 (M2-104-UM §7.1.3). Two fields filled in:
  - `ui_hint` (bits [5:4] of word 0): `0x00` Undeclared, `0x01`
    Receiver, `0x02` Sender, `0x03` Sender + Receiver. Hosts use this
    to pick FB icons / labels in their port pickers.
  - `max_sysex8_streams` (bits [7:2] of word 1 byte 2): replaces the
    `bool sysex8` reducer with the spec's 6-bit count (0..63
    concurrent streams). Pass `0` for "SysEx8 not supported" and `1`
    for "supports SysEx8" to match the prior bool behaviour exactly.

- **`midi2_dp_fb_info_cb` callback signature** gains `ui_hint` between
  `direction` and `first_group`. Recipes wiring `on_fb_info` directly
  must update their handler signature.

- **`midi2_dispatch.c on_fb_info` trampoline** decodes `ui_hint` from
  bits [5:4] and forwards it through the callback.

### Breaking — Utility (MT 0x0)

- **`midi2_msg_jr_clock(group, timestamp)` →
  `midi2_msg_jr_clock(timestamp)`** and **`midi2_msg_jr_timestamp(group,
  timestamp)` → `midi2_msg_jr_timestamp(timestamp)`**. Per M2-104-UM
  v1.1.2 section 1.4 the former Group field on Utility messages is now
  Reserved (must be zero). Removing the parameter prevents silently
  writing a non-zero Group on the wire from existing recipes.

- **New: `midi2_msg_noop(void)`** builder for the NOOP Utility message
  (status 0x0, all-zero payload). Trivial, but completes coverage of
  the five Utility statuses.

- **`midi2_dp_jr_clock_cb` and `midi2_dp_jr_timestamp_cb` typedefs**
  drop the leading `uint8_t group` parameter. Symmetric with the
  builder change: the wire bits are reserved, the dispatcher no
  longer parses them, and the callbacks no longer surface them.
  Callers that wired `on_jr_clock` / `on_jr_timestamp` directly must
  update the handler signature to `(uint16_t timestamp, void *ctx)`.

### Breaking — MIDI 2.0 Channel Voice (MT 0x4)

- **`midi2_msg_note_on(w, group, channel, note, velocity, attribute)` →
  `midi2_msg_note_on(w, group, channel, note, velocity, attr_type,
  attr_data)`** (and same for `midi2_msg_note_off`). The old single
  `uint16_t attribute` parameter folded the 8-bit `Attribute Type` and
  16-bit `Attribute Data` into a 16-bit slot, truncating Pitch7_9 and
  other 16-bit-wide attribute payloads to 8 bits. The dispatcher
  callback already exposed them as `(uint8_t attr_type, uint16_t
  attr_data)`, so the builder now matches.

- **`midi2_msg_program(...)` Bank Valid bit relocates** from bit 31 of
  word 1 (where it was overwriting byte 5's reserved MSB) to bit 0 of
  word 0's byte 4 (the spec's "option flags" lane per M2-104-UM section
  7.4.9). API signature unchanged; on-the-wire bit position fixed. The
  dispatch parser was updated symmetrically. Cross-checked against
  AM_MIDI2.0Lib `mt4ProgramChange`.

### Breaking — Flex Data (MT 0xD)

- **`midi2_msg_time_sig(w, group, numerator, denominator)` →
  `midi2_msg_time_sig(w, group, numerator, denominator,
  num_32nd_notes)`**. Per M2-104-UM section 7.5.4 the Set Time
  Signature message carries three numeric fields, not two: numerator,
  denominator (negative power of 2), and the SMF-compat "Number of 1/32
  Notes per 24 MIDI Clocks". The builder was emitting only the first
  two and leaving byte 3 of word 1 at zero. Default value per SMF1: 8.

### Tests

- `test_stream_config_request_jr_bits` — asserts JR bits in Stream
  Config Request.
- `test_stream_config_notify_jr_bits` — asserts JR bits in Stream
  Config Notify.
- `test_stream_fb_info_ui_hint` — asserts `ui_hint` and `direction`
  occupy independent bit lanes (any combination, any FB).
- `test_stream_fb_info_sysex8_streams` — asserts
  `max_sysex8_streams` covers the full 0..63 range.
- `test_dp_config_request_jr_bits`, `test_dp_config_notify_jr_bits`
  — round-trip builder→dispatcher preserves both JR flags.
- `test_dp_fb_info` — re-asserts the round-trip with `ui_hint=0x03`.
- `test_jr_clock` / `test_jr_timestamp` — asserts the Group field is
  reserved/zero per the Groupless rule.
- `test_noop` — covers the new builder.
- `test_program_change_bank` / `_no_bank` — assert that the Bank Valid
  bit lives in byte 4 option flags and byte 5 MSB stays reserved.
- `test_dp_note_on` adds `attr_type` / `attr_data` assertions to lock
  in 16-bit attribute_data round-tripping.
- `test_time_sig` asserts the `num_32nd_notes` SMF-compat byte.

Total: 328 assertions across the 8 host test binaries, ASan + UBSan
clean.

### Migration

Default-safe values reproduce the prior behaviour on the wire:

```c
midi2_msg_stream_config_request(w, protocol, /*rx_jr*/ false, /*tx_jr*/ false);
midi2_msg_stream_config_notify (w, protocol, /*rx_jr*/ false, /*tx_jr*/ false);
midi2_msg_stream_fb_info(w, active, fb_num, direction, /*ui_hint*/ 0x00,
                         first_group, num_groups, midi_ci_ver,
                         /*max_sysex8_streams*/ 0, protocol);
```

Recipes whose device actively emits JR Timestamps (typical when
`enableJRHeartbeat()` is on) should pass `tx_jr_enable=true`. Recipes
with a known FB role should declare `ui_hint` (`0x02` for senders,
`0x03` for bidirectional devices, `0x01` for pure listeners). Recipes
that previously passed `sysex8=true` now pass `max_sysex8_streams=1`.

For Utility / Channel Voice / Flex Data callers:

```c
/* old */ midi2_msg_jr_clock(group, ts);        /* new */ midi2_msg_jr_clock(ts);
/* old */ midi2_msg_jr_timestamp(group, ts);    /* new */ midi2_msg_jr_timestamp(ts);
/* old */ midi2_msg_note_on(w, g, ch, n, v,     /* new */ midi2_msg_note_on(w, g, ch, n, v,
                            (type<<8)|data8);                            type, data16);
/* old */ midi2_msg_time_sig(w, g, num, denom); /* new */ midi2_msg_time_sig(w, g, num, denom, 8);
```

Program Change keeps the same C signature; only the on-the-wire bit
moves. No recipe code change required there.

The repo's own consumer tests, AVR heredoc, and `examples/` C +
Arduino sketches were updated in the same release so `find_package`
consumers, the AVR cross-compile job, and the on-tree learning
material all build out of the box on v0.4.0. Recipes outside this
repo (midi2cpp, libDaisy fork, Teensy core fork) follow in their
own bumps.

## [0.3.4]

Fix the ESP-IDF Component Manager path. v0.3.3 routed
`idf_component_register` at `dist/midi2.c`, but the Component Manager
filters `dist/` out of the dependency tarball as a build-output
convention, so consumers saw `Include directory ... is not a
directory` at configure time.

### Fixed

- **ESP-IDF gate** in `CMakeLists.txt` now consumes the modular
  `src/midi2_proc.c`, `src/midi2_dispatch.c`, `src/midi2_conv.c`,
  `src/midi2_ci.c`, `src/midi2_ci_dispatch.c` (the source set that
  the Component Manager actually delivers). The header-only modules
  (`midi2_msg.h`, `midi2.h`, `midi2_ci_msg.h`) need no entry. Native
  CMake and PlatformIO consumers, which still build the amalgamated
  `dist/midi2.c`, are unaffected.

### Changed

- `idf_component.yml` example snippet pinned to `v0.3.4` (literal
  git tag, not a semver constraint, since `version:` for git
  dependencies is a git ref).

## [0.3.3]

ESP-IDF Component Manager support. midi2 can now be pulled into an
ESP-IDF project tree directly via the Component Manager:

```yaml
# main/idf_component.yml
dependencies:
  midi2:
    git: https://github.com/sauloverissimo/midi2.git
    version: ">=0.3.3"
```

The Component Manager clones midi2 under `managed_components/`,
ESP-IDF picks the directory up as a regular component (the top-level
`CMakeLists.txt` detects `ESP_PLATFORM` and routes to
`idf_component_register`). Native CMake consumers (Pico SDK, vcpkg,
FetchContent, system installs, etc.) are unaffected: the `if
(ESP_PLATFORM) ... return()` guard sits before `project()` so every
non-IDF path runs unchanged.

### Added

- **`idf_component.yml`** at the repo root with the component
  manifest (description, version, license, maintainers, tags).
- **ESP-IDF gate** at the top of `CMakeLists.txt` that calls
  `idf_component_register(SRCS dist/midi2.c INCLUDE_DIRS dist)`
  when `ESP_PLATFORM` is defined and returns before the native CMake
  build runs.

## [0.3.2]

CMake target + downstream package consumption. midi2 now ships an
official CMake build alongside the existing Makefile, so consumers can
pick the idiom that fits: `find_package(midi2 0.3.2 CONFIG)` for system
installs / vcpkg / conan, `FetchContent_MakeAvailable` on the GitHub
repo, or `add_subdirectory` of a vendored copy.

### Added

- **`CMakeLists.txt`** at repo root: builds `dist/midi2.c` into
  `libmidi2.a`, exposes the `midi2::midi2` alias target, declares
  `c_std_99` as a public compile feature, mirrors the Makefile's
  `-Wall -Wextra -Wpedantic` warnings on GCC/Clang/AppleClang.
- **install + export ruleset**: `install(EXPORT midi2-targets ...)`
  generates `midi2-config.cmake` and `midi2-config-version.cmake`
  (SameMajorVersion compatibility) so downstream
  `find_package(midi2 0.3.2 CONFIG)` succeeds. Install rules are gated
  by `PROJECT_IS_TOP_LEVEL` so subprojects pulling midi2 via
  FetchContent do not pollute their parent's install set.
- **`tests/cmake-consumer/`**: real downstream smoke
  (`CMakeLists.txt` + `main.c`) that the CI compiles, links against
  the imported `midi2::midi2` target, and runs.
- **CI `cmake-build` matrix** on `ubuntu-latest`, `macos-latest`,
  `windows-latest`. Configure -> build -> install -> downstream
  `find_package(midi2)` consumer compiled and run.
- **CI `cmake-fetchcontent`**: synthetic parent project pulls midi2
  via `FetchContent_Declare` and links `midi2::midi2` without an
  install step.
- **CI `cmake-build-sanitizers`**: ASan + UBSan parity for the
  amalgam build path.
- `architecture.png` referenced by the README.

### Fixed

- Makefile amalgam test isolated from `-I src` so the new
  `src/midi2.h` umbrella (added in v0.3.1) does not shadow
  `dist/midi2.h`. Without this fix the amalgam test compiled but
  failed to link.

## [0.3.1]

Arduino library compliance and Library Manager registration. No
runtime changes; same UMP and MIDI-CI surface as v0.3.0.

### Added

- **`library.properties`** for the Arduino Library Manager.
- **`src/midi2.h` umbrella** so Arduino IDE sketches can
  `#include <midi2.h>` and pick up every modular module in `src/`.
- **`examples/BasicUsage/BasicUsage.ino`** and
  **`examples/CIDiscovery/CIDiscovery.ino`**: two reference sketches
  the Arduino IDE surfaces under File > Examples after install.
- README install sections for Arduino IDE / arduino-cli and
  PlatformIO.

### Registered

- midi2 published on the Arduino Library Manager via PR
  `arduino/library-registry#8284` (merged 2026-05-07). Available in
  the Library Manager: search `midi2`.

## [0.3.0]

Property Exchange Subscribe/Notify state machine, UMP Stream and
SysEx8 fragmenters, registry symmetry, MT 0x4 to MT 0x2 downgrade,
and USB MIDI 1.0 cable-event helper. First release since v0.2.4
with a breaking struct change (semver minor bump justified); callers
using designated-initialiser style compile unchanged. Stays embedded
contract: only `<stdint.h>`, `<stdbool.h>`, `<string.h>` (no `<stdio.h>`,
no `snprintf`, no float formatter dragged in).

### Added

- **Subscribe / Notify state machine** (`midi2_ci_subscribe_add`,
  `_subscribe_remove`, `_notify_property_changed`,
  `_pe_set_subscribable`, `_get_subscriber_count`) per M2-101-UM
  sections 8.11 to 8.13. A caller-provided subscriber registry
  arrives via the new `midi2_ci_init_ex(...)`; legacy
  `midi2_ci_init()` stays source-compatible and delegates with a
  NULL subscriber array. Notify header is built with `memcpy` of a
  fixed JSON template, no `snprintf`.
- **`midi2_ci_remove_property`** and **`midi2_ci_reset_profiles` /
  `_reset_properties`**: symmetric registry helpers.
- **`midi2_ci_set_auto_invalidate_on_collision(state, enabled)`**
  (default on). Exposes the Invalidate MUID broadcast that the
  collision detector in v0.2.4 already emitted; apps can now opt
  out per M2-101-UM section 5.9 if they prefer manual control.
- **UMP Stream and SysEx8 fragmenters in `midi2_proc`**:
  `midi2_proc_send_endpoint_name` (status 0x003),
  `midi2_proc_send_product_id` (status 0x004),
  `midi2_proc_send_device_identity` (status 0x002, single UMP), and
  `midi2_proc_send_sysex8` (MT 0x5, 13 bytes per UMP). All delegate
  to the canonical single-packet builders in `midi2_msg.h`.
- **`midi2_msg_mt4_to_mt2`** inline helper: inverse of the existing
  `midi2_msg_mt2_to_mt4`. Bit-scales MIDI 2.0 Channel Voice down to
  MIDI 1.0 per M2-115. Lossy by spec: RPN / NRPN / per-note are
  dropped (no MIDI 1.0 single-word form). Returns 0 or 1; caller
  detects drops by counting.
- **`midi2_msg_cable_event_to_ump`** inline helper: USB MIDI v1.0
  4-byte cable event to UMP MT 0x2. Channel Voice (CIN 0x8-0xE) and
  System Common (CIN 0x2-0x3) handled. SysEx fragment CINs (0x4-0x7,
  0xF) and reserved CINs return false; SysEx fragments belong in
  `midi2_conv` (stateful).
- **`midi2_msg_set_group`** inline helper: rewrites word 0 Group
  nibble on MT 0x2 to 0x5, leaves Utility / System / Flex Data /
  Stream alone.
- **MT 0x1 System message wrappers** (10 inline helpers): named
  shortcuts wrapping `midi2_msg_system{,_2byte,_3byte}` with the
  canonical status byte for each MIDI 1.0 System Common and System
  Real-Time message per M2-104-UM section 4.3.
  - `midi2_msg_system_tune_request(group)` (0xF6)
  - `midi2_msg_system_timing_clock(group)` (0xF8)
  - `midi2_msg_system_start(group)` (0xFA)
  - `midi2_msg_system_continue(group)` (0xFB)
  - `midi2_msg_system_stop(group)` (0xFC)
  - `midi2_msg_system_active_sensing(group)` (0xFE)
  - `midi2_msg_system_reset(group)` (0xFF)
  - `midi2_msg_system_mtc(group, time_code)` (0xF1, 1 data byte)
  - `midi2_msg_system_song_select(group, song)` (0xF3, 1 data byte)
  - `midi2_msg_system_song_position(group, position)` (0xF2, 14-bit
    split as LSB/MSB)

  Pure DX, zero ROM cost (header inline). Removes the cargo cult of
  memorising magic status bytes (0xF1..0xFF) at the call site.

### Changed

- **Breaking:** `midi2_ci_property` gains a trailing `subscribable`
  bool. Designated-initialiser call sites compile unchanged;
  positional inits must append `false`.
- `midi2_ci_state` gains subscriber / auto_invalidate_on_collision
  fields. Documented as opaque, so this is non-breaking.
- `midi2_ci_add_property_static` / `_add_property_dynamic` now reset
  the new `subscribable` field so reused caller-provided slots start
  clean.

### Migration

Two paths for the breaking `midi2_ci_property` change:

```c
/* Before (positional init): */
midi2_ci_property props[] = {
  { "DeviceName", "MySynth", NULL, NULL }
};

/* After (either form works): */
midi2_ci_property props[] = {
  { "DeviceName", "MySynth", NULL, NULL, false }   /* explicit */
};
midi2_ci_property props[] = {
  { .name = "DeviceName", .static_value = "MySynth" }  /* designated */
};
```

Existing `midi2_ci_init` users see zero behaviour change; the
subscriber registry stays NULL. Opt in via `midi2_ci_init_ex`.

### Test suite

Full suite: **321 test functions across 8 suites**, all green.
Baseline v0.2.4 was 266 functions across 8 suites; this release
adds 55 net new test functions across the existing suites covering
the new public surface (Subscribe/Notify, fragmenters, MT4 to MT2
downgrade, USB cable event helper, registry symmetry, System
message wrappers). gcc -std=c99 -Wall -Wextra -Wpedantic clean,
ASan and UBSan clean.

## [0.2.4]

Convenience responder completeness. Every spec-required behavior from
M2-101-UM Appendix E is now handled inside `midi2_ci_process_sysex()`.
Applications no longer need to copy MUID regeneration, collision
detection, NAK-on-unknown, or PE Capability boilerplate. **Additive,
zero breaking change**: new state fields zero-initialise via
`midi2_ci_init()` and every new behavior is opt-in.

### Added

- **`midi2_ci_set_rng(state, rng_fn, ctx)`**: platform RNG injection.
  Unlocks automatic MUID regeneration for the next two features.
- **`midi2_ci_new_muid(state)`**: helper that returns a fresh 28-bit
  MUID avoiding the reserved values `0x00000000` and `0x0FFFFFFF`
  (broadcast). Falls back to a deterministic perturbation if no RNG
  installed, so callers always get a usable value.
- **Automatic Invalidate MUID (Sub-ID#2 0x7E) handling** inside
  `midi2_ci_process_sysex()`. When the incoming message's target MUID
  equals ours and an RNG is set, state->muid is regenerated.
- **Automatic MUID collision detection**. Every inbound CI message's
  src_muid is compared to ours before dispatch; on match, state->muid
  is regenerated and Invalidate MUID is broadcast for the old value.
- **`midi2_ci_set_nak_on_unknown(state, enabled)`**: when enabled, the
  convenience responder replies to any unsupported CI sub-id with a
  Sub-ID#2 0x7F NAK v2 (status 0x01 NOT_SUPPORTED). Off by default to
  preserve v0.2.3 semantics.
- **`midi2_ci_process_sysex()` now handles `MIDI2_CI_PE_CAPABILITY`**
  (0x30) with an automatic `MIDI2_CI_PE_CAPABILITY_REPLY` (0x31)
  declaring max_simultaneous = 1, PE v1.0. Without this, Initiators
  treated PE as unsupported and never sent PE_GET.
- **`midi2_proc_send_fb_name(fb_idx, name, write_fn, ctx)`**: UMP
  Stream Function Block Name Notification sender (MT 0xF status 0x12)
  per M2-104-UM §7.1.9. Handles the Complete / Start / Continue / End
  fragmentation scheme with 13 name bytes per UMP, spec-compliant up
  to 91 bytes total.
- 14 new unit tests across `test_midi2_ci.c` and `test_midi2_proc.c`
  covering all of the above. Full suite now 266 assertions across 8
  files, all green.

### Fixed

- **Discovery Reply `ci_cat` now advertises PROCESS_INQUIRY** (bit 4)
  alongside PROFILE_CONFIG (bit 2) and PROPERTY_EXCHANGE (bit 3). The
  library handled `MIDI2_CI_PI_CAPABILITY` via
  `ci_handle_process_inquiry` since v0.2.2 but the Discovery Reply
  only exposed two of the three categories it could service. Closes
  an Initiator-side negotiation gap.

## [0.2.3]

### Added

- **`dist/midi2.c`**: thin companion to `dist/midi2.h` for projects
  that want to keep the single-header include and a separate one-line
  implementation unit. `#define MIDI2_IMPLEMENTATION` + `#include
  "midi2.h"` inside `midi2.c`.
- `tools/amalgamate.sh` now generates both files and places them under
  `dist/` (moved from repo root).

### Changed

- Source headers carry spec references (M2-101-UM, M2-104-UM) and
  metadata across every module for traceability.

## [0.2.2]

### Added
- **midi2.h**: Single-header amalgamation of all 7 modules (stb-style). One file,
  drop into any project, `#define MIDI2_IMPLEMENTATION` in one `.c`. Auto-generated
  via `tools/amalgamate.sh`.
- **test_midi2_amalgam**: 20 new tests validating the amalgamated header across
  all modules. Total: 252 tests across 8 test suites.
- **midi2_msg**: `midi2_msg_mt2_to_mt4()` -- stateless protocol translation from
  MT 0x2 (MIDI 1.0 Channel Voice) to MT 0x4 (MIDI 2.0 Channel Voice) with proper
  value scaling per M2-104-UM v1.1.2 Section 7. Handles Note On velocity 0 ->
  Note Off, Pitch Bend LSB/MSB combine, Program Change without bank.
- **midi2_dispatch**: `upscale_mt2` flag. When true, incoming MT 0x2 messages are
  automatically translated to MT 0x4 and dispatched through on_note_on/on_cc/etc.
  Default false (backward compatible).
- Hardware validation: Daisy Seed (STM32H750) confirmed Device mode via STM32 HAL USB.
- Hardware validation table expanded with real board names (T-Display S3, Waveshare
  ESP32-P4, T-PicoC3, Raspberry Pi Pico/Pico 2, etc.) and Transport column replacing
  the previous TinyUSB column.

### Changed
- **midi2_conv**: Streaming SysEx support for any message length. The converter
  now emits UMP SysEx7 packets incrementally (START/CONTINUE/END) as bytes arrive,
  instead of buffering the entire message. This removes the previous 6-byte
  truncation limitation for SysEx.
- **midi2_conv**: `midi2_conv_init()` simplified to 2 parameters (state, group).
  The caller-provided SysEx buffer is no longer needed -- the converter uses a
  6-byte internal buffer. **Breaking change** from v0.2.0 API.
- Documentation updated: single-header integration as recommended path, examples
  use `#include "midi2.h"`, hardware validation with real board names and transports.

## [0.2.0]

### Added
- **midi2_dispatch**: New module with 42 granular callbacks for typed UMP message dispatch.
  Covers 100% of M2-104-UM v1.1.2 message types. Semantically-named parameters per
  message type. NULL callbacks silently skipped. Feed signature compatible with
  midi2_proc on_ump callback.
- **midi2_msg**: Complete UMP spec coverage. New construction functions:
  - Delta Clockstamp TPQ and DC (MT 0x0, status 0x3/0x4)
  - Registered/Assignable Per-Note Controllers (MT 0x4, status 0x0/0x1)
  - Relative Registered/Assignable Controllers (MT 0x4, status 0x4/0x5)
  - Mixed Data Set Header and Payload (MT 0x5, status 0x8/0x9)
  - Set Metronome (Flex Data, bank 0x00, status 0x02)
  - Set Chord Name (Flex Data, bank 0x00, status 0x06)
  - Key Signature with tonic note and channel addressing
  - Flex Data text builder for metadata (bank 0x01) and performance text (bank 0x02)
  - Endpoint Name, Product Instance ID, FB Name notifications (Stream)
  - All Flex Data status bank and text subtype enums
- **midi2_ci_msg**: New header-only module for MIDI-CI message construction and parsing.
  Covers all 31 MIDI-CI messages per M2-101-UM v1.2. MUID encoding, 14/28-bit field
  helpers, parse helpers for common fields.
- **midi2_ci_dispatch**: New module with 33 granular callbacks for typed MIDI-CI dispatch.
  Covers Management, Profile Configuration, Property Exchange, Process Inquiry.
- **midi2_ci** refactored: now uses midi2_ci_msg for message construction (eliminates
  duplicate encoding logic). Fixed spec-incorrect Profile Inquiry sub-IDs (was 0x24/0x25,
  corrected to 0x20/0x21). Replies now include version field per spec.
- 136 new tests (total: 213 across 7 modules).

### Fixed
- Utility message status nibble was at shift 16, corrected to shift 20 per
  M2-104-UM v1.1.2 Table 26. Affects JR Clock and JR Timestamp in v0.1.0.
- SysEx7/SysEx8 dispatch delivers status values matching the MIDI2_SYSEX7_*/MIDI2_SYSEX8_*
  enum values (0x00/0x10/0x20/0x30) instead of raw nibbles.

## [0.1.0]

Initial release.

### Added
- **midi2_msg**: UMP construction and parsing for all message types (MT 0x0-0xF).
  Value scaling with bit-replication (7/14/16/32-bit, round-trip safe).
  JR Timestamps, Stream messages, SysEx7, SysEx8, Flex Data.
- **midi2_proc**: UMP processor with group filtering, group remap,
  SysEx7 multi-packet reassembly, SysEx7 fragmented send.
- **midi2_ci**: MIDI-CI responder for Discovery Reply, Profile Inquiry Reply,
  and basic Property Exchange (PE Get).
- **midi2_conv**: MIDI 1.0 byte stream to UMP converter with Running Status,
  System Common, Real-Time pass-through, SysEx accumulation.
- 77 unit tests across all modules.
- Caller-provided storage for all stateful modules (no compile-time limits).
- Error codes for functions that can fail.
