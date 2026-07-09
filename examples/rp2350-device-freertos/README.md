# [midi2](../..) | Device MIDI 2.0
## Raspberry Pi Pico 2 (RP2350), FreeRTOS

Full-spec USB MIDI 2.0 device on the **Raspberry Pi Pico 2 (RP2350)**, pure C99
over **FreeRTOS-Kernel + TinyUSB upstream**. A deterministic UMP catalog emitter
cycles 58 entries covering every defined message-type category of M2-104-UM
v1.1.2 (per-note included, item by item), alongside a MIDI-CI Discovery + Profile
+ Property Exchange responder and a build-time stress loopback mode.

No TinyUSB fork: the USB MIDI 2.0 device drivers this build depends on
(`hathach/tinyusb` PR #3571 and #3738) are merged upstream.

## USB identity

| Field | Value |
|---|---|
| VID:PID | `0xCAFE:0x407A` (TinyUSB educational VID, development only) |
| Product | `RP2350FreeRTOSBench` |
| Manufacturer | `github.com/sauloverissimo` |
| UMP Endpoint Name | `RP2350 FreeRTOS Bench` (`CFG_TUD_MIDI2_EP_NAME`) |
| Function Block | 1 bidirectional, `firstGroup=0`, `numGroups=1` |
| MIDI-CI Manufacturer ID | `{0x7D, 0x00, 0x00}` (educational / non-commercial prefix) |
| MIDI-CI Family / Model / Version | `0x0001 / 0x0001 / 0x00010000` |

## Layering

This is the third runtime integration of the same midi2 C99 core. Where the
Zephyr sibling uses the native `usbd_midi2` class and a workqueue, and the
Arduino sketches run a bare loop, this recipe uses two FreeRTOS tasks and two
static queues. The core stays identical; only the runtime wiring changes.

```
TinyUSB upstream           USB enumeration, descriptors, alt-setting, and the
                           built-in UMP Stream Discovery responder (Endpoint /
                           Function Block / Stream Config), MIDI 2.0 device class.

FreeRTOS pipeline          usb_task (high prio) is the SOLE owner of tud_midi2_*:
  (src/pipeline.c)         it services tud_task_ext(1 ms), drains tx_queue to the
                           device, and frames inbound words into rx_queue on
                           tud_midi2_rx_cb. midi_task (low prio) feeds inbound
                           through midi2_proc and runs the catalog cycle. Two
                           static queues decouple the two tasks; all allocation
                           is static (zero heap).

midi2 (C99)                Typed dispatch, UMP construction, SysEx7 reassembly
  (../../src)              (midi2_proc), MIDI-CI responder (midi2_ci).

src/catalog.c              The enumerated UMP catalog (host-unit-tested).
src/ci_responder.c         MIDI-CI identity, one Profile, DeviceInfo property.
```

```
        rx_queue                              tx_queue
  ┌──────────────────┐                  ┌──────────────────┐
  │ usb_task:        │  tud_midi2_rx_cb │ midi_task:       │
  │  tud_task_ext ───┼── frame words ──▶│  midi2_proc_feed │
  │  drain tx_queue ◀┼── catalog / CI ──┤  emit_catalog    │
  │  tud_midi2_write │                  │  CI replies      │
  └──────────────────┘                  └──────────────────┘
   sole owner of tud_midi2_*             never touches TinyUSB
```

## Build

Requires:

- **Pico SDK 2.x** with `PICO_SDK_PATH` exported (RP2350 / `pico2` board support)
- **FreeRTOS-Kernel v11.2.0 or newer** with the Community-Supported-Ports
  submodule initialized (the RP2350 port `RP2350_ARM_NTZ` lives there):
  `git submodule update --init portable/ThirdParty/Community-Supported-Ports`
- **TinyUSB upstream** (`hathach/tinyusb`, merged MIDI 2.0 device drivers). The
  Pico SDK bundles an older TinyUSB with only the MIDI 1.0 class, so point
  `PICO_TINYUSB_PATH` at an upstream checkout (or let CMake FetchContent pull it).
- **arm-none-eabi-gcc**, **CMake 3.14+**

```bash
PICO_SDK_PATH=/path/to/pico-sdk \
PICO_TINYUSB_PATH=/path/to/tinyusb \
FREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel \
cmake -B build -DPICO_BOARD=pico2
cmake --build build -j
```

Flash `build/rp2350-device-freertos.uf2` onto the Pico 2 in BOOTSEL mode.

## Hardware

| Pin | Use |
|---|---|
| USB-C | MIDI 2.0 device (the only USB function, no CDC stdio) |
| GP0 | UART TX (debug log @ 115200 8N1) |
| GP1 | UART RX |
| BOOTSEL | Hold while plugging USB to enter flash mode |

## Spec coverage

A full-surface UMP emitter plus a MIDI-CI responder. The catalog demonstrates
every defined UMP message-type category; per-note MIDI 2.0 messages appear as
their own entries, not folded into a generic Channel Voice row.

| MT | Category | Catalog entries | Spec |
|---|---|---|---|
| 0x0 | Utility | NOOP, JR Clock, JR Timestamp, DCTPQ, Delta Clockstamp | M2-104-UM 7.2 |
| 0x1 | System RT / Common | MTC, Song Position, Song Select, Tune Request, Clock, Start, Continue, Stop, Active Sensing, Reset | 7.3 |
| 0x2 | MIDI 1.0 Channel Voice | Note Off/On, Poly Pressure, CC, Program, Channel Pressure, Pitch Bend | 7.3 |
| 0x3 | SysEx7 / Data64 | complete + start / continue / end | 7.7 |
| 0x4 | MIDI 2.0 Channel Voice | RPNC, APNC, RPN, NRPN, rel RPN/NRPN, **Per-Note Pitch Bend**, Note Off/On (16-bit vel + attr), Poly Pressure, CC, Program (bank valid), Channel Pressure, Pitch Bend, **Per-Note Management** | 7.4 |
| 0x5 | SysEx8 + Mixed Data Set | complete + start / continue / end, MDS header + payload | 7.8, 7.9 |
| 0xD | Flex Data | Set Tempo, Time Signature, Metronome, Key Signature, Chord Name, Composer Name, Lyrics | 7.5 |
| 0xF | UMP Stream (app-owned) | Device Identity, Start of Clip, End of Clip | 7.6 |

MIDI-CI surface: Discovery, Profile Configuration (one Profile), Property
Exchange (static `DeviceInfo`). The responder runs reactively via `midi2_proc`
SysEx7 reassembly.

**What this recipe does NOT cover (and why):**

- Endpoint / Function Block / Stream Configuration discovery: owned by the
  TinyUSB built-in responder, not emitted by the catalog (emitting them would
  duplicate the driver and invert the host/device direction).
- SysEx8 and Mixed Data Set are present for spec-coverage completeness only.
  SysEx7 / Data64 is the practical demo transport; SysEx8 has no market adoption.

## Modes

| Mode | Trigger | Behavior |
|---|---|---|
| Continuous cycle | mounted, alt=1 | catalog cycles 0..57..0, one entry every 50 ms |
| NoteOn trigger | inbound MIDI 2.0 NoteOn, `group=15` | fires catalog index = note number once |
| CC loop start/stop | inbound CC 120 / 121, `group=15` | pauses / resumes the cycle |
| Stress loopback | `-DSTRESS_LOOPBACK=ON` build | suspends the cycle, echoes every inbound UMP verbatim |

Group 15 is a control sentinel: the device advertises only group 0, so group 15
never collides with musical traffic.

## Stress

See [stress/results.md](stress/results.md). The stress build echoes inbound UMP
through the static-queue pipeline; the host harness `stress/stress_ump.py`
measures gaps, ordering, and throughput to demonstrate zero-loss under load.

## What lives where

```
rp2350-device-freertos/
├── README.md              this file
├── CMakeLists.txt         Pico SDK + FreeRTOS-Kernel + TinyUSB upstream
├── FreeRTOSConfig.h       static allocation, single-core, M33 port config
├── src/
│   ├── main.c             board + USB init, starts the scheduler
│   ├── pipeline.c/.h      usb_task + midi_task, static rx/tx queues
│   ├── catalog.c/.h       the enumerated UMP catalog (host-testable)
│   ├── ci_responder.c/.h  MIDI-CI responder
│   ├── freertos_hooks.c   static idle/timer task memory providers
│   ├── usb_descriptors.c  USB MIDI 2.0 descriptors
│   └── tusb_config.h      TinyUSB config
├── test/                  host unit tests for the catalog (ctest)
└── stress/                loopback harness + results
```

## License

MIT, inherits the parent midi2 library. TinyUSB and FreeRTOS-Kernel retain their
own licenses.
