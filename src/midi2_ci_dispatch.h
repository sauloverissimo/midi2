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
 * midi2_ci_dispatch.h - MIDI-CI typed dispatch (33 callbacks)
 *
 * Part of midi2 - Portable MIDI 2.0 library (C99)
 * https://github.com/sauloverissimo/midi2
 *
 * Spec: MIDI-CI (M2-101-UM v1.2, Jun 2023)
 * Version: 0.2.4
 */

#ifndef MIDI2_CI_DISPATCH_H
#define MIDI2_CI_DISPATCH_H

#include <stdint.h>
#include <stdbool.h>
#include "midi2_ci_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/*====================================================================+
 * midi2_ci_dispatch -- Typed MIDI-CI message dispatch
 *
 * Parses reassembled SysEx payloads and calls granular, semantically-
 * named callbacks for every MIDI-CI message type in M2-101-UM v1.2.
 *
 * Usage:
 *   midi2_ci_dispatch dp;
 *   midi2_ci_dispatch_init(&dp);
 *   dp.on_discovery = my_discovery_handler;
 *   dp.context = my_app;
 *
 *   // When a complete CI SysEx arrives (from midi2_proc on_sysex7):
 *   midi2_ci_dispatch_feed(&dp, group, sysex_data, sysex_len);
 *
 * All callbacks receive the common CI header fields (version, src_muid,
 * dst_muid, device_id) plus message-specific fields pre-parsed.
 * NULL callbacks are silently skipped.
 *====================================================================*/

/*--------------------------------------------------------------------+
 * Common header passed to all callbacks
 *--------------------------------------------------------------------*/
typedef struct {
  uint8_t  device_id;
  uint8_t  version;
  uint32_t src_muid;
  uint32_t dst_muid;
  uint8_t  group;      /**< UMP group the SysEx arrived on */
} midi2_ci_header;

/*--------------------------------------------------------------------+
 * Callback types: Management (0x70-0x7F)
 *--------------------------------------------------------------------*/
typedef void (*midi2_ci_dp_discovery_cb)(
    midi2_ci_header hdr, uint32_t mfr_id, uint16_t family, uint16_t model,
    uint32_t sw_rev, uint8_t ci_category, uint32_t max_sysex,
    uint8_t output_path_id, void *context);

typedef void (*midi2_ci_dp_discovery_reply_cb)(
    midi2_ci_header hdr, uint32_t mfr_id, uint16_t family, uint16_t model,
    uint32_t sw_rev, uint8_t ci_category, uint32_t max_sysex,
    uint8_t output_path_id, uint8_t function_block, void *context);

typedef void (*midi2_ci_dp_endpoint_info_cb)(
    midi2_ci_header hdr, uint8_t status, void *context);

typedef void (*midi2_ci_dp_endpoint_info_reply_cb)(
    midi2_ci_header hdr, uint8_t status,
    const uint8_t *info_data, uint16_t info_len, void *context);

typedef void (*midi2_ci_dp_invalidate_muid_cb)(
    midi2_ci_header hdr, uint32_t target_muid, void *context);

typedef void (*midi2_ci_dp_ack_cb)(
    midi2_ci_header hdr, uint8_t orig_sub_id,
    uint8_t status_code, uint8_t status_data,
    const uint8_t *details, uint16_t msg_len, const uint8_t *msg_text,
    void *context);

typedef void (*midi2_ci_dp_nak_cb)(
    midi2_ci_header hdr, uint8_t orig_sub_id,
    uint8_t status_code, uint8_t status_data,
    const uint8_t *details, uint16_t msg_len, const uint8_t *msg_text,
    void *context);

/*--------------------------------------------------------------------+
 * Callback types: Profile Configuration (0x20-0x2F)
 *--------------------------------------------------------------------*/
typedef void (*midi2_ci_dp_profile_inquiry_cb)(
    midi2_ci_header hdr, void *context);

typedef void (*midi2_ci_dp_profile_inquiry_reply_cb)(
    midi2_ci_header hdr,
    uint16_t enabled_count, const uint8_t *enabled_data,
    uint16_t disabled_count, const uint8_t *disabled_data,
    void *context);

typedef void (*midi2_ci_dp_set_profile_cb)(
    midi2_ci_header hdr, const uint8_t *profile_id,
    uint16_t num_channels, void *context);

typedef void (*midi2_ci_dp_profile_report_cb)(
    midi2_ci_header hdr, const uint8_t *profile_id,
    uint16_t num_channels, void *context);

typedef void (*midi2_ci_dp_profile_added_removed_cb)(
    midi2_ci_header hdr, const uint8_t *profile_id, void *context);

typedef void (*midi2_ci_dp_profile_details_cb)(
    midi2_ci_header hdr, const uint8_t *profile_id,
    uint8_t inquiry_target, void *context);

typedef void (*midi2_ci_dp_profile_details_reply_cb)(
    midi2_ci_header hdr, const uint8_t *profile_id,
    uint8_t inquiry_target, const uint8_t *data, uint16_t data_len,
    void *context);

typedef void (*midi2_ci_dp_profile_specific_cb)(
    midi2_ci_header hdr, const uint8_t *profile_id,
    const uint8_t *data, uint32_t data_len, void *context);

/*--------------------------------------------------------------------+
 * Callback types: Property Exchange (0x30-0x3F)
 *--------------------------------------------------------------------*/
typedef void (*midi2_ci_dp_pe_caps_cb)(
    midi2_ci_header hdr, uint8_t max_simultaneous,
    uint8_t pe_ver_major, uint8_t pe_ver_minor, void *context);

typedef void (*midi2_ci_dp_pe_data_cb)(
    midi2_ci_header hdr, uint8_t request_id,
    const uint8_t *header_data, uint16_t header_len,
    uint16_t num_chunks, uint16_t this_chunk,
    const uint8_t *body_data, uint16_t body_len, void *context);

/*--------------------------------------------------------------------+
 * Callback types: Process Inquiry (0x40-0x4F)
 *--------------------------------------------------------------------*/
typedef void (*midi2_ci_dp_pi_caps_cb)(
    midi2_ci_header hdr, void *context);

typedef void (*midi2_ci_dp_pi_caps_reply_cb)(
    midi2_ci_header hdr, uint8_t supported_features, void *context);

typedef void (*midi2_ci_dp_pi_midi_report_cb)(
    midi2_ci_header hdr, uint8_t msg_data_control,
    uint8_t system_bitmap, uint8_t channel_ctrl_bitmap,
    uint8_t note_data_bitmap, void *context);

typedef void (*midi2_ci_dp_pi_midi_report_reply_cb)(
    midi2_ci_header hdr, uint8_t system_bitmap,
    uint8_t channel_ctrl_bitmap, uint8_t note_data_bitmap,
    void *context);

typedef void (*midi2_ci_dp_pi_end_cb)(
    midi2_ci_header hdr, void *context);

/*--------------------------------------------------------------------+
 * Fallback
 *--------------------------------------------------------------------*/
typedef void (*midi2_ci_dp_unknown_cb)(
    midi2_ci_header hdr, uint8_t sub_id,
    const uint8_t *data, uint16_t length, void *context);

/*--------------------------------------------------------------------+
 * Dispatch state
 *--------------------------------------------------------------------*/
typedef struct {
  void *context;

  /* Management */
  midi2_ci_dp_discovery_cb           on_discovery;
  midi2_ci_dp_discovery_reply_cb     on_discovery_reply;
  midi2_ci_dp_endpoint_info_cb       on_endpoint_info;
  midi2_ci_dp_endpoint_info_reply_cb on_endpoint_info_reply;
  midi2_ci_dp_invalidate_muid_cb     on_invalidate_muid;
  midi2_ci_dp_ack_cb                 on_ack;
  midi2_ci_dp_nak_cb                 on_nak;

  /* Profile Configuration */
  midi2_ci_dp_profile_inquiry_cb       on_profile_inquiry;
  midi2_ci_dp_profile_inquiry_reply_cb on_profile_inquiry_reply;
  midi2_ci_dp_set_profile_cb           on_set_profile_on;
  midi2_ci_dp_set_profile_cb           on_set_profile_off;
  midi2_ci_dp_profile_report_cb        on_profile_enabled;
  midi2_ci_dp_profile_report_cb        on_profile_disabled;
  midi2_ci_dp_profile_added_removed_cb on_profile_added;
  midi2_ci_dp_profile_added_removed_cb on_profile_removed;
  midi2_ci_dp_profile_details_cb       on_profile_details;
  midi2_ci_dp_profile_details_reply_cb on_profile_details_reply;
  midi2_ci_dp_profile_specific_cb      on_profile_specific_data;

  /* Property Exchange */
  midi2_ci_dp_pe_caps_cb   on_pe_capability;
  midi2_ci_dp_pe_caps_cb   on_pe_capability_reply;
  midi2_ci_dp_pe_data_cb   on_pe_get;
  midi2_ci_dp_pe_data_cb   on_pe_get_reply;
  midi2_ci_dp_pe_data_cb   on_pe_set;
  midi2_ci_dp_pe_data_cb   on_pe_set_reply;
  midi2_ci_dp_pe_data_cb   on_pe_subscribe;
  midi2_ci_dp_pe_data_cb   on_pe_subscribe_reply;
  midi2_ci_dp_pe_data_cb   on_pe_notify;

  /* Process Inquiry */
  midi2_ci_dp_pi_caps_cb              on_pi_capability;
  midi2_ci_dp_pi_caps_reply_cb        on_pi_capability_reply;
  midi2_ci_dp_pi_midi_report_cb       on_pi_midi_report;
  midi2_ci_dp_pi_midi_report_reply_cb on_pi_midi_report_reply;
  midi2_ci_dp_pi_end_cb               on_pi_midi_report_end;

  /* Fallback */
  midi2_ci_dp_unknown_cb on_unknown;
} midi2_ci_dispatch;

/*--------------------------------------------------------------------+
 * Functions
 *--------------------------------------------------------------------*/

/** Initialize dispatch, zeroing all callbacks. */
void midi2_ci_dispatch_init(midi2_ci_dispatch *dp);

/** Feed a reassembled SysEx payload (without F0/F7).
 *  Parses the CI header, dispatches to the appropriate callback.
 *  Returns true if the message was recognized as MIDI-CI, false otherwise.
 *  @param group  UMP group the SysEx arrived on */
bool midi2_ci_dispatch_feed(midi2_ci_dispatch *dp, uint8_t group,
                               const uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* MIDI2_CI_DISPATCH_H */
