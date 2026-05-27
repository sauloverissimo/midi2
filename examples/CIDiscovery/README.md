# [midi2](../..) | CIDiscovery

Arduino sketch that runs the MIDI-CI Discovery responder standalone, with no USB transport in the loop. Useful as a starting point for any application that wants to advertise itself over MIDI-CI without first bringing up USB.

## What it demonstrates

1. **Initialise `midi2_ci_state`** with caller-provided storage for profiles and properties (zero allocation).
2. **Register identity**: manufacturer, family, model, software revision.
3. **Register one custom Profile**.
4. **Feed an inbound Discovery Inquiry** (Universal SysEx with CI sub-id 0x70).
5. **Watch the auto-generated Discovery Reply** flow back through `platform_write`, here just dumped over `Serial` as hex words.

## Build

Arduino IDE or arduino-cli, with the midi2 library installed. Open `CIDiscovery.ino` and Upload.

## Expected output

```
=== midi2 CIDiscovery ===
Identity + 1 custom profile registered. Sending fake Discovery Inquiry.
Reply UMP words: 0x30164B7E 0x7F0D70...
Auto-reply fired.
```

The hex words are the MT 0x3 SysEx7 packets carrying the Discovery Reply, fragmented as needed.

## Plugging into a real transport

`platform_write` is the seam. Replace its body with the USB MIDI 2.0 / BLE-MIDI / Network MIDI 2.0 send call for the target platform. Incoming SysEx from the host then goes into `midi2_ci_process_sysex` after reassembly (see `midi2_proc` for SysEx7 reassembly helpers).

## License

MIT.
