# Changelog

## [Unreleased]

### Changed
- **midi2_conv**: Streaming SysEx support for any message length. The converter
  now emits UMP SysEx7 packets incrementally (START/CONTINUE/END) as bytes arrive,
  instead of buffering the entire message. This removes the previous 6-byte
  truncation limitation for SysEx.
- **midi2_conv**: `midi2_conv_init()` simplified to 2 parameters (state, group).
  The caller-provided SysEx buffer is no longer needed -- the converter uses a
  6-byte internal buffer. **Breaking change** from v0.2.0 API.
- 5 new streaming SysEx tests (total: 218 across 7 modules).

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
