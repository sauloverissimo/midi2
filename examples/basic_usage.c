/*
 * midi2 basic usage example
 *
 * Shows how to build, parse, and process UMP messages.
 * Compile: gcc -std=c99 -I../src basic_usage.c ../src/midi2_proc.c ../src/midi2_ci.c -o basic_usage
 */

#include <stdio.h>
#include "midi2_msg.h"
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

/* --- Callbacks --- */

static void on_ump(const uint32_t *w, uint8_t wc, void *ctx) {
  (void)ctx; (void)wc;
  uint8_t mt = midi2_msg_get_mt(w);
  if (mt == MIDI2_MT_MIDI2_CV) {
    uint8_t status = midi2_msg_get_status(w) & 0xF0;
    uint8_t ch = midi2_msg_get_channel(w);
    if (status == MIDI2_STATUS_NOTE_ON) {
      printf("  Note On: ch=%d note=%d vel=%u\n",
        ch, midi2_msg_get_note(w), midi2_msg_get_velocity(w));
    } else if (status == MIDI2_STATUS_CC) {
      printf("  CC: ch=%d index=%d value=%u\n",
        ch, midi2_msg_get_note(w), midi2_msg_get_data(w));
    }
  }
}

static void on_sysex(uint8_t group, const uint8_t *data, uint16_t len, void *ctx) {
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

  midi2_msg_note_on(w, 0, 0, 60, 0xC000, 0);
  printf("Note On C4:  %08X %08X\n", (unsigned)w[0], (unsigned)w[1]);

  midi2_msg_cc(w, 0, 0, 74, 0x80000000);
  printf("CC 74 50%%:   %08X %08X\n", (unsigned)w[0], (unsigned)w[1]);

  midi2_msg_tempo(w, 0, 5000000);
  printf("Tempo 120:   %08X %08X %08X %08X\n",
    (unsigned)w[0], (unsigned)w[1], (unsigned)w[2], (unsigned)w[3]);

  /* --- 2. Value scaling --- */

  printf("\n=== Value scaling ===\n\n");

  uint16_t vel16 = midi2_msg_scale_up_7to16(127);
  printf("7-bit 127  -> 16-bit %u\n", vel16);

  uint8_t vel7 = midi2_msg_scale_down_16to7(0xC000);
  printf("16-bit 0xC000 -> 7-bit %u\n", vel7);

  /* --- 3. Process incoming messages --- */

  printf("\n=== Processing with midi2_proc ===\n\n");

  uint8_t sysex7_buf[128];
  uint8_t sysex8_buf[256];
  midi2_proc_state proc;
  midi2_proc_init(&proc, sysex7_buf, sizeof(sysex7_buf), sysex8_buf, sizeof(sysex8_buf));
  proc.on_ump = on_ump;
  proc.on_sysex7 = on_sysex;

  /* Feed a Note On */
  midi2_msg_note_on(w, 0, 0, 60, 0xFFFF, 0);
  midi2_proc_feed(&proc, w, 2);

  /* Feed a CC */
  midi2_msg_cc(w, 0, 0, 1, 0x40000000);
  midi2_proc_feed(&proc, w, 2);

  /* --- 4. MIDI-CI auto-response --- */

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
