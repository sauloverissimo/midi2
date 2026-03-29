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
  uint8_t status_nib = (words[0] >> 20) & 0x0F;
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

  if (status_nib == 0x00) {
    /* Complete: single-packet SysEx */
    if (state->on_sysex7) {
      state->on_sysex7(group, data, n, state->context);
    }
    return;
  }

  if (status_nib == 0x01) {
    /* Start */
    state->sysex_group = group;
    state->sysex_len = 0;
  } else if (group != state->sysex_group) {
    /* Different group mid-stream: discard in-progress, restart */
    state->sysex_group = group;
    state->sysex_len = 0;
    if (status_nib == 0x02) return;  /* Continue without start: drop */
  }

  /* Append data */
  {
    uint8_t i;
    for (i = 0; i < n && state->sysex_len < state->sysex_buf_size; i++) {
      state->sysex_buf[state->sysex_len++] = data[i];
    }
  }

  if (status_nib == 0x03) {
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

  uint8_t status_nib = (words[0] >> 20) & 0x0F;
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

  if (status_nib == 0x00) {
    /* Complete single-packet SysEx8 */
    if (state->on_sysex8) {
      state->on_sysex8(group, stream_id, data, n, state->context);
    }
    return;
  }

  if (status_nib == 0x01) {
    /* Start */
    state->sysex8_group = group;
    state->sysex8_stream_id = stream_id;
    state->sysex8_len = 0;
  } else if (group != state->sysex8_group || stream_id != state->sysex8_stream_id) {
    /* Different group or stream mid-stream: discard */
    state->sysex8_group = group;
    state->sysex8_stream_id = stream_id;
    state->sysex8_len = 0;
    if (status_nib == 0x02) return;
  }

  /* Append data */
  {
    uint8_t i;
    for (i = 0; i < n && state->sysex8_len < state->sysex8_buf_size; i++) {
      state->sysex8_buf[state->sysex8_len++] = data[i];
    }
  }

  if (status_nib == 0x03) {
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
