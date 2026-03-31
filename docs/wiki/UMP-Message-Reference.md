# UMP Message Reference

Developer reference for Universal MIDI Packet message types as implemented by midi2, based on M2-104-UM v1.1.2 published by [The MIDI Association](https://www.midi.org) and [AMEI](http://www.amei.or.jp).

## Message Types Overview

| MT | Name | Words | midi2_msg function prefix | midi2_dispatch callback |
|----|------|-------|--------------------------|------------------------|
| 0x0 | Utility | 1 | `midi2_msg_jr_*`, `midi2_msg_dctpq`, `midi2_msg_delta_clockstamp` | `on_noop`, `on_jr_clock`, `on_jr_timestamp`, `on_dctpq`, `on_dc` |
| 0x1 | System | 1 | `midi2_msg_system*` | `on_system` |
| 0x2 | MIDI 1.0 CV | 1 | `midi2_msg_from_midi1` | `on_cv1_note_on/off`, `on_cv1_cc`, etc. |
| 0x3 | SysEx7 | 2 | `midi2_msg_sysex7_packet` | `on_sysex7` |
| 0x4 | MIDI 2.0 CV | 2 | `midi2_msg_note_on/off`, `midi2_msg_cc`, etc. | `on_note_on/off`, `on_cc`, etc. |
| 0x5 | Data 128 | 4 | `midi2_msg_sysex8_packet`, `midi2_msg_mds_*` | `on_sysex8`, `on_mds_header/payload` |
| 0x6-0xC | Reserved | -- | -- | `on_unknown` |
| 0xD | Flex Data | 4 | `midi2_msg_tempo`, `midi2_msg_time_sig`, etc. | `on_tempo`, `on_time_sig`, etc. |
| 0xE | Reserved | -- | -- | `on_unknown` |
| 0xF | Stream | 4 | `midi2_msg_stream_*` | `on_endpoint_*`, `on_fb_*`, `on_config_*`, `on_clip` |

## MT 0x0: Utility Messages (1 word)

```
Word: [MT:4][group:4][status:4][data:20]
```

| Status | Name | Data field | midi2_msg |
|--------|------|-----------|-----------|
| 0x0 | NOOP | (ignored) | -- |
| 0x1 | JR Clock | 16-bit timestamp | `midi2_msg_jr_clock(group, ts)` |
| 0x2 | JR Timestamp | 16-bit timestamp | `midi2_msg_jr_timestamp(group, ts)` |
| 0x3 | Delta Clockstamp TPQ | 16-bit ticks/QN | `midi2_msg_dctpq(tpq)` |
| 0x4 | Delta Clockstamp | 20-bit ticks | `midi2_msg_delta_clockstamp(ticks)` |

## MT 0x1: System Messages (1 word)

```
Word: [MT:4][group:4][status:8][data1:8][data2:8]
```

| Status | Name |
|--------|------|
| 0xF1 | MIDI Time Code Quarter Frame |
| 0xF2 | Song Position Pointer |
| 0xF3 | Song Select |
| 0xF6 | Tune Request |
| 0xF8 | Timing Clock |
| 0xF9 | Tick |
| 0xFA | Start |
| 0xFB | Continue |
| 0xFC | Stop |
| 0xFE | Active Sensing |
| 0xFF | System Reset |

## MT 0x2: MIDI 1.0 Channel Voice (1 word)

```
Word: [MT:4][group:4][status:4][channel:4][data1:8][data2:8]
```

7-bit values. Same semantics as classic MIDI 1.0.

## MT 0x3: SysEx7 (2 words)

```
Word 0: [MT:4][group:4][status:4][numBytes:4][data0:8][data1:8]
Word 1: [data2:8][data3:8][data4:8][data5:8]
```

Status: 0x00=Complete, 0x10=Start, 0x20=Continue, 0x30=End. Max 6 data bytes per packet.

## MT 0x4: MIDI 2.0 Channel Voice (2 words)

```
Word 0: [MT:4][group:4][status:4][channel:4][byte3:8][byte4:8]
Word 1: [32-bit data]
```

| Status | Name | Word 1 content |
|--------|------|---------------|
| 0x80 | Note Off | [velocity:16][attribute:16] |
| 0x90 | Note On | [velocity:16][attribute:16] |
| 0xA0 | Poly Pressure | [32-bit pressure] |
| 0xB0 | Control Change | [32-bit value] |
| 0xC0 | Program Change | [B:1][program:7][reserved:8][bank_msb:8][bank_lsb:8] |
| 0xD0 | Channel Pressure | [32-bit pressure] |
| 0xE0 | Pitch Bend | [32-bit value, center=0x80000000] |
| 0x00 | Registered Per-Note Ctrl | [32-bit value] |
| 0x10 | Assignable Per-Note Ctrl | [32-bit value] |
| 0x20 | RPN | [32-bit value] |
| 0x30 | NRPN | [32-bit value] |
| 0x40 | Relative RPN | [32-bit signed] |
| 0x50 | Relative NRPN | [32-bit signed] |
| 0x60 | Per-Note Pitch Bend | [32-bit value] |
| 0xF0 | Per-Note Management | [flags in byte4] |

## MT 0x5: Data 128-bit (4 words)

### SysEx8 (status 0x0-0x3)

```
Word 0: [MT:4][group:4][status:4][numBytes:4][streamID:8][data0:8]
Words 1-3: [data bytes]
```

Max 13 data bytes per packet. 8-bit data (no 7-bit restriction).

### Mixed Data Set (status 0x8/0x9)

Header (0x8): [MT:4][group:4][0x8|mds_id:8][numBytes:16] + chunk info + mfr/device/sub IDs

Payload (0x9): [MT:4][group:4][0x9|mds_id:8][14 data bytes]

## MT 0xD: Flex Data (4 words)

```
Word 0: [MT:4][group:4][format:2][address:2][channel:4][bank:8][status:8]
Words 1-3: [message-specific data]
```

| Bank | Status | Name |
|------|--------|------|
| 0x00 | 0x00 | Set Tempo (10ns/QN in word 1) |
| 0x00 | 0x01 | Set Time Signature |
| 0x00 | 0x02 | Set Metronome |
| 0x00 | 0x05 | Set Key Signature |
| 0x00 | 0x06 | Set Chord Name |
| 0x01 | 0x00-0x0C | Metadata Text (13 subtypes: project, composer, copyright, etc.) |
| 0x02 | 0x00-0x04 | Performance Text (lyrics, language, ruby) |

## MT 0xF: UMP Stream Messages (4 words)

```
Word 0: [MT:4][format:2][status:10][data:16]
Words 1-3: [message-specific data]
```

| Status | Name |
|--------|------|
| 0x000 | Endpoint Discovery |
| 0x001 | Endpoint Info Notification |
| 0x002 | Device Identity Notification |
| 0x003 | Endpoint Name Notification |
| 0x004 | Product Instance ID Notification |
| 0x005 | Stream Configuration Request |
| 0x006 | Stream Configuration Notification |
| 0x010 | Function Block Discovery |
| 0x011 | Function Block Info Notification |
| 0x012 | Function Block Name Notification |
| 0x020 | Start of Clip |
| 0x021 | End of Clip |
