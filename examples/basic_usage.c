/*
 * midi2 basic usage example
 *
 * Shows how to build, parse, dispatch, and process UMP messages.
 *
 * Compile:
 *   gcc -std=c99 -I../src basic_usage.c \
 *       ../src/midi2_dispatch.c ../src/midi2_proc.c ../src/midi2_ci.c \
 *       -o basic_usage
 */

#include <stdio.h>
#include "midi2_msg.h"
#include "midi2_dispatch.h"
#include "midi2_proc.h"
#include "midi2_ci.h"

/* --- Simulated platform write (prints to stdout) --- */

static uint32_t platform_write(const uint32_t *words, uint32_t count, void *ctx) {
  uint32_t i;
  (void)ctx;
  printf("  TX:");
  for (i = 0; i < count; i++) printf(" %08X", (unsigned)words[i]);
  printf("\n");
  return count;
}

/* --- Dispatch callbacks --- */

static void on_note_on(uint8_t group, uint8_t channel, uint8_t note,
                          uint16_t velocity, uint8_t attr_type,
                          uint16_t attr_data, void *ctx) {
  (void)group; (void)attr_type; (void)attr_data; (void)ctx;
  printf("  Note On:  ch=%d note=%d vel=%u\n", channel, note, velocity);
}

static void on_note_off(uint8_t group, uint8_t channel, uint8_t note,
                           uint16_t velocity, uint8_t attr_type,
                           uint16_t attr_data, void *ctx) {
  (void)group; (void)velocity; (void)attr_type; (void)attr_data; (void)ctx;
  printf("  Note Off: ch=%d note=%d\n", channel, note);
}

static void on_cc(uint8_t group, uint8_t channel, uint8_t index,
                     uint32_t value, void *ctx) {
  (void)group; (void)ctx;
  printf("  CC:       ch=%d index=%d value=%u\n", channel, index, value);
}

static void on_tempo(uint8_t group, uint32_t ten_ns_per_qn, void *ctx) {
  (void)group; (void)ctx;
  double bpm = 60000000000.0 / (double)ten_ns_per_qn / 10.0;
  printf("  Tempo:    %.1f BPM\n", bpm);
}

static void on_sysex_complete(uint8_t group, const uint8_t *data,
                                 uint16_t len, void *ctx) {
  (void)ctx;
  uint16_t i;
  printf("  SysEx (group %d, %d bytes):", group, len);
  for (i = 0; i < len && i < 16; i++) printf(" %02X", data[i]);
  if (len > 16) printf(" ...");
  printf("\n");
}

int main(void) {
  /* --- 1. Build messages with midi2_msg --- */

  printf("=== Building UMP messages ===\n\n");

  uint32_t w[4];

  midi2_msg_note_on(w, 0, 0, 60, 0xC000, 0, 0);
  printf("Note On C4:  %08X %08X\n", (unsigned)w[0], (unsigned)w[1]);

  midi2_msg_cc(w, 0, 0, 74, 0x80000000);
  printf("CC 74 50%%:   %08X %08X\n", (unsigned)w[0], (unsigned)w[1]);

  midi2_msg_tempo(w, 0, 50000000);  /* 500ms in 10ns units = 120 BPM */
  printf("Tempo 120:   %08X %08X %08X %08X\n",
    (unsigned)w[0], (unsigned)w[1], (unsigned)w[2], (unsigned)w[3]);

  /* --- 2. Value scaling --- */

  printf("\n=== Value scaling ===\n\n");

  uint16_t vel16 = midi2_msg_scale_up_7to16(127);
  printf("7-bit 127  -> 16-bit %u\n", vel16);

  uint8_t vel7 = midi2_msg_scale_down_16to7(0xC000);
  printf("16-bit 0xC000 -> 7-bit %u\n", vel7);

  /* --- 3. Typed dispatch --- */

  printf("\n=== Typed dispatch (midi2_dispatch) ===\n\n");

  midi2_dispatch dp;
  midi2_dispatch_init(&dp);
  dp.on_note_on = on_note_on;
  dp.on_note_off = on_note_off;
  dp.on_cc = on_cc;
  dp.on_tempo = on_tempo;

  /* Feed messages directly to dispatch */
  midi2_msg_note_on(w, 0, 0, 60, 0xFFFF, 0, 0);
  midi2_dispatch_feed(w, 2, &dp);

  midi2_msg_cc(w, 0, 0, 1, 0x40000000);
  midi2_dispatch_feed(w, 2, &dp);

  midi2_msg_note_off(w, 0, 0, 60, 0, 0, 0);
  midi2_dispatch_feed(w, 2, &dp);

  midi2_msg_tempo(w, 0, 50000000);
  midi2_dispatch_feed(w, 4, &dp);

  /* --- 4. Process with proc + dispatch chained --- */

  printf("\n=== Processing with midi2_proc + dispatch ===\n\n");

  uint8_t sysex7_buf[128];
  uint8_t sysex8_buf[256];
  midi2_proc_state proc;
  midi2_proc_init(&proc, sysex7_buf, sizeof(sysex7_buf), sysex8_buf, sizeof(sysex8_buf));

  /* Chain: proc feeds dispatch */
  proc.on_ump = midi2_dispatch_feed;
  proc.context = &dp;
  proc.on_sysex7 = on_sysex_complete;

  /* Feed through proc (applies group filtering, SysEx reassembly) */
  midi2_msg_note_on(w, 0, 5, 36, 0x8000, 0, 0);
  midi2_proc_feed(&proc, w, 2);

  /* --- 5. MIDI-CI auto-response --- */

  printf("\n=== MIDI-CI Discovery ===\n\n");

  uint8_t profiles[2][5];
  midi2_ci_property props[1];
  midi2_ci_state ci;

  midi2_ci_init(&ci, 0xDEADBEEF, profiles, 2, props, 1);
  midi2_ci_set_identity(&ci, 0x00AABB, 0x0001, 0x0001, 0x00010000);
  midi2_ci_set_write_fn(&ci, platform_write, NULL);
  midi2_ci_add_profile(&ci, (const uint8_t[]){0x00, 0x21, 0x09, 0x00, 0x01});

  /* Simulate a Discovery Request arriving */
  uint8_t discovery_req[] = {
    0x7E, 0x7F, 0x0D, 0x70,
    0x01, 0x00, 0x00, 0x00,  /* source MUID */
    0x7F, 0x7F, 0x7F, 0x7F,  /* dest MUID (broadcast) */
    0x00, 0x00
  };

  printf("Incoming Discovery Request...\n");
  if (midi2_ci_process_sysex(&ci, 0, discovery_req, sizeof(discovery_req))) {
    printf("Response sent automatically.\n");
  }

  printf("\nDone.\n");
  return 0;
}
