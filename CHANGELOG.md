# Changelog

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
