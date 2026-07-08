/* Host unit tests for the rp2350-device-freertos UMP catalog.
 * Pure C99, links the midi2 core headers, no Pico SDK, no FreeRTOS.
 * Each assertion recomputes the expected UMP words with midi2_msg_* so the
 * test is independent of the catalog's internal constants. */
#include <assert.h>
#include <stdio.h>
#include "catalog.h"
#include "midi2_msg.h"

/* Test helper: return (by copy) the first catalog entry whose MT nibble and
 * status high-nibble match. status is given in the constant's full-byte form
 * (nibble in bits 7..4), matched channel-agnostically. Returns 1 on hit. */
static int find_entry_status(catalog_msg_t *out, uint8_t mt, uint8_t status) {
  uint32_t n = midi2_catalog_count();
  for (uint32_t i = 0; i < n; i++) {
    catalog_msg_t e;
    if (midi2_catalog_build(i, &e) == 0) continue;
    if (midi2_msg_get_mt(e.w) != mt) continue;
    if ((midi2_msg_get_status(e.w) & 0xF0) != (status & 0xF0)) continue;
    *out = e;
    return 1;
  }
  return 0;
}

static void test_count_nonzero(void) {
  assert(midi2_catalog_count() >= 100);
}

static void test_index0_is_noop(void) {
  catalog_msg_t got;
  uint8_t n = midi2_catalog_build(0, &got);
  assert(n == 1);
  assert(got.w[0] == midi2_msg_noop());   /* MT 0x0 Utility NOOP, 1 word */
}

int main(void) {
  test_count_nonzero();
  test_index0_is_noop();
  (void) find_entry_status;   /* used by later category tests */
  printf("all catalog tests passed\n");
  return 0;
}
