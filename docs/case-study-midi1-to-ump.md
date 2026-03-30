# Case Study: MIDI 1.0 to UMP Conversion

## The problem

Legacy MIDI instruments (keyboards, synths, controllers, pedals) speak
MIDI 1.0 over serial (DIN-5, TRS, UART at 31.25 kbaud). Modern hosts
expect USB MIDI 2.0 with UMP (Universal MIDI Packets). Bridging these
two worlds requires byte stream parsing, Running Status handling, SysEx
reassembly, and protocol translation.

## The hardware approach

The [MIDI2USB-Converter](https://github.com/collet-instruments/MIDI2USB-Converter)
project by Collet Instruments solves this with a dedicated STM32F401 board:

- Custom PCB with TRS MIDI IN/OUT + USB-C
- STM32F401 (Cortex-M4, 84 MHz, 256 KB Flash, 64 KB RAM)
- FreeRTOS for task scheduling
- TinyUSB for USB device
- AM_MIDI2.0Lib (C++) for MIDI 2.0 protocol
- tusb_ump for USB MIDI 2.0 device class driver
- Physical switch + LEDs for mode selection (MIDI 1.0 / 2.0)

The result is a standalone box that sits between the instrument and the
computer. Plug the old keyboard in one side, USB-C on the other,
the host sees a MIDI 2.0 device.

It works. But it requires a dedicated microcontroller, a custom PCB,
an RTOS, a C++ library, and a separate USB class driver -- all to convert
bytes into UMP words.

## The library approach

midi2 solves the same conversion with `midi2_conv` -- a single C99 module,
no dependencies, no RTOS, no allocation:

```c
#include "midi2_conv.h"

uint8_t sysex_buf[128];
midi2_conv_state conv;
midi2_conv_init(&conv, 0, sysex_buf, sizeof(sysex_buf));

// For each byte from the MIDI 1.0 serial port:
if (midi2_conv_feed(&conv, byte)) {
    // conv.ump[] has the UMP message, ready to send
    send_ump(conv.ump, conv.ump_words);
}
```

That's the entire conversion. Running Status, multi-byte messages, SysEx
accumulation, Real-Time interleaving -- all handled internally. The output
is a standard UMP (MT 0x2) that any MIDI 2.0 stack can consume.

## What changes

| Aspect | Hardware converter | midi2_conv |
|--------|-------------------|------------|
| **Where it runs** | Dedicated STM32 board | Any MCU, any platform, any OS |
| **Dependencies** | FreeRTOS + TinyUSB + AM_MIDI2.0Lib + tusb_ump | None. Only stdint.h. |
| **Language** | C++ | C99 |
| **Memory** | Heap allocation | Caller-provided buffers |
| **Integration** | Separate box between instrument and host | Drop 2 files into your firmware |
| **Cost** | Custom PCB + components + assembly | Zero. Already in your build. |

## Where this matters

Any project that already has a microcontroller with a UART (which is
virtually every embedded project) can add MIDI 1.0 input and convert it to
UMP without extra hardware:

**ESP32 with USB Device:**
A single ESP32-S3 can read MIDI bytes on UART RX, run them through
`midi2_conv_feed()`, and send the resulting UMP words out via TinyUSB as a
USB MIDI 2.0 device. No external converter needed.

**Raspberry Pi Pico / RP2040:**
Same pattern. UART in, `midi2_conv` in the middle, TinyUSB out. The Pico
becomes the converter -- no extra board.

**Multi-protocol hub:**
A device that already speaks USB MIDI 2.0 natively can add a TRS MIDI
input jack and use `midi2_conv` to merge legacy instruments into the same
UMP pipeline. One firmware, one device, both protocols.

## The point

A dedicated MIDI 1.0 to USB MIDI 2.0 converter is a valid product. But the
conversion logic itself is not a hardware problem -- it's a parsing problem.
By keeping it in a portable C99 library with zero dependencies, the
conversion goes wherever the code goes. Any MCU that can read a UART can
speak MIDI 2.0.

The hardware approach requires a separate board, a separate supply chain,
and a separate firmware. The library approach requires two files and six
lines of code.
