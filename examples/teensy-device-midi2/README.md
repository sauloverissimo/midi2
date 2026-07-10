# [midi2](../..) | Device MIDI 2.0
## Teensy 4.x (Arduino)

A complete USB MIDI 2.0 device in a single Arduino sketch. It runs the portable
midi2 C99 core over the Teensy core's native UMP endpoint (`usbMIDI2`) and
answers MIDI-CI Discovery, Profile Configuration and Property Exchange. A
one-note demo emits a MIDI 2.0 Note On every second so a host sees live traffic.

This is the loop-based sibling of the RTOS recipes: no threads, no queues. The
Teensy `loop()` reads inbound UMP, feeds `midi2_proc` for SysEx7 reassembly, and
the MIDI-CI responder replies inline. The C99 core is identical across all
recipes; only the transport wiring changes.

```
Teensy core (fork)   USB enumeration, USB MIDI 2.0 descriptors, alt-setting, and
                     the wire-level UMP endpoint (usbMIDI2.read / usbMIDI2.write).

midi2 (C99)          SysEx7 reassembly (midi2_proc), MIDI-CI responder (midi2_ci),
  (../../src)        typed UMP construction (midi2_msg).

sketch (this .ino)   identity, one Profile, three Property Exchange resources,
                     the read/feed loop, and the demo note.
```

## Requirements

- Teensy 4.0 or 4.1
- A Teensy core build that ships the USB MIDI 2.0 (`usbMIDI2`) endpoint
- Arduino IDE or arduino-cli with the Teensyduino core installed

In the Arduino IDE set **Tools > USB Type > "MIDI 2.0"**. With arduino-cli the
matching option is `usb=midi2`:

```bash
arduino-cli compile --fqbn teensy:avr:teensy41:usb=midi2 \
  --library /path/to/midi2 examples/teensy-device-midi2
arduino-cli upload  --fqbn teensy:avr:teensy41:usb=midi2 -p <port> \
  examples/teensy-device-midi2
```

## Identity

| Field | Value |
|---|---|
| MIDI-CI Manufacturer ID | `{0x7D, 0x00, 0x00}` (non-commercial prefix) |
| MIDI-CI Family / Model / SW rev | `0x0001 / 0x0001 / 0x00070000` |
| Profile | `{0x7E, 0x00, 0x00, 0x01, 0x00}` |
| Property Exchange | `DeviceInfo`, `ChannelList`, `ProgramList` |

`0x7D` is the reserved non-commercial Manufacturer ID prefix. A shipping product
would carry a Manufacturer ID assigned by the MIDI Association. The same values
appear in the MIDI-CI Discovery reply and in the `DeviceInfo` PE resource.

## Validation

Validated against the official MIDI 2.0 Workbench (midi2-dev): Discovery replies
with MIDI-CI version `0x02`, Profile Configuration lists the one Profile, and
Property Exchange serves all three resources with no timeouts or warnings.

## License

MIT, inherits the parent midi2 library.
