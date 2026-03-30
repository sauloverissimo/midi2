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

#include "midi2_dispatch.h"
#include <string.h>

/*--------------------------------------------------------------------+
 * Init
 *--------------------------------------------------------------------*/
void midi2_dispatch_init(midi2_dispatch *dp) {
  memset(dp, 0, sizeof(*dp));
}

/*--------------------------------------------------------------------+
 * Internal: dispatch MT 0x0 Utility (1 word)
 *--------------------------------------------------------------------*/
static void dispatch_utility(midi2_dispatch *dp, uint32_t w) {
  uint8_t group  = (uint8_t)((w >> 24) & 0x0F);
  uint8_t status = (uint8_t)((w >> 20) & 0x0F);

  switch (status) {
    case MIDI2_UTILITY_NOOP:
      if (dp->on_noop) dp->on_noop(dp->context);
      break;
    case MIDI2_UTILITY_JR_CLOCK:
      if (dp->on_jr_clock) dp->on_jr_clock(group, (uint16_t)(w & 0xFFFF), dp->context);
      break;
    case MIDI2_UTILITY_JR_TIMESTAMP:
      if (dp->on_jr_timestamp) dp->on_jr_timestamp(group, (uint16_t)(w & 0xFFFF), dp->context);
      break;
    case MIDI2_UTILITY_DCTPQ:
      if (dp->on_dctpq) dp->on_dctpq((uint16_t)(w & 0xFFFF), dp->context);
      break;
    case MIDI2_UTILITY_DC:
      if (dp->on_dc) dp->on_dc(w & 0x000FFFFF, dp->context);
      break;
    default:
      if (dp->on_unknown) dp->on_unknown(&w, 1, dp->context);
      break;
  }
}

/*--------------------------------------------------------------------+
 * Internal: dispatch MT 0x1 System (1 word)
 *--------------------------------------------------------------------*/
static void dispatch_system(midi2_dispatch *dp, uint32_t w) {
  if (!dp->on_system) return;
  uint8_t group  = (uint8_t)((w >> 24) & 0x0F);
  uint8_t status = (uint8_t)((w >> 16) & 0xFF);
  uint8_t data1  = (uint8_t)((w >> 8) & 0xFF);
  uint8_t data2  = (uint8_t)(w & 0xFF);
  dp->on_system(group, status, data1, data2, dp->context);
}

/*--------------------------------------------------------------------+
 * Internal: dispatch MT 0x2 MIDI 1.0 Channel Voice (1 word)
 *--------------------------------------------------------------------*/
static void dispatch_cv1(midi2_dispatch *dp, uint32_t w) {
  uint8_t group   = (uint8_t)((w >> 24) & 0x0F);
  uint8_t status  = (uint8_t)((w >> 16) & 0xF0);
  uint8_t channel = (uint8_t)((w >> 16) & 0x0F);
  uint8_t data1   = (uint8_t)((w >> 8) & 0x7F);
  uint8_t data2   = (uint8_t)(w & 0x7F);

  switch (status) {
    case 0x80:
      if (dp->on_cv1_note_off) dp->on_cv1_note_off(group, channel, data1, data2, dp->context);
      break;
    case 0x90:
      if (dp->on_cv1_note_on) dp->on_cv1_note_on(group, channel, data1, data2, dp->context);
      break;
    case 0xA0:
      if (dp->on_cv1_poly_pressure) dp->on_cv1_poly_pressure(group, channel, data1, data2, dp->context);
      break;
    case 0xB0:
      if (dp->on_cv1_cc) dp->on_cv1_cc(group, channel, data1, data2, dp->context);
      break;
    case 0xC0:
      if (dp->on_cv1_program) dp->on_cv1_program(group, channel, data1, dp->context);
      break;
    case 0xD0:
      if (dp->on_cv1_chan_pressure) dp->on_cv1_chan_pressure(group, channel, data1, dp->context);
      break;
    case 0xE0:
      if (dp->on_cv1_pitch_bend) {
        uint16_t pb = (uint16_t)((data2 << 7) | data1);
        dp->on_cv1_pitch_bend(group, channel, pb, dp->context);
      }
      break;
    default:
      if (dp->on_unknown) dp->on_unknown(&w, 1, dp->context);
      break;
  }
}

/*--------------------------------------------------------------------+
 * Internal: dispatch MT 0x3 SysEx7 (2 words)
 *--------------------------------------------------------------------*/
static void dispatch_sysex7(midi2_dispatch *dp, const uint32_t *w) {
  if (!dp->on_sysex7) return;
  uint8_t group   = (uint8_t)((w[0] >> 24) & 0x0F);
  uint8_t status  = (uint8_t)((w[0] >> 16) & 0xF0); /* matches MIDI2_SYSEX7_* enums */
  uint8_t num     = (uint8_t)((w[0] >> 16) & 0x0F);
  if (num > 6) num = 6;

  uint8_t data[6];
  /* bytes packed: w[0][15:8], w[0][7:0], w[1][31:24]..w[1][7:0] */
  data[0] = (uint8_t)((w[0] >> 8) & 0xFF);
  data[1] = (uint8_t)(w[0] & 0xFF);
  data[2] = (uint8_t)((w[1] >> 24) & 0xFF);
  data[3] = (uint8_t)((w[1] >> 16) & 0xFF);
  data[4] = (uint8_t)((w[1] >> 8) & 0xFF);
  data[5] = (uint8_t)(w[1] & 0xFF);

  dp->on_sysex7(group, status, data, num, dp->context);
}

/*--------------------------------------------------------------------+
 * Internal: dispatch MT 0x4 MIDI 2.0 Channel Voice (2 words)
 *--------------------------------------------------------------------*/
static void dispatch_cv2(midi2_dispatch *dp, const uint32_t *w) {
  uint8_t group   = (uint8_t)((w[0] >> 24) & 0x0F);
  uint8_t status  = (uint8_t)((w[0] >> 16) & 0xF0);
  uint8_t channel = (uint8_t)((w[0] >> 16) & 0x0F);
  uint8_t byte3   = (uint8_t)((w[0] >> 8) & 0xFF);  /* note/bank/index */
  uint8_t byte4   = (uint8_t)(w[0] & 0xFF);          /* attr_type/index/lsb */

  switch (status) {
    case MIDI2_STATUS_NOTE_ON:
      if (dp->on_note_on) {
        uint16_t velocity = (uint16_t)(w[1] >> 16);
        uint16_t attr_data = (uint16_t)(w[1] & 0xFFFF);
        dp->on_note_on(group, channel, byte3 & 0x7F, velocity, byte4, attr_data, dp->context);
      }
      break;

    case MIDI2_STATUS_NOTE_OFF:
      if (dp->on_note_off) {
        uint16_t velocity = (uint16_t)(w[1] >> 16);
        uint16_t attr_data = (uint16_t)(w[1] & 0xFFFF);
        dp->on_note_off(group, channel, byte3 & 0x7F, velocity, byte4, attr_data, dp->context);
      }
      break;

    case MIDI2_STATUS_POLY_PRESSURE:
      if (dp->on_poly_pressure) dp->on_poly_pressure(group, channel, byte3 & 0x7F, w[1], dp->context);
      break;

    case MIDI2_STATUS_CC:
      if (dp->on_cc) dp->on_cc(group, channel, byte3 & 0x7F, w[1], dp->context);
      break;

    case MIDI2_STATUS_PROGRAM: {
      if (dp->on_program) {
        uint8_t program  = (uint8_t)((w[1] >> 24) & 0x7F);
        bool bank_valid  = (w[1] & (UINT32_C(1) << 31)) != 0;
        uint8_t bank_msb = (uint8_t)((w[1] >> 8) & 0x7F);
        uint8_t bank_lsb = (uint8_t)(w[1] & 0x7F);
        dp->on_program(group, channel, program, bank_valid, bank_msb, bank_lsb, dp->context);
      }
      break;
    }

    case MIDI2_STATUS_CHAN_PRESSURE:
      if (dp->on_chan_pressure) dp->on_chan_pressure(group, channel, w[1], dp->context);
      break;

    case MIDI2_STATUS_PITCH_BEND:
      if (dp->on_pitch_bend) dp->on_pitch_bend(group, channel, w[1], dp->context);
      break;

    case MIDI2_STATUS_PER_NOTE_PB:
      if (dp->on_per_note_pb) dp->on_per_note_pb(group, channel, byte3 & 0x7F, w[1], dp->context);
      break;

    case MIDI2_STATUS_REG_PER_NOTE:
      if (dp->on_reg_per_note) dp->on_reg_per_note(group, channel, byte3 & 0x7F, byte4, w[1], dp->context);
      break;

    case MIDI2_STATUS_ASN_PER_NOTE:
      if (dp->on_asn_per_note) dp->on_asn_per_note(group, channel, byte3 & 0x7F, byte4, w[1], dp->context);
      break;

    case MIDI2_STATUS_RPN:
      if (dp->on_rpn) dp->on_rpn(group, channel, byte3 & 0x7F, byte4 & 0x7F, w[1], dp->context);
      break;

    case MIDI2_STATUS_NRPN:
      if (dp->on_nrpn) dp->on_nrpn(group, channel, byte3 & 0x7F, byte4 & 0x7F, w[1], dp->context);
      break;

    case MIDI2_STATUS_REL_RPN:
      if (dp->on_rel_rpn) dp->on_rel_rpn(group, channel, byte3 & 0x7F, byte4 & 0x7F, w[1], dp->context);
      break;

    case MIDI2_STATUS_REL_NRPN:
      if (dp->on_rel_nrpn) dp->on_rel_nrpn(group, channel, byte3 & 0x7F, byte4 & 0x7F, w[1], dp->context);
      break;

    case MIDI2_STATUS_PER_NOTE_MGMT:
      if (dp->on_per_note_mgmt) {
        bool detach = (byte4 & 0x02) != 0;
        bool reset  = (byte4 & 0x01) != 0;
        dp->on_per_note_mgmt(group, channel, byte3 & 0x7F, detach, reset, dp->context);
      }
      break;

    default:
      if (dp->on_unknown) dp->on_unknown(w, 2, dp->context);
      break;
  }
}

/*--------------------------------------------------------------------+
 * Internal: dispatch MT 0x5 Data 128-bit (4 words)
 *--------------------------------------------------------------------*/
static void dispatch_data128(midi2_dispatch *dp, const uint32_t *w) {
  uint8_t group       = (uint8_t)((w[0] >> 24) & 0x0F);
  uint8_t status_byte = (uint8_t)((w[0] >> 16) & 0xFF);
  uint8_t status_hi   = status_byte & 0xF0;
  uint8_t id_lo       = status_byte & 0x0F;

  if (status_hi == MIDI2_MDS_HEADER) {
    /* Mixed Data Set Header */
    if (dp->on_mds_header) {
      uint16_t num_bytes   = (uint16_t)(w[0] & 0xFFFF);
      uint16_t num_chunks  = (uint16_t)(w[1] >> 16);
      uint16_t this_chunk  = (uint16_t)(w[1] & 0xFFFF);
      uint16_t mfr_id      = (uint16_t)(w[2] >> 16);
      uint16_t device_id   = (uint16_t)(w[2] & 0xFFFF);
      uint16_t sub_id1     = (uint16_t)(w[3] >> 16);
      uint16_t sub_id2     = (uint16_t)(w[3] & 0xFFFF);
      dp->on_mds_header(group, id_lo, num_bytes, num_chunks, this_chunk,
                           mfr_id, device_id, sub_id1, sub_id2, dp->context);
    }
  } else if (status_hi == MIDI2_MDS_PAYLOAD) {
    /* Mixed Data Set Payload */
    if (dp->on_mds_payload) {
      uint8_t data[14];
      uint8_t i;
      for (i = 0; i < 14; i++) {
        uint8_t wi = (uint8_t)((i + 2) / 4);
        uint8_t sh = (uint8_t)(24 - ((i + 2) % 4) * 8);
        data[i] = (uint8_t)((w[wi] >> sh) & 0xFF);
      }
      dp->on_mds_payload(group, id_lo, data, 14, dp->context);
    }
  } else {
    /* SysEx8 (status 0x00..0x30) */
    uint8_t sysex_status = status_byte & 0xF0; /* matches MIDI2_SYSEX8_* enums */
    uint8_t num_bytes    = status_byte & 0x0F;

    if (dp->on_sysex8) {
      uint8_t stream_id = (uint8_t)((w[0] >> 8) & 0xFF);
      /* data bytes: w[0][7:0], w[1][31:0], w[2][31:0], w[3][31:0] = up to 13 */
      uint8_t data[13];
      uint8_t data_len = (num_bytes > 1) ? (uint8_t)(num_bytes - 1) : 0; /* subtract stream_id */
      if (data_len > 13) data_len = 13;
      uint8_t i;
      if (data_len >= 1) data[0] = (uint8_t)(w[0] & 0xFF);
      for (i = 1; i < data_len; i++) {
        uint8_t wi = (uint8_t)(1 + (i - 1) / 4);
        uint8_t sh = (uint8_t)(24 - ((i - 1) % 4) * 8);
        data[i] = (uint8_t)((w[wi] >> sh) & 0xFF);
      }
      dp->on_sysex8(group, sysex_status, stream_id, data, data_len, dp->context);
    } else if (dp->on_unknown) {
      dp->on_unknown(w, 4, dp->context);
    }
  }
}

/*--------------------------------------------------------------------+
 * Internal: dispatch MT 0xD Flex Data (4 words)
 *--------------------------------------------------------------------*/
static void dispatch_flex(midi2_dispatch *dp, const uint32_t *w) {
  uint8_t group   = (uint8_t)((w[0] >> 24) & 0x0F);
  uint8_t format  = (uint8_t)((w[0] >> 22) & 0x03);
  uint8_t address = (uint8_t)((w[0] >> 20) & 0x03);
  uint8_t channel = (uint8_t)((w[0] >> 16) & 0x0F);
  uint8_t bank    = (uint8_t)((w[0] >> 8) & 0xFF);
  uint8_t status  = (uint8_t)(w[0] & 0xFF);

  if (bank == MIDI2_FLEX_BANK_SETUP) {
    switch (status) {
      case MIDI2_FLEX_TEMPO:
        if (dp->on_tempo) dp->on_tempo(group, w[1], dp->context);
        break;

      case MIDI2_FLEX_TIME_SIG:
        if (dp->on_time_sig) {
          uint8_t num = (uint8_t)((w[1] >> 24) & 0xFF);
          uint8_t den = (uint8_t)((w[1] >> 16) & 0xFF);
          uint8_t n32 = (uint8_t)((w[1] >> 8) & 0xFF);
          dp->on_time_sig(group, num, den, n32, dp->context);
        }
        break;

      case MIDI2_FLEX_METRONOME:
        if (dp->on_metronome) {
          uint8_t clicks = (uint8_t)((w[1] >> 24) & 0xFF);
          uint8_t a1     = (uint8_t)((w[1] >> 16) & 0xFF);
          uint8_t a2     = (uint8_t)((w[1] >> 8) & 0xFF);
          uint8_t a3     = (uint8_t)(w[1] & 0xFF);
          uint8_t s1     = (uint8_t)((w[2] >> 24) & 0xFF);
          uint8_t s2     = (uint8_t)((w[2] >> 16) & 0xFF);
          dp->on_metronome(group, clicks, a1, a2, a3, s1, s2, dp->context);
        }
        break;

      case MIDI2_FLEX_KEY_SIG:
        if (dp->on_key_sig) {
          uint8_t sf_raw  = (uint8_t)((w[1] >> 28) & 0x0F);
          int8_t sf       = (sf_raw & 0x08) ? (int8_t)(sf_raw | 0xF0) : (int8_t)sf_raw;
          uint8_t tonic   = (uint8_t)((w[1] >> 24) & 0x0F);
          uint8_t keytype = (uint8_t)((w[1] >> 22) & 0x03);
          dp->on_key_sig(group, address, channel, sf, tonic, keytype, dp->context);
        }
        break;

      case MIDI2_FLEX_CHORD_NAME:
        if (dp->on_chord) {
          uint8_t tsf_raw = (uint8_t)((w[1] >> 28) & 0x0F);
          int8_t  tsf = (tsf_raw & 0x08) ? (int8_t)(tsf_raw | 0xF0) : (int8_t)tsf_raw;
          uint8_t tn  = (uint8_t)((w[1] >> 24) & 0x0F);
          uint8_t ct  = (uint8_t)((w[1] >> 16) & 0xFF);
          uint8_t a1t = (uint8_t)((w[1] >> 12) & 0x0F);
          uint8_t a1d = (uint8_t)((w[1] >> 8) & 0x0F);
          uint8_t a2t = (uint8_t)((w[1] >> 4) & 0x0F);
          uint8_t a2d = (uint8_t)(w[1] & 0x0F);
          uint8_t a3t = (uint8_t)((w[2] >> 28) & 0x0F);
          uint8_t a3d = (uint8_t)((w[2] >> 24) & 0x0F);
          uint8_t a4t = (uint8_t)((w[2] >> 20) & 0x0F);
          uint8_t a4d = (uint8_t)((w[2] >> 16) & 0x0F);
          uint8_t bsf_raw = (uint8_t)((w[3] >> 28) & 0x0F);
          int8_t  bsf = (bsf_raw & 0x08) ? (int8_t)(bsf_raw | 0xF0) : (int8_t)bsf_raw;
          uint8_t bn  = (uint8_t)((w[3] >> 24) & 0x0F);
          uint8_t bt  = (uint8_t)((w[3] >> 16) & 0xFF);
          uint8_t b1t = (uint8_t)((w[3] >> 12) & 0x0F);
          uint8_t b1d = (uint8_t)((w[3] >> 8) & 0x0F);
          uint8_t b2t = (uint8_t)((w[3] >> 4) & 0x0F);
          uint8_t b2d = (uint8_t)(w[3] & 0x0F);
          dp->on_chord(group, address, channel,
                          tsf, tn, ct,
                          a1t, a1d, a2t, a2d, a3t, a3d, a4t, a4d,
                          bsf, bn, bt, b1t, b1d, b2t, b2d,
                          dp->context);
        }
        break;

      default:
        if (dp->on_unknown) dp->on_unknown(w, 4, dp->context);
        break;
    }
  } else if (bank == MIDI2_FLEX_BANK_METADATA || bank == MIDI2_FLEX_BANK_PERF_TEXT) {
    /* Text messages: extract 12 bytes from words 1-3 */
    if (dp->on_flex_text) {
      uint8_t text[12];
      uint8_t i;
      for (i = 0; i < 12; i++) {
        uint8_t wi = (uint8_t)(1 + i / 4);
        uint8_t sh = (uint8_t)(24 - (i % 4) * 8);
        text[i] = (uint8_t)((w[wi] >> sh) & 0xFF);
      }
      /* find actual length (trim trailing zeros in complete/end packets) */
      uint8_t len = 12;
      if (format == 0 || format == 3) {
        while (len > 0 && text[len - 1] == 0) len--;
      }
      dp->on_flex_text(group, format, address, channel, bank, status, text, len, dp->context);
    }
  } else {
    if (dp->on_unknown) dp->on_unknown(w, 4, dp->context);
  }
}

/*--------------------------------------------------------------------+
 * Internal: dispatch MT 0xF UMP Stream (4 words)
 *--------------------------------------------------------------------*/
static void dispatch_stream(midi2_dispatch *dp, const uint32_t *w) {
  uint8_t  format = (uint8_t)((w[0] >> 26) & 0x03);
  uint16_t status = (uint16_t)((w[0] >> 16) & 0x3FF);

  switch (status) {
    case MIDI2_STREAM_ENDPOINT_DISCOVERY:
      if (dp->on_endpoint_discovery) {
        uint8_t maj = (uint8_t)((w[0] >> 8) & 0xFF);
        uint8_t min = (uint8_t)(w[0] & 0xFF);
        uint8_t filter = (uint8_t)(w[1] & 0xFF);
        dp->on_endpoint_discovery(maj, min, filter, dp->context);
      }
      break;

    case MIDI2_STREAM_ENDPOINT_INFO:
      if (dp->on_endpoint_info) {
        uint8_t maj = (uint8_t)((w[0] >> 8) & 0xFF);
        uint8_t min = (uint8_t)(w[0] & 0xFF);
        bool static_fb = (w[1] & (UINT32_C(1) << 31)) != 0;
        uint8_t num_fb = (uint8_t)((w[1] >> 24) & 0x7F);
        bool m2   = (w[1] & (UINT32_C(1) << 9)) != 0;
        bool m1   = (w[1] & (UINT32_C(1) << 8)) != 0;
        bool rxjr = (w[1] & (UINT32_C(1) << 1)) != 0;
        bool txjr = (w[1] & UINT32_C(1)) != 0;
        dp->on_endpoint_info(maj, min, static_fb, num_fb, m2, m1, rxjr, txjr, dp->context);
      }
      break;

    case MIDI2_STREAM_DEVICE_IDENTITY:
      if (dp->on_device_identity) {
        uint32_t mfr    = (w[1] >> 8) & 0x00FFFFFF;
        uint16_t family = (uint16_t)(w[2] >> 16);
        uint16_t model  = (uint16_t)(w[2] & 0xFFFF);
        uint32_t ver    = w[3];
        dp->on_device_identity(mfr, family, model, ver, dp->context);
      }
      break;

    case MIDI2_STREAM_ENDPOINT_NAME:
    case MIDI2_STREAM_PRODUCT_INSTANCE_ID: {
      if (dp->on_stream_text) {
        /* 2 bytes in w[0][15:0], 12 bytes in w[1..3] = 14 max */
        uint8_t data[14];
        data[0] = (uint8_t)((w[0] >> 8) & 0xFF);
        data[1] = (uint8_t)(w[0] & 0xFF);
        uint8_t i;
        for (i = 0; i < 12; i++) {
          uint8_t wi = (uint8_t)(1 + i / 4);
          uint8_t sh = (uint8_t)(24 - (i % 4) * 8);
          data[2 + i] = (uint8_t)((w[wi] >> sh) & 0xFF);
        }
        uint8_t len = 14;
        if (format == 0 || format == 3) {
          while (len > 0 && data[len - 1] == 0) len--;
        }
        dp->on_stream_text(status, format, data, len, dp->context);
      }
      break;
    }

    case MIDI2_STREAM_FB_NAME: {
      if (dp->on_fb_name) {
        uint8_t fb_num = (uint8_t)((w[0] >> 8) & 0xFF);
        /* 1 byte in w[0][7:0] + 12 in w[1..3] = 13 name bytes max */
        uint8_t name[13];
        name[0] = (uint8_t)(w[0] & 0xFF);
        uint8_t i;
        for (i = 0; i < 12; i++) {
          uint8_t wi = (uint8_t)(1 + i / 4);
          uint8_t sh = (uint8_t)(24 - (i % 4) * 8);
          name[1 + i] = (uint8_t)((w[wi] >> sh) & 0xFF);
        }
        uint8_t len = 13;
        if (format == 0 || format == 3) {
          while (len > 0 && name[len - 1] == 0) len--;
        }
        dp->on_fb_name(format, fb_num, name, len, dp->context);
      }
      break;
    }

    case MIDI2_STREAM_CONFIG_REQUEST:
      if (dp->on_config_request) {
        uint8_t proto = (uint8_t)((w[0] >> 8) & 0xFF);
        bool rxjr = (w[0] & 0x02) != 0;
        bool txjr = (w[0] & 0x01) != 0;
        dp->on_config_request(proto, rxjr, txjr, dp->context);
      }
      break;

    case MIDI2_STREAM_CONFIG_NOTIFY:
      if (dp->on_config_notify) {
        uint8_t proto = (uint8_t)((w[0] >> 8) & 0xFF);
        bool rxjr = (w[0] & 0x02) != 0;
        bool txjr = (w[0] & 0x01) != 0;
        dp->on_config_notify(proto, rxjr, txjr, dp->context);
      }
      break;

    case MIDI2_STREAM_FB_DISCOVERY:
      if (dp->on_fb_discovery) {
        uint8_t fb = (uint8_t)((w[0] >> 8) & 0xFF);
        uint8_t filter = (uint8_t)(w[0] & 0xFF);
        dp->on_fb_discovery(fb, filter, dp->context);
      }
      break;

    case MIDI2_STREAM_FB_INFO:
      if (dp->on_fb_info) {
        bool active     = (w[0] & (UINT32_C(1) << 15)) != 0;
        uint8_t fb_num  = (uint8_t)((w[0] >> 8) & 0x7F);
        uint8_t dir     = (uint8_t)(w[0] & 0x03);
        uint8_t first   = (uint8_t)((w[1] >> 24) & 0x0F);
        uint8_t ngrp    = (uint8_t)((w[1] >> 16) & 0x0F);
        uint8_t ci_ver  = (uint8_t)((w[1] >> 8) & 0xFF);
        uint8_t s8str   = (uint8_t)((w[1] >> 2) & 0x3F);
        uint8_t proto   = (uint8_t)(w[1] & 0x03);
        dp->on_fb_info(active, fb_num, dir, first, ngrp, ci_ver, s8str, proto, dp->context);
      }
      break;

    case MIDI2_STREAM_START_OF_CLIP:
      if (dp->on_clip) dp->on_clip(true, dp->context);
      break;

    case MIDI2_STREAM_END_OF_CLIP:
      if (dp->on_clip) dp->on_clip(false, dp->context);
      break;

    default:
      if (dp->on_unknown) dp->on_unknown(w, 4, dp->context);
      break;
  }
}

/*--------------------------------------------------------------------+
 * Public: feed one UMP message
 *
 * Signature matches midi2_proc_ump_cb so it can be used directly:
 *   proc.on_ump = midi2_dispatch_feed;
 *   proc.context = &dispatch;
 *--------------------------------------------------------------------*/
void midi2_dispatch_feed(const uint32_t *words, uint8_t word_count, void *context) {
  midi2_dispatch *dp = (midi2_dispatch *)context;
  if (!dp || !words || word_count == 0) return;

  uint8_t mt = (uint8_t)((words[0] >> 28) & 0x0F);

  switch (mt) {
    case MIDI2_MT_UTILITY:
      dispatch_utility(dp, words[0]);
      break;
    case MIDI2_MT_SYSTEM:
      dispatch_system(dp, words[0]);
      break;
    case MIDI2_MT_MIDI1_CV:
      dispatch_cv1(dp, words[0]);
      break;
    case MIDI2_MT_SYSEX7:
      dispatch_sysex7(dp, words);
      break;
    case MIDI2_MT_MIDI2_CV:
      dispatch_cv2(dp, words);
      break;
    case MIDI2_MT_DATA128:
      dispatch_data128(dp, words);
      break;
    case MIDI2_MT_FLEX_DATA:
      dispatch_flex(dp, words);
      break;
    case MIDI2_MT_STREAM:
      dispatch_stream(dp, words);
      break;
    default:
      if (dp->on_unknown) dp->on_unknown(words, word_count, dp->context);
      break;
  }
}
