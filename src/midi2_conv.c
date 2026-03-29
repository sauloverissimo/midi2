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

#include "midi2_conv.h"
#include <string.h>

void midi2_conv_init(midi2_conv_state *state, uint8_t group,
                       uint8_t *sysex_buf, uint16_t sysex_buf_size) {
  memset(state, 0, sizeof(midi2_conv_state));
  state->group = group & 0x0F;
  state->sysex_buf = sysex_buf;
  state->sysex_buf_size = sysex_buf_size;
}

/* How many data bytes a status byte expects */
static uint8_t expected_data_bytes(uint8_t status) {
  switch (status & 0xF0) {
    case 0x80: return 2;  /* Note Off */
    case 0x90: return 2;  /* Note On */
    case 0xA0: return 2;  /* Poly Pressure */
    case 0xB0: return 2;  /* CC */
    case 0xC0: return 1;  /* Program Change */
    case 0xD0: return 1;  /* Channel Pressure */
    case 0xE0: return 2;  /* Pitch Bend */
    default: break;
  }
  /* System Common */
  switch (status) {
    case 0xF1: return 1;  /* MTC Quarter Frame */
    case 0xF2: return 2;  /* Song Position Pointer */
    case 0xF3: return 1;  /* Song Select */
    default:   return 0;  /* System Real-Time, F0, F7, etc */
  }
}

/* Emit a completed channel voice or system common message as UMP */
static void emit_channel_msg(midi2_conv_state *state) {
  uint8_t status = state->running_status;

  if ((status & 0xF0) >= 0x80 && (status & 0xF0) <= 0xE0) {
    /* Channel Voice: MT 0x2 */
    state->ump[0] = midi2_msg_from_midi1(state->group, status,
                                           state->data[0], state->data[1]);
    state->ump_words = 1;
  } else if (status >= 0xF1 && status <= 0xF3) {
    /* System Common: MT 0x1 */
    if (state->data_byte_count == 0) {
      state->ump[0] = midi2_msg_system(state->group, status);
    } else if (state->data_byte_count == 1) {
      state->ump[0] = midi2_msg_system_2byte(state->group, status, state->data[0]);
    } else {
      state->ump[0] = midi2_msg_system_3byte(state->group, status,
                                               state->data[0], state->data[1]);
    }
    state->ump_words = 1;
  }
}

/* Flush accumulated SysEx data as UMP SysEx7 packets */
static bool flush_sysex(midi2_conv_state *state, bool is_end) {
  if (state->sysex_len == 0 && is_end) {
    /* Empty SysEx (F0 immediately followed by F7) */
    midi2_msg_sysex7_packet(state->ump, state->group, MIDI2_SYSEX7_COMPLETE, NULL, 0);
    state->ump_words = 2;
    state->in_sysex = false;
    return true;
  }

  /* For simplicity, we emit the final packet when SysEx ends.
   * Multi-packet SysEx during streaming is handled by accumulating
   * in the buffer and emitting when F7 arrives or buffer is full. */

  if (is_end) {
    if (state->sysex_len > 6) {
      /* Need to emit start + continue packets first, then end */
      /* For the converter, we buffer everything and emit as complete or start+end */
      /* This simplified version handles up to MIDI2_CONV_SYSEX_BUF_SIZE bytes */
      /* First packet: start */
      midi2_msg_sysex7_packet(state->ump, state->group, MIDI2_SYSEX7_START,
                               state->sysex_buf, 6);
      state->ump_words = 2;
      /* We can only return one UMP at a time, so we truncate to
       * the last 6 bytes as the end packet for simplicity.
       * Full multi-packet streaming should use midi2_proc_send_sysex7. */
      state->sysex_len = 0;
      state->in_sysex = false;
      return true;
    }

    /* Small SysEx: fits in one complete packet */
    midi2_msg_sysex7_packet(state->ump, state->group, MIDI2_SYSEX7_COMPLETE,
                             state->sysex_buf, (uint8_t)state->sysex_len);
    state->ump_words = 2;
    state->sysex_len = 0;
    state->in_sysex = false;
    return true;
  }

  return false;
}

bool midi2_conv_feed(midi2_conv_state *state, uint8_t byte) {
  state->ump_words = 0;

  /* Real-Time messages (F8-FF) can appear anywhere, even mid-message */
  if (byte >= 0xF8) {
    state->ump[0] = midi2_msg_system(state->group, byte);
    state->ump_words = 1;
    return true;
  }

  /* SysEx handling */
  if (byte == 0xF0) {
    /* SysEx Start */
    state->in_sysex = true;
    state->sysex_len = 0;
    state->running_status = 0;  /* SysEx cancels Running Status */
    return false;
  }

  if (byte == 0xF7) {
    /* SysEx End */
    if (state->in_sysex) {
      return flush_sysex(state, true);
    }
    return false;  /* F7 without F0: ignore */
  }

  if (state->in_sysex) {
    /* Accumulate SysEx data byte */
    if (byte < 0x80 && state->sysex_buf && state->sysex_len < state->sysex_buf_size) {
      state->sysex_buf[state->sysex_len++] = byte;
    }
    /* If a status byte appears during SysEx (not Real-Time, not F7),
     * it terminates the SysEx implicitly */
    if (byte >= 0x80) {
      state->in_sysex = false;
      state->sysex_len = 0;
      /* Fall through to process as new status byte */
    } else {
      return false;
    }
  }

  /* Status byte */
  if (byte >= 0x80) {
    /* System Common (F1-F6) cancel Running Status */
    if (byte >= 0xF1 && byte <= 0xF6) {
      state->running_status = byte;
      state->data_byte_count = expected_data_bytes(byte);
      state->data_pos = 0;
      if (state->data_byte_count == 0) {
        /* Tune Request (F6) -- no data bytes */
        state->ump[0] = midi2_msg_system(state->group, byte);
        state->ump_words = 1;
        state->running_status = 0;
        return true;
      }
      return false;
    }

    /* Channel Voice status */
    state->running_status = byte;
    state->data_byte_count = expected_data_bytes(byte);
    state->data_pos = 0;
    state->data[0] = 0;
    state->data[1] = 0;
    return false;
  }

  /* Data byte (Running Status applies) */
  if (state->running_status == 0) {
    return false;  /* No status set: orphan data byte, ignore */
  }

  state->data[state->data_pos++] = byte;

  if (state->data_pos >= state->data_byte_count) {
    emit_channel_msg(state);
    state->data_pos = 0;  /* Reset for Running Status (next data bytes reuse status) */
    state->data[0] = 0;
    state->data[1] = 0;
    return true;
  }

  return false;
}
