# [midi2](../..) | Device MIDI 2.0
## Teensy 4.x (Arduino)

[![Compliant with MIDI 2.0 Workbench](https://img.shields.io/badge/MIDI%202.0%20Workbench-compliant-0d9488?labelColor=17151f)](https://github.com/midi2-dev/MIDI2.0Workbench)

USB MIDI 2.0 device on the **Teensy 4.1** (Cortex-M7 @ 600 MHz, 1 MB SRAM) in a
single Arduino sketch. The portable midi2 C99 core runs directly over the
Teensy cores fork UMP endpoint (`usbMIDI2.read/write`): a bare `loop()`, no
RTOS, no C++ wrapper. It announces the UMP Stream identity, answers MIDI-CI
Discovery, Profile Configuration, Property Exchange and Process Inquiry, and
emits a one-note MIDI 2.0 demo so a host sees live traffic.

> ![override](https://img.shields.io/badge/-cores%20fork-purple.svg) **Teensy cores fork required.** Built against [`sauloverissimo/cores`](https://github.com/sauloverissimo/cores/tree/feature/usb-midi2-descriptors) branch `feature/usb-midi2-descriptors` (USB MIDI 2.0 descriptor + raw UMP I/O primitives, not yet submitted upstream). Overlay `teensy4/usb.c`, `teensy4/usb_desc.{c,h}`, `teensy4/usb_midi2.{c,h}` onto your Teensyduino install at `~/.arduino15/packages/teensy/hardware/avr/<version>/cores/teensy4/`. The fork is transport only: all UMP parsing, MIDI-CI and Stream logic lives in this sketch through the midi2 library.

## USB identity

| Field | Value |
|---|---|
| VID:PID | `16C0:0485` (PJRC `USB_TYPE = MIDI2` slot under the V-USB shared VID) |
| Manufacturer / Product | Teensyduino core defaults |
| UMP Endpoint Name | `Teensy 4.1 MIDI 2.0` (in-band UMP Stream notification) |
| Product Instance ID | `midi2.diy-teensy-0001` |
| FB 0 | `Main` (Bidirectional, 1 group, MIDI 1.0 + 2.0 protocols) |
| MIDI-CI Manufacturer ID | `{0x7D, 0x00, 0x00}` (non-commercial prefix) |
| MIDI-CI Family / Model / SW rev | `0x0001 / 0x0001 / 0x00070000` |
| Profile | `{0x7E, 0x00, 0x00, 0x01, 0x00}`, always on |
| Property Exchange | `DeviceInfo`, `ChannelList`, `ProgramList` |

`0x7D` is the reserved non-commercial Manufacturer ID prefix. A shipping
product carries a Manufacturer ID assigned by the MIDI Association and its own
VID:PID per USB-IF rules.

## Build

Requires Arduino IDE 2.x or arduino-cli with Teensyduino 1.60+, the cores fork
overlaid (see note above), and the midi2 library on the sketchbook.

```bash
arduino-cli compile --fqbn teensy:avr:teensy41:usb=midi2 .
arduino-cli upload  --fqbn teensy:avr:teensy41:usb=midi2 -p <port> .
```

In the Arduino IDE: Tools > USB Type > "MIDI 2.0" before pressing Upload.

## Layering

| Layer | Owns |
|---|---|
| Teensy cores fork | enumeration, MIDI 2.0 descriptors (alt 0 = MIDI 1.0, alt 1 = UMP), wire-level endpoint (`usbMIDI2.read/write`) |
| midi2 C99 core ([`../../src`](../../src)) | SysEx7 reassembly (`midi2_proc`), MIDI-CI responder (`midi2_ci`), UMP Stream and Channel Voice builders (`midi2_msg`) |
| this sketch | identity, UMP Stream announcement, one Profile, three Property Exchange resources, the read/feed loop, the demo note |

The UMP Stream identity (Endpoint Info, Device Identity, Endpoint Name,
Product Instance ID, Function Block Info) is announced from `setup()`. Hosts
that read only the static USB descriptors (Linux `snd-usb-midi2`) enumerate
without it; hosts that run active UMP Endpoint Discovery (Windows MIDI
Services) need these in-band notifications to register a complete endpoint.

## Validation

```bash
lsusb | grep 16c0:0485          # Teensyduino MIDI
aconnect -l                     # client N: 'Teensy MIDI 2.0' [type=kernel]
amidi -l                        # IO hw:N,1,0 Group 1 (Group 1-1)
ls /dev/snd/umpC*D0             # raw UMP stream
```

Validated against the official [MIDI 2.0 Workbench](https://github.com/midi2-dev/MIDI2.0Workbench)
on hardware: MIDI-CI Discovery replies with Message Version 0x02, Profile
Configuration lists the Profile, Property Exchange serves all three resources
with `totalCount` headers, zero errors and zero warnings. Set Profile On/Off
answers with Profile Enabled reports, MIDI Message Report answers with
Reply/End, and unlisted profiles or unused channels are NAKed, all verified
over native UMP.

## Spec coverage

| Surface | Spec | Notes |
|---|---|---|
| 0x3 SysEx7 | M2-104-UM 7.7 | inbound reassembly via `midi2_proc`, outbound MIDI-CI replies |
| 0x4 MIDI 2.0 CV | M2-104-UM 7.4 | demo Note On/Off, 16-bit velocity |
| 0xF UMP Stream | M2-104-UM 7.1 | Endpoint Info, Device Identity, Endpoint Name, Product Instance ID, FB Info (app-announced) |
| MIDI-CI Discovery | M2-101-UM | Message Version 0x02, Function Block addressing |
| Profile Configuration | M2-101-UM | inquiry + Set Profile On/Off with Profile Enabled report |
| Property Exchange | M2-105-UM | `DeviceInfo` (ID arrays + strings), `ChannelList`, `ProgramList`, built-in `ResourceList`, `totalCount` on list resources |
| Process Inquiry | M2-101-UM | Capabilities + MIDI Message Report (Reply / End) |

## Demo

A one-note MIDI 2.0 demo (C4, 16-bit velocity `0xC000`) fires once per second
from `loop()`, so any host monitor shows native MT 0x4 traffic immediately.

## License

MIT, inherits the parent midi2 library.
