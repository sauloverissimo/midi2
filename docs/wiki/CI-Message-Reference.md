# MIDI-CI Message Reference

Developer reference for MIDI-CI messages as implemented by midi2, based on M2-101-UM v1.2 published by [The MIDI Association](https://www.midi.org) and [AMEI](http://www.amei.or.jp).

## Common Header Format

All MIDI-CI messages are carried inside Universal System Exclusive (without F0/F7 when in UMP):

```
[0]  0x7E    Universal System Exclusive
[1]  DevID   0x00-0x0F=channel, 0x7E=group, 0x7F=function block
[2]  0x0D    MIDI-CI Sub-ID#1
[3]  Sub-ID  Message type (see tables below)
[4]  Ver     MIDI-CI Message Version (0x01=v1.1, 0x02=v1.2)
[5..8]       Source MUID (4 bytes, 7-bit LSB first)
[9..12]      Destination MUID (4 bytes, 7-bit LSB first)
[13..]       Message-specific data
```

MUID encoding: 28-bit value split across 4 bytes, 7 bits each, LSB first.
Broadcast MUID: `0x0FFFFFFF` (all bytes = 0x7F).

## Category 7: Management Messages (0x70-0x7F)

| Sub-ID | Message | Source | midi2_ci_msg builder | midi2_ci_dispatch callback |
|--------|---------|--------|---------------------|---------------------------|
| 0x70 | Discovery | Initiator | `midi2_ci_build_discovery` | `on_discovery` |
| 0x71 | Reply to Discovery | Responder | `midi2_ci_build_discovery_reply` | `on_discovery_reply` |
| 0x72 | Endpoint Information | Initiator | `midi2_ci_build_endpoint_info` | `on_endpoint_info` |
| 0x73 | Reply to Endpoint Information | Responder | `midi2_ci_build_endpoint_info_reply` | `on_endpoint_info_reply` |
| 0x7D | ACK | Either | `midi2_ci_build_ack` | `on_ack` |
| 0x7E | Invalidate MUID | Either | `midi2_ci_build_invalidate_muid` | `on_invalidate_muid` |
| 0x7F | NAK | Either | `midi2_ci_build_nak` | `on_nak` |

### Discovery (0x70) Fields

After header: Manufacturer ID (3 bytes), Device Family (2 bytes), Model Number (2 bytes), Software Revision (4 bytes), CI Category Supported (1 byte bitmap), Max SysEx Size (4 bytes), Output Path Id (v2+, 1 byte).

### CI Category Supported Bitmap

| Bit | Category | Sub-ID Range |
|-----|----------|-------------|
| D2 | Profile Configuration | 0x20-0x2F |
| D3 | Property Exchange | 0x30-0x3F |
| D4 | Process Inquiry | 0x40-0x4F |

### NAK Status Codes

| Code | Meaning |
|------|---------|
| 0x00 | NAK (generic) |
| 0x01 | MIDI-CI message not supported |
| 0x02 | MIDI-CI version not supported |
| 0x03 | Channel/Group/FB not in use |
| 0x04 | Profile not supported |
| 0x20 | Terminate PE inquiry |
| 0x21 | PE chunks out of sequence |
| 0x40 | Error, please retry |
| 0x41 | Message was malformed |
| 0x42 | Timeout |
| 0x43 | Busy, retry after wait |

## Category 2: Profile Configuration (0x20-0x2F)

| Sub-ID | Message | Source | midi2_ci_msg builder | midi2_ci_dispatch callback |
|--------|---------|--------|---------------------|---------------------------|
| 0x20 | Profile Inquiry | Initiator | `midi2_ci_build_profile_inquiry` | `on_profile_inquiry` |
| 0x21 | Reply to Profile Inquiry | Responder | `midi2_ci_build_profile_inquiry_reply` | `on_profile_inquiry_reply` |
| 0x22 | Set Profile On | Initiator | `midi2_ci_build_set_profile_on` | `on_set_profile_on` |
| 0x23 | Set Profile Off | Initiator | `midi2_ci_build_set_profile_off` | `on_set_profile_off` |
| 0x24 | Profile Enabled Report | Either | `midi2_ci_build_profile_enabled` | `on_profile_enabled` |
| 0x25 | Profile Disabled Report | Either | `midi2_ci_build_profile_disabled` | `on_profile_disabled` |
| 0x26 | Profile Added Report | Responder | `midi2_ci_build_profile_added` | `on_profile_added` |
| 0x27 | Profile Removed Report | Responder | `midi2_ci_build_profile_removed` | `on_profile_removed` |
| 0x28 | Profile Details Inquiry | Initiator | `midi2_ci_build_profile_details` | `on_profile_details` |
| 0x29 | Reply to Profile Details | Responder | `midi2_ci_build_profile_details_reply` | `on_profile_details_reply` |
| 0x2F | Profile Specific Data | Either | `midi2_ci_build_profile_specific_data` | `on_profile_specific_data` |

### Profile ID Format (5 bytes)

| Byte | Standard (0x7E) | Manufacturer |
|------|----------------|-------------|
| 1 | 0x7E | Manufacturer SysEx ID byte 1 |
| 2 | Profile Bank | Manufacturer SysEx ID byte 2 |
| 3 | Profile Number | Manufacturer SysEx ID byte 3 |
| 4 | Profile Version | Manufacturer Specific |
| 5 | Profile Level | Manufacturer Specific |

## Category 3: Property Exchange (0x30-0x3F)

| Sub-ID | Message | Source | midi2_ci_msg builder | midi2_ci_dispatch callback |
|--------|---------|--------|---------------------|---------------------------|
| 0x30 | PE Capabilities Inquiry | Initiator | `midi2_ci_build_pe_capability` | `on_pe_capability` |
| 0x31 | Reply to PE Capabilities | Responder | `midi2_ci_build_pe_capability_reply` | `on_pe_capability_reply` |
| 0x34 | Get Property Data | Initiator | `midi2_ci_build_pe_get` | `on_pe_get` |
| 0x35 | Reply to Get Property | Responder | `midi2_ci_build_pe_get_reply` | `on_pe_get_reply` |
| 0x36 | Set Property Data | Initiator | `midi2_ci_build_pe_set` | `on_pe_set` |
| 0x37 | Reply to Set Property | Responder | `midi2_ci_build_pe_set_reply` | `on_pe_set_reply` |
| 0x38 | Subscription | Either | `midi2_ci_build_pe_subscribe` | `on_pe_subscribe` |
| 0x39 | Reply to Subscription | Either | `midi2_ci_build_pe_subscribe_reply` | `on_pe_subscribe_reply` |
| 0x3F | Notify | Either | `midi2_ci_build_pe_notify` | `on_pe_notify` |

### PE Data Message Format (shared by 0x34-0x3F)

All PE data messages share this structure after the CI header:

```
[13]     Request ID
[14..15] Header Data Length (14-bit, LSB first)
[16..]   Header Data (JSON, 7-bit encoded)
[..]     Number of Chunks in Message (14-bit)
[..]     Number of This Chunk (14-bit, starts from 1)
[..]     Body Data Length (14-bit)
[..]     Body Data (property content)
```

Chunking rules:
- Header Data only in first chunk (chunk 2+ has header_len = 0)
- num_chunks = 0 means "unknown number of chunks"
- Final chunk: num_chunks = this_chunk (or this_chunk = 0 if data is bad)

## Category 4: Process Inquiry (0x40-0x4F)

| Sub-ID | Message | Source | midi2_ci_msg builder | midi2_ci_dispatch callback |
|--------|---------|--------|---------------------|---------------------------|
| 0x40 | PI Capabilities Inquiry | Initiator | `midi2_ci_build_pi_capability` | `on_pi_capability` |
| 0x41 | Reply to PI Capabilities | Responder | `midi2_ci_build_pi_capability_reply` | `on_pi_capability_reply` |
| 0x42 | MIDI Message Report Inquiry | Initiator | `midi2_ci_build_pi_midi_report` | `on_pi_midi_report` |
| 0x43 | Reply to MIDI Message Report | Responder | `midi2_ci_build_pi_midi_report_reply` | `on_pi_midi_report_reply` |
| 0x44 | End of MIDI Message Report | Responder | `midi2_ci_build_pi_midi_report_end` | `on_pi_midi_report_end` |

### MIDI Message Report Bitmaps

System Messages: D0=MTC Quarter Frame, D1=Song Position, D2=Song Select

Channel Controller: D0=Pitchbend, D1=CC, D2=RPN, D3=NRPN, D4=Program Change, D5=Channel Pressure

Note Data: D0=Notes, D1=Poly Pressure, D2=Per-Note PB, D3=Reg Per-Note Ctrl, D4=Asn Per-Note Ctrl

### Message Data Control Values

| Value | Behavior |
|-------|----------|
| 0x00 | Query capabilities only (Begin/End replies with bitmaps, no data) |
| 0x01 | Report only non-default values and active notes |
| 0x7F | Full report of all supported parameters |

## Deprecated Messages

Category 1: Protocol Negotiation (0x10-0x1F) -- deprecated in MIDI-CI v1.2. Replaced by UMP Stream Configuration messages (MT 0xF, status 0x005/0x006).
