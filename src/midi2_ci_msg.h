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
 * midi2_ci_msg.h - MIDI-CI message construction and parsing
 *
 * Part of midi2 - Portable MIDI 2.0 library (C99)
 * https://github.com/sauloverissimo/midi2
 *
 * Spec: MIDI-CI (M2-101-UM v1.2, Jun 2023)
 * Version: 0.2.3
 */

#ifndef MIDI2_CI_MSG_H
#define MIDI2_CI_MSG_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*====================================================================+
 * midi2_ci_msg -- MIDI-CI message construction and parsing
 *
 * Header-only, stateless. Builds and reads MIDI-CI SysEx payloads
 * per M2-101-UM MIDI-CI Specification v1.2.
 *
 * All build functions write into a caller-provided buffer and return
 * the number of bytes written. The buffer does NOT include F0/F7
 * delimiters (those are added by the SysEx transport layer).
 *
 * All parse functions extract fields from a received SysEx payload
 * (also without F0/F7).
 *====================================================================*/

/*--------------------------------------------------------------------+
 * Constants
 *--------------------------------------------------------------------*/

#define MIDI2_CI_BROADCAST_MUID  UINT32_C(0x0FFFFFFF)
#define MIDI2_CI_VERSION_1       0x01
#define MIDI2_CI_VERSION_2       0x02  /* current: MIDI-CI v1.2 */

/*--------------------------------------------------------------------+
 * Sub-ID#2 values (Appendix D, M2-101-UM)
 *--------------------------------------------------------------------*/
enum {
  /* Category 7: Management (0x70-0x7F) */
  MIDI2_CI_DISCOVERY             = 0x70,
  MIDI2_CI_DISCOVERY_REPLY       = 0x71,
  MIDI2_CI_ENDPOINT_INFO         = 0x72,
  MIDI2_CI_ENDPOINT_INFO_REPLY   = 0x73,
  MIDI2_CI_ACK                   = 0x7D,
  MIDI2_CI_INVALIDATE_MUID       = 0x7E,
  MIDI2_CI_NAK                   = 0x7F,

  /* Category 2: Profile Configuration (0x20-0x2F) */
  MIDI2_CI_PROFILE_INQUIRY       = 0x20,
  MIDI2_CI_PROFILE_INQUIRY_REPLY = 0x21,
  MIDI2_CI_SET_PROFILE_ON        = 0x22,
  MIDI2_CI_SET_PROFILE_OFF       = 0x23,
  MIDI2_CI_PROFILE_ENABLED       = 0x24,
  MIDI2_CI_PROFILE_DISABLED      = 0x25,
  MIDI2_CI_PROFILE_ADDED         = 0x26,
  MIDI2_CI_PROFILE_REMOVED       = 0x27,
  MIDI2_CI_PROFILE_DETAILS       = 0x28,
  MIDI2_CI_PROFILE_DETAILS_REPLY = 0x29,
  MIDI2_CI_PROFILE_SPECIFIC_DATA = 0x2F,

  /* Category 3: Property Exchange (0x30-0x3F) */
  MIDI2_CI_PE_CAPABILITY         = 0x30,
  MIDI2_CI_PE_CAPABILITY_REPLY   = 0x31,
  MIDI2_CI_PE_GET                = 0x34,
  MIDI2_CI_PE_GET_REPLY          = 0x35,
  MIDI2_CI_PE_SET                = 0x36,
  MIDI2_CI_PE_SET_REPLY          = 0x37,
  MIDI2_CI_PE_SUBSCRIBE          = 0x38,
  MIDI2_CI_PE_SUBSCRIBE_REPLY    = 0x39,
  MIDI2_CI_PE_NOTIFY             = 0x3F,

  /* Category 4: Process Inquiry (0x40-0x4F) */
  MIDI2_CI_PI_CAPABILITY         = 0x40,
  MIDI2_CI_PI_CAPABILITY_REPLY   = 0x41,
  MIDI2_CI_PI_MIDI_REPORT        = 0x42,
  MIDI2_CI_PI_MIDI_REPORT_REPLY  = 0x43,
  MIDI2_CI_PI_MIDI_REPORT_END    = 0x44,
};

/*--------------------------------------------------------------------+
 * Capability Inquiry Category Supported bitmap (Table 7)
 *--------------------------------------------------------------------*/
enum {
  MIDI2_CI_CAT_PROFILE_CONFIG    = 0x04,  /* bit 2 */
  MIDI2_CI_CAT_PROPERTY_EXCHANGE = 0x08,  /* bit 3 */
  MIDI2_CI_CAT_PROCESS_INQUIRY   = 0x10,  /* bit 4 */
};

/*--------------------------------------------------------------------+
 * NAK Status Codes (Table 16)
 *--------------------------------------------------------------------*/
enum {
  MIDI2_CI_NAK_OK                = 0x00,
  MIDI2_CI_NAK_NOT_SUPPORTED     = 0x01,
  MIDI2_CI_NAK_VERSION_ERR       = 0x02,
  MIDI2_CI_NAK_CH_NOT_IN_USE     = 0x03,
  MIDI2_CI_NAK_PROFILE_NOT_SUPP  = 0x04,
  MIDI2_CI_NAK_TERMINATE_PE      = 0x20,
  MIDI2_CI_NAK_PE_OUT_OF_SEQ     = 0x21,
  MIDI2_CI_NAK_ERROR_RETRY       = 0x40,
  MIDI2_CI_NAK_MALFORMED         = 0x41,
  MIDI2_CI_NAK_TIMEOUT           = 0x42,
  MIDI2_CI_NAK_BUSY              = 0x43,
};

/*--------------------------------------------------------------------+
 * ACK Status Codes (Table 14)
 *--------------------------------------------------------------------*/
enum {
  MIDI2_CI_ACK_OK                = 0x00,
  MIDI2_CI_ACK_TIMEOUT_WAIT      = 0x10,
};

/*--------------------------------------------------------------------+
 * MUID utilities
 *--------------------------------------------------------------------*/

/** Read 28-bit MUID from 4 bytes (LSB first, 7-bit encoding). */
static inline uint32_t midi2_ci_read_muid(const uint8_t *p) {
  return (uint32_t)(p[0] & 0x7F)
       | ((uint32_t)(p[1] & 0x7F) << 7)
       | ((uint32_t)(p[2] & 0x7F) << 14)
       | ((uint32_t)(p[3] & 0x7F) << 21);
}

/** Write 28-bit MUID as 4 bytes (LSB first, 7-bit encoding). */
static inline void midi2_ci_write_muid(uint8_t *p, uint32_t muid) {
  p[0] = (uint8_t)((muid >> 0) & 0x7F);
  p[1] = (uint8_t)((muid >> 7) & 0x7F);
  p[2] = (uint8_t)((muid >> 14) & 0x7F);
  p[3] = (uint8_t)((muid >> 21) & 0x7F);
}

/*--------------------------------------------------------------------+
 * Common header: parse
 *
 * Standard CI SysEx layout (without F0/F7):
 *   [0]  0x7E  Universal System Exclusive
 *   [1]  Device ID (0x00-0x0F = channel, 0x7E = group, 0x7F = function block)
 *   [2]  0x0D  MIDI-CI Sub-ID#1
 *   [3]  Sub-ID#2 (message type)
 *   [4]  MIDI-CI Message Version/Format
 *   [5..8]   Source MUID (4 bytes, LSB first)
 *   [9..12]  Destination MUID (4 bytes, LSB first)
 *   [13..]   Message-specific data
 *--------------------------------------------------------------------*/

static inline uint8_t  midi2_ci_get_device_id(const uint8_t *d)    { return d[1]; }
static inline uint8_t  midi2_ci_get_sub_id(const uint8_t *d)       { return d[3]; }
static inline uint8_t  midi2_ci_get_version(const uint8_t *d)      { return d[4]; }
static inline uint32_t midi2_ci_get_src_muid(const uint8_t *d)     { return midi2_ci_read_muid(&d[5]); }
static inline uint32_t midi2_ci_get_dst_muid(const uint8_t *d)     { return midi2_ci_read_muid(&d[9]); }

/** Check if SysEx payload is a MIDI-CI message (7E xx 0D ...). */
static inline bool midi2_ci_is_ci(const uint8_t *d, uint16_t len) {
  return len >= 13 && d[0] == 0x7E && d[2] == 0x0D;
}

/*--------------------------------------------------------------------+
 * Common header: build
 *
 * Returns offset after header (13 bytes). Caller continues writing
 * message-specific data at the returned offset.
 *--------------------------------------------------------------------*/

static inline uint16_t midi2_ci_build_header(uint8_t *buf, uint8_t device_id,
                                                uint8_t sub_id, uint8_t version,
                                                uint32_t src_muid, uint32_t dst_muid) {
  buf[0] = 0x7E;
  buf[1] = device_id;
  buf[2] = 0x0D;
  buf[3] = sub_id;
  buf[4] = version;
  midi2_ci_write_muid(&buf[5], src_muid);
  midi2_ci_write_muid(&buf[9], dst_muid);
  return 13;
}

/*--------------------------------------------------------------------+
 * 14-bit value read/write (2 bytes, LSB first, 7-bit per byte)
 *
 * All multi-byte fields inside SysEx use 7-bit encoding (bit 7 = 0).
 * Range: 0 to 16383 (0x3FFF). Used for profile counts, data lengths,
 * chunk numbers, and channel counts in MIDI-CI messages.
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_read_14(const uint8_t *p) {
  return (uint16_t)(p[0] & 0x7F) | ((uint16_t)(p[1] & 0x7F) << 7);
}
static inline void midi2_ci_write_14(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0x7F);
  p[1] = (uint8_t)((v >> 7) & 0x7F);
}

/*--------------------------------------------------------------------+
 * 28-bit value read/write (4 bytes, LSB first, 7-bit each)
 *--------------------------------------------------------------------*/
static inline uint32_t midi2_ci_read_28(const uint8_t *p) {
  return midi2_ci_read_muid(p); /* same encoding */
}
static inline void midi2_ci_write_28(uint8_t *p, uint32_t v) {
  midi2_ci_write_muid(p, v);
}

/*--------------------------------------------------------------------+
 * Parse helpers for common message fields
 *
 * All offsets assume standard header (13 bytes). Message-specific data
 * starts at byte 13. These extract fields without the caller needing
 * to know byte offsets.
 *--------------------------------------------------------------------*/

/* Discovery / Discovery Reply: device identification fields at offset 13.
 * Manufacturer ID: 3 bytes, literal SysEx ID bytes (all <= 0x7F).
 * Family/Model: 2 bytes each, 7-bit LSB-first (14-bit value).
 * SW Revision: 4 bytes, 7-bit LSB-first (28-bit value).
 * These match the "Device Inquiry" Universal SysEx format (section 5.5.1). */
static inline uint32_t midi2_ci_get_mfr_id(const uint8_t *d) {
  return (uint32_t)d[13] | ((uint32_t)d[14] << 8) | ((uint32_t)d[15] << 16);
}
static inline uint16_t midi2_ci_get_family(const uint8_t *d) {
  return midi2_ci_read_14(&d[16]);
}
static inline uint16_t midi2_ci_get_model(const uint8_t *d) {
  return midi2_ci_read_14(&d[18]);
}
static inline uint32_t midi2_ci_get_sw_rev(const uint8_t *d) {
  return midi2_ci_read_28(&d[20]);
}
static inline uint8_t midi2_ci_get_ci_category(const uint8_t *d) {
  return d[24];
}
static inline uint32_t midi2_ci_get_max_sysex(const uint8_t *d) {
  return midi2_ci_read_28(&d[25]);
}

/* Invalidate MUID: target MUID at offset 13 */
static inline uint32_t midi2_ci_get_target_muid(const uint8_t *d) {
  return midi2_ci_read_muid(&d[13]);
}

/* ACK/NAK (v2+): fields after header */
static inline uint8_t midi2_ci_get_orig_sub_id(const uint8_t *d) { return d[13]; }
static inline uint8_t midi2_ci_get_nak_status_code(const uint8_t *d) { return d[14]; }
static inline uint8_t midi2_ci_get_nak_status_data(const uint8_t *d) { return d[15]; }

/* Profile messages: profile ID at offset 13 (after header) */
static inline const uint8_t *midi2_ci_get_profile_id(const uint8_t *d) { return &d[13]; }

/* Profile Inquiry Reply: counts */
static inline uint16_t midi2_ci_get_enabled_count(const uint8_t *d) {
  return midi2_ci_read_14(&d[13]);
}

/* PE data messages: request_id at offset 13 */
static inline uint8_t midi2_ci_get_pe_request_id(const uint8_t *d) { return d[13]; }
static inline uint16_t midi2_ci_get_pe_header_len(const uint8_t *d) {
  return midi2_ci_read_14(&d[14]);
}

/* Process Inquiry MIDI Report: bitmaps */
static inline uint8_t midi2_ci_get_pi_msg_data_control(const uint8_t *d) { return d[13]; }
static inline uint8_t midi2_ci_get_pi_system_bitmap(const uint8_t *d) { return d[14]; }
static inline uint8_t midi2_ci_get_pi_channel_ctrl_bitmap(const uint8_t *d) { return d[16]; }
static inline uint8_t midi2_ci_get_pi_note_data_bitmap(const uint8_t *d) { return d[17]; }

/*====================================================================+
 * CATEGORY 7: MANAGEMENT MESSAGES
 *====================================================================*/

/*--------------------------------------------------------------------+
 * Discovery (0x70) -- Table 6
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_discovery(
    uint8_t *buf, uint8_t version, uint32_t src_muid,
    uint32_t mfr_id, uint16_t family, uint16_t model, uint32_t sw_rev,
    uint8_t ci_category, uint32_t max_sysex, uint8_t output_path_id) {
  uint16_t p = midi2_ci_build_header(buf, 0x7F, MIDI2_CI_DISCOVERY, version,
                                        src_muid, MIDI2_CI_BROADCAST_MUID);
  /* Device Manufacturer (3 bytes SysEx ID) */
  buf[p++] = (uint8_t)((mfr_id >> 0) & 0x7F);
  buf[p++] = (uint8_t)((mfr_id >> 8) & 0x7F);
  buf[p++] = (uint8_t)((mfr_id >> 16) & 0x7F);
  /* Device Family (2 bytes LSB first) */
  buf[p++] = (uint8_t)(family & 0x7F);
  buf[p++] = (uint8_t)((family >> 7) & 0x7F);
  /* Model Number (2 bytes LSB first) */
  buf[p++] = (uint8_t)(model & 0x7F);
  buf[p++] = (uint8_t)((model >> 7) & 0x7F);
  /* Software Revision (4 bytes) */
  midi2_ci_write_28(&buf[p], sw_rev); p += 4;
  /* Capability Inquiry Category Supported */
  buf[p++] = ci_category;
  /* Receivable Maximum SysEx Size (4 bytes LSB first) */
  midi2_ci_write_28(&buf[p], max_sysex); p += 4;
  /* Output Path Id (v2+) */
  if (version >= MIDI2_CI_VERSION_2) {
    buf[p++] = output_path_id;
  }
  return p;
}

/*--------------------------------------------------------------------+
 * Reply to Discovery (0x71) -- Table 8
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_discovery_reply(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint32_t mfr_id, uint16_t family, uint16_t model, uint32_t sw_rev,
    uint8_t ci_category, uint32_t max_sysex,
    uint8_t output_path_id, uint8_t function_block) {
  uint16_t p = midi2_ci_build_header(buf, 0x7F, MIDI2_CI_DISCOVERY_REPLY, version,
                                        src_muid, dst_muid);
  buf[p++] = (uint8_t)((mfr_id >> 0) & 0x7F);
  buf[p++] = (uint8_t)((mfr_id >> 8) & 0x7F);
  buf[p++] = (uint8_t)((mfr_id >> 16) & 0x7F);
  buf[p++] = (uint8_t)(family & 0x7F);
  buf[p++] = (uint8_t)((family >> 7) & 0x7F);
  buf[p++] = (uint8_t)(model & 0x7F);
  buf[p++] = (uint8_t)((model >> 7) & 0x7F);
  midi2_ci_write_28(&buf[p], sw_rev); p += 4;
  buf[p++] = ci_category;
  midi2_ci_write_28(&buf[p], max_sysex); p += 4;
  if (version >= MIDI2_CI_VERSION_2) {
    buf[p++] = output_path_id;
    buf[p++] = function_block;
  }
  return p;
}

/*--------------------------------------------------------------------+
 * Inquiry: Endpoint Information (0x72) -- Table 9
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_endpoint_info(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t status) {
  uint16_t p = midi2_ci_build_header(buf, 0x7F, MIDI2_CI_ENDPOINT_INFO, version,
                                        src_muid, dst_muid);
  buf[p++] = status;
  return p;
}

/*--------------------------------------------------------------------+
 * Reply to Endpoint Information (0x73) -- Table 11
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_endpoint_info_reply(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t status, const uint8_t *info_data, uint16_t info_len) {
  uint16_t p = midi2_ci_build_header(buf, 0x7F, MIDI2_CI_ENDPOINT_INFO_REPLY, version,
                                        src_muid, dst_muid);
  buf[p++] = status;
  midi2_ci_write_14(&buf[p], info_len); p += 2;
  if (info_data && info_len > 0) {
    memcpy(&buf[p], info_data, info_len);
    p += info_len;
  }
  return p;
}

/*--------------------------------------------------------------------+
 * Invalidate MUID (0x7E) -- Table 12
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_invalidate_muid(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t target_muid) {
  uint16_t p = midi2_ci_build_header(buf, 0x7F, MIDI2_CI_INVALIDATE_MUID, version,
                                        src_muid, MIDI2_CI_BROADCAST_MUID);
  midi2_ci_write_muid(&buf[p], target_muid); p += 4;
  return p;
}

/*--------------------------------------------------------------------+
 * ACK (0x7D) -- Table 13 (v2+)
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_ack(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t device_id, uint8_t orig_sub_id,
    uint8_t status_code, uint8_t status_data,
    const uint8_t *details, uint16_t msg_len, const uint8_t *msg_text) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_ACK, version,
                                        src_muid, dst_muid);
  buf[p++] = orig_sub_id;
  buf[p++] = status_code;
  buf[p++] = status_data;
  /* 5 bytes details */
  if (details) { memcpy(&buf[p], details, 5); } else { memset(&buf[p], 0, 5); }
  p += 5;
  /* Message text */
  midi2_ci_write_14(&buf[p], msg_len); p += 2;
  if (msg_text && msg_len > 0) {
    memcpy(&buf[p], msg_text, msg_len);
    p += msg_len;
  }
  return p;
}

/*--------------------------------------------------------------------+
 * NAK (0x7F) -- Table 15 (v2+)
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_nak(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t device_id, uint8_t orig_sub_id,
    uint8_t status_code, uint8_t status_data,
    const uint8_t *details, uint16_t msg_len, const uint8_t *msg_text) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_NAK, version,
                                        src_muid, dst_muid);
  if (version >= MIDI2_CI_VERSION_2) {
    buf[p++] = orig_sub_id;
    buf[p++] = status_code;
    buf[p++] = status_data;
    if (details) { memcpy(&buf[p], details, 5); } else { memset(&buf[p], 0, 5); }
    p += 5;
    midi2_ci_write_14(&buf[p], msg_len); p += 2;
    if (msg_text && msg_len > 0) {
      memcpy(&buf[p], msg_text, msg_len);
      p += msg_len;
    }
  }
  return p;
}

/*====================================================================+
 * CATEGORY 2: PROFILE CONFIGURATION MESSAGES
 *====================================================================*/

/*--------------------------------------------------------------------+
 * Profile Inquiry (0x20) -- Table 17
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_profile_inquiry(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t device_id) {
  return midi2_ci_build_header(buf, device_id, MIDI2_CI_PROFILE_INQUIRY, version,
                                  src_muid, dst_muid);
}

/*--------------------------------------------------------------------+
 * Reply to Profile Inquiry (0x21) -- Table 18
 * enabled/disabled: arrays of 5-byte profile IDs
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_profile_inquiry_reply(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t device_id,
    const uint8_t (*enabled)[5], uint16_t enabled_count,
    const uint8_t (*disabled)[5], uint16_t disabled_count) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_PROFILE_INQUIRY_REPLY,
                                        version, src_muid, dst_muid);
  midi2_ci_write_14(&buf[p], enabled_count); p += 2;
  { uint16_t i; for (i = 0; i < enabled_count; i++) { memcpy(&buf[p], enabled[i], 5); p += 5; } }
  midi2_ci_write_14(&buf[p], disabled_count); p += 2;
  { uint16_t i; for (i = 0; i < disabled_count; i++) { memcpy(&buf[p], disabled[i], 5); p += 5; } }
  return p;
}

/*--------------------------------------------------------------------+
 * Set Profile On (0x22) -- Table 24
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_set_profile_on(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t device_id, const uint8_t profile_id[5], uint16_t num_channels) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_SET_PROFILE_ON,
                                        version, src_muid, dst_muid);
  memcpy(&buf[p], profile_id, 5); p += 5;
  if (version >= MIDI2_CI_VERSION_2) {
    midi2_ci_write_14(&buf[p], num_channels); p += 2;
  }
  return p;
}

/*--------------------------------------------------------------------+
 * Set Profile Off (0x23) -- Table 25
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_set_profile_off(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t device_id, const uint8_t profile_id[5]) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_SET_PROFILE_OFF,
                                        version, src_muid, dst_muid);
  memcpy(&buf[p], profile_id, 5); p += 5;
  if (version >= MIDI2_CI_VERSION_2) {
    buf[p++] = 0x00; buf[p++] = 0x00; /* reserved */
  }
  return p;
}

/*--------------------------------------------------------------------+
 * Profile Enabled Report (0x24) -- Table 26
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_profile_enabled(
    uint8_t *buf, uint8_t version, uint32_t src_muid,
    uint8_t device_id, const uint8_t profile_id[5], uint16_t num_channels) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_PROFILE_ENABLED,
                                        version, src_muid, MIDI2_CI_BROADCAST_MUID);
  memcpy(&buf[p], profile_id, 5); p += 5;
  if (version >= MIDI2_CI_VERSION_2) {
    midi2_ci_write_14(&buf[p], num_channels); p += 2;
  }
  return p;
}

/*--------------------------------------------------------------------+
 * Profile Disabled Report (0x25) -- Table 27
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_profile_disabled(
    uint8_t *buf, uint8_t version, uint32_t src_muid,
    uint8_t device_id, const uint8_t profile_id[5], uint16_t num_channels) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_PROFILE_DISABLED,
                                        version, src_muid, MIDI2_CI_BROADCAST_MUID);
  memcpy(&buf[p], profile_id, 5); p += 5;
  if (version >= MIDI2_CI_VERSION_2) {
    midi2_ci_write_14(&buf[p], num_channels); p += 2;
  }
  return p;
}

/*--------------------------------------------------------------------+
 * Profile Added Report (0x26) -- Table 20
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_profile_added(
    uint8_t *buf, uint8_t version, uint32_t src_muid,
    uint8_t device_id, const uint8_t profile_id[5]) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_PROFILE_ADDED,
                                        version, src_muid, MIDI2_CI_BROADCAST_MUID);
  memcpy(&buf[p], profile_id, 5); p += 5;
  return p;
}

/*--------------------------------------------------------------------+
 * Profile Removed Report (0x27) -- Table 21
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_profile_removed(
    uint8_t *buf, uint8_t version, uint32_t src_muid,
    uint8_t device_id, const uint8_t profile_id[5]) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_PROFILE_REMOVED,
                                        version, src_muid, MIDI2_CI_BROADCAST_MUID);
  memcpy(&buf[p], profile_id, 5); p += 5;
  return p;
}

/*--------------------------------------------------------------------+
 * Profile Details Inquiry (0x28) -- Table 22
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_profile_details(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t device_id, const uint8_t profile_id[5], uint8_t inquiry_target) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_PROFILE_DETAILS,
                                        version, src_muid, dst_muid);
  memcpy(&buf[p], profile_id, 5); p += 5;
  buf[p++] = inquiry_target;
  return p;
}

/*--------------------------------------------------------------------+
 * Reply to Profile Details (0x29) -- Table 23
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_profile_details_reply(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t device_id, const uint8_t profile_id[5],
    uint8_t inquiry_target, const uint8_t *data, uint16_t data_len) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_PROFILE_DETAILS_REPLY,
                                        version, src_muid, dst_muid);
  memcpy(&buf[p], profile_id, 5); p += 5;
  buf[p++] = inquiry_target;
  midi2_ci_write_14(&buf[p], data_len); p += 2;
  if (data && data_len > 0) { memcpy(&buf[p], data, data_len); p += data_len; }
  return p;
}

/*--------------------------------------------------------------------+
 * Profile Specific Data (0x2F) -- Table 28
 *
 * Note: When dst_muid is Broadcast, data_len shall not exceed 512 bytes
 * and the message shall use chunking if needed (per spec 7.12).
 * This function does not enforce the limit -- the caller is responsible.
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_profile_specific_data(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t device_id, const uint8_t profile_id[5],
    const uint8_t *data, uint32_t data_len) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_PROFILE_SPECIFIC_DATA,
                                        version, src_muid, dst_muid);
  memcpy(&buf[p], profile_id, 5); p += 5;
  midi2_ci_write_28(&buf[p], data_len); p += 4;
  if (data && data_len > 0) {
    memcpy(&buf[p], data, data_len);
    p += (uint16_t)data_len;
  }
  return p;
}

/*====================================================================+
 * CATEGORY 3: PROPERTY EXCHANGE MESSAGES
 *====================================================================*/

/*--------------------------------------------------------------------+
 * PE Capabilities Inquiry (0x30) -- Table 30
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_pe_capability(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t max_simultaneous, uint8_t pe_ver_major, uint8_t pe_ver_minor) {
  uint16_t p = midi2_ci_build_header(buf, 0x7F, MIDI2_CI_PE_CAPABILITY, version,
                                        src_muid, dst_muid);
  buf[p++] = max_simultaneous;
  if (version >= MIDI2_CI_VERSION_2) {
    buf[p++] = pe_ver_major;
    buf[p++] = pe_ver_minor;
  }
  return p;
}

/*--------------------------------------------------------------------+
 * Reply to PE Capabilities (0x31) -- Table 32
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_pe_capability_reply(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t max_simultaneous, uint8_t pe_ver_major, uint8_t pe_ver_minor) {
  uint16_t p = midi2_ci_build_header(buf, 0x7F, MIDI2_CI_PE_CAPABILITY_REPLY, version,
                                        src_muid, dst_muid);
  buf[p++] = max_simultaneous;
  if (version >= MIDI2_CI_VERSION_2) {
    buf[p++] = pe_ver_major;
    buf[p++] = pe_ver_minor;
  }
  return p;
}

/*--------------------------------------------------------------------+
 * PE Data Message (generic builder for Get/Set/Subscribe/Notify)
 *
 * Used by: 0x34 Get, 0x35 Get Reply, 0x36 Set, 0x37 Set Reply,
 *          0x38 Subscribe, 0x39 Subscribe Reply, 0x3F Notify
 *
 * All PE data messages share the same structure:
 *   header + request_id + header_len + header_data +
 *   num_chunks + this_chunk + body_len + body_data
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_pe_data(
    uint8_t *buf, uint8_t version, uint8_t sub_id,
    uint32_t src_muid, uint32_t dst_muid,
    uint8_t request_id,
    const uint8_t *header_data, uint16_t header_len,
    uint16_t num_chunks, uint16_t this_chunk,
    const uint8_t *body_data, uint16_t body_len) {
  uint16_t p = midi2_ci_build_header(buf, 0x7F, sub_id, version,
                                        src_muid, dst_muid);
  buf[p++] = request_id;
  /* Header data */
  midi2_ci_write_14(&buf[p], header_len); p += 2;
  if (header_data && header_len > 0) { memcpy(&buf[p], header_data, header_len); p += header_len; }
  /* Chunk info */
  midi2_ci_write_14(&buf[p], num_chunks); p += 2;
  midi2_ci_write_14(&buf[p], this_chunk); p += 2;
  /* Body data */
  midi2_ci_write_14(&buf[p], body_len); p += 2;
  if (body_data && body_len > 0) { memcpy(&buf[p], body_data, body_len); p += body_len; }
  return p;
}

/* Convenience wrappers */
static inline uint16_t midi2_ci_build_pe_get(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t request_id, const uint8_t *header, uint16_t header_len) {
  return midi2_ci_build_pe_data(buf, version, MIDI2_CI_PE_GET, src_muid, dst_muid,
                                   request_id, header, header_len, 1, 1, NULL, 0);
}

static inline uint16_t midi2_ci_build_pe_get_reply(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t request_id, const uint8_t *header, uint16_t header_len,
    uint16_t num_chunks, uint16_t this_chunk,
    const uint8_t *body, uint16_t body_len) {
  return midi2_ci_build_pe_data(buf, version, MIDI2_CI_PE_GET_REPLY, src_muid, dst_muid,
                                   request_id, header, header_len, num_chunks, this_chunk,
                                   body, body_len);
}

static inline uint16_t midi2_ci_build_pe_set(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t request_id, const uint8_t *header, uint16_t header_len,
    uint16_t num_chunks, uint16_t this_chunk,
    const uint8_t *body, uint16_t body_len) {
  return midi2_ci_build_pe_data(buf, version, MIDI2_CI_PE_SET, src_muid, dst_muid,
                                   request_id, header, header_len, num_chunks, this_chunk,
                                   body, body_len);
}

static inline uint16_t midi2_ci_build_pe_set_reply(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t request_id, const uint8_t *header, uint16_t header_len) {
  return midi2_ci_build_pe_data(buf, version, MIDI2_CI_PE_SET_REPLY, src_muid, dst_muid,
                                   request_id, header, header_len, 1, 1, NULL, 0);
}

static inline uint16_t midi2_ci_build_pe_subscribe(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t request_id, const uint8_t *header, uint16_t header_len,
    uint16_t num_chunks, uint16_t this_chunk,
    const uint8_t *body, uint16_t body_len) {
  return midi2_ci_build_pe_data(buf, version, MIDI2_CI_PE_SUBSCRIBE, src_muid, dst_muid,
                                   request_id, header, header_len, num_chunks, this_chunk,
                                   body, body_len);
}

static inline uint16_t midi2_ci_build_pe_subscribe_reply(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t request_id, const uint8_t *header, uint16_t header_len,
    uint16_t num_chunks, uint16_t this_chunk,
    const uint8_t *body, uint16_t body_len) {
  return midi2_ci_build_pe_data(buf, version, MIDI2_CI_PE_SUBSCRIBE_REPLY, src_muid, dst_muid,
                                   request_id, header, header_len, num_chunks, this_chunk,
                                   body, body_len);
}

static inline uint16_t midi2_ci_build_pe_notify(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t request_id, const uint8_t *header, uint16_t header_len,
    uint16_t num_chunks, uint16_t this_chunk,
    const uint8_t *body, uint16_t body_len) {
  return midi2_ci_build_pe_data(buf, version, MIDI2_CI_PE_NOTIFY, src_muid, dst_muid,
                                   request_id, header, header_len, num_chunks, this_chunk,
                                   body, body_len);
}

/*====================================================================+
 * CATEGORY 4: PROCESS INQUIRY MESSAGES
 *====================================================================*/

/*--------------------------------------------------------------------+
 * PI Capabilities Inquiry (0x40) -- Table 40
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_pi_capability(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid) {
  return midi2_ci_build_header(buf, 0x7F, MIDI2_CI_PI_CAPABILITY, version,
                                  src_muid, dst_muid);
}

/*--------------------------------------------------------------------+
 * Reply to PI Capabilities (0x41) -- Table 41
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_pi_capability_reply(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t supported_features) {
  uint16_t p = midi2_ci_build_header(buf, 0x7F, MIDI2_CI_PI_CAPABILITY_REPLY, version,
                                        src_muid, dst_muid);
  buf[p++] = supported_features;
  return p;
}

/*--------------------------------------------------------------------+
 * MIDI Message Report Inquiry (0x42) -- Table 43
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_pi_midi_report(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t device_id, uint8_t msg_data_control,
    uint8_t system_bitmap, uint8_t reserved,
    uint8_t channel_ctrl_bitmap, uint8_t note_data_bitmap) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_PI_MIDI_REPORT, version,
                                        src_muid, dst_muid);
  buf[p++] = msg_data_control;
  buf[p++] = system_bitmap;
  buf[p++] = reserved;
  buf[p++] = channel_ctrl_bitmap;
  buf[p++] = note_data_bitmap;
  return p;
}

/*--------------------------------------------------------------------+
 * Reply to MIDI Message Report (0x43) -- Table 45
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_pi_midi_report_reply(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t device_id,
    uint8_t system_bitmap, uint8_t reserved,
    uint8_t channel_ctrl_bitmap, uint8_t note_data_bitmap) {
  uint16_t p = midi2_ci_build_header(buf, device_id, MIDI2_CI_PI_MIDI_REPORT_REPLY,
                                        version, src_muid, dst_muid);
  buf[p++] = system_bitmap;
  buf[p++] = reserved;
  buf[p++] = channel_ctrl_bitmap;
  buf[p++] = note_data_bitmap;
  return p;
}

/*--------------------------------------------------------------------+
 * End of MIDI Message Report (0x44) -- Table 46
 *--------------------------------------------------------------------*/
static inline uint16_t midi2_ci_build_pi_midi_report_end(
    uint8_t *buf, uint8_t version, uint32_t src_muid, uint32_t dst_muid,
    uint8_t device_id) {
  return midi2_ci_build_header(buf, device_id, MIDI2_CI_PI_MIDI_REPORT_END, version,
                                  src_muid, dst_muid);
}

#ifdef __cplusplus
}
#endif

#endif /* MIDI2_CI_MSG_H */
