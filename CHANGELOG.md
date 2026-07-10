# Changelog

Format based on Keep a Changelog. This project follows Semantic Versioning.

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
