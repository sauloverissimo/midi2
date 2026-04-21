# Changelog

## [0.2.4] - 2026-04-21

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

## [0.2.3] - 2026-04-06

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

## [0.2.2] - 2026-04-03

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

## [0.2.0] - 2026-03-30

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

## [0.1.0] - 2026-03-28

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
