# Hardware Validation

Validated integration of midi2 on real hardware platforms.

## Validation Results

| Platform | MCU | Transport | Device | Host |
|----------|-----|-----------|--------|------|
| T-Display S3 (ESP-IDF) | ESP32-S3 | TinyUSB, ESP-NOW, BLE, UART, USB-OTG | ✅ | ✅ |
| T-Display S3 Amoled (ESP-IDF) | ESP32-S3 | TinyUSB, ESP-NOW, BLE, UART, USB-OTG | ✅ | ✅ |
| Waveshare ESP32-P4 (ESP-IDF) | ESP32-P4 | TinyUSB, BLE | ✅ | ✅ |
| T-PicoC3 (SDK-Pico) | RP2040 | TinyUSB | ✅ | -- |
| T-PicoC3 (ESP-IDF) | ESP32-C3 | TinyUSB | ✅ | -- |
| ESP32-S3 (Arduino) | ESP32-S3 | TinyUSB, BLE | ✅ | ✅ |
| Teensy 4.1 | i.MX RT1062 | Native USB | ✅ | ✅ |
| Raspberry Pi Pico | RP2040 | TinyUSB | ✅ | -- |
| Waveshare RP2040-Zero | RP2040 | TinyUSB | ✅ | -- |
| Raspberry Pi Pico 2 | RP2350 | TinyUSB, PIO-USB | ✅ | ✅ |
| Daisy Seed | STM32H750 | STM32 HAL USB | ✅ | -- |
| ESP32-C6 | ESP32-C6 | TinyUSB, BLE | 🔜 | -- |
| Nordic nRF52840 | nRF52840 | TinyUSB, BLE | 🔜 | -- |
| Adafruit Feather RP2040 | RP2040 | TinyUSB, BLE | 🔜 | -- |
| Xiao SAMD21 | SAMD21 | TinyUSB | 🔜 | -- |
| Xiao Renesas RA4M1 | RA4M1 | TinyUSB | 🔜 | -- |
| Windows | x86_64 | MSVC build + tests | ✅ | -- |
| Linux | x86_64 | gcc/clang build + tests | ✅ | -- |

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
