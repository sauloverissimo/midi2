# Hardware Validation

Validated integration of midi2 on real hardware platforms.

## Validation Results

| Platform | MCU | Transport | Device | Host | TinyUSB |
|----------|-----|-----------|--------|------|---------|
| ESP32-S3 (ESP-IDF) | ESP32-S3 | TinyUSB | ✅ | ✅ | ✅ |
| ESP32-P4 (ESP-IDF) | ESP32-P4 | TinyUSB | ✅ | ✅ | ✅ |
| ESP32-S3 (Arduino) | ESP32-S3 | TinyUSB | ✅ | ✅ | ✅ |
| Teensy 4.1 | i.MX RT1062 | Native USB | ✅ | ✅ | -- |
| RP2040 | RP2040 | TinyUSB | ✅ | -- | ✅ |
| RP2350 | RP2350 | TinyUSB + PIO-USB | ✅ | ✅ | ✅ |
| Daisy Seed | STM32H750 | TinyUSB | 🔜 | 🔜 | 🔜 |
| Arduino Pro Micro | ATmega32U4 | HID USB | 🔜 | -- | -- |
| Arduino Leonardo | ATmega32U4 | HID USB | 🔜 | -- | -- |
| Arduino Uno | ATmega328P | Serial MIDI | 🔜 | -- | -- |
| Windows | x86_64 | MSVC build + tests | ✅ | -- | -- |
| Linux | x86_64 | gcc/clang build + tests | ✅ | -- | -- |

**Legend**: ✅ Pass | 🔜 Coming soon | -- Not applicable

## What was tested

Each validated platform covers:

**Device Mode**
- UMP Note On/Off roundtrip (build, send, receive on host)
- UMP CC with 32-bit resolution
- SysEx7 fragmentation and reassembly
- Value scaling (7-bit legacy to 16/32-bit MIDI 2.0)
- Endpoint Discovery response
- Function Block Info response

**Host Mode**
- Enumerate connected MIDI 2.0 device
- Receive and dispatch UMP messages
- SysEx7 reassembly from device
- Group filtering
