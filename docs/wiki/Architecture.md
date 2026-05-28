# Architecture

## The 4-Layer Model

midi2 is layer 2 in a 4-layer MIDI 2.0 stack:

```
Layer 1: Transport     Hardware drivers (TinyUSB, BLE, Serial, Network)
Layer 2: midi2         Portable C infrastructure (THIS LIBRARY)
Layer 3: Platform      Ecosystem wrapper (ESP32, Teensy, etc.)
Layer 4: Application   Your synth, controller, or tool
```

### Layer 1: Transport

Moves raw UMP words (32-bit integers) between hardware and software. Transport knows nothing about music.

| Transport | Platforms | Protocol |
|-----------|-----------|----------|
| TinyUSB MIDI 2.0 | ESP32-S3, RP2040, RP2350, STM32, SAMD | USB Device + Host |
| Teensy native USB | Teensy 4.x (NXP i.MX RT) | USB Device |
| USBHost_t36 | Teensy 4.x | USB Host |
| PIO-USB | RP2040, RP2350 | Software USB Host |
| ESP-IDF USB Host | ESP32-S3, ESP32-P4 | USB Host |
| BLE MIDI | nRF52, ESP32, any BLE SoC | Bluetooth Low Energy |
| Network MIDI 2.0 | Any with networking | UDP (M2-124-UM) |
| Serial DIN/TRS | Any with UART | Classic MIDI 1.0 byte stream |

### Layer 2: midi2 (this library)

Seven modules, each with a clear responsibility:

```
midi2_msg.h              Stateless: construct, parse, scale UMP words
  |
  +-- midi2_dispatch     Typed dispatch: 49 callbacks for UMP messages
  +-- midi2_proc         Pipeline: group filter, SysEx reassembly, remap
  +-- midi2_conv         Conversion: MIDI 1.0 byte stream -> UMP
  +-- midi2_ci_msg       Stateless: construct, parse CI SysEx payloads
       |
       +-- midi2_ci_dispatch  Typed dispatch: 33 callbacks for CI messages
       +-- midi2_ci           Convenience: auto-responder for simple devices
```

Key properties:
- Pure C99, no C++ required
- Zero heap allocation (caller provides all buffers)
- No threads, no OS dependencies
- Compiles on AVR, ARM Cortex-M, x86, RISC-V, XMOS xcore

### Layer 3: Platform

A C++ wrapper that adapts midi2 to a specific hardware ecosystem. The platform:
- Creates the transport connection
- Feeds raw UMP words into midi2
- Translates midi2 dispatch callbacks into developer-friendly API
- Handles platform-specific concerns (RTOS tasks, events, interrupts)

### Layer 4: Application

The end user's code. Sees only the platform API. Never includes midi2 headers directly.

## Data Flow

### Receiving a MIDI 2.0 message

```
USB endpoint interrupt (Layer 1)
  -> tud_midi2_rx_cb()
    -> tud_midi2_ump_read(words)        [raw UMP words]
      -> midi2_proc_feed(&proc, words)  [Layer 2: filter, reassemble]
        -> midi2_dispatch_feed(words, wc, &dp)  [Layer 2: route by MT]
          -> dp.on_note_on(group, ch, note, vel, ...)  [Layer 2: callback]
            -> platform.fireNoteOn(ch, note, vel)  [Layer 3: adapt]
              -> user.onNoteOn(ch, note, vel)  [Layer 4: application]
```

### Sending a MIDI 2.0 message

```
user.sendNoteOn(ch, note, vel)          [Layer 4]
  -> platform.sendNoteOn(ch, note, vel) [Layer 3]
    -> midi2_msg_note_on(w, group, ch, note, vel, attr) [Layer 2: construct]
      -> tud_midi2_ump_write(w, 2)      [Layer 1: transmit]
```

### Receiving a MIDI-CI message

```
SysEx7 reassembled by midi2_proc       [Layer 2: reassembly]
  -> proc.on_sysex7(group, data, len)
    -> midi2_ci_dispatch_feed(&ci_dp, group, data, len)  [Layer 2: parse CI]
      -> ci_dp.on_discovery(hdr, mfr, family, ...)  [Layer 2: callback]
        -> platform decides: build reply using midi2_ci_build_discovery_reply()
          -> midi2_proc_send_sysex7(group, reply, len, write_fn, ctx) [Layer 2: send]
```

## Module Dependencies

```
                    midi2_msg.h (no dependencies)
                   /      |       \          \
          dispatch    proc      conv      ci_msg.h (no dependencies)
                                          /         \
                                   ci_dispatch      ci (responder)
```

No circular dependencies. Each module can be used independently (except ci_dispatch and ci which depend on ci_msg).

## Memory Model

midi2 never calls `malloc`. All state is in caller-provided structs:

| Module | State struct | Typical size (32-bit) |
|--------|--------------|-----------------------|
| midi2_msg | None (stateless) | 0 bytes |
| midi2_dispatch | `midi2_dispatch` (function pointers) | ~180 bytes |
| midi2_proc | `midi2_proc_state` + SysEx buffers | ~60 bytes + buffers |
| midi2_conv | `midi2_conv_state` (6-byte internal SysEx) | ~30 bytes |
| midi2_ci_msg | None (stateless) | 0 bytes |
| midi2_ci_dispatch | `midi2_ci_dispatch` (function pointers) | ~140 bytes |
| midi2_ci | `midi2_ci_state` + profile/property arrays | ~60 bytes + arrays |

Total overhead for a full stack (all modules): ~470 bytes + user-chosen buffers.

## Continuous Integration

Every push triggers 12 parallel CI jobs verifying portability and correctness:

**Desktop (compile + run 350 tests):**
- gcc (Linux x64) -- primary compiler
- clang (Linux x64) -- catches different warning classes
- Apple clang (macOS) -- Darwin compatibility
- MSVC (Windows) -- Microsoft C11 mode
- gcc 32-bit (Linux x86) -- verifies no 64-bit assumptions in pointer/integer math
- gcc + AddressSanitizer + UndefinedBehaviorSanitizer -- catches buffer overflows, use-after-free, signed overflow, null dereference, and other memory/UB bugs at runtime

**Embedded (cross-compile all modules):**
- ARM Cortex-M4 (`arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb`) -- covers ESP32-S3, STM32, nRF52, SAMD, Daisy
- AArch64 (`aarch64-linux-gnu-gcc`) -- covers Raspberry Pi, 64-bit ARM SoCs
- RISC-V 64 (`riscv64-linux-gnu-gcc`) -- covers ESP32-C3/C6, future RISC-V platforms
- ESP32 component check (`gcc -DESP_PLATFORM`) -- verifies ESP-IDF component compatibility
- AVR ATmega328P (`avr-gcc`) -- verifies header-only modules on 8-bit MCU (Arduino Uno/Nano)
- Zephyr native_sim/native/64 (`west` + Zephyr SDK) -- builds and runs the smoke consumer, confirms the west module integration
