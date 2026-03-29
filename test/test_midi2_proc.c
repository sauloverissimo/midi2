/*
 * midi2_proc unit tests
 * Compile: gcc -std=c99 -Wall -I../src test_midi2_proc.c ../src/midi2_proc.c -o test_midi2_proc
 */

#include <stdio.h>
#include <string.h>
#include "midi2_proc.h"

static int passed = 0;
static int failed = 0;

#define TEST(name) printf("  %-50s ", name)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* --- Test helpers --- */

static uint32_t last_ump[4];
static uint8_t  last_ump_wc;
static int      ump_cb_count;

static void test_ump_cb(const uint32_t *words, uint8_t wc, void *ctx) {
  (void)ctx;
  memcpy(last_ump, words, wc * 4);
  last_ump_wc = wc;
  ump_cb_count++;
}

static uint8_t  last_sysex_group;
static uint8_t  last_sysex_data[256];
static uint16_t last_sysex_len;
static int      sysex_cb_count;

static void test_sysex_cb(uint8_t group, const uint8_t *data, uint16_t len, void *ctx) {
  (void)ctx;
  last_sysex_group = group;
  memcpy(last_sysex_data, data, len);
  last_sysex_len = len;
  sysex_cb_count++;
}

static uint32_t write_buf[64];
static uint32_t write_buf_pos;

static uint32_t test_write_fn(const uint32_t *words, uint32_t count, void *ctx) {
  (void)ctx;
  uint32_t i;
  for (i = 0; i < count && write_buf_pos < 64; i++) {
    write_buf[write_buf_pos++] = words[i];
  }
  return count;
}

static uint8_t proc_sysex_buf[128];
static uint8_t proc_sysex8_buf[256];

static void reset_state(void) {
  memset(last_ump, 0, sizeof(last_ump));
  last_ump_wc = 0;
  ump_cb_count = 0;
  last_sysex_group = 0xFF;
  memset(last_sysex_data, 0, sizeof(last_sysex_data));
  last_sysex_len = 0;
  sysex_cb_count = 0;
  memset(write_buf, 0, sizeof(write_buf));
  write_buf_pos = 0;
}

/* --- Init tests --- */

void test_init_defaults(void) {
  TEST("init: group_mask = 0xFFFF");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));
  CHECK(s.group_mask == 0xFFFF, "expected all groups enabled");
  CHECK(s.group_map[0] == 0, "map[0] == 0");
  CHECK(s.group_map[15] == 15, "map[15] == 15");
  CHECK(s.sysex_group == 0xFF, "no active sysex");
  CHECK(s.on_ump == NULL, "no ump callback");
  CHECK(s.on_sysex7 == NULL, "no sysex callback");
  PASS();
}

/* --- Feed tests --- */

void test_feed_note_on_delivered(void) {
  TEST("feed: Note On delivered via on_ump");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));
  s.on_ump = test_ump_cb;
  reset_state();

  uint32_t w[2];
  midi2_msg_note_on(w, 0, 0, 60, 0xC000, 0);
  midi2_proc_feed(&s, w, 2);

  CHECK(ump_cb_count == 1, "callback called once");
  CHECK(last_ump_wc == 2, "2 words");
  CHECK(midi2_msg_get_note(last_ump) == 60, "note 60");
  PASS();
}

void test_feed_group_filtered(void) {
  TEST("feed: group 1 filtered out when mask = 0x0001");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));
  s.on_ump = test_ump_cb;
  s.group_mask = 0x0001;  /* only group 0 */
  reset_state();

  uint32_t w[2];
  midi2_msg_note_on(w, 1, 0, 60, 0xC000, 0);  /* group 1 */
  midi2_proc_feed(&s, w, 2);

  CHECK(ump_cb_count == 0, "callback NOT called");
  PASS();
}

void test_feed_group_0_passes(void) {
  TEST("feed: group 0 passes when mask = 0x0001");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));
  s.on_ump = test_ump_cb;
  s.group_mask = 0x0001;
  reset_state();

  uint32_t w[2];
  midi2_msg_note_on(w, 0, 0, 60, 0xC000, 0);  /* group 0 */
  midi2_proc_feed(&s, w, 2);

  CHECK(ump_cb_count == 1, "callback called");
  PASS();
}

void test_feed_utility_bypasses_filter(void) {
  TEST("feed: Utility (MT 0x0) bypasses group filter");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));
  s.on_ump = test_ump_cb;
  s.group_mask = 0x0000;  /* no groups */
  reset_state();

  /* MT 0x0, group 0 */
  uint32_t w = 0x00000000;
  midi2_proc_feed(&s, &w, 1);

  CHECK(ump_cb_count == 1, "utility delivered despite mask=0");
  PASS();
}

void test_feed_stream_bypasses_filter(void) {
  TEST("feed: Stream (MT 0xF) bypasses group filter");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));
  s.on_ump = test_ump_cb;
  s.group_mask = 0x0000;
  reset_state();

  /* MT 0xF, 4 words */
  uint32_t w[4] = { 0xF0000000, 0, 0, 0 };
  midi2_proc_feed(&s, w, 4);

  CHECK(ump_cb_count == 1, "stream delivered despite mask=0");
  PASS();
}

/* --- SysEx7 reassembly --- */

void test_sysex7_complete(void) {
  TEST("sysex7: complete single-packet");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));
  s.on_sysex7 = test_sysex_cb;
  reset_state();

  uint32_t w[2];
  uint8_t data[] = {0x7E, 0x7F, 0x09};
  midi2_msg_sysex7_packet(w, 0, MIDI2_SYSEX7_COMPLETE, data, 3);
  midi2_proc_feed(&s, w, 2);

  CHECK(sysex_cb_count == 1, "callback called");
  CHECK(last_sysex_len == 3, "3 bytes");
  CHECK(last_sysex_data[0] == 0x7E, "data[0]");
  CHECK(last_sysex_data[1] == 0x7F, "data[1]");
  CHECK(last_sysex_data[2] == 0x09, "data[2]");
  PASS();
}

void test_sysex7_multi_packet(void) {
  TEST("sysex7: multi-packet reassembly (12 bytes)");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));
  s.on_sysex7 = test_sysex_cb;
  reset_state();

  uint32_t w[2];
  uint8_t part1[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  uint8_t part2[] = {0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C};

  midi2_msg_sysex7_packet(w, 0, MIDI2_SYSEX7_START, part1, 6);
  midi2_proc_feed(&s, w, 2);
  CHECK(sysex_cb_count == 0, "not delivered yet");

  midi2_msg_sysex7_packet(w, 0, MIDI2_SYSEX7_END, part2, 6);
  midi2_proc_feed(&s, w, 2);
  CHECK(sysex_cb_count == 1, "delivered after END");
  CHECK(last_sysex_len == 12, "12 bytes total");
  CHECK(last_sysex_data[0] == 0x01, "first byte");
  CHECK(last_sysex_data[11] == 0x0C, "last byte");
  PASS();
}

void test_sysex7_group_interleave_discards(void) {
  TEST("sysex7: different group mid-stream discards");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));
  s.on_sysex7 = test_sysex_cb;
  reset_state();

  uint32_t w[2];
  uint8_t data[] = {0x01, 0x02, 0x03};

  /* Start on group 0 */
  midi2_msg_sysex7_packet(w, 0, MIDI2_SYSEX7_START, data, 3);
  midi2_proc_feed(&s, w, 2);

  /* Start on group 1 (different group, discards group 0 in-progress) */
  midi2_msg_sysex7_packet(w, 1, MIDI2_SYSEX7_START, data, 3);
  midi2_proc_feed(&s, w, 2);

  /* End on group 1 */
  midi2_msg_sysex7_packet(w, 1, MIDI2_SYSEX7_END, data, 3);
  midi2_proc_feed(&s, w, 2);

  CHECK(sysex_cb_count == 1, "only group 1 delivered");
  CHECK(last_sysex_group == 1, "from group 1");
  CHECK(last_sysex_len == 6, "6 bytes (start + end)");
  PASS();
}

/* --- Group remap --- */

void test_remap_identity(void) {
  TEST("remap: identity map does not change group");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));

  uint32_t w[2];
  midi2_msg_note_on(w, 3, 0, 60, 0xC000, 0);
  midi2_proc_remap_group(&s, w);
  CHECK(midi2_msg_get_group(w) == 3, "group unchanged");
  PASS();
}

void test_remap_changes_group(void) {
  TEST("remap: map[0] = 5 changes group 0 to 5");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));
  s.group_map[0] = 5;

  uint32_t w[2];
  midi2_msg_note_on(w, 0, 0, 60, 0xC000, 0);
  midi2_proc_remap_group(&s, w);
  CHECK(midi2_msg_get_group(w) == 5, "group remapped to 5");
  PASS();
}

void test_remap_skips_utility(void) {
  TEST("remap: Utility (MT 0x0) not remapped");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));
  s.group_map[0] = 5;

  uint32_t w = 0x00000000;  /* MT 0x0, group 0 */
  midi2_proc_remap_group(&s, &w);
  CHECK(((w >> 24) & 0x0F) == 0, "group unchanged");
  PASS();
}

/* --- SysEx7 send (fragmentation) --- */

void test_send_sysex7_short(void) {
  TEST("send sysex7: 3 bytes = 1 complete packet");
  reset_state();
  uint8_t data[] = {0x7E, 0x7F, 0x09};
  midi2_proc_send_sysex7(0, data, 3, test_write_fn, NULL);
  CHECK(write_buf_pos == 2, "1 packet (2 words)");
  CHECK(midi2_msg_get_mt(write_buf) == MIDI2_MT_SYSEX7, "MT 0x3");
  CHECK(((write_buf[0] >> 20) & 0x0F) == 0x00, "status=complete");
  PASS();
}

void test_send_sysex7_long(void) {
  TEST("send sysex7: 15 bytes = 3 packets (start+cont+end)");
  reset_state();
  uint8_t data[15];
  int i;
  for (i = 0; i < 15; i++) data[i] = (uint8_t)(i + 1);

  midi2_proc_send_sysex7(0, data, 15, test_write_fn, NULL);
  CHECK(write_buf_pos == 6, "3 packets (6 words)");

  /* Check statuses: start, continue, end */
  CHECK(((write_buf[0] >> 20) & 0x0F) == 0x01, "packet 1 = start");
  CHECK(((write_buf[2] >> 20) & 0x0F) == 0x02, "packet 2 = continue");
  CHECK(((write_buf[4] >> 20) & 0x0F) == 0x03, "packet 3 = end");
  PASS();
}

void test_send_sysex7_exact_6(void) {
  TEST("send sysex7: 6 bytes = 1 complete packet");
  reset_state();
  uint8_t data[] = {1, 2, 3, 4, 5, 6};
  midi2_proc_send_sysex7(0, data, 6, test_write_fn, NULL);
  CHECK(write_buf_pos == 2, "1 packet");
  CHECK(((write_buf[0] >> 20) & 0x0F) == 0x00, "status=complete");
  PASS();
}

void test_send_sysex7_exact_12(void) {
  TEST("send sysex7: 12 bytes = 2 packets (start+end)");
  reset_state();
  uint8_t data[12];
  int i;
  for (i = 0; i < 12; i++) data[i] = (uint8_t)(i + 1);

  midi2_proc_send_sysex7(0, data, 12, test_write_fn, NULL);
  CHECK(write_buf_pos == 4, "2 packets (4 words)");
  CHECK(((write_buf[0] >> 20) & 0x0F) == 0x01, "packet 1 = start");
  CHECK(((write_buf[2] >> 20) & 0x0F) == 0x03, "packet 2 = end");
  PASS();
}

/* --- SysEx8 reassembly --- */

static uint8_t  last_sysex8_stream_id;
static uint8_t  last_sysex8_data[256];
static uint16_t last_sysex8_len;
static int      sysex8_cb_count;

static void test_sysex8_cb(uint8_t group, uint8_t stream_id,
                             const uint8_t *data, uint16_t len, void *ctx) {
  (void)group; (void)ctx;
  last_sysex8_stream_id = stream_id;
  memcpy(last_sysex8_data, data, len);
  last_sysex8_len = len;
  sysex8_cb_count++;
}

void test_sysex8_complete(void) {
  TEST("sysex8: complete single-packet");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));
  s.on_sysex8 = test_sysex8_cb;
  sysex8_cb_count = 0;
  last_sysex8_len = 0;

  uint32_t w[4];
  uint8_t data[] = {0xAA, 0xBB, 0xCC};
  midi2_msg_sysex8_packet(w, 0, MIDI2_SYSEX8_COMPLETE, 1, data, 3);
  midi2_proc_feed(&s, w, 4);

  CHECK(sysex8_cb_count == 1, "callback called");
  CHECK(last_sysex8_stream_id == 1, "stream_id=1");
  CHECK(last_sysex8_len == 3, "3 bytes");
  CHECK(last_sysex8_data[0] == 0xAA, "data[0]");
  PASS();
}

void test_sysex8_multi_packet(void) {
  TEST("sysex8: multi-packet reassembly");
  midi2_proc_state s;
  midi2_proc_init(&s, proc_sysex_buf, sizeof(proc_sysex_buf), proc_sysex8_buf, sizeof(proc_sysex8_buf));
  s.on_sysex8 = test_sysex8_cb;
  sysex8_cb_count = 0;
  last_sysex8_len = 0;

  uint32_t w[4];
  uint8_t part1[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  uint8_t part2[] = {0x06, 0x07, 0x08};

  midi2_msg_sysex8_packet(w, 0, MIDI2_SYSEX8_START, 2, part1, 5);
  midi2_proc_feed(&s, w, 4);
  CHECK(sysex8_cb_count == 0, "not delivered yet");

  midi2_msg_sysex8_packet(w, 0, MIDI2_SYSEX8_END, 2, part2, 3);
  midi2_proc_feed(&s, w, 4);
  CHECK(sysex8_cb_count == 1, "delivered after END");
  CHECK(last_sysex8_len == 8, "8 bytes total");
  CHECK(last_sysex8_stream_id == 2, "stream_id=2");
  CHECK(last_sysex8_data[0] == 0x01, "first byte");
  CHECK(last_sysex8_data[7] == 0x08, "last byte");
  PASS();
}

/* --- Main --- */

int main(void) {
  printf("\n=== midi2_proc Unit Tests ===\n\n");

  printf("[Init]\n");
  test_init_defaults();

  printf("\n[Feed & Group Filter]\n");
  test_feed_note_on_delivered();
  test_feed_group_filtered();
  test_feed_group_0_passes();
  test_feed_utility_bypasses_filter();
  test_feed_stream_bypasses_filter();

  printf("\n[SysEx7 Reassembly]\n");
  test_sysex7_complete();
  test_sysex7_multi_packet();
  test_sysex7_group_interleave_discards();

  printf("\n[Group Remap]\n");
  test_remap_identity();
  test_remap_changes_group();
  test_remap_skips_utility();

  printf("\n[SysEx7 Send]\n");
  test_send_sysex7_short();
  test_send_sysex7_long();
  test_send_sysex7_exact_6();
  test_send_sysex7_exact_12();

  printf("\n[SysEx8 Reassembly]\n");
  test_sysex8_complete();
  test_sysex8_multi_packet();

  printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
