# MIDI 2.0 Specification Guide

Complete reference of all MIDI 2.0 specifications published by the MIDI Association (MA) and AMEI, organized by role in the ecosystem.

## Core Specifications

These 4 documents plus the overview define the foundation of MIDI 2.0. A device must implement at least one path (A or B) from M2-100-U Section 5 to claim MIDI 2.0 compatibility.

| Document | Title | Version | Published | midi2 Coverage |
|----------|-------|---------|-----------|----------------|
| **M2-100-U** | MIDI 2.0 Specification Overview | v1.1 | June 2023 | Reference only (defines minimum requirements) |
| **M2-101-UM** | MIDI Capability Inquiry (MIDI-CI) | v1.2 | June 2023 | **100%** -- all 31 messages, 4 categories |
| **M2-102-U** | Common Rules for MIDI-CI Profiles | v1.1 | June 2023 | Partial -- CI Profile messages covered; behavior rules are platform-level |
| **M2-103-UM** | Common Rules for MIDI-CI Property Exchange | v1.2 | June 2023 | Partial -- CI PE messages covered; JSON schema interpretation is platform-level |
| **M2-104-UM** | UMP Format and MIDI 2.0 Protocol | v1.1.2 | Oct 2023 | **100%** -- all message types MT 0x0 through MT 0xF |

### M2-100-U: MIDI 2.0 Specification Overview

The master document. Defines what constitutes "MIDI 2.0 compatible" and lists the minimum requirements:

**Path A (MIDI-CI):** Implement MIDI-CI discovery + at least one of: Profile Configuration, Property Exchange, or Process Inquiry.

**Path B (UMP):** Implement UMP endpoint discovery + at least one of: MIDI 2.0 Channel Voice, JR Timestamps, SysEx8, or Mixed Data Set.

A device can implement both paths.

### M2-101-UM: MIDI-CI Specification

Defines the bidirectional negotiation protocol. 31 message types in 4 categories:

| Category | Sub-ID Range | Messages | Purpose |
|----------|-------------|----------|---------|
| Management | 0x70-0x7F | Discovery, Reply, Endpoint Info, Invalidate MUID, ACK, NAK | Device identification, MUID lifecycle |
| Profile Configuration | 0x20-0x2F | Inquiry, Reply, Set On/Off, Enabled/Disabled, Added/Removed, Details, Specific Data | Negotiate device behavior profiles |
| Property Exchange | 0x30-0x3F | Capabilities, Get/Set + Reply, Subscribe/Reply, Notify | Get/set device properties via JSON |
| Process Inquiry | 0x40-0x4F | Capabilities, MIDI Message Report + Reply + End | Discover current state of MIDI parameters |

**Minimum requirements** (Appendix E): Every MIDI-CI device shall: respond to Discovery, respond to Invalidate MUID, be able to send NAK.

**Deprecated**: Protocol Negotiation (0x10-0x1F) -- replaced by UMP Stream Configuration in M2-104-UM.

### M2-102-U: Common Rules for Profiles

Defines how MIDI-CI Profiles work: what a Profile specification must contain, how Enabled/Disabled states work, multi-channel profiles, and the Profile ID format (5 bytes: 0x7E for standard profiles, or manufacturer SysEx ID for proprietary).

**Not protocol** -- defines behavior rules. midi2 provides the CI messages for Profile negotiation; the rules are implemented at the platform/application layer.

### M2-103-UM: Common Rules for Property Exchange

Defines how Property Exchange works: JSON header format, Resource naming, chunking rules, subscription model, and the Property Exchange version field.

**Key concept**: PE uses JSON for headers and sometimes for body data. JSON parsing is intentionally outside midi2's scope (zero-allocation constraint). The platform layer chooses a JSON parser.

### M2-104-UM: UMP Format and MIDI 2.0 Protocol

The biggest core spec. Defines:

| Section | Content |
|---------|---------|
| 5. Data Format | 16 Message Types (MT 0x0-0xF), word sizes, group field |
| 7.1 Utility (MT 0x0) | NOOP, JR Clock, JR Timestamp, Delta Clockstamp |
| 7.2 System (MT 0x1) | Timing Clock, Start, Stop, Continue, Song Position, etc. |
| 7.3 MIDI 1.0 CV (MT 0x2) | Legacy messages in UMP wrapper |
| 7.4 MIDI 2.0 CV (MT 0x4) | Note On/Off (16-bit vel), CC (32-bit), RPN/NRPN, Per-Note |
| 7.5 Flex Data (MT 0xD) | Tempo, Time Sig, Key Sig, Metronome, Chord, Text |
| 7.6-7.7 SysEx7 (MT 0x3) | 7-bit System Exclusive |
| 7.8 SysEx8 (MT 0x5) | 8-bit System Exclusive |
| 7.9 Mixed Data Set (MT 0x5) | Non-MIDI payloads (firmware, XML) |
| 7.10 Manufacturer IDs | 16-bit encoding for SysEx8/MDS |
| 8. Stream Messages (MT 0xF) | Endpoint/FB Discovery, Config, Clip markers |
| 4. Value Scaling | Bit-replication for 7/14/16/32 bit round-trip |

---

## Value Scaling Specification

| Document | Title | Version | midi2 Coverage |
|----------|-------|---------|----------------|
| **M2-115-U** | MIDI 2.0 Bit Scaling and Resolution | v1.0.2 | **100%** |

Defines the bit-replication algorithm for converting between 7-bit, 14-bit, 16-bit, and 32-bit values. midi2 implements all 6 scaling functions with guaranteed round-trip safety.

---

## MIDI Clip File Specification

| Document | Title | Version | midi2 Coverage |
|----------|-------|---------|----------------|
| **M2-116-U** | MIDI Clip File | v1.0 | Partial |

Defines a file format for storing UMP sequences. Uses Delta Clockstamp TPQ/DC messages (MT 0x0) and Start/End of Clip markers (MT 0xF). midi2 provides the message construction for these; a full file reader/writer would be a separate module.

---

## Property Exchange Resource Specifications

These define **JSON schemas** for specific PE Resources. They are application-level content, not protocol infrastructure. midi2 provides the PE message transport; these schemas are interpreted by the platform or application.

| Document | Title | Version | What it defines |
|----------|-------|---------|-----------------|
| **M2-105-UM** | PE Foundational Resources | v1.1.1 | ResourceList, DeviceInfo, ChannelList, CMList |
| **M2-106-UM** | PE Mode Resources | v1.01 | Device modes (e.g., performance vs. edit) |
| **M2-107-UM** | PE ProgramList Resource | v1.01 | List of programs/patches/presets |
| **M2-108-UM** | Channel Resources | v1.01 | Per-channel properties |
| **M2-109-UM** | LocalOn Resource | v1.01 | Local control on/off |
| **M2-111-UM** | Get and Set Device State | v1.0 | Save/restore full device state |
| **M2-112-UM** | ExternalSync Resource | v1.0 | External clock synchronization |
| **M2-117-UM** | Controller Resources | v1.0 | Controller assignment/mapping |

---

## Profile Specifications

These define **device behavior** when a specific Profile is enabled. They are application-level -- midi2 provides the Profile negotiation protocol; the behavior is implemented by the device.

| Document | Title | Version | What it defines |
|----------|-------|---------|-----------------|
| **M2-113-UM** | Default CC Mapping Profile | v1.0 | Standard CC assignments (volume, pan, etc.) |
| **M2-118-UM** | GM2 Function Block Profile | v1.0 | General MIDI 2 at function block level |
| **M2-119-UM** | GM2 Single Channel Profile | v1.0 | General MIDI 2 per-channel |
| **M2-120-UM** | MPE Profile | v2.0.3 | MIDI Polyphonic Expression |
| **M2-121-UM** | Drawbar Organ Profile | v1.0.2 | Organ drawbar control |
| **M2-122-UM** | Rotary Speaker Profile | v1.0.2 | Leslie speaker simulation |
| **M2-123-UM** | Orchestral Articulation Profile | v1.0 | Note-On attribute for articulation |
| **M2-125-UM** | Default Drum Note Map Profile | v1.0 | Standard drum note assignments |

---

## Transport Specifications

These define how UMP words travel over specific physical/wireless connections. They are Layer 1 -- not midi2's responsibility.

| Document | Title | Version | Transport |
|----------|-------|---------|-----------|
| **USB-MIDI v2.0** | USB MIDI 2.0 | v2.0 | USB Device and Host |
| **M2-124-UM** | Network MIDI 2.0 UDP | v1.0 | Ethernet/WiFi via UDP |
| *(not yet published)* | BLE MIDI 2.0 | -- | Bluetooth Low Energy |

---

## Spec Map: What Lives Where

```
                    MIDI 2.0 Specifications
                    =======================

         Core Protocol                    Content & Behavior
         =============                    ==================

    M2-104-UM  UMP Format  ----+     M2-105..M2-112, M2-117
    M2-101-UM  MIDI-CI     ----+        PE Resource Schemas
    M2-102-U   Profile Rules --+           (JSON content)
    M2-103-UM  PE Rules    ----+
    M2-115-U   Bit Scaling ----+     M2-113, M2-118..M2-125
                                        Profile Behaviors
         |                               (device logic)
         |
         v
    +----------+                     +------------------+
    |  midi2   |  <-- covers this    |  Platform/App    |
    | (Layer 2)|                     |  (Layer 3 & 4)   |
    +----------+                     +------------------+

         Transport Specs
         ===============
    USB-MIDI v2.0, M2-124-UM Network, BLE MIDI
                |
                v
         +------------+
         | TinyUSB /  |  <-- covers this
         | Transport  |
         | (Layer 1)  |
         +------------+
```

---

## Version Compatibility

MIDI 2.0 specs use version numbers. Key versions supported by midi2:

| Spec | midi2 implements | Notes |
|------|-----------------|-------|
| UMP v1.1.2 | Full | Latest as of Oct 2023 |
| MIDI-CI v1.2 | Full messages, v1 + v2 fields | Version-conditional field handling |
| PE Common Rules v1.2 | Message format only | JSON content is platform-level |
| Bit Scaling v1.0.2 | Full | Round-trip safe algorithm |

MIDI-CI messages include a Version/Format byte. midi2 handles both v1 (0x01) and v2 (0x02) formats, including version-conditional fields (e.g., Discovery Reply has `output_path_id` and `function_block` only in v2).
