# Hardware Validation

Validated integration of midi2 on real hardware platforms.

## Validation Results

| Platform | MCU | Transport | Device | Host | Status |
|----------|-----|-----------|--------|------|--------|
| ESP32-S3 (ESP-IDF) | ESP32-S3 | TinyUSB | Pass | Pass | Validated |
| ESP32-P4 (ESP-IDF) | ESP32-P4 | TinyUSB | Pass | Pass | Validated |
| ESP32-S3 (Arduino) | ESP32-S3 | TinyUSB | Pass | Pass | Validated |
| Teensy 4.1 | i.MX RT1062 | Native USB | Pass | Pass | Validated |

## What was tested

Each platform validates:

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
