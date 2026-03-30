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

#include "midi2_ci_dispatch.h"
#include <string.h>

void midi2_ci_dispatch_init(midi2_ci_dispatch *dp) {
  memset(dp, 0, sizeof(*dp));
}

/*--------------------------------------------------------------------+
 * Internal: build common header struct from raw data
 *--------------------------------------------------------------------*/
static midi2_ci_header make_hdr(const uint8_t *d, uint8_t group) {
  midi2_ci_header h;
  h.device_id = midi2_ci_get_device_id(d);
  h.version   = midi2_ci_get_version(d);
  h.src_muid  = midi2_ci_get_src_muid(d);
  h.dst_muid  = midi2_ci_get_dst_muid(d);
  h.group     = group;
  return h;
}

/*--------------------------------------------------------------------+
 * Internal: parse PE data message (shared by Get/Set/Subscribe/Notify)
 *--------------------------------------------------------------------*/
static void dispatch_pe_data(midi2_ci_dp_pe_data_cb cb, midi2_ci_header hdr,
                                const uint8_t *d, uint16_t len, void *ctx) {
  if (!cb) return;
  if (len < 14) return;  /* header(13) + request_id(1) minimum */
  uint8_t request_id = d[13];
  uint16_t p = 14;

  /* Header data */
  uint16_t hdr_len = 0;
  if (p + 2 <= len) { hdr_len = midi2_ci_read_14(&d[p]); p += 2; }
  const uint8_t *hdr_data = (hdr_len > 0 && p + hdr_len <= len) ? &d[p] : NULL;
  if (hdr_len > 0) p += hdr_len;

  /* Chunk info */
  uint16_t num_chunks = 0, this_chunk = 0;
  if (p + 2 <= len) { num_chunks = midi2_ci_read_14(&d[p]); p += 2; }
  if (p + 2 <= len) { this_chunk = midi2_ci_read_14(&d[p]); p += 2; }

  /* Body data */
  uint16_t body_len = 0;
  if (p + 2 <= len) { body_len = midi2_ci_read_14(&d[p]); p += 2; }
  const uint8_t *body_data = (body_len > 0 && p + body_len <= len) ? &d[p] : NULL;

  cb(hdr, request_id, hdr_data, hdr_len, num_chunks, this_chunk,
        body_data, body_len, ctx);
}

/*--------------------------------------------------------------------+
 * Feed
 *--------------------------------------------------------------------*/
bool midi2_ci_dispatch_feed(midi2_ci_dispatch *dp, uint8_t group,
                               const uint8_t *data, uint16_t length) {
  if (!dp || !data) return false;
  if (!midi2_ci_is_ci(data, length)) return false;

  midi2_ci_header hdr = make_hdr(data, group);
  uint8_t sub_id = midi2_ci_get_sub_id(data);

  switch (sub_id) {

    /*--- Management ---*/

    case MIDI2_CI_DISCOVERY: {
      if (!dp->on_discovery || length < 29) break;
      uint8_t out_path = (hdr.version >= MIDI2_CI_VERSION_2 && length >= 30) ? data[29] : 0;
      dp->on_discovery(hdr, midi2_ci_get_mfr_id(data), midi2_ci_get_family(data),
                           midi2_ci_get_model(data), midi2_ci_get_sw_rev(data),
                           midi2_ci_get_ci_category(data), midi2_ci_get_max_sysex(data),
                           out_path, dp->context);
      return true;
    }

    case MIDI2_CI_DISCOVERY_REPLY: {
      if (!dp->on_discovery_reply || length < 29) break;
      uint8_t out_path = 0, fb = 0x7F;
      if (hdr.version >= MIDI2_CI_VERSION_2 && length >= 31) {
        out_path = data[29];
        fb = data[30];
      }
      dp->on_discovery_reply(hdr, midi2_ci_get_mfr_id(data), midi2_ci_get_family(data),
                                 midi2_ci_get_model(data), midi2_ci_get_sw_rev(data),
                                 midi2_ci_get_ci_category(data), midi2_ci_get_max_sysex(data),
                                 out_path, fb, dp->context);
      return true;
    }

    case MIDI2_CI_ENDPOINT_INFO: {
      if (!dp->on_endpoint_info || length < 14) break;
      dp->on_endpoint_info(hdr, data[13], dp->context);
      return true;
    }

    case MIDI2_CI_ENDPOINT_INFO_REPLY: {
      if (!dp->on_endpoint_info_reply || length < 16) break;
      uint8_t status = data[13];
      uint16_t info_len = midi2_ci_read_14(&data[14]);
      const uint8_t *info = (info_len > 0 && length >= 16 + info_len) ? &data[16] : NULL;
      dp->on_endpoint_info_reply(hdr, status, info, info_len, dp->context);
      return true;
    }

    case MIDI2_CI_INVALIDATE_MUID: {
      if (!dp->on_invalidate_muid || length < 17) break;
      dp->on_invalidate_muid(hdr, midi2_ci_get_target_muid(data), dp->context);
      return true;
    }

    case MIDI2_CI_ACK: {
      if (!dp->on_ack || length < 13) break;
      uint8_t orig = 0, sc = 0, sd = 0;
      const uint8_t *det = NULL;
      uint16_t ml = 0;
      const uint8_t *mt = NULL;
      if (length >= 23) {
        orig = data[13]; sc = data[14]; sd = data[15];
        det = &data[16]; /* 5 bytes */
        ml = midi2_ci_read_14(&data[21]);
        mt = (ml > 0 && length >= 23 + ml) ? &data[23] : NULL;
      }
      dp->on_ack(hdr, orig, sc, sd, det, ml, mt, dp->context);
      return true;
    }

    case MIDI2_CI_NAK: {
      if (!dp->on_nak || length < 13) break;
      uint8_t orig = 0, sc = 0, sd = 0;
      const uint8_t *det = NULL;
      uint16_t ml = 0;
      const uint8_t *mt = NULL;
      if (hdr.version >= MIDI2_CI_VERSION_2 && length >= 23) {
        orig = data[13]; sc = data[14]; sd = data[15];
        det = &data[16];
        ml = midi2_ci_read_14(&data[21]);
        mt = (ml > 0 && length >= 23 + ml) ? &data[23] : NULL;
      }
      dp->on_nak(hdr, orig, sc, sd, det, ml, mt, dp->context);
      return true;
    }

    /*--- Profile Configuration ---*/

    case MIDI2_CI_PROFILE_INQUIRY: {
      if (!dp->on_profile_inquiry) break;
      dp->on_profile_inquiry(hdr, dp->context);
      return true;
    }

    case MIDI2_CI_PROFILE_INQUIRY_REPLY: {
      if (!dp->on_profile_inquiry_reply || length < 15) break;
      uint16_t en_count = midi2_ci_read_14(&data[13]);
      uint16_t en_bytes = (uint16_t)(en_count * 5);
      if (15 + en_bytes + 2 > length) break;  /* bounds check */
      const uint8_t *en_data = &data[15];
      uint16_t dis_off = (uint16_t)(15 + en_bytes);
      uint16_t dis_count = 0;
      const uint8_t *dis_data = NULL;
      if (dis_off + 2 <= length) {
        dis_count = midi2_ci_read_14(&data[dis_off]);
        dis_data = &data[dis_off + 2];
      }
      dp->on_profile_inquiry_reply(hdr, en_count, en_data, dis_count, dis_data, dp->context);
      return true;
    }

    case MIDI2_CI_SET_PROFILE_ON: {
      if (!dp->on_set_profile_on || length < 18) break;
      uint16_t nch = 0;
      if (hdr.version >= MIDI2_CI_VERSION_2 && length >= 20) {
        nch = midi2_ci_read_14(&data[18]);
      }
      dp->on_set_profile_on(hdr, &data[13], nch, dp->context);
      return true;
    }

    case MIDI2_CI_SET_PROFILE_OFF: {
      if (!dp->on_set_profile_off || length < 18) break;
      dp->on_set_profile_off(hdr, &data[13], 0, dp->context);
      return true;
    }

    case MIDI2_CI_PROFILE_ENABLED: {
      if (!dp->on_profile_enabled || length < 18) break;
      uint16_t nch = 0;
      if (hdr.version >= MIDI2_CI_VERSION_2 && length >= 20) {
        nch = midi2_ci_read_14(&data[18]);
      }
      dp->on_profile_enabled(hdr, &data[13], nch, dp->context);
      return true;
    }

    case MIDI2_CI_PROFILE_DISABLED: {
      if (!dp->on_profile_disabled || length < 18) break;
      uint16_t nch = 0;
      if (hdr.version >= MIDI2_CI_VERSION_2 && length >= 20) {
        nch = midi2_ci_read_14(&data[18]);
      }
      dp->on_profile_disabled(hdr, &data[13], nch, dp->context);
      return true;
    }

    case MIDI2_CI_PROFILE_ADDED: {
      if (!dp->on_profile_added || length < 18) break;
      dp->on_profile_added(hdr, &data[13], dp->context);
      return true;
    }

    case MIDI2_CI_PROFILE_REMOVED: {
      if (!dp->on_profile_removed || length < 18) break;
      dp->on_profile_removed(hdr, &data[13], dp->context);
      return true;
    }

    case MIDI2_CI_PROFILE_DETAILS: {
      if (!dp->on_profile_details || length < 19) break;
      dp->on_profile_details(hdr, &data[13], data[18], dp->context);
      return true;
    }

    case MIDI2_CI_PROFILE_DETAILS_REPLY: {
      if (!dp->on_profile_details_reply || length < 21) break;
      uint8_t target = data[18];
      uint16_t dl = midi2_ci_read_14(&data[19]);
      const uint8_t *dd = (dl > 0 && length >= 21 + dl) ? &data[21] : NULL;
      dp->on_profile_details_reply(hdr, &data[13], target, dd, dl, dp->context);
      return true;
    }

    case MIDI2_CI_PROFILE_SPECIFIC_DATA: {
      if (!dp->on_profile_specific_data || length < 22) break;
      uint32_t dl = midi2_ci_read_28(&data[18]);
      const uint8_t *dd = (dl > 0 && length >= 22 + dl) ? &data[22] : NULL;
      dp->on_profile_specific_data(hdr, &data[13], dd, dl, dp->context);
      return true;
    }

    /*--- Property Exchange ---*/

    case MIDI2_CI_PE_CAPABILITY: {
      if (!dp->on_pe_capability || length < 14) break;
      uint8_t max_sim = data[13];
      uint8_t maj = 0, min = 0;
      if (hdr.version >= MIDI2_CI_VERSION_2 && length >= 16) {
        maj = data[14]; min = data[15];
      }
      dp->on_pe_capability(hdr, max_sim, maj, min, dp->context);
      return true;
    }

    case MIDI2_CI_PE_CAPABILITY_REPLY: {
      if (!dp->on_pe_capability_reply || length < 14) break;
      uint8_t max_sim = data[13];
      uint8_t maj = 0, min = 0;
      if (hdr.version >= MIDI2_CI_VERSION_2 && length >= 16) {
        maj = data[14]; min = data[15];
      }
      dp->on_pe_capability_reply(hdr, max_sim, maj, min, dp->context);
      return true;
    }

    case MIDI2_CI_PE_GET:
      dispatch_pe_data(dp->on_pe_get, hdr, data, length, dp->context);
      return dp->on_pe_get != NULL;

    case MIDI2_CI_PE_GET_REPLY:
      dispatch_pe_data(dp->on_pe_get_reply, hdr, data, length, dp->context);
      return dp->on_pe_get_reply != NULL;

    case MIDI2_CI_PE_SET:
      dispatch_pe_data(dp->on_pe_set, hdr, data, length, dp->context);
      return dp->on_pe_set != NULL;

    case MIDI2_CI_PE_SET_REPLY:
      dispatch_pe_data(dp->on_pe_set_reply, hdr, data, length, dp->context);
      return dp->on_pe_set_reply != NULL;

    case MIDI2_CI_PE_SUBSCRIBE:
      dispatch_pe_data(dp->on_pe_subscribe, hdr, data, length, dp->context);
      return dp->on_pe_subscribe != NULL;

    case MIDI2_CI_PE_SUBSCRIBE_REPLY:
      dispatch_pe_data(dp->on_pe_subscribe_reply, hdr, data, length, dp->context);
      return dp->on_pe_subscribe_reply != NULL;

    case MIDI2_CI_PE_NOTIFY:
      dispatch_pe_data(dp->on_pe_notify, hdr, data, length, dp->context);
      return dp->on_pe_notify != NULL;

    /*--- Process Inquiry ---*/

    case MIDI2_CI_PI_CAPABILITY: {
      if (!dp->on_pi_capability) break;
      dp->on_pi_capability(hdr, dp->context);
      return true;
    }

    case MIDI2_CI_PI_CAPABILITY_REPLY: {
      if (!dp->on_pi_capability_reply || length < 14) break;
      dp->on_pi_capability_reply(hdr, data[13], dp->context);
      return true;
    }

    case MIDI2_CI_PI_MIDI_REPORT: {
      if (!dp->on_pi_midi_report || length < 18) break;
      dp->on_pi_midi_report(hdr, data[13], data[14],
                                data[16], data[17], dp->context);
      return true;
    }

    case MIDI2_CI_PI_MIDI_REPORT_REPLY: {
      if (!dp->on_pi_midi_report_reply || length < 17) break;
      dp->on_pi_midi_report_reply(hdr, data[13], data[15], data[16], dp->context);
      return true;
    }

    case MIDI2_CI_PI_MIDI_REPORT_END: {
      if (!dp->on_pi_midi_report_end) break;
      dp->on_pi_midi_report_end(hdr, dp->context);
      return true;
    }

    default:
      break;
  }

  /* Unknown or no callback registered */
  if (dp->on_unknown) {
    dp->on_unknown(hdr, sub_id, data, length, dp->context);
  }
  return false;
}
