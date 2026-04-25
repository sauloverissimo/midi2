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
 * midi2_proc.h - UMP stream processing, group filtering, value scaling
 *
 * Part of midi2 - Portable MIDI 2.0 library (C99)
 * https://github.com/sauloverissimo/midi2
 *
 * Spec: MIDI 2.0 UMP (M2-104-UM v1.1.2, Nov 2024)
 * Version: 0.3.0
 */

#ifndef MIDI2_PROC_H
#define MIDI2_PROC_H

#include <stdint.h>
#include <stdbool.h>
#include "midi2_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------+
 * Callback types
 *--------------------------------------------------------------------*/

/* Called for each UMP message that passes the group filter */
typedef void (*midi2_proc_ump_cb)(const uint32_t *words, uint8_t word_count, void *context);

/* Called when a complete SysEx7 message has been reassembled */
typedef void (*midi2_proc_sysex7_cb)(uint8_t group, const uint8_t *data,
                                      uint16_t length, void *context);

/* Called when a complete SysEx8 message has been reassembled */
typedef void (*midi2_proc_sysex8_cb)(uint8_t group, uint8_t stream_id,
                                      const uint8_t *data, uint16_t length, void *context);

/* Called when midi2_proc needs to send UMP words (e.g. SysEx7 fragmented send) */
typedef uint32_t (*midi2_proc_write_fn)(const uint32_t *words, uint32_t count, void *context);

/*--------------------------------------------------------------------+
 * State struct (user-allocated)
 *
 * The caller provides the SysEx reassembly buffer. This allows any
 * buffer size without compile-time limits. Example:
 *
 *   uint8_t sysex_buf[256];
 *   midi2_proc_state proc;
 *   midi2_proc_init(&proc, sysex_buf, sizeof(sysex_buf));
 *--------------------------------------------------------------------*/

typedef struct {
  /** Group filtering: bitmask of which groups to deliver (default 0xFFFF = all) */
  uint16_t group_mask;

  /** Group remap table for outgoing messages (default identity) */
  uint8_t group_map[16];

  /** SysEx7 reassembly: caller-provided buffer */
  uint8_t  *sysex_buf;        /**< pointer to caller's buffer, or NULL to disable */
  uint16_t  sysex_buf_size;   /**< capacity of sysex_buf */
  uint16_t  sysex_len;        /**< current accumulated length */
  uint8_t   sysex_group;      /**< 0xFF = no active SysEx */

  /** SysEx8 reassembly: caller-provided buffer (separate from SysEx7) */
  uint8_t  *sysex8_buf;
  uint16_t  sysex8_buf_size;
  uint16_t  sysex8_len;
  uint8_t   sysex8_group;      /**< 0xFF = no active SysEx8 */
  uint8_t   sysex8_stream_id;  /**< stream ID of active SysEx8 */

  /** Callbacks */
  midi2_proc_ump_cb     on_ump;
  midi2_proc_sysex7_cb  on_sysex7;
  midi2_proc_sysex8_cb  on_sysex8;
  void                 *context;
} midi2_proc_state;

/*--------------------------------------------------------------------+
 * Functions
 *--------------------------------------------------------------------*/

/** Initialize state with caller-provided SysEx buffers.
 *  @param state           State struct (caller-allocated)
 *  @param sysex7_buf      Buffer for SysEx7 reassembly, or NULL to disable
 *  @param sysex7_buf_size Size of sysex7_buf in bytes
 *  @param sysex8_buf      Buffer for SysEx8 reassembly, or NULL to disable
 *  @param sysex8_buf_size Size of sysex8_buf in bytes */
void midi2_proc_init(midi2_proc_state *state,
                       uint8_t *sysex7_buf, uint16_t sysex7_buf_size,
                       uint8_t *sysex8_buf, uint16_t sysex8_buf_size);

/* Feed UMP words from transport. Processes, filters, dispatches to callbacks.
 * word_count must match the message size (1, 2, or 4 words). */
void midi2_proc_feed(midi2_proc_state *state, const uint32_t *words, uint8_t word_count);

/* Apply group remap to outgoing words (modifies word 0 in-place).
 * Only remaps message types that have a group field (not Utility or Stream). */
void midi2_proc_remap_group(midi2_proc_state *state, uint32_t *words);

/* Multi-packet SysEx7 send helper. Fragments data into UMP packets (max 6 bytes each),
 * calls write_fn for each 2-word packet. data does NOT include F0/F7 delimiters. */
void midi2_proc_send_sysex7(uint8_t group, const uint8_t *data, uint16_t length,
                              midi2_proc_write_fn write_fn, void *context);

/* M2-104-UM §7.1.9 Function Block Name Notification sender.
 * UMP Stream MT 0xF, status 0x12. Fragments the UTF-8 name across
 * Complete / Start / Continue / End 4-word packets (13 name bytes per
 * UMP; total name limited to 91 bytes per spec). Remaining bytes of
 * the final packet are zero-padded per spec. (v0.2.4+) */
void midi2_proc_send_fb_name(uint8_t fb_idx, const char *name,
                               midi2_proc_write_fn write_fn, void *context);

/* M2-104-UM §7.1.7 Endpoint Name Notification sender.
 * UMP Stream MT 0xF, status 0x003. Fragments UTF-8 name across
 * Complete / Start / Continue / End 4-word packets (14 name bytes
 * per UMP). Empty name sends nothing. (v0.3.0+) */
void midi2_proc_send_endpoint_name(const char *name,
                                    midi2_proc_write_fn write_fn, void *context);

/* M2-104-UM §7.1.8 Product Instance ID Notification sender.
 * UMP Stream MT 0xF, status 0x004. Fragmentation identical to
 * Endpoint Name (14 bytes per UMP). Empty id sends nothing.
 * (v0.3.0+) */
void midi2_proc_send_product_id(const char *id,
                                 midi2_proc_write_fn write_fn, void *context);

/* M2-104-UM §7.1.6 Device Identity Notification sender.
 * UMP Stream MT 0xF, status 0x002. Always emits a single 4-word UMP
 * (no fragmentation). Kept for callsite symmetry with the other
 * Stream senders. manufacturer_id uses lower 24 bits only.
 * (v0.3.0+) */
void midi2_proc_send_device_identity(uint32_t manufacturer_id,
                                      uint16_t family_id, uint16_t model_id,
                                      uint32_t version_id,
                                      midi2_proc_write_fn write_fn, void *context);

/* M2-104-UM §7.8 SysEx8 sender. MT 0x5. Fragments raw 8-bit data
 * into 4-word packets (13 data bytes per UMP; stream_id rides in
 * word 0 bits [15:8]). status nibble encodes Complete / Start /
 * Continue / End per M2-104-UM Table 14. Zero-length sends nothing.
 * (v0.3.0+) */
void midi2_proc_send_sysex8(uint8_t group, uint8_t stream_id,
                             const uint8_t *data, uint16_t length,
                             midi2_proc_write_fn write_fn, void *context);

#ifdef __cplusplus
}
#endif

#endif /* MIDI2_PROC_H */
