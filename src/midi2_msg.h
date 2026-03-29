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

#ifndef MIDI2_MSG_H
#define MIDI2_MSG_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------+
 * UMP Message Types (bits [31:28] of word 0)
 *--------------------------------------------------------------------*/
enum {
  MIDI2_MT_UTILITY    = 0x00,  /* 1 word  */
  MIDI2_MT_SYSTEM     = 0x01,  /* 1 word  */
  MIDI2_MT_MIDI1_CV   = 0x02,  /* 1 word  */
  MIDI2_MT_SYSEX7     = 0x03,  /* 2 words */
  MIDI2_MT_MIDI2_CV   = 0x04,  /* 2 words */
  MIDI2_MT_DATA128    = 0x05,  /* 4 words */
  MIDI2_MT_FLEX_DATA  = 0x0D,  /* 4 words */
  MIDI2_MT_STREAM     = 0x0F,  /* 4 words */
};

/*--------------------------------------------------------------------+
 * MIDI 2.0 Channel Voice Status (upper nibble, MT 0x4)
 *--------------------------------------------------------------------*/
enum {
  MIDI2_STATUS_NOTE_OFF      = 0x80,
  MIDI2_STATUS_NOTE_ON       = 0x90,
  MIDI2_STATUS_POLY_PRESSURE = 0xA0,
  MIDI2_STATUS_CC            = 0xB0,
  MIDI2_STATUS_PROGRAM       = 0xC0,
  MIDI2_STATUS_CHAN_PRESSURE  = 0xD0,
  MIDI2_STATUS_PITCH_BEND    = 0xE0,
  MIDI2_STATUS_PER_NOTE_MGMT = 0xF0,
  MIDI2_STATUS_RPN           = 0x20,
  MIDI2_STATUS_NRPN          = 0x30,
  MIDI2_STATUS_PER_NOTE_PB   = 0x60,
  MIDI2_STATUS_PER_NOTE_CC   = 0x10,
};

/*--------------------------------------------------------------------+
 * SysEx7 Status
 *--------------------------------------------------------------------*/
enum {
  MIDI2_SYSEX7_COMPLETE = 0x00,
  MIDI2_SYSEX7_START    = 0x10,
  MIDI2_SYSEX7_CONTINUE = 0x20,
  MIDI2_SYSEX7_END      = 0x30,
};

/*--------------------------------------------------------------------+
 * Flex Data Status
 *--------------------------------------------------------------------*/
enum {
  MIDI2_FLEX_TEMPO    = 0x00,
  MIDI2_FLEX_TIME_SIG = 0x01,
  MIDI2_FLEX_KEY_SIG  = 0x05,
};

/*--------------------------------------------------------------------+
 * Word Count
 *--------------------------------------------------------------------*/
static inline uint8_t midi2_msg_word_count(uint8_t mt) {
  switch (mt) {
    case MIDI2_MT_UTILITY:
    case MIDI2_MT_SYSTEM:
    case MIDI2_MT_MIDI1_CV:
      return 1;
    case MIDI2_MT_SYSEX7:
    case MIDI2_MT_MIDI2_CV:
      return 2;
    case MIDI2_MT_DATA128:
    case MIDI2_MT_FLEX_DATA:
    case MIDI2_MT_STREAM:
      return 4;
    default:
      return 1;
  }
}

/*--------------------------------------------------------------------+
 * Parsing (field extraction from word 0)
 *--------------------------------------------------------------------*/

/** @brief Extract message type (bits [31:28] of word 0). */
static inline uint8_t  midi2_msg_get_mt(const uint32_t *w)       { return (uint8_t)((w[0] >> 28) & 0x0F); }
/** @brief Extract group number (bits [27:24] of word 0). Range: 0-15. */
static inline uint8_t  midi2_msg_get_group(const uint32_t *w)    { return (uint8_t)((w[0] >> 24) & 0x0F); }
/** @brief Extract full status byte (bits [23:16] of word 0). Includes status nibble + channel. */
static inline uint8_t  midi2_msg_get_status(const uint32_t *w)   { return (uint8_t)((w[0] >> 16) & 0xFF); }
/** @brief Extract channel number (bits [19:16] of word 0). Range: 0-15. */
static inline uint8_t  midi2_msg_get_channel(const uint32_t *w)  { return (uint8_t)((w[0] >> 16) & 0x0F); }
/** @brief Extract note/index field (bits [15:8] of word 0). Full 8-bit field. */
static inline uint8_t  midi2_msg_get_note(const uint32_t *w)     { return (uint8_t)((w[0] >> 8) & 0xFF); }
/** @brief Extract velocity (bits [31:16] of word 1). 16-bit MIDI 2.0 velocity. */
static inline uint16_t midi2_msg_get_velocity(const uint32_t *w) { return (uint16_t)(w[1] >> 16); }
/** @brief Extract full word 1 (32-bit data payload). */
static inline uint32_t midi2_msg_get_data(const uint32_t *w)     { return w[1]; }

/*--------------------------------------------------------------------+
 * Value Scaling (MIDI 2.0 spec section 4.2.1)
 *
 * Bit-replication for symmetric round-trip: scaleDown(scaleUp(x)) == x
 *--------------------------------------------------------------------*/

/** @brief Scale 7-bit (0-127) to 16-bit (0-65535) with bit-replication. */
static inline uint16_t midi2_msg_scale_up_7to16(uint8_t v) {
  uint16_t x = (uint16_t)(v & 0x7F);
  return (uint16_t)((x << 9) | (x << 2) | (x >> 5));
}

/** @brief Scale 7-bit (0-127) to 32-bit (0-0xFFFFFFFF) with bit-replication. */
static inline uint32_t midi2_msg_scale_up_7to32(uint8_t v) {
  uint32_t x = (uint32_t)(v & 0x7F);
  return (x << 25) | (x << 18) | (x << 11) | (x << 4) | (x >> 3);
}

/** @brief Scale 14-bit (0-16383) to 32-bit (0-0xFFFFFFFF) with bit-replication. */
static inline uint32_t midi2_msg_scale_up_14to32(uint16_t v) {
  uint32_t x = (uint32_t)(v & 0x3FFF);
  return (x << 18) | (x << 4) | (x >> 10);
}

/** @brief Scale 16-bit to 7-bit. */
static inline uint8_t  midi2_msg_scale_down_16to7(uint16_t v)  { return (uint8_t)(v >> 9); }
/** @brief Scale 32-bit to 7-bit. */
static inline uint8_t  midi2_msg_scale_down_32to7(uint32_t v)  { return (uint8_t)(v >> 25); }
/** @brief Scale 32-bit to 14-bit. */
static inline uint16_t midi2_msg_scale_down_32to14(uint32_t v) { return (uint16_t)(v >> 18); }

/*--------------------------------------------------------------------+
 * MIDI 2.0 Channel Voice Construction (MT 0x4, 2 words)
 *
 * All write into a uint32_t w[2] provided by caller.
 *--------------------------------------------------------------------*/

/* Internal: build word 0 for MT 0x4 */
static inline uint32_t midi2_msg_build_cv2_w0(uint8_t group, uint8_t status,
                                          uint8_t channel, uint8_t b3, uint8_t b4) {
  return ((uint32_t)MIDI2_MT_MIDI2_CV << 28)
       | ((uint32_t)(group & 0x0F) << 24)
       | ((uint32_t)(status & 0xF0) << 16)
       | ((uint32_t)(channel & 0x0F) << 16)
       | ((uint32_t)b3 << 8)
       | (uint32_t)b4;
}

/**
 * @brief Build a MIDI 2.0 Note On message (MT 0x4, 2 words).
 * @param w         Output: uint32_t[2] provided by caller.
 * @param group     UMP group (0-15).
 * @param channel   MIDI channel (0-15).
 * @param note      Note number (0-127).
 * @param velocity  16-bit velocity (0x0000-0xFFFF). Use midi2_msg_scale_up_7to16() for legacy values.
 * @param attribute Note attribute (0 = none, or type<<8 | value). */
static inline void midi2_msg_note_on(uint32_t *w, uint8_t group, uint8_t channel,
                                      uint8_t note, uint16_t velocity, uint16_t attribute) {
  uint8_t attr_type = (attribute != 0) ? (uint8_t)(attribute >> 8) : 0;
  w[0] = midi2_msg_build_cv2_w0(group, MIDI2_STATUS_NOTE_ON, channel, note & 0x7F, attr_type);
  w[1] = ((uint32_t)velocity << 16) | (attribute & 0xFFFF);
}

/** @brief Build a MIDI 2.0 Note Off message (MT 0x4, 2 words). See midi2_msg_note_on() for params. */
static inline void midi2_msg_note_off(uint32_t *w, uint8_t group, uint8_t channel,
                                       uint8_t note, uint16_t velocity, uint16_t attribute) {
  uint8_t attr_type = (attribute != 0) ? (uint8_t)(attribute >> 8) : 0;
  w[0] = midi2_msg_build_cv2_w0(group, MIDI2_STATUS_NOTE_OFF, channel, note & 0x7F, attr_type);
  w[1] = ((uint32_t)velocity << 16) | (attribute & 0xFFFF);
}

/** @brief Build a MIDI 2.0 Control Change (MT 0x4). @param value 32-bit CC value. */
static inline void midi2_msg_cc(uint32_t *w, uint8_t group, uint8_t channel,
                                 uint8_t index, uint32_t value) {
  w[0] = midi2_msg_build_cv2_w0(group, MIDI2_STATUS_CC, channel, index & 0x7F, 0);
  w[1] = value;
}

/** @brief Build a MIDI 2.0 Program Change (MT 0x4). @param bank_valid true to include bank select. */
static inline void midi2_msg_program(uint32_t *w, uint8_t group, uint8_t channel,
                                      uint8_t program, bool bank_valid,
                                      uint8_t bank_msb, uint8_t bank_lsb) {
  w[0] = midi2_msg_build_cv2_w0(group, MIDI2_STATUS_PROGRAM, channel, 0, 0);
  w[1] = (bank_valid ? (UINT32_C(1) << 31) : 0)
       | ((uint32_t)(program & 0x7F) << 24)
       | (bank_valid ? (((uint32_t)(bank_msb & 0x7F) << 8) | (bank_lsb & 0x7F)) : 0);
}

/** @brief Build a MIDI 2.0 Pitch Bend (MT 0x4). @param value 32-bit, 0x80000000 = center. */
static inline void midi2_msg_pitch_bend(uint32_t *w, uint8_t group, uint8_t channel,
                                         uint32_t value) {
  w[0] = midi2_msg_build_cv2_w0(group, MIDI2_STATUS_PITCH_BEND, channel, 0, 0);
  w[1] = value;
}

/** @brief Build a MIDI 2.0 Channel Pressure (MT 0x4). @param pressure 32-bit pressure value. */
static inline void midi2_msg_chan_pressure(uint32_t *w, uint8_t group, uint8_t channel,
                                            uint32_t pressure) {
  w[0] = midi2_msg_build_cv2_w0(group, MIDI2_STATUS_CHAN_PRESSURE, channel, 0, 0);
  w[1] = pressure;
}

/** @brief Build a MIDI 2.0 Polyphonic Key Pressure (MT 0x4). */
static inline void midi2_msg_poly_pressure(uint32_t *w, uint8_t group, uint8_t channel,
                                             uint8_t note, uint32_t pressure) {
  w[0] = midi2_msg_build_cv2_w0(group, MIDI2_STATUS_POLY_PRESSURE, channel, note & 0x7F, 0);
  w[1] = pressure;
}

/** @brief Build a MIDI 2.0 RPN message (MT 0x4). @param msb RPN bank. @param lsb RPN index. */
static inline void midi2_msg_rpn(uint32_t *w, uint8_t group, uint8_t channel,
                                   uint8_t msb, uint8_t lsb, uint32_t value) {
  w[0] = midi2_msg_build_cv2_w0(group, MIDI2_STATUS_RPN, channel, msb & 0x7F, lsb & 0x7F);
  w[1] = value;
}

/** @brief Build a MIDI 2.0 NRPN message (MT 0x4). Non-registered parameter. */
static inline void midi2_msg_nrpn(uint32_t *w, uint8_t group, uint8_t channel,
                                    uint8_t msb, uint8_t lsb, uint32_t value) {
  w[0] = midi2_msg_build_cv2_w0(group, MIDI2_STATUS_NRPN, channel, msb & 0x7F, lsb & 0x7F);
  w[1] = value;
}

/*--------------------------------------------------------------------+
 * Per-Note Expression (MT 0x4, 2 words)
 *--------------------------------------------------------------------*/
/** @brief Build a per-note Pitch Bend (MT 0x4). Independent pitch per note. */
static inline void midi2_msg_per_note_pb(uint32_t *w, uint8_t group, uint8_t channel,
                                           uint8_t note, uint32_t value) {
  w[0] = midi2_msg_build_cv2_w0(group, MIDI2_STATUS_PER_NOTE_PB, channel, note & 0x7F, 0);
  w[1] = value;
}

/** @brief Build a per-note CC (MT 0x4). Independent controller per note. */
static inline void midi2_msg_per_note_cc(uint32_t *w, uint8_t group, uint8_t channel,
                                           uint8_t note, uint8_t index, uint32_t value) {
  w[0] = midi2_msg_build_cv2_w0(group, MIDI2_STATUS_PER_NOTE_CC, channel, note & 0x7F, index);
  w[1] = value;
}

/** @brief Build a per-note Management message (MT 0x4). @param detach release from voice. @param reset reset controllers. */
static inline void midi2_msg_per_note_mgmt(uint32_t *w, uint8_t group, uint8_t channel,
                                             uint8_t note, bool detach, bool reset) {
  uint8_t flags = (detach ? 0x02 : 0) | (reset ? 0x01 : 0);
  w[0] = midi2_msg_build_cv2_w0(group, MIDI2_STATUS_PER_NOTE_MGMT, channel, note & 0x7F, flags);
  w[1] = 0;
}

/*--------------------------------------------------------------------+
 * System Messages (MT 0x1, 1 word)
 *--------------------------------------------------------------------*/
static inline uint32_t midi2_msg_system(uint8_t group, uint8_t status) {
  return ((uint32_t)MIDI2_MT_SYSTEM << 28)
       | ((uint32_t)(group & 0x0F) << 24)
       | ((uint32_t)status << 16);
}

static inline uint32_t midi2_msg_system_2byte(uint8_t group, uint8_t status, uint8_t data1) {
  return midi2_msg_system(group, status) | ((uint32_t)data1 << 8);
}

static inline uint32_t midi2_msg_system_3byte(uint8_t group, uint8_t status,
                                                uint8_t data1, uint8_t data2) {
  return midi2_msg_system(group, status) | ((uint32_t)data1 << 8) | (uint32_t)data2;
}

/*--------------------------------------------------------------------+
 * Flex Data (MT 0xD, 4 words)
 *
 * Word 0: [MT:4][group:4][format:2][address:2][channel:4][statusBank:8][status:8]
 * Tempo, TimeSignature, KeySignature are group-level (address=0b01).
 *--------------------------------------------------------------------*/

/* Internal: flex data word 0 builder */
static inline uint32_t midi2_msg_build_flex_w0(uint8_t group, uint8_t status) {
  return ((uint32_t)MIDI2_MT_FLEX_DATA << 28)
       | ((uint32_t)(group & 0x0F) << 24)
       | ((uint32_t)0x01 << 20)   /* address = 0b01 (group-level) */
       | (uint32_t)status;
}

/* Tempo: word1 = 10ns per quarter note */
static inline void midi2_msg_tempo(uint32_t *w, uint8_t group, uint32_t ten_ns_per_qn) {
  memset(w, 0, 16);
  w[0] = midi2_msg_build_flex_w0(group, MIDI2_FLEX_TEMPO);
  w[1] = ten_ns_per_qn;
}

/* Time Signature: word1 = [numerator:8][denominator:8][0:16] */
static inline void midi2_msg_time_sig(uint32_t *w, uint8_t group,
                                        uint8_t numerator, uint8_t denominator) {
  memset(w, 0, 16);
  w[0] = midi2_msg_build_flex_w0(group, MIDI2_FLEX_TIME_SIG);
  w[1] = ((uint32_t)numerator << 24) | ((uint32_t)denominator << 16);
}

/* Key Signature: word1 = [sharpsFlats:4][tonicNote:4][keyType:2][0:22]
 * sharpsFlats: -7 to +7 (4-bit signed). keyType: 0=major, 1=minor. */
static inline void midi2_msg_key_sig(uint32_t *w, uint8_t group,
                                       int8_t sharps_flats, bool minor) {
  memset(w, 0, 16);
  w[0] = midi2_msg_build_flex_w0(group, MIDI2_FLEX_KEY_SIG);
  uint8_t sf4 = (uint8_t)(sharps_flats & 0x0F);
  uint8_t key_type = minor ? 1 : 0;
  w[1] = ((uint32_t)sf4 << 28) | ((uint32_t)key_type << 22);
}

/*--------------------------------------------------------------------+
 * SysEx7 Single Packet (MT 0x3, 2 words)
 *
 * Word 0: [MT:4][group:4][status:4][numBytes:4][data0:8][data1:8]
 * Word 1: [data2:8][data3:8][data4:8][data5:8]
 *--------------------------------------------------------------------*/
static inline void midi2_msg_sysex7_packet(uint32_t *w, uint8_t group,
                                             uint8_t status, const uint8_t *data, uint8_t len) {
  if (len > 6) len = 6;
  w[0] = ((uint32_t)MIDI2_MT_SYSEX7 << 28)
       | ((uint32_t)(group & 0x0F) << 24)
       | ((uint32_t)(status | (len & 0x0F)) << 16);
  w[1] = 0;
  uint8_t i;
  for (i = 0; i < len; i++) {
    uint8_t wi = (i < 2) ? 0 : 1;
    uint8_t sh = (i < 2) ? (uint8_t)(8 - i * 8) : (uint8_t)(24 - (i - 2) * 8);
    w[wi] |= ((uint32_t)data[i] << sh);
  }
}

/*--------------------------------------------------------------------+
 * Utility Messages -- JR Timestamps (MT 0x0, 1 word)
 *
 * Word: [MT:4][group:4][status:4][0:4][timestamp:16]
 * JR Clock status = 0x0010, JR Timestamp status = 0x0020
 * Timestamp unit: 1/31250 of a second (~32us)
 *--------------------------------------------------------------------*/
enum {
  MIDI2_UTILITY_JR_CLOCK     = 0x01,
  MIDI2_UTILITY_JR_TIMESTAMP = 0x02,
};

static inline uint32_t midi2_msg_jr_clock(uint8_t group, uint16_t timestamp) {
  return ((uint32_t)MIDI2_MT_UTILITY << 28)
       | ((uint32_t)(group & 0x0F) << 24)
       | ((uint32_t)MIDI2_UTILITY_JR_CLOCK << 16)
       | (uint32_t)timestamp;
}

static inline uint32_t midi2_msg_jr_timestamp(uint8_t group, uint16_t timestamp) {
  return ((uint32_t)MIDI2_MT_UTILITY << 28)
       | ((uint32_t)(group & 0x0F) << 24)
       | ((uint32_t)MIDI2_UTILITY_JR_TIMESTAMP << 16)
       | (uint32_t)timestamp;
}

/*--------------------------------------------------------------------+
 * Stream Messages (MT 0xF, 4 words)
 *
 * Word 0: [MT:4][format:2][status:10][data:16]
 * Words 1-3: payload (depends on status)
 *
 * format: 0b00 = complete, 0b01 = start, 0b10 = continue, 0b11 = end
 *--------------------------------------------------------------------*/
enum {
  MIDI2_STREAM_ENDPOINT_DISCOVERY  = 0x000,
  MIDI2_STREAM_ENDPOINT_INFO       = 0x001,
  MIDI2_STREAM_DEVICE_IDENTITY     = 0x002,
  MIDI2_STREAM_ENDPOINT_NAME       = 0x003,
  MIDI2_STREAM_PRODUCT_INSTANCE_ID = 0x004,
  MIDI2_STREAM_CONFIG_REQUEST      = 0x005,
  MIDI2_STREAM_CONFIG_NOTIFY       = 0x006,
  MIDI2_STREAM_FB_DISCOVERY        = 0x010,
  MIDI2_STREAM_FB_INFO             = 0x011,
  MIDI2_STREAM_FB_NAME             = 0x012,
  MIDI2_STREAM_START_OF_CLIP       = 0x020,
  MIDI2_STREAM_END_OF_CLIP         = 0x021,
};

/* Internal: build stream word 0 */
static inline uint32_t midi2_msg_build_stream_w0(uint8_t format, uint16_t status) {
  return ((uint32_t)MIDI2_MT_STREAM << 28)
       | ((uint32_t)(format & 0x03) << 26)
       | ((uint32_t)(status & 0x3FF) << 16);
}

/* Endpoint Discovery: request endpoint info from a device
 * ump_ver_major/minor: UMP version we support
 * filter: bitmask of what to request (bit 0=endpoint info, 1=device identity,
 *         2=endpoint name, 3=product instance ID, 4=stream config) */
static inline void midi2_msg_stream_endpoint_discovery(uint32_t *w,
                                                         uint8_t ump_ver_major,
                                                         uint8_t ump_ver_minor,
                                                         uint8_t filter) {
  memset(w, 0, 16);
  w[0] = midi2_msg_build_stream_w0(0, MIDI2_STREAM_ENDPOINT_DISCOVERY)
       | ((uint32_t)ump_ver_major << 8)
       | (uint32_t)ump_ver_minor;
  w[1] = (uint32_t)filter;
}

/* Endpoint Info Reply
 * static_fb: true if function blocks are static
 * num_fb: number of function blocks (0-31)
 * midi1_proto: supports MIDI 1.0 protocol
 * midi2_proto: supports MIDI 2.0 protocol
 * rx_jr/tx_jr: supports JR timestamps */
static inline void midi2_msg_stream_endpoint_info(uint32_t *w,
                                                    uint8_t ump_ver_major,
                                                    uint8_t ump_ver_minor,
                                                    bool static_fb, uint8_t num_fb,
                                                    bool midi2_proto, bool midi1_proto,
                                                    bool rx_jr, bool tx_jr) {
  memset(w, 0, 16);
  w[0] = midi2_msg_build_stream_w0(0, MIDI2_STREAM_ENDPOINT_INFO)
       | ((uint32_t)ump_ver_major << 8)
       | (uint32_t)ump_ver_minor;
  w[1] = (static_fb ? (UINT32_C(1) << 31) : 0)
       | ((uint32_t)(num_fb & 0x7F) << 24)
       | (midi2_proto ? (UINT32_C(1) << 9) : 0)
       | (midi1_proto ? (UINT32_C(1) << 8) : 0)
       | (rx_jr ? (UINT32_C(1) << 1) : 0)
       | (tx_jr ? UINT32_C(1) : 0);
}

/* Device Identity Notification */
static inline void midi2_msg_stream_device_identity(uint32_t *w,
                                                      uint32_t manufacturer_id,
                                                      uint16_t family_id,
                                                      uint16_t model_id,
                                                      uint32_t version_id) {
  memset(w, 0, 16);
  w[0] = midi2_msg_build_stream_w0(0, MIDI2_STREAM_DEVICE_IDENTITY);
  w[1] = (manufacturer_id & 0x00FFFFFF) << 8;
  w[2] = ((uint32_t)family_id << 16) | (uint32_t)model_id;
  w[3] = version_id;
}

/* Stream Configuration Request/Notify
 * protocol: 0x01 = MIDI 1.0, 0x02 = MIDI 2.0 */
static inline void midi2_msg_stream_config_request(uint32_t *w, uint8_t protocol) {
  memset(w, 0, 16);
  w[0] = midi2_msg_build_stream_w0(0, MIDI2_STREAM_CONFIG_REQUEST)
       | ((uint32_t)protocol << 8);
}

static inline void midi2_msg_stream_config_notify(uint32_t *w, uint8_t protocol) {
  memset(w, 0, 16);
  w[0] = midi2_msg_build_stream_w0(0, MIDI2_STREAM_CONFIG_NOTIFY)
       | ((uint32_t)protocol << 8);
}

/* Function Block Discovery
 * fb_num: function block number to query (0xFF = all)
 * filter: bitmask (bit 0 = FB info, bit 1 = FB name) */
static inline void midi2_msg_stream_fb_discovery(uint32_t *w, uint8_t fb_num, uint8_t filter) {
  memset(w, 0, 16);
  w[0] = midi2_msg_build_stream_w0(0, MIDI2_STREAM_FB_DISCOVERY)
       | ((uint32_t)fb_num << 8)
       | (uint32_t)filter;
}

/* Function Block Info
 * active: FB is active
 * fb_num: function block number
 * direction: 0x00=input, 0x01=output, 0x02=bidirectional
 * first_group: first group in this FB
 * num_groups: number of groups
 * midi_ci_ver: MIDI-CI version support (0=none, 1=1.1, 2=1.2)
 * sysex8: supports SysEx8
 * protocol: 0x00=unknown, 0x01=MIDI1, 0x02=MIDI2, 0x03=both */
static inline void midi2_msg_stream_fb_info(uint32_t *w,
                                              bool active, uint8_t fb_num,
                                              uint8_t direction,
                                              uint8_t first_group, uint8_t num_groups,
                                              uint8_t midi_ci_ver, bool sysex8,
                                              uint8_t protocol) {
  memset(w, 0, 16);
  w[0] = midi2_msg_build_stream_w0(0, MIDI2_STREAM_FB_INFO)
       | (active ? (UINT32_C(1) << 15) : 0)
       | ((uint32_t)(fb_num & 0x7F) << 8)
       | (uint32_t)(direction & 0x03);
  w[1] = ((uint32_t)(first_group & 0x0F) << 24)
       | ((uint32_t)(num_groups & 0x0F) << 16)
       | ((uint32_t)(midi_ci_ver & 0x03) << 8)
       | (sysex8 ? (UINT32_C(1) << 2) : 0)
       | (uint32_t)(protocol & 0x03);
}

/* Start/End of Clip */
static inline void midi2_msg_stream_start_of_clip(uint32_t *w) {
  memset(w, 0, 16);
  w[0] = midi2_msg_build_stream_w0(0, MIDI2_STREAM_START_OF_CLIP);
}

static inline void midi2_msg_stream_end_of_clip(uint32_t *w) {
  memset(w, 0, 16);
  w[0] = midi2_msg_build_stream_w0(0, MIDI2_STREAM_END_OF_CLIP);
}

/*--------------------------------------------------------------------+
 * SysEx8 (MT 0x5, 4 words)
 *
 * Word 0: [MT:4][group:4][status:4][numBytes:4][streamID:8][data0:8]
 * Word 1: [data1:8][data2:8][data3:8][data4:8]
 * Word 2: [data5:8][data6:8][data7:8][data8:8]
 * Word 3: [data9:8][data10:8][data11:8][data12:8]
 *
 * Max 13 data bytes per packet. 8-bit data (no 7-bit restriction).
 * SysEx8 and SysEx7 coexist in the same UMP stream as different MT values.
 *--------------------------------------------------------------------*/
enum {
  MIDI2_SYSEX8_COMPLETE = 0x00,
  MIDI2_SYSEX8_START    = 0x10,
  MIDI2_SYSEX8_CONTINUE = 0x20,
  MIDI2_SYSEX8_END      = 0x30,
};

static inline void midi2_msg_sysex8_packet(uint32_t *w, uint8_t group,
                                             uint8_t status, uint8_t stream_id,
                                             const uint8_t *data, uint8_t len) {
  if (len > 13) len = 13;
  memset(w, 0, 16);
  /* numBytes includes stream_id + data bytes */
  uint8_t num_bytes = (uint8_t)(len + 1);
  w[0] = ((uint32_t)MIDI2_MT_DATA128 << 28)
       | ((uint32_t)(group & 0x0F) << 24)
       | ((uint32_t)(status | (num_bytes & 0x0F)) << 16)
       | ((uint32_t)stream_id << 8);

  /* Pack data bytes: data[0] goes to w[0] bits [7:0],
   * data[1..4] into w[1], data[5..8] into w[2], data[9..12] into w[3] */
  uint8_t i;
  if (len >= 1) w[0] |= (uint32_t)data[0];
  for (i = 1; i < len; i++) {
    uint8_t wi = (uint8_t)(1 + (i - 1) / 4);
    uint8_t sh = (uint8_t)(24 - ((i - 1) % 4) * 8);
    w[wi] |= ((uint32_t)data[i] << sh);
  }
}

/*--------------------------------------------------------------------+
 * MIDI 1.0 Byte Stream to UMP Conversion (stateless, single message)
 *
 * Converts a complete MIDI 1.0 message (1-3 bytes) to a UMP word (MT 0x2).
 * Does NOT handle Running Status or SysEx -- that requires state (see midi2_conv).
 * Useful when the platform already parsed the byte stream.
 *--------------------------------------------------------------------*/
static inline uint32_t midi2_msg_from_midi1(uint8_t group,
                                              uint8_t status, uint8_t data1, uint8_t data2) {
  return ((uint32_t)MIDI2_MT_MIDI1_CV << 28)
       | ((uint32_t)(group & 0x0F) << 24)
       | ((uint32_t)status << 16)
       | ((uint32_t)(data1 & 0x7F) << 8)
       | (uint32_t)(data2 & 0x7F);
}

#ifdef __cplusplus
}
#endif

#endif /* MIDI2_MSG_H */
