/*
 * midi2_conv unit tests
 * Compile: gcc -std=c99 -Wall -I../src test_midi2_conv.c ../src/midi2_conv.c -o test_midi2_conv
 */

#include <stdio.h>
#include <string.h>
#include "midi2_conv.h"

static int passed = 0;
static int failed = 0;

static uint8_t conv_sysex_buf[128];

#define TEST(name) printf("  %-55s ", name)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* --- Note On --- */

void test_note_on_3bytes(void) {
  TEST("Note On: 0x90 0x3C 0x7F -> MT 0x2");
  midi2_conv_state s;
  midi2_conv_init(&s, 0, conv_sysex_buf, sizeof(conv_sysex_buf));

  CHECK(!midi2_conv_feed(&s, 0x90), "status: not ready");
  CHECK(!midi2_conv_feed(&s, 0x3C), "data1: not ready");
  CHECK(midi2_conv_feed(&s, 0x7F), "data2: ready");
  CHECK(s.ump_words == 1, "1 word");
  CHECK(midi2_msg_get_mt(s.ump) == MIDI2_MT_MIDI1_CV, "MT=0x2");
  CHECK((s.ump[0] >> 16 & 0xFF) == 0x90, "status=0x90");
  CHECK((s.ump[0] >> 8 & 0x7F) == 0x3C, "note=60");
  CHECK((s.ump[0] & 0x7F) == 0x7F, "vel=127");
  PASS();
}

/* --- Running Status --- */

void test_running_status(void) {
  TEST("Running Status: 2nd note without status byte");
  midi2_conv_state s;
  midi2_conv_init(&s, 0, conv_sysex_buf, sizeof(conv_sysex_buf));

  /* First note: full message */
  midi2_conv_feed(&s, 0x90);
  midi2_conv_feed(&s, 0x3C);
  CHECK(midi2_conv_feed(&s, 0x7F), "first note ready");

  /* Second note: Running Status (no status byte) */
  CHECK(!midi2_conv_feed(&s, 0x40), "data1: not ready");
  CHECK(midi2_conv_feed(&s, 0x60), "data2: ready");
  CHECK((s.ump[0] >> 8 & 0x7F) == 0x40, "note=64");
  CHECK((s.ump[0] & 0x7F) == 0x60, "vel=96");
  PASS();
}

/* --- Program Change (1 data byte) --- */

void test_program_change(void) {
  TEST("Program Change: 0xC0 0x05 -> 1 data byte");
  midi2_conv_state s;
  midi2_conv_init(&s, 0, conv_sysex_buf, sizeof(conv_sysex_buf));

  CHECK(!midi2_conv_feed(&s, 0xC0), "status: not ready");
  CHECK(midi2_conv_feed(&s, 0x05), "data: ready");
  CHECK((s.ump[0] >> 16 & 0xFF) == 0xC0, "status=0xC0");
  CHECK((s.ump[0] >> 8 & 0x7F) == 0x05, "program=5");
  PASS();
}

/* --- Real-Time mid-message --- */

void test_realtime_mid_message(void) {
  TEST("Real-Time F8 mid-message: does not break parsing");
  midi2_conv_state s;
  midi2_conv_init(&s, 0, conv_sysex_buf, sizeof(conv_sysex_buf));

  midi2_conv_feed(&s, 0x90);
  midi2_conv_feed(&s, 0x3C);

  /* Real-Time arrives between data bytes */
  CHECK(midi2_conv_feed(&s, 0xF8), "F8 emits immediately");
  CHECK(s.ump_words == 1, "1 word");
  CHECK(midi2_msg_get_mt(s.ump) == MIDI2_MT_SYSTEM, "MT=System");
  CHECK(((s.ump[0] >> 16) & 0xFF) == 0xF8, "status=F8");

  /* Continue with the data byte */
  CHECK(midi2_conv_feed(&s, 0x7F), "data2: note completes");
  CHECK(midi2_msg_get_mt(s.ump) == MIDI2_MT_MIDI1_CV, "MT=MIDI1 CV");
  CHECK((s.ump[0] >> 8 & 0x7F) == 0x3C, "note=60");
  PASS();
}

/* --- System Common cancels Running Status --- */

void test_system_common_cancels_running(void) {
  TEST("Tune Request (F6): cancels Running Status");
  midi2_conv_state s;
  midi2_conv_init(&s, 0, conv_sysex_buf, sizeof(conv_sysex_buf));

  /* Set Running Status */
  midi2_conv_feed(&s, 0x90);
  midi2_conv_feed(&s, 0x3C);
  midi2_conv_feed(&s, 0x7F);

  /* Tune Request */
  CHECK(midi2_conv_feed(&s, 0xF6), "F6 emits immediately");
  CHECK(((s.ump[0] >> 16) & 0xFF) == 0xF6, "status=F6");

  /* Data byte after F6: orphan (no Running Status) */
  CHECK(!midi2_conv_feed(&s, 0x3C), "orphan data: ignored");
  PASS();
}

/* --- Song Position Pointer (F2, 2 data bytes) --- */

void test_song_position_pointer(void) {
  TEST("Song Position Pointer: F2 + 2 data bytes");
  midi2_conv_state s;
  midi2_conv_init(&s, 0, conv_sysex_buf, sizeof(conv_sysex_buf));

  CHECK(!midi2_conv_feed(&s, 0xF2), "status");
  CHECK(!midi2_conv_feed(&s, 0x40), "data1");
  CHECK(midi2_conv_feed(&s, 0x20), "data2: ready");
  CHECK(midi2_msg_get_mt(s.ump) == MIDI2_MT_SYSTEM, "MT=System");
  CHECK(((s.ump[0] >> 16) & 0xFF) == 0xF2, "status=F2");
  CHECK(((s.ump[0] >> 8) & 0xFF) == 0x40, "data1");
  CHECK((s.ump[0] & 0xFF) == 0x20, "data2");
  PASS();
}

/* --- SysEx --- */

void test_sysex_short(void) {
  TEST("SysEx: F0 01 02 03 F7 -> complete packet");
  midi2_conv_state s;
  midi2_conv_init(&s, 0, conv_sysex_buf, sizeof(conv_sysex_buf));

  CHECK(!midi2_conv_feed(&s, 0xF0), "F0: start");
  CHECK(!midi2_conv_feed(&s, 0x01), "data");
  CHECK(!midi2_conv_feed(&s, 0x02), "data");
  CHECK(!midi2_conv_feed(&s, 0x03), "data");
  CHECK(midi2_conv_feed(&s, 0xF7), "F7: end, packet ready");
  CHECK(s.ump_words == 2, "2 words (SysEx7)");
  CHECK(midi2_msg_get_mt(s.ump) == MIDI2_MT_SYSEX7, "MT=SysEx7");
  PASS();
}

void test_sysex_cancels_running_status(void) {
  TEST("SysEx: cancels Running Status");
  midi2_conv_state s;
  midi2_conv_init(&s, 0, conv_sysex_buf, sizeof(conv_sysex_buf));

  /* Set Running Status */
  midi2_conv_feed(&s, 0x90);
  midi2_conv_feed(&s, 0x3C);
  midi2_conv_feed(&s, 0x7F);

  /* SysEx */
  midi2_conv_feed(&s, 0xF0);
  midi2_conv_feed(&s, 0x01);
  midi2_conv_feed(&s, 0xF7);

  /* Data byte after SysEx: orphan */
  CHECK(!midi2_conv_feed(&s, 0x3C), "orphan: Running Status cleared");
  PASS();
}

void test_sysex_empty(void) {
  TEST("SysEx: F0 F7 -> empty complete packet");
  midi2_conv_state s;
  midi2_conv_init(&s, 0, conv_sysex_buf, sizeof(conv_sysex_buf));

  CHECK(!midi2_conv_feed(&s, 0xF0), "F0: start");
  CHECK(midi2_conv_feed(&s, 0xF7), "F7: end");
  CHECK(s.ump_words == 2, "2 words");
  CHECK(midi2_msg_get_mt(s.ump) == MIDI2_MT_SYSEX7, "MT=SysEx7");
  PASS();
}

/* --- Group --- */

void test_group_assignment(void) {
  TEST("Group: assigned group appears in UMP");
  midi2_conv_state s;
  midi2_conv_init(&s, 5, conv_sysex_buf, sizeof(conv_sysex_buf));

  midi2_conv_feed(&s, 0x90);
  midi2_conv_feed(&s, 0x3C);
  midi2_conv_feed(&s, 0x7F);

  CHECK(midi2_msg_get_group(s.ump) == 5, "group=5");
  PASS();
}

/* --- Orphan data byte --- */

void test_orphan_data_byte(void) {
  TEST("Orphan data byte: ignored when no status");
  midi2_conv_state s;
  midi2_conv_init(&s, 0, conv_sysex_buf, sizeof(conv_sysex_buf));

  CHECK(!midi2_conv_feed(&s, 0x3C), "data without status: ignored");
  CHECK(s.ump_words == 0, "no output");
  PASS();
}

/* --- New status cancels previous --- */

void test_new_status_cancels_previous(void) {
  TEST("New status byte cancels previous incomplete");
  midi2_conv_state s;
  midi2_conv_init(&s, 0, conv_sysex_buf, sizeof(conv_sysex_buf));

  midi2_conv_feed(&s, 0x90);  /* Note On */
  midi2_conv_feed(&s, 0x3C);  /* data1 */
  /* No data2 -- new status arrives */
  CHECK(!midi2_conv_feed(&s, 0xB0), "new CC status: not ready");
  CHECK(!midi2_conv_feed(&s, 0x07), "CC data1");
  CHECK(midi2_conv_feed(&s, 0x64), "CC data2: ready");
  CHECK((s.ump[0] >> 16 & 0xFF) == 0xB0, "status=CC");
  CHECK((s.ump[0] >> 8 & 0x7F) == 0x07, "CC=7 (volume)");
  CHECK((s.ump[0] & 0x7F) == 0x64, "value=100");
  PASS();
}

/* --- Main --- */

int main(void) {
  printf("\n=== midi2_conv Unit Tests ===\n\n");

  printf("[Channel Voice]\n");
  test_note_on_3bytes();
  test_running_status();
  test_program_change();

  printf("\n[Real-Time]\n");
  test_realtime_mid_message();

  printf("\n[System Common]\n");
  test_system_common_cancels_running();
  test_song_position_pointer();

  printf("\n[SysEx]\n");
  test_sysex_short();
  test_sysex_cancels_running_status();
  test_sysex_empty();

  printf("\n[Group]\n");
  test_group_assignment();

  printf("\n[Edge Cases]\n");
  test_orphan_data_byte();
  test_new_status_cancels_previous();

  printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
