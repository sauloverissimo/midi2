# Module Reference

## Overview

| Module | Type | Files | Purpose |
|--------|------|-------|---------|
| midi2_msg | Header-only | `.h` | UMP construction, parsing, value scaling |
| midi2_dispatch | Compiled | `.h` + `.c` | 49 typed UMP callbacks |
| midi2_proc | Compiled | `.h` + `.c` | SysEx reassembly, group filter, remap |
| midi2_conv | Compiled | `.h` + `.c` | MIDI 1.0 byte stream to UMP |
| midi2_ci_msg | Header-only | `.h` | CI message construction, parsing |
| midi2_ci_dispatch | Compiled | `.h` + `.c` | 33 typed CI callbacks |
| midi2_ci | Compiled | `.h` + `.c` | Convenience CI responder |

---

## midi2_msg.h

**Type**: Header-only, stateless. No compilation needed.

### UMP Construction

Every UMP message type has a construction function:

```c
/* MIDI 2.0 Channel Voice (MT 0x4) */
midi2_msg_note_on(w, group, channel, note, velocity, attribute);
midi2_msg_note_off(w, group, channel, note, velocity, attribute);
midi2_msg_cc(w, group, channel, index, value);
midi2_msg_program(w, group, channel, program, bank_valid, bank_msb, bank_lsb);
midi2_msg_pitch_bend(w, group, channel, value);
midi2_msg_chan_pressure(w, group, channel, pressure);
midi2_msg_poly_pressure(w, group, channel, note, pressure);
midi2_msg_rpn(w, group, channel, msb, lsb, value);
midi2_msg_nrpn(w, group, channel, msb, lsb, value);
midi2_msg_rel_rpn(w, group, channel, msb, lsb, value);
midi2_msg_rel_nrpn(w, group, channel, msb, lsb, value);
midi2_msg_per_note_pb(w, group, channel, note, value);
midi2_msg_per_note_cc(w, group, channel, note, index, value);
midi2_msg_per_note_mgmt(w, group, channel, note, detach, reset);
midi2_msg_reg_per_note_ctrl(w, group, channel, note, index, value);
midi2_msg_asn_per_note_ctrl(w, group, channel, note, index, value);

/* System (MT 0x1) */
midi2_msg_system(group, status);
midi2_msg_system_2byte(group, status, data1);
midi2_msg_system_3byte(group, status, data1, data2);
/* v0.3.0+ named wrappers */
midi2_msg_tune_request(group);
midi2_msg_timing_clock(group);
midi2_msg_start(group);
midi2_msg_continue(group);
midi2_msg_stop(group);
midi2_msg_active_sensing(group);
midi2_msg_reset(group);
midi2_msg_mtc(group, frame, value);
midi2_msg_song_select(group, song);
midi2_msg_song_position(group, position);

/* Utility (MT 0x0) */
midi2_msg_jr_clock(group, timestamp);
midi2_msg_jr_timestamp(group, timestamp);
midi2_msg_dctpq(tpq);
midi2_msg_delta_clockstamp(ticks);

/* Flex Data (MT 0xD) */
midi2_msg_tempo(w, group, ten_ns_per_qn);
midi2_msg_time_sig(w, group, numerator, denominator);
midi2_msg_metronome(w, group, clicks, a1, a2, a3, s1, s2);
midi2_msg_key_sig(w, group, sharps_flats, minor);
midi2_msg_key_sig_full(w, group, address, channel, sharps_flats, tonic, key_type);
midi2_msg_chord_name(w, group, address, channel, ...);
midi2_msg_flex_text(w, group, format, address, channel, bank, status, text, len);

/* Stream (MT 0xF) */
midi2_msg_stream_endpoint_discovery(w, ver_major, ver_minor, filter);
midi2_msg_stream_endpoint_info(w, ver_major, ver_minor, static_fb, num_fb, ...);
midi2_msg_stream_device_identity(w, mfr, family, model, version);
midi2_msg_stream_endpoint_name(w, format, name, len);
midi2_msg_stream_product_id(w, format, id, len);
midi2_msg_stream_config_request(w, protocol);
midi2_msg_stream_config_notify(w, protocol);
midi2_msg_stream_fb_discovery(w, fb_num, filter);
midi2_msg_stream_fb_info(w, active, fb_num, direction, ...);
midi2_msg_stream_fb_name(w, format, fb_num, name, len);
midi2_msg_stream_start_of_clip(w);
midi2_msg_stream_end_of_clip(w);

/* SysEx7 (MT 0x3) */
midi2_msg_sysex7_packet(w, group, status, data, len);

/* SysEx8 (MT 0x5) */
midi2_msg_sysex8_packet(w, group, status, stream_id, data, len);
midi2_msg_mds_header(w, group, mds_id, num_bytes, num_chunks, ...);
midi2_msg_mds_payload(w, group, mds_id, data, len);

/* MIDI 1.0 within UMP (MT 0x2) */
midi2_msg_from_midi1(group, status, data1, data2);

/* v0.3.0+ helpers */
midi2_msg_set_group(w, group);                    /* rewrite group field */
midi2_msg_mt4_to_mt2(mt4, mt2_out);               /* downgrade MT 0x4 -> MT 0x2 */
midi2_msg_cable_event_to_ump(cable, status, data1, data2, w);  /* USB MIDI 1.0 cable event -> UMP */
```

### UMP Parsing

```c
midi2_msg_get_mt(w);         /* Message Type (4 bits) */
midi2_msg_get_group(w);      /* Group (0-15) */
midi2_msg_get_status(w);     /* Status byte */
midi2_msg_get_channel(w);    /* Channel (0-15) */
midi2_msg_get_note(w);       /* Note/index field */
midi2_msg_get_velocity(w);   /* 16-bit velocity */
midi2_msg_get_data(w);       /* 32-bit word 1 */
midi2_msg_word_count(mt);    /* Words per message type */
```

### Value Scaling

```c
midi2_msg_scale_up_7to16(v);     /* 7-bit -> 16-bit (bit-replicated) */
midi2_msg_scale_up_7to32(v);     /* 7-bit -> 32-bit */
midi2_msg_scale_up_14to32(v);    /* 14-bit -> 32-bit */
midi2_msg_scale_down_16to7(v);   /* 16-bit -> 7-bit */
midi2_msg_scale_down_32to7(v);   /* 32-bit -> 7-bit */
midi2_msg_scale_down_32to14(v);  /* 32-bit -> 14-bit */
```

All scaling is round-trip safe: `scale_down(scale_up(x)) == x`.

---

## midi2_dispatch

**Type**: Compiled (`.h` + `.c`). Links with: `midi2_msg.h`.

49 granular callbacks, one per UMP message type. Feed signature is compatible with `midi2_proc` `on_ump` callback for zero-cost chaining.

### Callbacks by Message Type

**Utility (MT 0x0):** `on_noop`, `on_jr_clock`, `on_jr_timestamp`, `on_dctpq`, `on_dc`

**System (MT 0x1):** `on_system`

**MIDI 1.0 CV (MT 0x2):** `on_cv1_note_on`, `on_cv1_note_off`, `on_cv1_cc`, `on_cv1_program`, `on_cv1_chan_pressure`, `on_cv1_poly_pressure`, `on_cv1_pitch_bend`

**SysEx7 (MT 0x3):** `on_sysex7`

**MIDI 2.0 CV (MT 0x4):** `on_note_on`, `on_note_off`, `on_cc`, `on_program`, `on_pitch_bend`, `on_chan_pressure`, `on_poly_pressure`, `on_per_note_pb`, `on_reg_per_note`, `on_asn_per_note`, `on_rpn`, `on_nrpn`, `on_rel_rpn`, `on_rel_nrpn`, `on_per_note_mgmt`

**Data 128-bit (MT 0x5):** `on_sysex8`, `on_mds_header`, `on_mds_payload`

**Flex Data (MT 0xD):** `on_tempo`, `on_time_sig`, `on_metronome`, `on_key_sig`, `on_chord`, `on_flex_text`

**Stream (MT 0xF):** `on_endpoint_discovery`, `on_endpoint_info`, `on_device_identity`, `on_stream_text`, `on_fb_name`, `on_config_request`, `on_config_notify`, `on_fb_discovery`, `on_fb_info`, `on_clip`

**Fallback:** `on_unknown`

---

## midi2_proc

**Type**: Compiled. Links with: `midi2_msg.h`.

### Features

- **Group filtering**: bitmask of which groups to accept (default: all)
- **Group remap**: translate group numbers (e.g., map group 0 to group 5)
- **SysEx7 reassembly**: multi-packet SysEx7 -> single callback with complete data
- **SysEx8 reassembly**: multi-packet SysEx8 -> single callback with complete data
- **SysEx7 fragmented send**: split large SysEx into 6-byte UMP packets
- **Function Block Name send** (v0.2.4+): UMP Stream fragmentation (MT 0xF status 0x12) per M2-104-UM §7.1.9
- **Endpoint Name / Product Instance ID / Device Identity senders** (v0.3.0+): Stream MT 0xF fragmentation per M2-104-UM §7.1.6, §7.1.7, §7.1.8
- **SysEx8 fragmented send** (v0.3.0+): MT 0x5 fragmentation with stream_id per M2-104-UM §7.8

### Usage

```c
uint8_t sysex7_buf[256];
uint8_t sysex8_buf[512];
midi2_proc_state proc;
midi2_proc_init(&proc, sysex7_buf, sizeof(sysex7_buf),
                       sysex8_buf, sizeof(sysex8_buf));

proc.on_ump = my_ump_handler;       /* raw UMP callback */
proc.on_sysex7 = my_sysex7_handler; /* reassembled SysEx7 */
proc.on_sysex8 = my_sysex8_handler; /* reassembled SysEx8 */
proc.group_mask = 0x000F;           /* accept groups 0-3 only */

midi2_proc_feed(&proc, words, word_count);

/* Out-going helpers (no state required) */
midi2_proc_send_sysex7(group, bytes, len, write_fn, ctx);
midi2_proc_send_fb_name(fb_idx, "My Function Block", write_fn, ctx);
midi2_proc_send_endpoint_name("My Endpoint", write_fn, ctx);                    /* v0.3.0 */
midi2_proc_send_product_id("ACME-CTRL-001", write_fn, ctx);                     /* v0.3.0 */
midi2_proc_send_device_identity(mfr, family, model, version, write_fn, ctx);    /* v0.3.0 */
midi2_proc_send_sysex8(group, stream_id, data, len, write_fn, ctx);             /* v0.3.0 */
```

---

## midi2_conv

**Type**: Compiled. Links with: `midi2_msg.h`.

Converts MIDI 1.0 byte stream (Serial DIN, UART) to UMP. Handles Running Status, System Common, Real-Time interleaving, and streaming SysEx (any length, emitted as UMP SysEx7 packets: START/CONTINUE/END).

```c
midi2_conv_state conv;
midi2_conv_init(&conv, 0);  /* group 0, no external buffer needed */

while (serial_available()) {
    uint8_t byte = serial_read();
    if (midi2_conv_feed(&conv, byte)) {
        process_ump(conv.ump, conv.ump_words);
    }
}
```

### Protocol translation (MT 0x2 <-> MT 0x4)

`midi2_msg_mt2_to_mt4()` translates a 1-word MT 0x2 message to a 2-word MT 0x4 message with proper value scaling per M2-104-UM v1.1.2 Section 7. Stateless, defined in `midi2_msg.h`.

```c
uint32_t mt2 = midi2_msg_from_midi1(0, 0x90, 60, 100);  /* MT 0x2 Note On */
uint32_t mt4[2];
midi2_msg_mt2_to_mt4(mt2, mt4);  /* MT 0x4 with 16-bit velocity */
```

The reverse exists in v0.3.0+: `midi2_msg_mt4_to_mt2()` downgrades a 2-word MT 0x4 to a 1-word MT 0x2 with `scale_down_16to7` on velocity.

```c
uint32_t mt4_msg[2];
midi2_msg_note_on(mt4_msg, 0, 0, 60, 0xC000, 0);
uint32_t mt2_msg;
midi2_msg_mt4_to_mt2(mt4_msg, &mt2_msg);  /* 7-bit velocity */
```

The `midi2_dispatch` supports automatic upscaling via the `upscale_mt2` flag:

```c
midi2_dispatch dp;
midi2_dispatch_init(&dp);
dp.upscale_mt2 = true;   /* MT 0x2 translated to MT 0x4 before dispatch */
dp.on_note_on = my_handler;  /* receives both MT 0x2 and MT 0x4 messages */
```

---

## midi2_ci_msg.h

**Type**: Header-only, stateless. No compilation needed.

Construction and parsing for all 32 MIDI-CI messages per M2-101-UM v1.2.

### Categories

**Management (0x70-0x7F):** Discovery, Reply, Endpoint Info, Reply, Invalidate MUID, ACK, NAK

**Profile (0x20-0x2F):** Inquiry, Reply, Set On/Off, Enabled/Disabled Report, Added/Removed Report, Details Inquiry/Reply, Specific Data

**Property Exchange (0x30-0x3F):** Capabilities, Reply, Get/Set + Reply, Subscribe/Reply, Notify

**Process Inquiry (0x40-0x4F):** Capabilities, Reply, MIDI Message Report/Reply/End

### Usage

```c
uint8_t buf[64];
uint16_t len = midi2_ci_build_discovery_reply(buf, MIDI2_CI_VERSION_2,
    my_muid, peer_muid, mfr_id, family, model, sw_rev, ci_cat, max_sysex,
    output_path, function_block);
send_sysex(group, buf, len);
```

---

## midi2_ci_dispatch

**Type**: Compiled. Links with: `midi2_ci_msg.h`.

33 callbacks for typed CI message dispatch (32 messages plus an `on_unknown` fallback). Parses reassembled SysEx and delivers pre-parsed fields.

```c
midi2_ci_dispatch ci_dp;
midi2_ci_dispatch_init(&ci_dp);
ci_dp.on_discovery = my_discovery_handler;
ci_dp.on_profile_inquiry = my_profile_inquiry_handler;
ci_dp.on_pe_get = my_pe_get_handler;

/* Feed from midi2_proc on_sysex7 callback */
midi2_ci_dispatch_feed(&ci_dp, group, sysex_data, sysex_len);
```

---

## midi2_ci

**Type**: Compiled. Links with: `midi2_ci_msg.h`, `midi2_proc.h`.

Convenience auto-responder for devices. Since **v0.2.4** it covers the
full M2-101-UM Appendix E "minimum MIDI-CI" checklist when an RNG is
installed and `nak_on_unknown` is enabled:

- Discovery Reply advertising Profile + PE + Process Inquiry
- Profile Inquiry Reply
- PE Capability Reply + PE Get/Set
- Process Inquiry Capability Reply
- Invalidate MUID -> regenerate state->muid via installed RNG
- MUID collision detection -> regenerate and broadcast Invalidate
- NAK (Sub-ID#2 0x7F, status NOT_SUPPORTED) for unsupported sub-ids

**v0.3.0** adds a Subscribe/Notify state machine for property change tracking:

- `midi2_ci_subscribe_add(&ci, peer_muid, resource)` registers a subscriber
- Property changes call `midi2_ci_notify(&ci, resource, body, len)` to broadcast PE Notify
- `midi2_ci_init_ex()` extends `midi2_ci_init` with subscriber storage
- `auto_invalidate_on_collision` flag automates MUID collision broadcast

```c
static uint32_t platform_rng(void *ctx) { (void)ctx; return esp_random(); }

midi2_ci_state ci;
uint8_t profiles[4][5];
midi2_ci_property props[3];
midi2_ci_init(&ci, muid_seed, profiles, 4, props, 3);
midi2_ci_set_identity(&ci, mfr_id, family, model, version);
midi2_ci_set_write_fn(&ci, my_sysex_write, ctx);
midi2_ci_set_rng(&ci, platform_rng, NULL);       /* v0.2.4 */
midi2_ci_set_nak_on_unknown(&ci, true);          /* v0.2.4 */
midi2_ci_add_profile(&ci, gm2_profile_id);
midi2_ci_add_property_static(&ci, "Product", "My Device");

/* In your SysEx receive callback (works for both SysEx7 and SysEx8): */
midi2_ci_process_sysex(&ci, group, data, length);
```

**Simplified PE Get**: the built-in PE Get handler returns the first
property with a non-NULL value (no JSON header parsing). For full
control, wire `midi2_ci_dispatch` callbacks directly.
