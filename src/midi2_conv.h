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
 * midi2_conv.h - MIDI 1.0 byte stream to UMP, protocol translation
 *
 * Part of midi2 - Portable MIDI 2.0 library (C99)
 * https://github.com/sauloverissimo/midi2
 *
 * Spec: MIDI 2.0 UMP (M2-104-UM v1.1.2, Nov 2024)
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
 *     do {
 *       // conv.ump[] contains a completed UMP message
 *       // conv.ump_words tells how many words (1 or 2)
 *       process(conv.ump, conv.ump_words);
 *     } while (midi2_conv_next(&conv));
 *   }
 *
 * One fed byte usually produces one message, but can produce two: the UMP
 * spec (M2-104-UM 7.7.1) lets System Real-Time interleave inside a SysEx and
 * makes any other status terminate that SysEx, so the byte's own message can
 * queue behind the SysEx packet it displaced. The do/while drains both.
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

  /** Second message produced by the same fed byte (M2-104-UM 7.7.1
   *  interspersing), drained via midi2_conv_next(). Holds at most one
   *  System message word today; sized for one full 64-bit message. */
  uint32_t pending[2];
  uint8_t  pending_words;

  /** Debug-only reentrancy guard (see the single-context contract on
   *  midi2_conv_feed). Always present so the struct size matches between debug
   *  and release builds. */
  bool     in_feed;
} midi2_conv_state;

/** Initialize converter state.
 *  @param state         State struct (caller-allocated). Safe to pass NULL
 *                       (function is no-op).
 *  @param group         UMP group to assign to converted messages (0-15). */
void midi2_conv_init(midi2_conv_state *state, uint8_t group);

/* Feed one MIDI 1.0 byte. Returns true when a complete UMP message is ready
 * in state->ump[]. Returns false if more bytes are needed, or if state is
 * NULL (safe to call with NULL state).
 *
 * SysEx of any length is fully supported via streaming UMP SysEx7 packets.
 *
 * A fed byte can produce up to TWO messages (see midi2_conv_next): after
 * feed() returns true, drain with next() before feeding the next byte.
 * Debug builds assert if a queued message is left undrained; release builds
 * drop it.
 *
 * Single-context: feed each state instance from one execution context at a
 * time. Do not re-enter feed on the same instance from a callback or another
 * context (e.g. an ISR). Violations are caught by a debug-build assertion
 * (compiled out under NDEBUG). */
bool midi2_conv_feed(midi2_conv_state *state, uint8_t byte);

/* Advance to the next message produced by the last fed byte.
 *
 * One byte can yield two UMP messages (M2-104-UM 7.7.1): a System Real-Time
 * byte landing inside a SysEx is emitted after the partial packet holding the
 * bytes that preceded it, and any other status byte terminates the SysEx --
 * the closing packet comes first, the interrupting byte's own message (F4,
 * F5, F6) queues behind it. The canonical consumption loop is therefore:
 *
 *   if (midi2_conv_feed(&conv, byte)) {
 *     do {
 *       process(conv.ump, conv.ump_words);
 *     } while (midi2_conv_next(&conv));
 *   }
 *
 * Returns true when another message was moved into state->ump[]; false when
 * nothing is pending (ump_words is set to 0) or state is NULL. For ordinary
 * bytes the loop body runs once and next() returns false immediately.
 *
 * Same single-context contract as midi2_conv_feed. */
bool midi2_conv_next(midi2_conv_state *state);

#ifdef __cplusplus
}
#endif

#endif /* MIDI2_CONV_H */
