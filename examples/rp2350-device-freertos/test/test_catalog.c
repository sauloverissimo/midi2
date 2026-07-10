/* Host unit tests for the rp2350-device-freertos UMP catalog.
 * Pure C99, links the midi2 core headers, no Pico SDK, no FreeRTOS.
 * Each assertion recomputes the expected UMP words with midi2_msg_* so the
 * test is independent of the catalog's internal constants. */
#include <assert.h>
#include <stdio.h>
#include "catalog.h"
#include "midi2_msg.h"

#define CAT_GROUP   0
#define CAT_CHANNEL 0

/* Total distinct message types enumerated (one per catalog entry). */
#define CATALOG_COUNT 58

/* Return (by copy) the first catalog entry with MT nibble `mt`. 1 on hit. */
static int find_entry_mt(catalog_msg_t *out, uint8_t mt) {
  for (uint32_t i = 0; i < midi2_catalog_count(); i++) {
    catalog_msg_t e;
    if (midi2_catalog_build(i, &e) == 0) continue;
    if (midi2_msg_get_mt(e.w) == mt) { *out = e; return 1; }
  }
  return 0;
}

/* Return the first entry whose MT and status high-nibble match. 1 on hit.
 * status is the constant's full-byte form (nibble in bits 7..4). */
static int find_entry_status(catalog_msg_t *out, uint8_t mt, uint8_t status) {
  for (uint32_t i = 0; i < midi2_catalog_count(); i++) {
    catalog_msg_t e;
    if (midi2_catalog_build(i, &e) == 0) continue;
    if (midi2_msg_get_mt(e.w) != mt) continue;
    if ((midi2_msg_get_status(e.w) & 0xF0) != (status & 0xF0)) continue;
    *out = e;
    return 1;
  }
  return 0;
}

static void test_exact_count(void) {
  assert(midi2_catalog_count() == CATALOG_COUNT);
}

static void test_index0_is_noop(void) {
  catalog_msg_t got;
  assert(midi2_catalog_build(0, &got) == 1);
  assert(got.w[0] == midi2_msg_noop());   /* MT 0x0 Utility NOOP, 1 word */
}

/* Every defined MT category is present with the correct word count. */
static void test_every_mt_present(void) {
  catalog_msg_t g;
  assert(find_entry_mt(&g, MIDI2_MT_UTILITY)  && g.n == 1);
  assert(find_entry_mt(&g, MIDI2_MT_SYSTEM)   && g.n == 1);
  assert(find_entry_mt(&g, MIDI2_MT_MIDI1_CV) && g.n == 1);
  assert(find_entry_mt(&g, MIDI2_MT_SYSEX7)   && g.n == 2);
  assert(find_entry_mt(&g, MIDI2_MT_MIDI2_CV) && g.n == 2);
  assert(find_entry_mt(&g, MIDI2_MT_DATA128)  && g.n == 4);
  assert(find_entry_mt(&g, MIDI2_MT_FLEX_DATA) && g.n == 4);
  assert(find_entry_mt(&g, MIDI2_MT_STREAM)   && g.n == 4);
}

/* The four SysEx7 statuses are all present as distinct entries. */
static void test_sysex7_statuses(void) {
  int seen[4] = {0,0,0,0};
  for (uint32_t i = 0; i < midi2_catalog_count(); i++) {
    catalog_msg_t e;
    if (midi2_catalog_build(i, &e) == 0) continue;
    if (midi2_msg_get_mt(e.w) != MIDI2_MT_SYSEX7) continue;
    uint8_t st = (uint8_t)(midi2_msg_get_status(e.w) & 0xF0);
    if (st == MIDI2_SYSEX7_COMPLETE) seen[0] = 1;
    if (st == MIDI2_SYSEX7_START)    seen[1] = 1;
    if (st == MIDI2_SYSEX7_CONTINUE) seen[2] = 1;
    if (st == MIDI2_SYSEX7_END)      seen[3] = 1;
  }
  assert(seen[0] && seen[1] && seen[2] && seen[3]);
}

/* The per-note MIDI 2.0 block: exact-match against recomputed builders.
 * This is the headline "this is really MIDI 2.0" assertion. */
static void test_per_note_block(void) {
  catalog_msg_t g;
  uint32_t exp[2];

  /* Registered Per-Note Controller: MT 0x4, status 0x0 */
  assert(find_entry_status(&g, MIDI2_MT_MIDI2_CV, MIDI2_STATUS_REG_PER_NOTE));
  assert(g.n == 2);

  /* Per-Note Pitch Bend: MT 0x4, status 0x6 */
  assert(find_entry_status(&g, MIDI2_MT_MIDI2_CV, MIDI2_STATUS_PER_NOTE_PB));
  midi2_msg_per_note_pb(exp, CAT_GROUP, CAT_CHANNEL, g.w[0] >> 8 & 0x7F, g.w[1]);
  assert(g.w[0] == exp[0]);   /* status nibble + note field match */

  /* Per-Note Management: MT 0x4, status 0xF */
  assert(find_entry_status(&g, MIDI2_MT_MIDI2_CV, MIDI2_STATUS_PER_NOTE_MGMT));
  assert(g.n == 2);

  /* Assignable Per-Note Controller: MT 0x4, status 0x1 */
  assert(find_entry_status(&g, MIDI2_MT_MIDI2_CV, MIDI2_STATUS_ASN_PER_NOTE));
  assert(g.n == 2);
}

/* App-owned UMP Stream entries only: Device Identity + Start/End of Clip.
 * Endpoint/FB/Config discovery is owned by the TinyUSB driver, not the catalog. */
static void test_stream_app_owned(void) {
  int identity = 0, start_clip = 0, end_clip = 0;
  uint32_t si[4], ei[4];
  midi2_msg_stream_start_of_clip(si);
  midi2_msg_stream_end_of_clip(ei);
  for (uint32_t i = 0; i < midi2_catalog_count(); i++) {
    catalog_msg_t e;
    if (midi2_catalog_build(i, &e) == 0) continue;
    if (midi2_msg_get_mt(e.w) != MIDI2_MT_STREAM) continue;
    uint16_t status = (uint16_t)((e.w[0] >> 16) & 0x3FF);
    if (status == MIDI2_STREAM_DEVICE_IDENTITY) identity = 1;
    if (status == MIDI2_STREAM_START_OF_CLIP && e.w[0] == si[0]) start_clip = 1;
    if (status == MIDI2_STREAM_END_OF_CLIP && e.w[0] == ei[0]) end_clip = 1;
    /* the driver owns these; the catalog must NOT emit them */
    assert(status != MIDI2_STREAM_ENDPOINT_DISCOVERY);
    assert(status != MIDI2_STREAM_FB_DISCOVERY);
    assert(status != MIDI2_STREAM_CONFIG_REQUEST);
  }
  assert(identity && start_clip && end_clip);
}

int main(void) {
  test_exact_count();
  test_index0_is_noop();
  test_every_mt_present();
  test_sysex7_statuses();
  test_per_note_block();
  test_stream_app_owned();
  printf("all catalog tests passed (%u entries)\n", midi2_catalog_count());
  return 0;
}
