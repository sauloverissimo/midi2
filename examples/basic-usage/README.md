# [midi2](../..) | basic-usage

Arduino sketch that exercises the four core surfaces of midi2 on any Arduino-compatible board. No MIDI transport required: the sketch builds messages, parses them, and prints decoded events over `Serial`.

## What it demonstrates

1. **UMP construction** with `midi2_msg_note_on`, `midi2_msg_cc`, `midi2_msg_tempo`.
2. **Value scaling** between 7-bit, 14-bit, 16-bit and 32-bit, round-trip safe (M2-115-U).
3. **Typed dispatch** via `midi2_dispatch_feed` into `on_note_on`, `on_cc`, `on_tempo` callbacks.
4. **SysEx7 reassembly + group filtering** via `midi2_proc_state`, chained into the dispatcher.

The dispatcher updates `g_last_tempo_bpm` whenever a Flex Data Tempo arrives; the main `loop()` prints it once per second.

## Build

Arduino IDE or arduino-cli, with the midi2 library installed:

```
Sketch -> Include Library -> Manage Libraries -> midi2
```

Or drop a clone of this repo into `~/Arduino/libraries/midi2/`. Open `basic-usage.ino` and Upload.

## Validated on

- Teensy 4.x (`architectures=teensy4` in `library.properties`).
- Host-side `g++` builds via the same source tree (the `.ino` is valid C++).

## Plugging into a real transport

The sketch does not call out to USB MIDI by itself. To put it on a wire, register a write callback on the platform USB MIDI driver (TinyUSB MIDI 2.0 fork, Teensy core USB MIDI 2.0 fork, ESP32 native, BLE-MIDI 2.0) and forward outbound UMP words there.

## License

MIT.
