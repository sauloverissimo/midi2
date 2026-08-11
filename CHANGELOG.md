# Changelog

Format based on Keep a Changelog. This project follows Semantic Versioning.

## [Unreleased]

### Added

- `midi2_conv_next()`: drains the additional message a single MIDI 1.0 byte
  can produce. Per M2-104-UM 7.7.1, System Real-Time may interleave inside a
  SysEx and any other status terminates it, so one byte can yield two UMP
  messages; the canonical consumption loop is now
  `if (midi2_conv_feed(...)) do { ... } while (midi2_conv_next(...));`.
  For ordinary bytes the loop body runs once. Debug builds assert if a queued
  message is left undrained; release builds drop it.

### Changed

- `midi2_conv` now applies the M2-104-UM 7.7.1 interspersing rules:
  - A Real-Time byte inside a SysEx is emitted after the partial packet
    holding the bytes that preceded it on the wire, preserving its timing
    position (previously it was emitted first, reordering it ahead of data
    it followed).
  - A status byte inside a SysEx closes the message with a COMPLETE or END
    packet carrying the bytes received (previously the buffered bytes were
    dropped and an already-emitted START was left unterminated).
  - A SysEx Start while another SysEx is open closes the previous message
    the same way before starting the new one.
  Verified against the byte vectors from AM_MIDI2.0Lib issues #16, #23 and
  #24; the first packets for the #24 vector now match that issue's expected
  words exactly.

## [0.8.0]

### Added

- Example `atmega32u4-device-arduino`: USB MIDI 2.0 device on the Arduino
  Leonardo over the midi2duino transport, with UMP Stream and MIDI-CI
  responders. MIDI-CI replies are queued atomically and inbound Data128 is
  echoed directly, so a full TX ring cannot corrupt USB output.
- Example `atmega32u4-device-baremetal`: USB MIDI 2.0 device on the Arduino
  Pro Micro, bare metal C99 over the midi2lufa transport, with the 58-entry
  M2-104 catalog, UMP Stream and MIDI-CI responders, and a MIDI 1.0
  fallback riff. Validated on hardware (Linux ALSA UMP, echo, per-boot MUID).

### Changed

- Device identity field byte order in the Stream and MIDI-CI builders now
  follows M2-104 Figure 14 and the MIDI-CI Discovery tables: the packed
  `manufacturer_id` is `id1<<16 | id2<<8 | id3` (the educational prefix
  0x7D packs as 0x7D0000), family and model are 14-bit values sent as
  7-bit LSB/MSB pairs, and the version is 28 bits sent as four 7-bit
  bytes LSB first. Callers that passed the manufacturer id in the
  previous packed form must update; the wire layout is cross-checked
  against the MIDI 2.0 Workbench, the Linux kernel UMP driver and
  Windows MIDI Services.

### Fixed

- MIDI-CI responder re-announces via Discovery after receiving an Invalidate
  MUID or detecting a MUID collision (Workbench ci1.2).
- Multi-packet senders (`midi2_proc_send_sysex7`, `midi2_proc_send_sysex8`,
  `midi2_proc_send_fb_name`, endpoint name and product id) stop at the first
  short write instead of continuing past it, so a full transport sink can no
  longer leave a gap in the middle of a message. The receiver resynchronizes
  on the next Start packet.

## [0.7.0]

Validated end to end against the official MIDI 2.0 Workbench on hardware
(Teensy 4.1, RP2350, Raspberry Pi Pico): Discovery, Profile Configuration, and
Property Exchange with zero errors and zero warnings.

### Added

- MIDI-CI responder handles Set Profile On/Off: a listed always-on profile
  answers with a Profile Enabled Report, an unlisted profile is NAKed.
- MIDI-CI responder handles Process Inquiry MIDI Message Report: Reply and End
  messages for Function Block and in-use channel requests, NAK for channels
  not in use.

### Fixed

- Property Exchange Get replies for list resources (ResourceList, ChannelList,
  ProgramList) now carry `totalCount` in the header, as M2-105 requires for
  paginable resources. Object resources such as DeviceInfo are unchanged.

### Changed

- Example set reduced to three real USB MIDI 2.0 devices sharing the C99 core:
  `teensy-device-midi2` (Arduino loop), `rp2350-device-freertos`
  (FreeRTOS + TinyUSB), and `rpi-pico-device-zephyr` (native `usbd_midi2`). The
  two synthetic sketches (`basic-usage`, `ci-discovery`) were removed. The
  Teensy sketch announces the UMP Stream identity in-band so hosts running
  active Endpoint Discovery register it.

## [0.6.1]

- MIDI-CI responder declares Message Version 0x02 across all replies.
- Property Exchange Get/Set replies carry a status header, match the requested
  resource, and return 404 when it does not exist.
- Added `midi2_ci_set_capabilities` and a built-in `ResourceList` resource.

## [0.6.0]

- `midi2_dispatch_feed` and `midi2_proc_feed` drop a typed message shorter than
  its type requires instead of over-reading.
- `midi2_ci` returns `MIDI2_CI_ERR_NULL` for a NULL state or name in the
  property and subscribe entry points.
- Added an overridable `MIDI2_ASSERT` and a debug-only reentrancy guard (both
  compiled out under `NDEBUG`). Reproducible amalgam (no build-date stamp).

## [0.5.0]

- Zephyr module support and a Raspberry Pi Pico example.
- NULL arguments at public entry points are no-ops or zero returns;
  `midi2_msg_word_count` covers all 16 message types.

## [0.4.0]

Breaking signature changes so the builders emit spec fields they previously
dropped. Default-safe values reproduce the prior wire behaviour; migration
snippets are in git history.

- UMP Stream: Config Request and Notify gain JR enable bits; FB Info gains
  `ui_hint` and `max_sysex8_streams`.
- Utility: `midi2_msg_jr_clock` and `_jr_timestamp` drop the now-reserved Group
  parameter; added `midi2_msg_noop`.
- MIDI 2.0 Channel Voice: Note On/Off split the attribute into `attr_type` plus
  a 16-bit `attr_data`; Program Change Bank Valid bit moved to the spec lane.
- Flex Data: `midi2_msg_time_sig` gains `num_32nd_notes`.

## [0.3.4]

- ESP-IDF gate consumes the modular `src/*.c` set the Component Manager
  delivers (it filters `dist/` out of the dependency tarball).

## [0.3.3]

- ESP-IDF Component Manager support via a root `idf_component.yml` and an
  `ESP_PLATFORM` gate ahead of `project()`, so native CMake consumers are
  unaffected.

## [0.3.2]

- CMake build alongside the Makefile: `midi2::midi2` target, install and export
  ruleset for `find_package(midi2 CONFIG)`, FetchContent support, and a
  downstream consumer test.

## [0.3.1]

- Arduino Library Manager compliance: `library.properties`, the `src/midi2.h`
  umbrella, and reference sketches. Registered on the Arduino Library Manager.

## [0.3.0]

- Property Exchange Subscribe/Notify state machine (M2-101-UM 8.11 to 8.13) via
  `midi2_ci_init_ex`; legacy `midi2_ci_init` stays source-compatible.
- UMP Stream and SysEx8 fragmenters in `midi2_proc`; MT 0x4 to MT 0x2 downgrade;
  USB MIDI 1.0 cable-event helper; System message wrappers.
- Breaking: `midi2_ci_property` gains a trailing `subscribable` bool
  (designated-initialiser call sites compile unchanged).

## [0.2.4]

- MIDI-CI responder completeness (M2-101-UM Appendix E): MUID regeneration and
  collision detection, NAK-on-unknown, and an automatic PE Capability reply.
  Additive, no breaking change.

## [0.2.3]

- `dist/midi2.c` companion to the single header; `tools/amalgamate.sh` emits
  both files under `dist/`. Spec references added across the module headers.

## [0.2.2]

- Single-header amalgam (`midi2.h`, stb-style).
- `midi2_msg_mt2_to_mt4` protocol translation with value scaling and a dispatch
  `upscale_mt2` flag; streaming SysEx7 in `midi2_conv`.

## [0.2.0]

- `midi2_dispatch` (42 typed UMP callbacks), `midi2_ci_msg` and
  `midi2_ci_dispatch` (full MIDI-CI surface), complete UMP construction.
- Fixed the Utility status nibble shift and the SysEx dispatch status values.

## [0.1.0]

Initial release: `midi2_msg`, `midi2_proc`, `midi2_ci`, `midi2_conv`. Caller
provided storage, zero allocation, C99.
