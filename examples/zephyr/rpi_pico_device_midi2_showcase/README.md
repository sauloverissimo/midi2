# midi2 Zephyr example: Raspberry Pi Pico USB MIDI 2.0 device showcase

Full-spec USB MIDI 2.0 device on the **Raspberry Pi Pico (RP2040)** under Zephyr v4.4+. Headless 14-scene cycle demonstrates every MIDI 2.0 message category beyond MIDI 1.0, plus a MIDI-CI Discovery + Profile + Property Exchange responder, all running on top of Zephyr's `usbd_midi2` device class.

## USB identity

| Field | Value |
|---|---|
| VID:PID | `2FE3:40A0` (Zephyr project VID, development only) |
| Product | `midi2 RP2040 Showcase` |
| Manufacturer | `github.com/sauloverissimo` |
| UMP Endpoint Name | `midi2 RP2040 Showcase` |
| Function Block | 1 bidirectional, `firstGroup=0`, `numGroups=1`, name `Main` |

## Layering

```
Zephyr usbd_midi2          USB enumeration, descriptors, alt-setting
                           CONFIG_MIDI2_UMP_STREAM_RESPONDER handles
                           UMP Stream (MT 0xF) Endpoint Discovery from
                           the zephyr,midi2-device devicetree node.

midi2 (C99)                Typed dispatch, message construction,
                           SysEx7 reassembly, MIDI-CI responder.

src/main.c                 Scene orchestration, JR Heartbeat,
                           identity bootstrap.
```

## Build

Requires:

- Zephyr v4.4+ workspace with this repo registered as a west module
- Zephyr SDK 1.0.1 (provides `arm-zephyr-eabi-gcc`)
- CMake 3.20+, Ninja, Python venv with `west`, `pykwalify`, `pyelftools`, `jsonschema`

```bash
# West manifest entry, see ../../README.md "Zephyr (west module)"
#   - name: midi2
#     url: https://github.com/sauloverissimo/midi2
#     revision: v0.5.0
#     path: modules/lib/midi2

cd examples/zephyr/rpi_pico_device_midi2_showcase
west build -b rpi_pico .
```

Out-of-workspace builds: set `ZEPHYR_BASE` and pass `-DZEPHYR_EXTRA_MODULES=/path/to/midi2`:

```bash
export ZEPHYR_BASE=/path/to/zephyrproject/zephyr
cmake -B build -GNinja -DBOARD=rpi_pico \
  -DZEPHYR_EXTRA_MODULES=/path/to/midi2
cmake --build build
```

## Flash

Hold BOOTSEL on the Pico, plug USB, drag `build/zephyr/zephyr.uf2` to the mounted `RPI-RP2` drive. Or:

```bash
picotool load build/zephyr/zephyr.uf2 -fx
```

After reset the same USB-C reappears as `midi2 RP2040 Showcase` (USB MIDI 2.0 device).

## Memory footprint

| Region | Used | Capacity | Use |
|---|---|---|---|
| Flash (`.text` + `.data`) | ~67 KB | 2 MB | 3.4% |
| SRAM (`.data` + `.bss`) | ~14 KB | 264 KB | 5.4% |
| UF2 size on disk | 135 KB | n/a | bootloader + image |

Headroom for application-specific code, larger CI property tables, additional UMP groups.

## Validation

```bash
lsusb | grep 2fe3:40a0
amidi -l
# I/O  hw:N,1,0  Group 1 (Main)

PORT=$(aseqdump -l | grep "midi2 RP2040" | awk '{print $1}' | tr -d ':')
timeout 60 aseqdump -p ${PORT}
```

Microsoft MIDI Services Console (Windows): `midi enumerate midi-services-endpoints -i` lists the device with Native data format = UMP, MIDI 2.0 Protocol = True. macOS Audio MIDI Setup shows it as a UMP endpoint.

## Spec coverage

| UMP MT | Spec | Scenes | midi2 builders |
|---|---|---|---|
| 0x0 Utility | M2-104-UM §7.2 | H, N + heartbeat | `midi2_msg_jr_timestamp`, `midi2_msg_jr_clock`, `midi2_msg_noop`, `midi2_msg_dctpq`, `midi2_msg_delta_clockstamp` |
| 0x1 System Common / Real-Time | M2-104-UM §7.6 | L | `midi2_msg_system_*` (10 helpers) |
| 0x2 MIDI 1.0 Channel Voice | M2-104-UM §7.3 | K | `midi2_msg_from_midi1` |
| 0x3 SysEx7 | M2-104-UM §7.7 | G | `midi2_msg_sysex7_packet` (Start + Continue + End) |
| 0x4 MIDI 2.0 Channel Voice | M2-104-UM §7.4 | B, C, D, E, F | Note On/Off, CC, PB, Poly/Channel Pressure, Program with Bank, Per-Note family, RPN/NRPN, Relative variants, Note Attribute |
| 0xD Flex Data | M2-104-UM §7.5 | A, M | Tempo, Time Sig, Key Sig, Metronome, Chord Name, Metadata Text |
| 0xF UMP Stream | M2-104-UM §7.1 | A, J + Zephyr responder | `midi2_msg_stream_start_of_clip`, `midi2_msg_stream_end_of_clip` (Zephyr handles Endpoint + FB Discovery) |

**MIDI-CI** (M2-101-UM): Discovery, 1 custom Profile, 3 Property Exchange properties (static `DeviceInfo`, dynamic `ChannelList`, subscribable `OverlayRate`), MUID collision auto-regenerate, NAK-on-unknown.

**SysEx8** (MT 0x5) is intentionally not exercised. Host-side support is rare in the field today; the bench would emit packets nothing consumes.

## Scene timeline (~28.5 s loop, one pass per host enumeration cycle)

| Scene | Start (ms) | Content |
|---|---|---|
| A | 0 | Flex Data: Tempo 120 BPM, Time Sig 4/4, Key Sig C, Metronome, Chord Name Cmaj7, Start of Clip |
| B | 400..6500 | Sustained C4 with Per-Note Pitch Bend 5 Hz vibrato + Registered PNC #7 + Assignable PNC #74 + Per-Note Management Reset |
| C | 7000..11000 | Chromatic walk C5..G#5 (8 steps × 500 ms): 16-bit velocity, 32-bit CC #74, 32-bit Pitch Bend, 32-bit Poly Pressure |
| D | 11500 | Program 42 with Bank MSB 0x10 LSB 0x05 (single UMP) |
| E | 12500..14000 | RPN 0/0 + NRPN 0x12/0x34 + Relative RPN + Relative NRPN |
| F | 14800..16500 | Note On with Attribute pitch_7_9 (E4 +50¢) |
| G | 17200 | SysEx7 Universal Identity Reply (Start+End) + 18-byte bulk (Start+Continue+End) |
| H | 17700 | DCTPQ 480 + Delta Clockstamp 240 ticks |
| I | 18500 | PE Notify OverlayRate to subscribers |
| J | 19500 | End of Clip |
| K | 20500 | MIDI 1.0 Channel Voice burst (Note On/Off, CC, Pitch Bend, Program) |
| L | 22500 | System burst: Timing Clock, Start, Continue, Stop, Active Sensing, Reset, Tune Request, MTC, Song Select, Song Position |
| M | 25000 | Flex Metadata Text: Project Name, Composer Name |
| N | 26500 | Utility extras: Noop + JR Clock |

JR Timestamp heartbeat (500 ms, MT 0x0) runs always while mounted. LED on `led0` follows USB-ready state. Every scene logs via `printk` to the Zephyr console.

## What this example does NOT do

- No audio synthesis. The device is a wire-format showcase, not a synth.
- No display. The Pi Pico has no on-board display; logs go to the Zephyr console.
- No SysEx8 emission (no host-side support today).
- No host-only Process Inquiry replies (midi2's `midi2_ci` convenience responder does not implement PI replies; use `midi2_ci_dispatch` for finer control).

## License

MIT.
