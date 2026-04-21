/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Saulo Verissimo
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * midi2_dispatch.h - UMP typed dispatch (42 callbacks)
 *
 * Part of midi2 - Portable MIDI 2.0 library (C99)
 * https://github.com/sauloverissimo/midi2
 *
 * Spec: MIDI 2.0 UMP (M2-104-UM v1.1.2, Nov 2024)
 * Version: 0.2.4
 */

#ifndef MIDI2_DISPATCH_H
#define MIDI2_DISPATCH_H

#include <stdint.h>
#include <stdbool.h>
#include "midi2_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------+
 * midi2_dispatch -- Typed UMP message dispatch
 *
 * Parses raw UMP words and calls granular, semantically-named callbacks
 * for every message type defined in M2-104-UM v1.1.2.
 *
 * Usage:
 *   midi2_dispatch dp;
 *   midi2_dispatch_init(&dp);
 *   dp.on_note_on = my_note_on_handler;
 *   dp.context = my_app;
 *
 *   // In your receive loop (or as midi2_proc on_ump callback):
 *   midi2_dispatch_feed(&dp, words, word_count);
 *
 * Any callback left NULL is silently skipped (zero overhead beyond
 * the NULL check). The dispatch struct is caller-allocated.
 *--------------------------------------------------------------------*/

/*--------------------------------------------------------------------+
 * MT 0x0: Utility
 *--------------------------------------------------------------------*/
typedef void (*midi2_dp_noop_cb)(void *context);
typedef void (*midi2_dp_jr_clock_cb)(uint8_t group, uint16_t timestamp, void *context);
typedef void (*midi2_dp_jr_timestamp_cb)(uint8_t group, uint16_t timestamp, void *context);
typedef void (*midi2_dp_dctpq_cb)(uint16_t tpq, void *context);
typedef void (*midi2_dp_dc_cb)(uint32_t ticks, void *context);

/*--------------------------------------------------------------------+
 * MT 0x1: System Common & System Real Time
 *--------------------------------------------------------------------*/
typedef void (*midi2_dp_system_cb)(uint8_t group, uint8_t status,
                                     uint8_t data1, uint8_t data2, void *context);

/*--------------------------------------------------------------------+
 * MT 0x2: MIDI 1.0 Channel Voice
 *--------------------------------------------------------------------*/
typedef void (*midi2_dp_cv1_note_cb)(uint8_t group, uint8_t channel,
                                       uint8_t note, uint8_t velocity, void *context);
typedef void (*midi2_dp_cv1_cc_cb)(uint8_t group, uint8_t channel,
                                     uint8_t index, uint8_t value, void *context);
typedef void (*midi2_dp_cv1_program_cb)(uint8_t group, uint8_t channel,
                                          uint8_t program, void *context);
typedef void (*midi2_dp_cv1_pressure_cb)(uint8_t group, uint8_t channel,
                                           uint8_t value, void *context);
typedef void (*midi2_dp_cv1_pitch_bend_cb)(uint8_t group, uint8_t channel,
                                             uint16_t value, void *context);
typedef void (*midi2_dp_cv1_poly_pressure_cb)(uint8_t group, uint8_t channel,
                                                uint8_t note, uint8_t value, void *context);

/*--------------------------------------------------------------------+
 * MT 0x4: MIDI 2.0 Channel Voice
 *--------------------------------------------------------------------*/
typedef void (*midi2_dp_note_on_cb)(uint8_t group, uint8_t channel,
                                      uint8_t note, uint16_t velocity,
                                      uint8_t attr_type, uint16_t attr_data, void *context);
typedef void (*midi2_dp_note_off_cb)(uint8_t group, uint8_t channel,
                                       uint8_t note, uint16_t velocity,
                                       uint8_t attr_type, uint16_t attr_data, void *context);
typedef void (*midi2_dp_poly_pressure_cb)(uint8_t group, uint8_t channel,
                                            uint8_t note, uint32_t value, void *context);
typedef void (*midi2_dp_cc_cb)(uint8_t group, uint8_t channel,
                                 uint8_t index, uint32_t value, void *context);
typedef void (*midi2_dp_program_cb)(uint8_t group, uint8_t channel,
                                      uint8_t program, bool bank_valid,
                                      uint8_t bank_msb, uint8_t bank_lsb, void *context);
typedef void (*midi2_dp_chan_pressure_cb)(uint8_t group, uint8_t channel,
                                           uint32_t value, void *context);
typedef void (*midi2_dp_pitch_bend_cb)(uint8_t group, uint8_t channel,
                                         uint32_t value, void *context);
typedef void (*midi2_dp_per_note_pb_cb)(uint8_t group, uint8_t channel,
                                          uint8_t note, uint32_t value, void *context);
typedef void (*midi2_dp_per_note_ctrl_cb)(uint8_t group, uint8_t channel,
                                            uint8_t note, uint8_t index,
                                            uint32_t value, void *context);
typedef void (*midi2_dp_rpn_cb)(uint8_t group, uint8_t channel,
                                  uint8_t bank, uint8_t index,
                                  uint32_t value, void *context);
typedef void (*midi2_dp_per_note_mgmt_cb)(uint8_t group, uint8_t channel,
                                            uint8_t note, bool detach, bool reset,
                                            void *context);

/*--------------------------------------------------------------------+
 * MT 0x3: SysEx7 (raw packet -- reassembly is in midi2_proc)
 * MT 0x5: SysEx8 / Mixed Data Set
 *--------------------------------------------------------------------*/
typedef void (*midi2_dp_sysex7_cb)(uint8_t group, uint8_t status,
                                     const uint8_t *data, uint8_t len, void *context);
typedef void (*midi2_dp_sysex8_cb)(uint8_t group, uint8_t status,
                                     uint8_t stream_id,
                                     const uint8_t *data, uint8_t len, void *context);
typedef void (*midi2_dp_mds_header_cb)(uint8_t group, uint8_t mds_id,
                                         uint16_t num_bytes, uint16_t num_chunks,
                                         uint16_t this_chunk, uint16_t mfr_id,
                                         uint16_t device_id, uint16_t sub_id1,
                                         uint16_t sub_id2, void *context);
/** MDS Payload callback. Always delivers 14 bytes per UMP packet.
 *  The actual valid byte count for the chunk is in the MDS Header's num_bytes field;
 *  the caller must track header state to know when payload ends. */
typedef void (*midi2_dp_mds_payload_cb)(uint8_t group, uint8_t mds_id,
                                          const uint8_t *data, uint8_t len, void *context);

/*--------------------------------------------------------------------+
 * MT 0xD: Flex Data
 *--------------------------------------------------------------------*/
typedef void (*midi2_dp_tempo_cb)(uint8_t group, uint32_t ten_ns_per_qn, void *context);
typedef void (*midi2_dp_time_sig_cb)(uint8_t group, uint8_t numerator,
                                       uint8_t denominator, uint8_t num_32nd_notes,
                                       void *context);
typedef void (*midi2_dp_metronome_cb)(uint8_t group, uint8_t primary_clicks,
                                        uint8_t accent_1, uint8_t accent_2, uint8_t accent_3,
                                        uint8_t subdiv_1, uint8_t subdiv_2, void *context);
typedef void (*midi2_dp_key_sig_cb)(uint8_t group, uint8_t address, uint8_t channel,
                                      int8_t sharps_flats, uint8_t tonic, uint8_t key_type,
                                      void *context);
typedef void (*midi2_dp_chord_cb)(uint8_t group, uint8_t address, uint8_t channel,
                                    int8_t tonic_sf, uint8_t tonic_note, uint8_t chord_type,
                                    uint8_t alt1_type, uint8_t alt1_deg,
                                    uint8_t alt2_type, uint8_t alt2_deg,
                                    uint8_t alt3_type, uint8_t alt3_deg,
                                    uint8_t alt4_type, uint8_t alt4_deg,
                                    int8_t bass_sf, uint8_t bass_note, uint8_t bass_type,
                                    uint8_t bass_alt1_type, uint8_t bass_alt1_deg,
                                    uint8_t bass_alt2_type, uint8_t bass_alt2_deg,
                                    void *context);
typedef void (*midi2_dp_flex_text_cb)(uint8_t group, uint8_t format,
                                        uint8_t address, uint8_t channel,
                                        uint8_t bank, uint8_t status,
                                        const uint8_t *text, uint8_t len, void *context);

/*--------------------------------------------------------------------+
 * MT 0xF: UMP Stream
 *--------------------------------------------------------------------*/
typedef void (*midi2_dp_endpoint_discovery_cb)(uint8_t ump_ver_major, uint8_t ump_ver_minor,
                                                 uint8_t filter, void *context);
typedef void (*midi2_dp_endpoint_info_cb)(uint8_t ump_ver_major, uint8_t ump_ver_minor,
                                            bool static_fb, uint8_t num_fb,
                                            bool midi2_proto, bool midi1_proto,
                                            bool rx_jr, bool tx_jr, void *context);
typedef void (*midi2_dp_device_identity_cb)(uint32_t manufacturer_id,
                                              uint16_t family_id, uint16_t model_id,
                                              uint32_t version_id, void *context);
typedef void (*midi2_dp_stream_text_cb)(uint16_t status, uint8_t format,
                                          const uint8_t *data, uint8_t len, void *context);
typedef void (*midi2_dp_fb_name_cb)(uint8_t format, uint8_t fb_num,
                                      const uint8_t *name, uint8_t len, void *context);
typedef void (*midi2_dp_config_cb)(uint8_t protocol, bool rx_jr, bool tx_jr, void *context);
typedef void (*midi2_dp_fb_discovery_cb)(uint8_t fb_num, uint8_t filter, void *context);
typedef void (*midi2_dp_fb_info_cb)(bool active, uint8_t fb_num, uint8_t direction,
                                      uint8_t first_group, uint8_t num_groups,
                                      uint8_t midi_ci_ver, uint8_t max_sysex8_streams,
                                      uint8_t protocol, void *context);
typedef void (*midi2_dp_clip_cb)(bool start, void *context);

/*--------------------------------------------------------------------+
 * Fallback
 *--------------------------------------------------------------------*/
typedef void (*midi2_dp_unknown_cb)(const uint32_t *words, uint8_t word_count, void *context);

/*--------------------------------------------------------------------+
 * Dispatch State
 *--------------------------------------------------------------------*/
typedef struct {
  void *context;   /**< User pointer passed to all callbacks */

  /** When true, incoming MT 0x2 (MIDI 1.0 CV) messages are translated
   *  to MT 0x4 (MIDI 2.0 CV) with proper value scaling and dispatched
   *  through the on_note_on/on_cc/etc. callbacks. The on_cv1_* callbacks
   *  are NOT called when upscale is active.
   *  When false (default), MT 0x2 goes to on_cv1_* as before. */
  bool upscale_mt2;

  /* MT 0x0: Utility */
  midi2_dp_noop_cb            on_noop;
  midi2_dp_jr_clock_cb        on_jr_clock;
  midi2_dp_jr_timestamp_cb    on_jr_timestamp;
  midi2_dp_dctpq_cb           on_dctpq;
  midi2_dp_dc_cb              on_dc;

  /* MT 0x1: System */
  midi2_dp_system_cb          on_system;

  /* MT 0x2: MIDI 1.0 Channel Voice */
  midi2_dp_cv1_note_cb        on_cv1_note_on;
  midi2_dp_cv1_note_cb        on_cv1_note_off;
  midi2_dp_cv1_poly_pressure_cb on_cv1_poly_pressure;
  midi2_dp_cv1_cc_cb          on_cv1_cc;
  midi2_dp_cv1_program_cb     on_cv1_program;
  midi2_dp_cv1_pressure_cb    on_cv1_chan_pressure;
  midi2_dp_cv1_pitch_bend_cb  on_cv1_pitch_bend;

  /* MT 0x3: SysEx7 (per-packet; use midi2_proc for reassembly) */
  midi2_dp_sysex7_cb          on_sysex7;

  /* MT 0x4: MIDI 2.0 Channel Voice */
  midi2_dp_note_on_cb         on_note_on;
  midi2_dp_note_off_cb        on_note_off;
  midi2_dp_poly_pressure_cb   on_poly_pressure;
  midi2_dp_cc_cb              on_cc;
  midi2_dp_program_cb         on_program;
  midi2_dp_chan_pressure_cb    on_chan_pressure;
  midi2_dp_pitch_bend_cb      on_pitch_bend;
  midi2_dp_per_note_pb_cb     on_per_note_pb;
  midi2_dp_per_note_ctrl_cb   on_reg_per_note;
  midi2_dp_per_note_ctrl_cb   on_asn_per_note;
  midi2_dp_rpn_cb             on_rpn;
  midi2_dp_rpn_cb             on_nrpn;
  midi2_dp_rpn_cb             on_rel_rpn;
  midi2_dp_rpn_cb             on_rel_nrpn;
  midi2_dp_per_note_mgmt_cb   on_per_note_mgmt;

  /* MT 0x5: SysEx8 / Mixed Data Set */
  midi2_dp_sysex8_cb          on_sysex8;
  midi2_dp_mds_header_cb      on_mds_header;
  midi2_dp_mds_payload_cb     on_mds_payload;

  /* MT 0xD: Flex Data */
  midi2_dp_tempo_cb           on_tempo;
  midi2_dp_time_sig_cb        on_time_sig;
  midi2_dp_metronome_cb       on_metronome;
  midi2_dp_key_sig_cb         on_key_sig;
  midi2_dp_chord_cb           on_chord;
  midi2_dp_flex_text_cb       on_flex_text;

  /* MT 0xF: UMP Stream */
  midi2_dp_endpoint_discovery_cb on_endpoint_discovery;
  midi2_dp_endpoint_info_cb      on_endpoint_info;
  midi2_dp_device_identity_cb    on_device_identity;
  midi2_dp_stream_text_cb        on_stream_text;  /* endpoint name, product instance id */
  midi2_dp_fb_name_cb            on_fb_name;      /* function block name (separate: has fb_num) */
  midi2_dp_config_cb             on_config_request;
  midi2_dp_config_cb             on_config_notify;
  midi2_dp_fb_discovery_cb       on_fb_discovery;
  midi2_dp_fb_info_cb            on_fb_info;
  midi2_dp_clip_cb               on_clip;

  /* Fallback for unknown/future MTs */
  midi2_dp_unknown_cb            on_unknown;
} midi2_dispatch;

/*--------------------------------------------------------------------+
 * Functions
 *--------------------------------------------------------------------*/

/** Initialize dispatch, zeroing all callbacks. */
void midi2_dispatch_init(midi2_dispatch *dp);

/** Feed one UMP message. Parses and dispatches to the appropriate callback.
 *  word_count must match the message size (1, 2, or 4 words).
 *  Can be used directly as midi2_proc on_ump callback. */
void midi2_dispatch_feed(const uint32_t *words, uint8_t word_count, void *context);

#ifdef __cplusplus
}
#endif

#endif /* MIDI2_DISPATCH_H */
