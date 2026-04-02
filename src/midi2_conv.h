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

#ifndef MIDI2_CONV_H
#define MIDI2_CONV_H

#include <stdint.h>
#include <stdbool.h>
#include "midi2_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------+
 * MIDI 1.0 Byte Stream to UMP Converter
 *
 * Converts serial MIDI 1.0 bytes (DIN-5, TRS, UART) into UMP words.
 * Handles Running Status, multi-byte messages, and SysEx (F0..F7).
 *
 * SysEx is emitted as streaming UMP SysEx7 packets:
 *   - Every 6 bytes: emits START or CONTINUE (2 UMP words)
 *   - On F7: emits END or COMPLETE with remaining bytes
 *   - No caller-provided buffer needed (6-byte internal buffer)
 *
 * Usage:
 *   midi2_conv_state conv;
 *   midi2_conv_init(&conv, 0);  // group 0
 *
 *   // For each incoming byte:
 *   if (midi2_conv_feed(&conv, byte)) {
 *     // conv.ump[] contains the completed UMP message
 *     // conv.ump_words tells how many words (1 or 2)
 *     process(conv.ump, conv.ump_words);
 *   }
 *--------------------------------------------------------------------*/

typedef struct {
  /** Configuration */
  uint8_t group;

  /** Running Status state */
  uint8_t running_status;
  uint8_t data_byte_count;
  uint8_t data_pos;
  uint8_t data[2];

  /** SysEx state: 6-byte internal buffer for streaming */
  uint8_t  sysex_buf[6];       /**< internal buffer (one UMP packet worth) */
  uint8_t  sysex_len;          /**< bytes accumulated in sysex_buf (0-6) */
  bool     in_sysex;           /**< currently inside F0..F7 */
  bool     sysex_started;      /**< true after START emitted */

  /** Output: completed UMP message */
  uint32_t ump[4];
  uint8_t  ump_words;
} midi2_conv_state;

/** Initialize converter state.
 *  @param state         State struct (caller-allocated)
 *  @param group         UMP group to assign to converted messages */
void midi2_conv_init(midi2_conv_state *state, uint8_t group);

/* Feed one MIDI 1.0 byte. Returns true when a complete UMP message is ready
 * in state->ump[]. Returns false if more bytes are needed.
 *
 * SysEx of any length is fully supported via streaming UMP SysEx7 packets.
 * Each call produces at most one UMP message (1 or 2 words). */
bool midi2_conv_feed(midi2_conv_state *state, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* MIDI2_CONV_H */
