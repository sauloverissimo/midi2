/*
 * midi2_conv unit tests
 * Compile: gcc -std=c99 -Wall -I../src test_midi2_conv.c ../src/midi2_conv.c -o test_midi2_conv
 */

#include <stdio.h>
#include <string.h>
#include "midi2_conv.h"

static int passed = 0;
static int failed = 0;

#define TEST(name) printf("  %-55s ", name)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* --- Note On --- */

void test_note_on_3bytes(void) {
  TEST("Note On: 0x90 0x3C 0x7F -> MT 0x2");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

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
  midi2_conv_init(&s, 0);

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
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0xC0), "status: not ready");
  CHECK(midi2_conv_feed(&s, 0x05), "data: ready");
  CHECK((s.ump[0] >> 16 & 0xFF) == 0xC0, "status=0xC0");
  CHECK((s.ump[0] >> 8 & 0x7F) == 0x05, "program=5");
  PASS();
}

/* --- Note Off / Pressure / Pitch Bend --- */

/* MT 0x2 layout, M2-104-UM 7.3: 0x2 | group | opcode | channel | d1 | d2.
 * The expected words are written out from the spec, so a wrong opcode or a
 * wrong data-byte count fails the literal. Non-zero channels assert the
 * channel field as well. */

void test_note_off(void) {
  TEST("Note Off: 0x80 + 2 data bytes -> MT 0x2 opcode 8");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0x80), "status: not ready");
  CHECK(!midi2_conv_feed(&s, 0x3C), "note: still not ready (needs 2)");
  CHECK(midi2_conv_feed(&s, 0x40), "velocity: ready");
  CHECK(s.ump_words == 1, "1 word");
  CHECK(s.ump[0] == 0x20803C40, "0x20803C40");
  PASS();
}

void test_note_off_running_status(void) {
  TEST("Note Off: running status repeats with 2 bytes each");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  midi2_conv_feed(&s, 0x85);          /* Note Off, channel 5 */
  midi2_conv_feed(&s, 0x3C);
  CHECK(midi2_conv_feed(&s, 0x40), "first note off ready");
  CHECK(s.ump[0] == 0x20853C40, "channel 5 preserved");

  CHECK(!midi2_conv_feed(&s, 0x3E), "running status: first byte");
  CHECK(midi2_conv_feed(&s, 0x00), "running status: second byte completes");
  CHECK(s.ump[0] == 0x20853E00, "second note off via running status");
  PASS();
}

void test_poly_pressure(void) {
  TEST("Poly Pressure: 0xA0 + 2 data bytes -> opcode A");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0xA2), "status, channel 2");
  CHECK(!midi2_conv_feed(&s, 0x40), "note: needs one more");
  CHECK(midi2_conv_feed(&s, 0x55), "pressure: ready");
  CHECK(s.ump[0] == 0x20A24055, "0x20A24055");
  PASS();
}

void test_channel_pressure(void) {
  TEST("Channel Pressure: 0xD0 + 1 data byte -> opcode D");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0xD3), "status, channel 3");
  CHECK(midi2_conv_feed(&s, 0x33), "single data byte completes it");
  CHECK(s.ump[0] == 0x20D33300, "0x20D33300, second data byte zero");
  PASS();
}

void test_pitch_bend(void) {
  TEST("Pitch Bend: 0xE0 + 2 data bytes -> opcode E");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0xE1), "status, channel 1");
  CHECK(!midi2_conv_feed(&s, 0x00), "LSB: needs one more");
  CHECK(midi2_conv_feed(&s, 0x40), "MSB: ready");
  CHECK(s.ump[0] == 0x20E10040, "0x20E10040 (centre)");
  PASS();
}

/* --- System Common carrying data bytes --- */

/* Cross-checked against the library's own constructors: the converter reaching
 * the same word by a different route is stronger than a literal alone. */

void test_mtc_quarter_frame(void) {
  TEST("MTC Quarter Frame: F1 + 1 data byte");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0xF1), "status: not ready");
  CHECK(midi2_conv_feed(&s, 0x25), "data byte completes it");
  CHECK(s.ump[0] == 0x10F12500, "0x10F12500");
  CHECK(s.ump[0] == midi2_msg_system_mtc(0, 0x25), "matches constructor");
  PASS();
}

void test_song_select(void) {
  TEST("Song Select: F3 + 1 data byte");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0xF3), "status: not ready");
  CHECK(midi2_conv_feed(&s, 0x07), "data byte completes it");
  CHECK(s.ump[0] == 0x10F30700, "0x10F30700");
  CHECK(s.ump[0] == midi2_msg_system_song_select(0, 0x07), "matches constructor");
  PASS();
}

/* --- Real-Time mid-message --- */

void test_realtime_mid_message(void) {
  TEST("Real-Time F8 mid-message: does not break parsing");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

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
  midi2_conv_init(&s, 0);

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
  midi2_conv_init(&s, 0);

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
  midi2_conv_init(&s, 0);

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
  midi2_conv_init(&s, 0);

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
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0xF0), "F0: start");
  CHECK(midi2_conv_feed(&s, 0xF7), "F7: end");
  CHECK(s.ump_words == 2, "2 words");
  CHECK(midi2_msg_get_mt(s.ump) == MIDI2_MT_SYSEX7, "MT=SysEx7");
  PASS();
}

/* --- SysEx long (streaming) --- */

void test_sysex_exactly_6(void) {
  TEST("SysEx 6 bytes: F0 + 6 data + F7 -> COMPLETE");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0xF0), "F0");
  uint8_t i;
  for (i = 0; i < 5; i++)
    CHECK(!midi2_conv_feed(&s, 0x10 + i), "data");
  CHECK(midi2_conv_feed(&s, 0xF7), "F7 -> packet");
  CHECK(s.ump_words == 2, "2 words");
  /* status nibble should be COMPLETE (0x00) since no START was emitted */
  uint8_t status = (s.ump[0] >> 16) & 0xF0;
  CHECK(status == MIDI2_SYSEX7_COMPLETE, "COMPLETE");
  PASS();
}

void test_sysex_7_bytes(void) {
  TEST("SysEx 7 bytes: START(6) + END(1)");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  midi2_conv_feed(&s, 0xF0);
  uint8_t i;
  int got_start = 0;
  for (i = 0; i < 6; i++) {
    if (midi2_conv_feed(&s, 0x20 + i)) {
      /* 6th byte triggers START */
      got_start = 1;
      CHECK(s.ump_words == 2, "2 words");
      uint8_t st = (s.ump[0] >> 16) & 0xF0;
      CHECK(st == MIDI2_SYSEX7_START, "START");
    }
  }
  CHECK(got_start, "got START at byte 6");

  /* 7th data byte + F7 */
  CHECK(!midi2_conv_feed(&s, 0x26), "7th byte: accumulating");
  CHECK(midi2_conv_feed(&s, 0xF7), "F7 -> END");
  uint8_t st = (s.ump[0] >> 16) & 0xF0;
  CHECK(st == MIDI2_SYSEX7_END, "END");
  uint8_t nb = (s.ump[0] >> 16) & 0x0F;
  CHECK(nb == 1, "1 byte in END packet");
  PASS();
}

void test_sysex_12_bytes(void) {
  TEST("SysEx 12 bytes: START(6) + CONTINUE(6) + END(0)");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  midi2_conv_feed(&s, 0xF0);
  uint8_t i;
  int packets = 0;
  for (i = 0; i < 12; i++) {
    if (midi2_conv_feed(&s, 0x30 + (i % 16))) {
      packets++;
    }
  }
  CHECK(packets == 2, "START + CONTINUE during data");

  CHECK(midi2_conv_feed(&s, 0xF7), "F7 -> END");
  uint8_t st = (s.ump[0] >> 16) & 0xF0;
  CHECK(st == MIDI2_SYSEX7_END, "END");
  uint8_t nb = (s.ump[0] >> 16) & 0x0F;
  CHECK(nb == 0, "0 bytes in END packet");
  PASS();
}

void test_sysex_30_bytes_ci(void) {
  TEST("SysEx 30 bytes (CI-sized): START + 3*CONTINUE + END");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  midi2_conv_feed(&s, 0xF0);
  uint8_t i;
  int packets = 0;
  for (i = 0; i < 30; i++) {
    if (midi2_conv_feed(&s, i & 0x7F)) {
      uint8_t st = (s.ump[0] >> 16) & 0xF0;
      if (packets == 0)
        CHECK(st == MIDI2_SYSEX7_START, "first=START");
      else
        CHECK(st == MIDI2_SYSEX7_CONTINUE, "mid=CONTINUE");
      packets++;
    }
  }
  CHECK(packets == 5, "5 packets during 30 data bytes"); /* 30/6 = 5 */

  CHECK(midi2_conv_feed(&s, 0xF7), "F7 -> END");
  uint8_t st = (s.ump[0] >> 16) & 0xF0;
  CHECK(st == MIDI2_SYSEX7_END, "final=END");
  uint8_t nb = (s.ump[0] >> 16) & 0x0F;
  CHECK(nb == 0, "0 remaining bytes in END");
  PASS();
}

void test_sysex_13_bytes(void) {
  TEST("SysEx 13 bytes: START(6) + CONTINUE(6) + END(1)");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  midi2_conv_feed(&s, 0xF0);
  uint8_t i;
  int packets = 0;
  for (i = 0; i < 13; i++) {
    if (midi2_conv_feed(&s, 0x40 + (i % 16)))
      packets++;
  }
  CHECK(packets == 2, "START + CONTINUE during data");

  CHECK(midi2_conv_feed(&s, 0xF7), "F7 -> END");
  uint8_t st = (s.ump[0] >> 16) & 0xF0;
  CHECK(st == MIDI2_SYSEX7_END, "END");
  uint8_t nb = (s.ump[0] >> 16) & 0x0F;
  CHECK(nb == 1, "1 byte in END");
  PASS();
}

/* --- Group --- */

void test_group_assignment(void) {
  TEST("Group: assigned group appears in UMP");
  midi2_conv_state s;
  midi2_conv_init(&s, 5);

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
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0x3C), "data without status: ignored");
  CHECK(s.ump_words == 0, "no output");
  PASS();
}

/* --- New status cancels previous --- */

void test_new_status_cancels_previous(void) {
  TEST("New status byte cancels previous incomplete");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

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

/* --- NULL paths --- */

void test_init_null_safe(void) {
  TEST("conv_init: NULL state is no-op (no crash)");
  midi2_conv_init(NULL, 0);
  /* If we reach here, no segfault occurred */
  PASS();
}

void test_feed_null_safe(void) {
  TEST("conv_feed: NULL state returns false (no crash)");
  CHECK(!midi2_conv_feed(NULL, 0x90), "NULL state rejected");
  CHECK(!midi2_conv_feed(NULL, 0xF8), "NULL state rejected (real-time)");
  CHECK(!midi2_conv_feed(NULL, 0xF0), "NULL state rejected (sysex start)");
  PASS();
}

/* Track 2: the wrapper must clear in_feed on every return path (conv_feed has
 * many). Verify it is clear after a return-false byte, a return-true byte, and
 * a real-time byte (a distinct early-return path). If any path leaked the flag,
 * the next feed would trip the debug assert. */
void test_conv_in_feed_cleared_all_paths(void) {
  TEST("Track 2: in_feed cleared after every return path");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);
  CHECK(!midi2_conv_feed(&s, 0x90), "status byte: not ready");
  CHECK(s.in_feed == false, "in_feed clear after return-false path");
  CHECK(!midi2_conv_feed(&s, 0x3C), "data1: not ready");
  CHECK(s.in_feed == false, "in_feed clear after data path");
  CHECK(midi2_conv_feed(&s, 0x7F), "data2: ready");
  CHECK(s.in_feed == false, "in_feed clear after return-true path");
  CHECK(midi2_conv_feed(&s, 0xF8), "real-time: ready");
  CHECK(s.in_feed == false, "in_feed clear after real-time early-return path");
  PASS();
}

/* --- Undefined System Common --- */

/* F4 and F5 are undefined System Common: the spec fixes no data-byte count, so
 * a converter must not reserve bytes for them. Reserving two swallows the head
 * of whatever message follows and corrupts it. Emit them as a standalone
 * one-byte System message and leave the next message untouched. */
void test_undefined_system_common_no_data_bytes(void) {
  TEST("SysCom F4/F5: emitted alone, next message intact");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  CHECK(midi2_conv_feed(&s, 0xF4), "F4: ready immediately");
  CHECK(s.ump_words == 1, "1 word");
  CHECK(((s.ump[0] >> 16) & 0xFF) == 0xF4, "status F4");

  CHECK(!midi2_conv_feed(&s, 0x90), "Note On status");
  CHECK(!midi2_conv_feed(&s, 0x3C), "note");
  CHECK(midi2_conv_feed(&s, 0x7F), "velocity: message complete");
  CHECK(s.ump[0] == 0x20903C7F, "Note On survived F4 intact");

  CHECK(midi2_conv_feed(&s, 0xF5), "F5: ready immediately");
  CHECK(((s.ump[0] >> 16) & 0xFF) == 0xF5, "status F5");
  PASS();
}

/* The data-byte count for F4/F5 being undefined means some devices do send
 * bytes after them. Those bytes have no defined meaning, so they are dropped
 * as orphans; what matters is that they do not corrupt the next message. */
void test_undefined_system_common_trailing_data_dropped(void) {
  TEST("SysCom F4 + stray data: data dropped, next intact");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  CHECK(midi2_conv_feed(&s, 0xF4), "F4: ready");
  CHECK(!midi2_conv_feed(&s, 0x11), "stray data byte: dropped");
  CHECK(s.ump_words == 0, "no UMP from stray data");
  CHECK(!midi2_conv_feed(&s, 0x22), "stray data byte: dropped");
  CHECK(s.ump_words == 0, "no UMP from stray data");

  CHECK(!midi2_conv_feed(&s, 0x90), "Note On status");
  CHECK(!midi2_conv_feed(&s, 0x3C), "note");
  CHECK(midi2_conv_feed(&s, 0x7F), "velocity: complete");
  CHECK(s.ump[0] == 0x20903C7F, "Note On survived stray data intact");
  PASS();
}

/* --- Back-to-back SysEx runs --- */

/* Six complete SysEx messages in one stream. The failure mode worth guarding
 * against is a converter that reuses one output buffer across runs and ends up
 * repeating earlier packets. Every packet here must be distinct and correct. */
void test_sysex_back_to_back_runs(void) {
  TEST("SysEx: six back-to-back runs, each packet distinct");
  midi2_conv_state s;
  static const uint8_t stream[] = {
    0xF0, 0x0A, 0x0B, 0x0C, 0x0D, 0x0F, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0xF7,
    0xF0, 0x4A, 0x4B, 0x4C, 0x4D, 0x4F, 0xF7,
    0xF0, 0x5A, 0x5B, 0x5C, 0x5D, 0xF7,
    0xF0, 0x6A, 0x6B, 0x6C, 0xF7,
    0xF0, 0x7A, 0x7B, 0xF7
  };
  static const uint32_t expect[][2] = {
    { 0x30160A0B, 0x0C0D0F1A },  /* START, 6 bytes */
    { 0x30351B1C, 0x1D1E1F00 },  /* END, 5 bytes   */
    { 0x30054A4B, 0x4C4D4F00 },  /* COMPLETE, 5    */
    { 0x30045A5B, 0x5C5D0000 },  /* COMPLETE, 4    */
    { 0x30036A6B, 0x6C000000 },  /* COMPLETE, 3    */
    { 0x30027A7B, 0x00000000 }   /* COMPLETE, 2    */
  };
  size_t i;
  uint8_t got = 0;
  midi2_conv_init(&s, 0);

  for (i = 0; i < sizeof(stream); i++) {
    if (midi2_conv_feed(&s, stream[i])) {
      CHECK(got < 6, "no more packets than expected");
      CHECK(s.ump_words == 2, "SysEx packet is 2 words");
      CHECK(s.ump[0] == expect[got][0], "word 0 matches");
      CHECK(s.ump[1] == expect[got][1], "word 1 matches");
      got++;
    }
  }
  CHECK(got == 6, "exactly 6 packets produced");
  PASS();
}

/* --- Interspersed messages (M2-104-UM 7.7.1): one byte, two UMPs --- */

/* Drain helper: run a byte stream through the canonical feed/next loop,
 * capturing every message in emission order. Returns the message count. */
static uint8_t drain_stream(midi2_conv_state *s, const uint8_t *bytes,
                            size_t n, uint32_t got[][2], uint8_t cap) {
  uint8_t count = 0;
  size_t i;
  for (i = 0; i < n; i++) {
    if (midi2_conv_feed(s, bytes[i])) {
      do {
        if (count < cap) {
          got[count][0] = s->ump[0];
          got[count][1] = s->ump_words > 1 ? s->ump[1] : 0;
        }
        count++;
      } while (midi2_conv_next(s));
    }
  }
  return count;
}

void test_next_nothing_pending(void) {
  TEST("next: nothing pending -> false, NULL safe");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_next(&s), "fresh state: nothing pending");
  CHECK(s.ump_words == 0, "ump_words zeroed");

  midi2_conv_feed(&s, 0x90);
  midi2_conv_feed(&s, 0x3C);
  CHECK(midi2_conv_feed(&s, 0x7F), "plain note on ready");
  CHECK(!midi2_conv_next(&s), "ordinary byte queues nothing");

  CHECK(!midi2_conv_next(NULL), "NULL state: false");
  PASS();
}

/* Byte vector from AM_MIDI2.0Lib#24. A Real-Time clock landing inside a
 * SysEx must come out AFTER the partial packet holding the bytes that
 * preceded it on the wire -- that ordering is the reason 7.7.1 permits the
 * interleave at all. The first two UMPs match the issue's expectation
 * exactly; the tail differs only in packet partitioning (CONTINUE(6)+END(0)
 * vs END(6)), which the spec leaves free. */
void test_realtime_in_sysex_wire_order(void) {
  TEST("7.7.1: F8 in SysEx -> partial packet, then F8");
  midi2_conv_state s;
  static const uint8_t stream[] = {
    0xF0, 0x0A, 0x0B, 0x0C, 0x0D, 0x0F,
    0xF8,
    0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0xF7
  };
  uint32_t got[8][2];
  uint8_t n;
  midi2_conv_init(&s, 0);

  n = drain_stream(&s, stream, sizeof(stream), got, 8);
  CHECK(n == 4, "exactly 4 messages");
  CHECK(got[0][0] == 0x30150A0B, "START(5) word 0 -- matches #24");
  CHECK(got[0][1] == 0x0C0D0F00, "START(5) word 1 -- matches #24");
  CHECK(got[1][0] == 0x10F80000, "clock after the payload it followed");
  CHECK(got[2][0] == 0x30262A2B, "CONTINUE(6) word 0");
  CHECK(got[2][1] == 0x2C2D2E2F, "CONTINUE(6) word 1");
  CHECK(got[3][0] == 0x30300000, "END(0) closes the message");
  PASS();
}

void test_status_terminates_short_sysex(void) {
  TEST("7.7.1: status in short SysEx -> COMPLETE, then on");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  midi2_conv_feed(&s, 0xF0);
  midi2_conv_feed(&s, 0x11);
  midi2_conv_feed(&s, 0x22);
  CHECK(midi2_conv_feed(&s, 0x90), "interrupting status closes the SysEx");
  CHECK(s.ump_words == 2, "closing packet is 2 words");
  CHECK(s.ump[0] == 0x30021122, "COMPLETE(2) keeps the wire bytes");
  CHECK(s.ump[1] == 0x00000000, "padded with zeros");
  CHECK(!midi2_conv_next(&s), "0x90 waits for data: nothing queued");

  CHECK(!midi2_conv_feed(&s, 0x3C), "note");
  CHECK(midi2_conv_feed(&s, 0x7F), "velocity: the note on survives");
  CHECK(s.ump[0] == 0x20903C7F, "note on intact after the close");
  PASS();
}

void test_status_terminates_streamed_sysex(void) {
  TEST("7.7.1: status after START -> END closes the stream");
  midi2_conv_state s;
  uint8_t i;
  midi2_conv_init(&s, 0);

  midi2_conv_feed(&s, 0xF0);
  for (i = 1; i <= 5; i++)
    midi2_conv_feed(&s, i);          /* 01..05 */
  CHECK(midi2_conv_feed(&s, 0x06), "6th byte: START(6) goes out");
  CHECK(s.ump[0] == 0x30160102, "START(6)");
  midi2_conv_feed(&s, 0x07);
  midi2_conv_feed(&s, 0x08);

  CHECK(midi2_conv_feed(&s, 0x90), "status closes the open stream");
  CHECK(s.ump[0] == 0x30320708, "END(2) carries the buffered bytes");
  CHECK(s.ump[1] == 0x00000000, "padded");
  CHECK(!midi2_conv_next(&s), "nothing queued");

  midi2_conv_feed(&s, 0x3C);
  CHECK(midi2_conv_feed(&s, 0x7F), "note completes");
  CHECK(s.ump[0] == 0x20903C7F, "note on intact");
  PASS();
}

void test_status_terminates_sysex_empty_buffer(void) {
  TEST("7.7.1: status after full packet -> END(0), no dangle");
  midi2_conv_state s;
  uint8_t i;
  midi2_conv_init(&s, 0);

  midi2_conv_feed(&s, 0xF0);
  for (i = 1; i <= 5; i++)
    midi2_conv_feed(&s, 0x10 + i);
  CHECK(midi2_conv_feed(&s, 0x16), "START(6) out, buffer now empty");

  CHECK(midi2_conv_feed(&s, 0x90), "status still closes the stream");
  CHECK(s.ump[0] == 0x30300000, "END(0): START is never left dangling");
  CHECK(s.ump[1] == 0x00000000, "empty payload");
  CHECK(!midi2_conv_next(&s), "nothing queued");
  PASS();
}

/* F0 immediately followed by a status byte: the SysEx has no content and no
 * packet went out, so there is nothing to close -- the interrupting byte's
 * message goes out directly, with nothing queued behind it. */
void test_status_terminates_empty_sysex(void) {
  TEST("7.7.1: status in empty SysEx -> no packet, direct");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  midi2_conv_feed(&s, 0xF0);
  CHECK(midi2_conv_feed(&s, 0xF6), "F6 emitted directly, no packet first");
  CHECK(s.ump[0] == 0x10F60000, "Tune Request immediate");
  CHECK(s.ump_words == 1, "1 word");
  CHECK(!midi2_conv_next(&s), "nothing queued");

  midi2_conv_feed(&s, 0xF0);
  CHECK(!midi2_conv_feed(&s, 0x90), "CV status: nothing to emit yet");
  CHECK(s.ump_words == 0, "no packet from the empty SysEx");
  midi2_conv_feed(&s, 0x3C);
  CHECK(midi2_conv_feed(&s, 0x7F), "note completes");
  CHECK(s.ump[0] == 0x20903C7F, "note on intact");
  PASS();
}

void test_tune_request_terminates_sysex(void) {
  TEST("7.7.1: F6 in SysEx -> COMPLETE, then F6 queued");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  midi2_conv_feed(&s, 0xF0);
  midi2_conv_feed(&s, 0x11);
  CHECK(midi2_conv_feed(&s, 0xF6), "F6 closes the SysEx first");
  CHECK(s.ump[0] == 0x30011100, "COMPLETE(1) with the wire byte");
  CHECK(midi2_conv_next(&s), "F6's own message is queued behind");
  CHECK(s.ump[0] == 0x10F60000, "Tune Request");
  CHECK(s.ump_words == 1, "1 word");
  CHECK(!midi2_conv_next(&s), "queue drained");

  CHECK(!midi2_conv_feed(&s, 0x3C), "F6 cancelled Running Status: orphan");
  PASS();
}

/* --- Init contract --- */

/* init() must leave no field behind, whatever the struct held before: a
 * stale running-status byte would turn the first orphan data byte into a
 * fabricated Channel Voice message. Seed the struct with garbage and prove
 * every field is wiped. */
void test_init_wipes_dirty_state(void) {
  TEST("init: wipes pre-existing garbage in every field");
  midi2_conv_state s;
  uint8_t i;
  memset(&s, 0xFF, sizeof(s));  /* worst case: every byte non-zero */
  midi2_conv_init(&s, 5);

  CHECK(s.group == 5, "group set from argument");
  CHECK(s.running_status == 0, "running_status cleared");
  CHECK(s.data_byte_count == 0, "data_byte_count cleared");
  CHECK(s.data_pos == 0, "data_pos cleared");
  CHECK(s.data[0] == 0 && s.data[1] == 0, "data[] cleared");
  CHECK(s.sysex_len == 0, "sysex_len cleared");
  CHECK(s.in_sysex == false, "in_sysex cleared");
  CHECK(s.sysex_started == false, "sysex_started cleared");
  CHECK(s.ump_words == 0, "ump_words cleared");
  CHECK(s.pending_words == 0, "pending_words cleared");
  CHECK(s.in_feed == false, "in_feed cleared");
  for (i = 0; i < 6; i++)
    CHECK(s.sysex_buf[i] == 0, "sysex_buf cleared");
  PASS();
}

/* Behavioural half of the same contract: after init over dirty memory, data
 * bytes arriving with no status byte must produce nothing at all. */
void test_init_no_fabricated_message(void) {
  TEST("init: data bytes over dirty memory emit nothing");
  midi2_conv_state s;
  memset(&s, 0xFF, sizeof(s));
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0x3C), "first data byte: no message");
  CHECK(s.ump_words == 0, "no UMP fabricated");
  CHECK(!midi2_conv_feed(&s, 0x40), "second data byte: still nothing");
  CHECK(s.ump_words == 0, "no UMP fabricated");
  PASS();
}

/* --- Malformed SysEx framing --- */

/* A stray F7 with no SysEx open must produce nothing. The failure mode worth
 * guarding against is emitting a packet that reuses the previous packet's byte
 * count, which injects data bytes that were never on the wire. */
void test_sysex_stray_f7(void) {
  TEST("SysEx: stray F7 with no F0 -> ignored");
  midi2_conv_state s;
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0xF7), "F7 without F0: no message");
  CHECK(s.ump_words == 0, "no UMP emitted");
  PASS();
}

void test_sysex_double_f7(void) {
  TEST("SysEx: F0 .. F7 F7 -> second F7 ignored");
  midi2_conv_state s;
  uint8_t i;
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0xF0), "F0: start");
  for (i = 0; i < 5; i++)
    CHECK(!midi2_conv_feed(&s, 0x21 + i), "data");
  CHECK(midi2_conv_feed(&s, 0xF7), "first F7: packet ready");
  CHECK(s.ump_words == 2, "2 words");
  CHECK(((s.ump[0] >> 16) & 0xF0) == MIDI2_SYSEX7_COMPLETE, "COMPLETE");
  CHECK(((s.ump[0] >> 16) & 0x0F) == 5, "5 data bytes");

  CHECK(!midi2_conv_feed(&s, 0xF7), "second F7: no message");
  CHECK(s.ump_words == 0, "no phantom packet");
  PASS();
}

/* A second F0 terminates the previous SysEx (7.7.1). The bytes it had
 * accumulated were on the wire, so they go out in the closing packet rather
 * than being dropped; the new message then starts clean, with no stale bytes
 * leaking into it. Feeding data before the restart is what makes this bite. */
void test_sysex_restart_closes_previous(void) {
  TEST("SysEx: F0 data F0 -> previous closed, bytes kept");
  midi2_conv_state s;
  uint8_t i;
  midi2_conv_init(&s, 0);

  CHECK(!midi2_conv_feed(&s, 0xF0), "F0: start");
  CHECK(!midi2_conv_feed(&s, 0x11), "data buffered");
  CHECK(!midi2_conv_feed(&s, 0x12), "data buffered");

  CHECK(midi2_conv_feed(&s, 0xF0), "second F0 closes the first message");
  CHECK(s.ump_words == 2, "closing packet is 2 words");
  CHECK(s.ump[0] == 0x30021112, "COMPLETE(2) carries the wire bytes");
  CHECK(s.ump[1] == 0x00000000, "padded");
  CHECK(!midi2_conv_next(&s), "F0 queues nothing of its own");

  for (i = 0; i < 4; i++)
    CHECK(!midi2_conv_feed(&s, 0x31 + i), "data after restart");
  CHECK(midi2_conv_feed(&s, 0xF7), "F7: packet ready");
  CHECK(s.ump_words == 2, "2 words");
  CHECK(((s.ump[0] >> 16) & 0xF0) == MIDI2_SYSEX7_COMPLETE, "COMPLETE");
  CHECK(((s.ump[0] >> 16) & 0x0F) == 4, "4 data bytes, no stale leak");
  CHECK(((s.ump[0] >> 8) & 0xFF) == 0x31, "first byte is post-restart data");
  PASS();
}

/* --- Main --- */

int main(void) {
  printf("\n=== midi2_conv Unit Tests ===\n\n");

  printf("[Channel Voice]\n");
  test_note_on_3bytes();
  test_running_status();
  test_program_change();
  test_note_off();
  test_note_off_running_status();
  test_poly_pressure();
  test_channel_pressure();
  test_pitch_bend();

  printf("\n[Real-Time]\n");
  test_realtime_mid_message();

  printf("\n[System Common]\n");
  test_system_common_cancels_running();
  test_song_position_pointer();
  test_mtc_quarter_frame();
  test_song_select();

  printf("\n[SysEx]\n");
  test_sysex_short();
  test_sysex_cancels_running_status();
  test_sysex_empty();

  printf("\n[SysEx Streaming]\n");
  test_sysex_exactly_6();
  test_sysex_7_bytes();
  test_sysex_12_bytes();
  test_sysex_30_bytes_ci();
  test_sysex_13_bytes();

  printf("\n[Group]\n");
  test_group_assignment();

  printf("\n[Malformed SysEx]\n");
  test_sysex_stray_f7();
  test_sysex_double_f7();
  test_sysex_restart_closes_previous();
  test_sysex_back_to_back_runs();

  printf("\n[Undefined System Common]\n");
  test_undefined_system_common_no_data_bytes();
  test_undefined_system_common_trailing_data_dropped();

  printf("\n[Interspersed messages (7.7.1)]\n");
  test_next_nothing_pending();
  test_realtime_in_sysex_wire_order();
  test_status_terminates_short_sysex();
  test_status_terminates_streamed_sysex();
  test_status_terminates_sysex_empty_buffer();
  test_status_terminates_empty_sysex();
  test_tune_request_terminates_sysex();

  printf("\n[Edge Cases]\n");
  test_orphan_data_byte();
  test_new_status_cancels_previous();

  printf("\n[Init Contract]\n");
  test_init_wipes_dirty_state();
  test_init_no_fabricated_message();

  printf("\n[NULL Paths]\n");
  test_init_null_safe();
  test_feed_null_safe();
  test_conv_in_feed_cleared_all_paths();

  printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
