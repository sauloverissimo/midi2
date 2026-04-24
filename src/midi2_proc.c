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
 * midi2_proc.c - UMP stream processing implementation
 *
 * Part of midi2 - Portable MIDI 2.0 library (C99)
 * https://github.com/sauloverissimo/midi2
 *
 * Spec: MIDI 2.0 UMP (M2-104-UM v1.1.2, Nov 2024)
 * Version: 0.2.4
 */

#include "midi2_proc.h"
#include <string.h>

/*--------------------------------------------------------------------+
 * Init
 *--------------------------------------------------------------------*/
void midi2_proc_init(midi2_proc_state *state,
                       uint8_t *sysex7_buf, uint16_t sysex7_buf_size,
                       uint8_t *sysex8_buf, uint16_t sysex8_buf_size) {
  uint8_t i;
  memset(state, 0, sizeof(midi2_proc_state));
  state->group_mask = 0xFFFF;
  state->sysex_group = 0xFF;
  state->sysex_buf = sysex7_buf;
  state->sysex_buf_size = sysex7_buf_size;
  state->sysex8_buf = sysex8_buf;
  state->sysex8_buf_size = sysex8_buf_size;
  state->sysex8_group = 0xFF;
  for (i = 0; i < 16; i++) {
    state->group_map[i] = i;
  }
}

/*--------------------------------------------------------------------+
 * SysEx7 reassembly (internal)
 *--------------------------------------------------------------------*/
static void sysex7_process(midi2_proc_state *state, uint8_t group, const uint32_t *words) {
  if (state->sysex_buf == NULL) {
    /* No buffer provided: deliver raw SysEx packets without reassembly */
    return;
  }
  uint8_t status_nib = (words[0] >> 16) & 0xF0;  /* matches MIDI2_SYSEX7_* enums */
  uint8_t num_bytes  = (words[0] >> 16) & 0x0F;

  /* Extract data bytes from SysEx7 UMP packet */
  uint8_t data[6];
  uint8_t n = 0;
  if (num_bytes >= 1) data[n++] = (words[0] >> 8) & 0x7F;
  if (num_bytes >= 2) data[n++] = (words[0] >> 0) & 0x7F;
  if (num_bytes >= 3) data[n++] = (words[1] >> 24) & 0x7F;
  if (num_bytes >= 4) data[n++] = (words[1] >> 16) & 0x7F;
  if (num_bytes >= 5) data[n++] = (words[1] >> 8) & 0x7F;
  if (num_bytes >= 6) data[n++] = (words[1] >> 0) & 0x7F;

  if (status_nib == MIDI2_SYSEX7_COMPLETE) {
    /* Complete: single-packet SysEx */
    if (state->on_sysex7) {
      state->on_sysex7(group, data, n, state->context);
    }
    return;
  }

  if (status_nib == MIDI2_SYSEX7_START) {
    /* Start */
    state->sysex_group = group;
    state->sysex_len = 0;
  } else if (group != state->sysex_group) {
    /* Different group mid-stream: discard in-progress, restart */
    state->sysex_group = group;
    state->sysex_len = 0;
    if (status_nib == MIDI2_SYSEX7_CONTINUE) return;  /* Continue without start: drop */
  }

  /* Append data */
  {
    uint8_t i;
    for (i = 0; i < n && state->sysex_len < state->sysex_buf_size; i++) {
      state->sysex_buf[state->sysex_len++] = data[i];
    }
  }

  if (status_nib == MIDI2_SYSEX7_END) {
    /* End: deliver complete message */
    if (state->on_sysex7) {
      state->on_sysex7(group, state->sysex_buf, state->sysex_len, state->context);
    }
    state->sysex_group = 0xFF;
    state->sysex_len = 0;
  }
}

/*--------------------------------------------------------------------+
 * SysEx8 reassembly (internal)
 *--------------------------------------------------------------------*/
static void sysex8_process(midi2_proc_state *state, uint8_t group, const uint32_t *words) {
  if (state->sysex8_buf == NULL) return;

  uint8_t status_nib = (words[0] >> 16) & 0xF0;  /* matches MIDI2_SYSEX8_* enums */
  uint8_t num_bytes  = (words[0] >> 16) & 0x0F;  /* includes stream_id */
  uint8_t stream_id  = (words[0] >> 8) & 0xFF;

  /* Extract data bytes (num_bytes - 1, since stream_id is counted) */
  uint8_t data[13];
  uint8_t n = 0;
  uint8_t total_data = (num_bytes > 1) ? (uint8_t)(num_bytes - 1) : 0;

  if (total_data >= 1) data[n++] = words[0] & 0xFF;
  if (total_data >= 2) data[n++] = (words[1] >> 24) & 0xFF;
  if (total_data >= 3) data[n++] = (words[1] >> 16) & 0xFF;
  if (total_data >= 4) data[n++] = (words[1] >> 8) & 0xFF;
  if (total_data >= 5) data[n++] = words[1] & 0xFF;
  if (total_data >= 6) data[n++] = (words[2] >> 24) & 0xFF;
  if (total_data >= 7) data[n++] = (words[2] >> 16) & 0xFF;
  if (total_data >= 8) data[n++] = (words[2] >> 8) & 0xFF;
  if (total_data >= 9) data[n++] = words[2] & 0xFF;
  if (total_data >= 10) data[n++] = (words[3] >> 24) & 0xFF;
  if (total_data >= 11) data[n++] = (words[3] >> 16) & 0xFF;
  if (total_data >= 12) data[n++] = (words[3] >> 8) & 0xFF;
  if (total_data >= 13) data[n++] = words[3] & 0xFF;

  if (status_nib == MIDI2_SYSEX8_COMPLETE) {
    /* Complete single-packet SysEx8 */
    if (state->on_sysex8) {
      state->on_sysex8(group, stream_id, data, n, state->context);
    }
    return;
  }

  if (status_nib == MIDI2_SYSEX8_START) {
    /* Start */
    state->sysex8_group = group;
    state->sysex8_stream_id = stream_id;
    state->sysex8_len = 0;
  } else if (group != state->sysex8_group || stream_id != state->sysex8_stream_id) {
    /* Different group or stream mid-stream: discard */
    state->sysex8_group = group;
    state->sysex8_stream_id = stream_id;
    state->sysex8_len = 0;
    if (status_nib == MIDI2_SYSEX8_CONTINUE) return;
  }

  /* Append data */
  {
    uint8_t i;
    for (i = 0; i < n && state->sysex8_len < state->sysex8_buf_size; i++) {
      state->sysex8_buf[state->sysex8_len++] = data[i];
    }
  }

  if (status_nib == MIDI2_SYSEX8_END) {
    /* End */
    if (state->on_sysex8) {
      state->on_sysex8(group, state->sysex8_stream_id,
                        state->sysex8_buf, state->sysex8_len, state->context);
    }
    state->sysex8_group = 0xFF;
    state->sysex8_len = 0;
  }
}

/*--------------------------------------------------------------------+
 * Feed
 *--------------------------------------------------------------------*/
void midi2_proc_feed(midi2_proc_state *state, const uint32_t *words, uint8_t word_count) {
  uint8_t mt = midi2_msg_get_mt(words);
  uint8_t group = midi2_msg_get_group(words);

  (void)word_count; /* caller provides for API clarity; MT determines actual size */

  /* SysEx8: reassemble before group filtering (same rationale as SysEx7) */
  if (mt == MIDI2_MT_DATA128) {
    sysex8_process(state, group, words);
  }

  /* SysEx7 is processed before group filtering by design: MIDI-CI responses
   * (delivered via on_sysex7) must work regardless of group filter settings.
   * SysEx7 messages from filtered-out groups will still be reassembled and
   * delivered via on_sysex7. This is intentional: MIDI-CI must work
   * regardless of group filter settings. */
  if (mt == MIDI2_MT_SYSEX7) {
    sysex7_process(state, group, words);
  }

  /* Group filtering: bypass for Utility (MT 0x0) and Stream (MT 0xF) */
  if (mt != MIDI2_MT_UTILITY && mt != MIDI2_MT_STREAM) {
    if (!(state->group_mask & (1u << group))) return;
  }

  /* Dispatch to UMP callback (SysEx7/8 already handled above via callbacks) */
  if (mt != MIDI2_MT_SYSEX7 && mt != MIDI2_MT_DATA128 && state->on_ump) {
    state->on_ump(words, midi2_msg_word_count(mt), state->context);
  }
}

/*--------------------------------------------------------------------+
 * Group remap
 *--------------------------------------------------------------------*/
void midi2_proc_remap_group(midi2_proc_state *state, uint32_t *words) {
  uint8_t mt = midi2_msg_get_mt(words);
  if (mt != MIDI2_MT_UTILITY && mt != MIDI2_MT_STREAM) {
    uint8_t group = midi2_msg_get_group(words);
    uint8_t new_group = state->group_map[group & 0x0F];
    words[0] = (words[0] & 0xF0FFFFFF) | ((uint32_t)(new_group & 0x0F) << 24);
  }
}

/*--------------------------------------------------------------------+
 * SysEx7 send (fragmentation)
 *--------------------------------------------------------------------*/
void midi2_proc_send_sysex7(uint8_t group, const uint8_t *data, uint16_t length,
                              midi2_proc_write_fn write_fn, void *context) {
  uint32_t w[2];
  uint16_t offset = 0;

  if (length == 0) return;

  if (length <= 6) {
    midi2_msg_sysex7_packet(w, group, MIDI2_SYSEX7_COMPLETE, data, (uint8_t)length);
    write_fn(w, 2, context);
    return;
  }

  /* Start */
  midi2_msg_sysex7_packet(w, group, MIDI2_SYSEX7_START, data, 6);
  write_fn(w, 2, context);
  offset = 6;

  /* Continue */
  while (offset + 6 < length) {
    midi2_msg_sysex7_packet(w, group, MIDI2_SYSEX7_CONTINUE, data + offset, 6);
    write_fn(w, 2, context);
    offset += 6;
  }

  /* End */
  {
    uint8_t remaining = (uint8_t)(length - offset);
    midi2_msg_sysex7_packet(w, group, MIDI2_SYSEX7_END, data + offset, remaining);
    write_fn(w, 2, context);
  }
}

/*--------------------------------------------------------------------+
 * Function Block Name Notification (UMP Stream MT 0xF status 0x12)
 *
 * M2-104-UM §7.1.9. 4-word packet; 1 name byte at byte 3 of word 0 plus
 * 12 more bytes across words 1-3, so 13 bytes of name per UMP. Uses
 * Form = Complete (0) for <=13 bytes, Start/Continue/End otherwise.
 * Spec mandates max 91 bytes total; we silently truncate at 91.
 *--------------------------------------------------------------------*/
#define MIDI2_FB_NAME_BYTES_PER_UMP 13u
#define MIDI2_FB_NAME_MAX_BYTES     91u

void midi2_proc_send_fb_name(uint8_t fb_idx, const char *name,
                               midi2_proc_write_fn write_fn, void *context) {
  if (!write_fn || !name) return;

  uint16_t total = 0;
  while (name[total] && total < MIDI2_FB_NAME_MAX_BYTES) total++;
  if (total == 0) return;

  uint16_t offset = 0;
  while (offset < total) {
    uint16_t remaining = (uint16_t)(total - offset);
    uint8_t  n = (remaining > MIDI2_FB_NAME_BYTES_PER_UMP)
                 ? (uint8_t)MIDI2_FB_NAME_BYTES_PER_UMP
                 : (uint8_t)remaining;
    uint8_t is_first = (offset == 0);
    uint8_t is_last  = (remaining <= MIDI2_FB_NAME_BYTES_PER_UMP);
    uint8_t form = (is_first && is_last) ? 0u   /* Complete */
                 : is_first              ? 1u   /* Start */
                 : is_last               ? 3u   /* End */
                                         : 2u;  /* Continue */

    uint32_t msg[4] = {0};
    msg[0] = ((uint32_t)0xFu << 28)
           | ((uint32_t)form << 26)
           | ((uint32_t)0x12u << 16)
           | ((uint32_t)fb_idx << 8);
    const uint8_t *p = (const uint8_t *)(name + offset);
    if (n > 0) msg[0] |= (uint32_t)p[0];
    uint8_t i;
    for (i = 1; i < n; i++) {
      uint8_t widx  = (uint8_t)(1u + (i - 1u) / 4u);
      uint8_t shift = (uint8_t)(24u - ((i - 1u) % 4u) * 8u);
      msg[widx] |= ((uint32_t)p[i] << shift);
    }
    write_fn(msg, 4, context);
    offset += n;
  }
}

/*--------------------------------------------------------------------+
 * UMP Stream text senders: Endpoint Name (status 0x003),
 * Product Instance ID (status 0x004). M2-104-UM §7.1.7 / §7.1.8.
 *
 * 14 payload bytes per UMP (bytes 0-1 live in word 0 bits [15:0],
 * bytes 2-13 in words 1-3). Fragments into Complete / Start /
 * Continue / End packets. Reuses midi2_msg_stream_endpoint_name /
 * midi2_msg_stream_product_id inline builders so the word layout
 * stays canonical.
 *--------------------------------------------------------------------*/
#define MIDI2_STREAM_TEXT_BYTES_PER_UMP 14u
#define MIDI2_STREAM_TEXT_MAX_BYTES     98u /* 7 UMPs cap per spec */

typedef void (*stream_text_builder_fn)(uint32_t *w, uint8_t format,
                                       const uint8_t *data, uint8_t len);

static void stream_text_emit(stream_text_builder_fn builder,
                              const char *text,
                              midi2_proc_write_fn write_fn, void *context) {
  if (!write_fn || !text) return;
  uint16_t total = 0;
  while (text[total] && total < MIDI2_STREAM_TEXT_MAX_BYTES) total++;
  if (total == 0) return;

  uint16_t offset = 0;
  while (offset < total) {
    uint16_t remaining = (uint16_t)(total - offset);
    uint8_t  n = (remaining > MIDI2_STREAM_TEXT_BYTES_PER_UMP)
                 ? (uint8_t)MIDI2_STREAM_TEXT_BYTES_PER_UMP
                 : (uint8_t)remaining;
    uint8_t is_first = (offset == 0);
    uint8_t is_last  = (remaining <= MIDI2_STREAM_TEXT_BYTES_PER_UMP);
    uint8_t form = (is_first && is_last) ? 0u   /* Complete */
                 : is_first              ? 1u   /* Start */
                 : is_last               ? 3u   /* End */
                                         : 2u;  /* Continue */
    uint32_t msg[4];
    builder(msg, form, (const uint8_t *)(text + offset), n);
    write_fn(msg, 4, context);
    offset += n;
  }
}

void midi2_proc_send_endpoint_name(const char *name,
                                    midi2_proc_write_fn write_fn, void *context) {
  stream_text_emit(midi2_msg_stream_endpoint_name, name, write_fn, context);
}

void midi2_proc_send_product_id(const char *id,
                                 midi2_proc_write_fn write_fn, void *context) {
  stream_text_emit(midi2_msg_stream_product_id, id, write_fn, context);
}

/*--------------------------------------------------------------------+
 * Device Identity Notification sender (M2-104-UM §7.1.6).
 * Single 4-word UMP, no fragmentation. Delegates to the inline
 * builder for byte layout.
 *--------------------------------------------------------------------*/
void midi2_proc_send_device_identity(uint32_t manufacturer_id,
                                      uint16_t family_id, uint16_t model_id,
                                      uint32_t version_id,
                                      midi2_proc_write_fn write_fn, void *context) {
  if (!write_fn) return;
  uint32_t msg[4];
  midi2_msg_stream_device_identity(msg, manufacturer_id, family_id,
                                    model_id, version_id);
  write_fn(msg, 4, context);
}

/*--------------------------------------------------------------------+
 * SysEx8 sender (M2-104-UM §7.8).
 *
 * 13 data bytes per UMP (word 0 low byte carries data[0], words 1-3
 * carry the remaining 12). stream_id rides word 0 bits [15:8]. The
 * status nibble in bits [23:20] encodes Complete/Start/Continue/End
 * per Table 14. Delegates to midi2_msg_sysex8_packet per packet so
 * the status/num_bytes field stays aligned with the canonical
 * builder.
 *--------------------------------------------------------------------*/
#define MIDI2_SYSEX8_BYTES_PER_UMP 13u

void midi2_proc_send_sysex8(uint8_t group, uint8_t stream_id,
                             const uint8_t *data, uint16_t length,
                             midi2_proc_write_fn write_fn, void *context) {
  if (!write_fn) return;
  if (length == 0) return;
  if (!data) return;

  uint16_t offset = 0;
  while (offset < length) {
    uint16_t remaining = (uint16_t)(length - offset);
    uint8_t  n = (remaining > MIDI2_SYSEX8_BYTES_PER_UMP)
                 ? (uint8_t)MIDI2_SYSEX8_BYTES_PER_UMP
                 : (uint8_t)remaining;
    uint8_t is_first = (offset == 0);
    uint8_t is_last  = (remaining <= MIDI2_SYSEX8_BYTES_PER_UMP);
    uint8_t status = (is_first && is_last) ? MIDI2_SYSEX8_COMPLETE
                   : is_first              ? MIDI2_SYSEX8_START
                   : is_last               ? MIDI2_SYSEX8_END
                                           : MIDI2_SYSEX8_CONTINUE;
    uint32_t msg[4];
    midi2_msg_sysex8_packet(msg, group, status, stream_id,
                             data + offset, n);
    write_fn(msg, 4, context);
    offset += n;
  }
}
