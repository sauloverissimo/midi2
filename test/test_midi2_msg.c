/*
 * midi2_msg.h unit tests
 * Compile: gcc -std=c99 -I../src test_midi2_msg.c -o test_midi2_msg && ./test_midi2_msg
 */

#include <stdio.h>
#include <string.h>
#include "midi2_msg.h"

static int passed = 0;
static int failed = 0;

#define TEST(name) printf("  %-50s ", name)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* --- Value Scaling --- */

void test_scale_7to16_zero(void) {
  TEST("scale 7->16: 0 -> 0");
  CHECK(midi2_msg_scale_up_7to16(0) == 0, "expected 0");
  PASS();
}

void test_scale_7to16_max(void) {
  TEST("scale 7->16: 127 -> 65535");
  CHECK(midi2_msg_scale_up_7to16(127) == 65535, "expected 65535");
  PASS();
}

void test_scale_roundtrip_7_16_7(void) {
  TEST("scale roundtrip 7->16->7 (all 128 values)");
  int i;
  for (i = 0; i <= 127; i++) {
    uint16_t up = midi2_msg_scale_up_7to16((uint8_t)i);
    uint8_t down = midi2_msg_scale_down_16to7(up);
    if (down != i) { FAIL("roundtrip mismatch"); return; }
  }
  PASS();
}

void test_scale_roundtrip_7_32_7(void) {
  TEST("scale roundtrip 7->32->7 (all 128 values)");
  int i;
  for (i = 0; i <= 127; i++) {
    uint32_t up = midi2_msg_scale_up_7to32((uint8_t)i);
    uint8_t down = midi2_msg_scale_down_32to7(up);
    if (down != i) { FAIL("roundtrip mismatch"); return; }
  }
  PASS();
}

void test_scale_roundtrip_14_32_14(void) {
  TEST("scale roundtrip 14->32->14 (sample)");
  uint16_t vals[] = {0, 1, 8192, 16383, 100, 5000};
  int i;
  for (i = 0; i < 6; i++) {
    uint32_t up = midi2_msg_scale_up_14to32(vals[i]);
    uint16_t down = midi2_msg_scale_down_32to14(up);
    if (down != vals[i]) { FAIL("roundtrip mismatch"); return; }
  }
  PASS();
}

/* --- Word Count --- */

void test_word_count(void) {
  TEST("word count for all known MTs");
  CHECK(midi2_msg_word_count(0x00) == 1, "MT 0x0");
  CHECK(midi2_msg_word_count(0x01) == 1, "MT 0x1");
  CHECK(midi2_msg_word_count(0x02) == 1, "MT 0x2");
  CHECK(midi2_msg_word_count(0x03) == 2, "MT 0x3");
  CHECK(midi2_msg_word_count(0x04) == 2, "MT 0x4");
  CHECK(midi2_msg_word_count(0x05) == 4, "MT 0x5");
  CHECK(midi2_msg_word_count(0x0D) == 4, "MT 0xD");
  CHECK(midi2_msg_word_count(0x0F) == 4, "MT 0xF");
  PASS();
}

/* --- Channel Voice --- */

void test_note_on(void) {
  TEST("Note On: group=0, ch=0, note=60, vel=0xC000");
  uint32_t w[2];
  midi2_msg_note_on(w, 0, 0, 60, 0xC000, 0);
  CHECK(midi2_msg_get_mt(w) == 0x04, "MT");
  CHECK(midi2_msg_get_group(w) == 0, "group");
  CHECK((midi2_msg_get_status(w) & 0xF0) == 0x90, "status");
  CHECK(midi2_msg_get_channel(w) == 0, "channel");
  CHECK(midi2_msg_get_note(w) == 60, "note");
  CHECK(midi2_msg_get_velocity(w) == 0xC000, "velocity");
  PASS();
}

void test_note_on_group_channel(void) {
  TEST("Note On: group=3, ch=9, note=36, vel=0xFFFF");
  uint32_t w[2];
  midi2_msg_note_on(w, 3, 9, 36, 0xFFFF, 0);
  CHECK(midi2_msg_get_group(w) == 3, "group");
  CHECK(midi2_msg_get_channel(w) == 9, "channel");
  CHECK(midi2_msg_get_note(w) == 36, "note");
  CHECK(midi2_msg_get_velocity(w) == 0xFFFF, "velocity");
  PASS();
}

void test_note_off(void) {
  TEST("Note Off: group=0, ch=0, note=60");
  uint32_t w[2];
  midi2_msg_note_off(w, 0, 0, 60, 0, 0);
  CHECK((midi2_msg_get_status(w) & 0xF0) == 0x80, "status");
  CHECK(midi2_msg_get_note(w) == 60, "note");
  PASS();
}

void test_cc(void) {
  TEST("CC: index=74, value=0x80000000");
  uint32_t w[2];
  midi2_msg_cc(w, 1, 0, 74, 0x80000000);
  CHECK(midi2_msg_get_group(w) == 1, "group");
  CHECK((midi2_msg_get_status(w) & 0xF0) == 0xB0, "status");
  CHECK(midi2_msg_get_note(w) == 74, "index");
  CHECK(w[1] == 0x80000000, "value");
  PASS();
}

void test_program_change_bank(void) {
  TEST("Program Change: prog=5, bank=1/2");
  uint32_t w[2];
  midi2_msg_program(w, 0, 0, 5, true, 1, 2);
  CHECK((midi2_msg_get_status(w) & 0xF0) == 0xC0, "status");
  CHECK(w[1] & (1u << 31), "bank valid");
  CHECK(((w[1] >> 24) & 0x7F) == 5, "program");
  CHECK(((w[1] >> 8) & 0x7F) == 1, "bankMSB");
  CHECK((w[1] & 0x7F) == 2, "bankLSB");
  PASS();
}

void test_program_change_no_bank(void) {
  TEST("Program Change: prog=10, no bank");
  uint32_t w[2];
  midi2_msg_program(w, 0, 0, 10, false, 0, 0);
  CHECK(!(w[1] & (1u << 31)), "bank valid off");
  CHECK(((w[1] >> 24) & 0x7F) == 10, "program");
  PASS();
}

void test_pitch_bend(void) {
  TEST("Pitch Bend: center=0x80000000");
  uint32_t w[2];
  midi2_msg_pitch_bend(w, 0, 0, 0x80000000);
  CHECK((midi2_msg_get_status(w) & 0xF0) == 0xE0, "status");
  CHECK(w[1] == 0x80000000, "value");
  PASS();
}

void test_rpn(void) {
  TEST("RPN: bank=0, index=0");
  uint32_t w[2];
  midi2_msg_rpn(w, 0, 0, 0, 0, 0x02000000);
  CHECK((midi2_msg_get_status(w) & 0xF0) == 0x20, "status");
  CHECK(w[1] == 0x02000000, "value");
  PASS();
}

/* --- Per-Note Expression --- */

void test_per_note_pb(void) {
  TEST("Per-Note Pitch Bend: note=60");
  uint32_t w[2];
  midi2_msg_per_note_pb(w, 0, 0, 60, 0x80000000);
  CHECK((midi2_msg_get_status(w) & 0xF0) == 0x60, "status");
  CHECK(midi2_msg_get_note(w) == 60, "note");
  CHECK(w[1] == 0x80000000, "value");
  PASS();
}

void test_per_note_cc(void) {
  TEST("Per-Note CC: note=48, index=7");
  uint32_t w[2];
  midi2_msg_per_note_cc(w, 0, 0, 48, 7, 0xFFFFFFFF);
  CHECK((midi2_msg_get_status(w) & 0xF0) == 0x10, "status");
  CHECK(midi2_msg_get_note(w) == 48, "note");
  CHECK((w[0] & 0xFF) == 7, "index");
  CHECK(w[1] == 0xFFFFFFFF, "value");
  PASS();
}

/* --- System Messages --- */

void test_system_timing_clock(void) {
  TEST("System: Timing Clock group=2");
  uint32_t w = midi2_msg_system(2, 0xF8);
  CHECK(((w >> 28) & 0x0F) == 0x01, "MT");
  CHECK(((w >> 24) & 0x0F) == 2, "group");
  CHECK(((w >> 16) & 0xFF) == 0xF8, "status");
  PASS();
}

void test_system_spp(void) {
  TEST("System: Song Position Pointer");
  uint32_t w = midi2_msg_system_3byte(0, 0xF2, 0x40, 0x20);
  CHECK(((w >> 16) & 0xFF) == 0xF2, "status");
  CHECK(((w >> 8) & 0xFF) == 0x40, "data1");
  CHECK((w & 0xFF) == 0x20, "data2");
  PASS();
}

/* --- Flex Data --- */

void test_tempo(void) {
  TEST("Flex: Tempo 120 BPM");
  uint32_t w[4];
  midi2_msg_tempo(w, 0, 5000000);
  CHECK(midi2_msg_get_mt(w) == 0x0D, "MT");
  CHECK(((w[0] >> 20) & 0x03) == 0x01, "address=group");
  CHECK(w[1] == 5000000, "10ns/qn");
  PASS();
}

void test_time_sig(void) {
  TEST("Flex: Time Signature 6/8");
  uint32_t w[4];
  midi2_msg_time_sig(w, 0, 6, 3);
  CHECK(((w[1] >> 24) & 0xFF) == 6, "numerator");
  CHECK(((w[1] >> 16) & 0xFF) == 3, "denominator");
  PASS();
}

void test_key_sig_major(void) {
  TEST("Flex: Key Sig D major (2 sharps)");
  uint32_t w[4];
  midi2_msg_key_sig(w, 0, 2, false);
  CHECK(((w[1] >> 28) & 0x0F) == 2, "sharpsFlats");
  CHECK(((w[1] >> 22) & 0x03) == 0, "keyType=major");
  PASS();
}

void test_key_sig_minor(void) {
  TEST("Flex: Key Sig Bb minor (-5)");
  uint32_t w[4];
  midi2_msg_key_sig(w, 0, -5, true);
  CHECK(((w[1] >> 28) & 0x0F) == 0x0B, "sharpsFlats=-5");
  CHECK(((w[1] >> 22) & 0x03) == 1, "keyType=minor");
  PASS();
}

/* --- SysEx7 --- */

void test_sysex7_3bytes(void) {
  TEST("SysEx7: Complete 3 bytes");
  uint32_t w[2];
  uint8_t data[] = {0x7E, 0x7F, 0x09};
  midi2_msg_sysex7_packet(w, 0, MIDI2_SYSEX7_COMPLETE, data, 3);
  CHECK(midi2_msg_get_mt(w) == 0x03, "MT");
  CHECK(((w[0] >> 16) & 0x0F) == 3, "numBytes");
  CHECK(((w[0] >> 8) & 0xFF) == 0x7E, "data[0]");
  CHECK((w[0] & 0xFF) == 0x7F, "data[1]");
  CHECK(((w[1] >> 24) & 0xFF) == 0x09, "data[2]");
  PASS();
}

void test_sysex7_6bytes(void) {
  TEST("SysEx7: 6 bytes packed");
  uint32_t w[2];
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  midi2_msg_sysex7_packet(w, 0, MIDI2_SYSEX7_COMPLETE, data, 6);
  CHECK(((w[0] >> 16) & 0x0F) == 6, "numBytes");
  CHECK(((w[0] >> 8) & 0xFF) == 0x01, "data[0]");
  CHECK((w[0] & 0xFF) == 0x02, "data[1]");
  CHECK(((w[1] >> 24) & 0xFF) == 0x03, "data[2]");
  CHECK(((w[1] >> 16) & 0xFF) == 0x04, "data[3]");
  CHECK(((w[1] >> 8) & 0xFF) == 0x05, "data[4]");
  CHECK((w[1] & 0xFF) == 0x06, "data[5]");
  PASS();
}

/* --- JR Timestamps --- */

void test_jr_clock(void) {
  TEST("JR Clock: group=0, timestamp=1000");
  uint32_t w = midi2_msg_jr_clock(0, 1000);
  CHECK(((w >> 28) & 0x0F) == 0x00, "MT=Utility");
  CHECK(((w >> 24) & 0x0F) == 0, "group=0");
  CHECK(((w >> 16) & 0xFF) == 0x01, "status=JR Clock");
  CHECK((w & 0xFFFF) == 1000, "timestamp=1000");
  PASS();
}

void test_jr_timestamp(void) {
  TEST("JR Timestamp: group=2, timestamp=0xFFFF");
  uint32_t w = midi2_msg_jr_timestamp(2, 0xFFFF);
  CHECK(((w >> 24) & 0x0F) == 2, "group=2");
  CHECK(((w >> 16) & 0xFF) == 0x02, "status=JR Timestamp");
  CHECK((w & 0xFFFF) == 0xFFFF, "timestamp=max");
  PASS();
}

/* --- Stream Messages --- */

void test_stream_endpoint_discovery(void) {
  TEST("Stream: Endpoint Discovery");
  uint32_t w[4];
  midi2_msg_stream_endpoint_discovery(w, 1, 1, 0x1F);
  CHECK(midi2_msg_get_mt(w) == 0x0F, "MT=Stream");
  CHECK(((w[0] >> 16) & 0x3FF) == 0x000, "status=discovery");
  CHECK(((w[0] >> 8) & 0xFF) == 1, "ump major=1");
  CHECK((w[0] & 0xFF) == 1, "ump minor=1");
  CHECK(w[1] == 0x1F, "filter=all");
  PASS();
}

void test_stream_endpoint_info(void) {
  TEST("Stream: Endpoint Info");
  uint32_t w[4];
  midi2_msg_stream_endpoint_info(w, 1, 1, true, 2, true, true, false, false);
  CHECK(midi2_msg_get_mt(w) == 0x0F, "MT=Stream");
  CHECK(((w[0] >> 16) & 0x3FF) == 0x001, "status=info");
  CHECK(w[1] & (1u << 31), "static FB");
  CHECK(((w[1] >> 24) & 0x7F) == 2, "num_fb=2");
  CHECK(w[1] & (1u << 9), "MIDI 2.0 proto");
  CHECK(w[1] & (1u << 8), "MIDI 1.0 proto");
  PASS();
}

void test_stream_device_identity(void) {
  TEST("Stream: Device Identity");
  uint32_t w[4];
  midi2_msg_stream_device_identity(w, 0x00AABB, 0x1234, 0x5678, 0xDEADBEEF);
  CHECK(((w[0] >> 16) & 0x3FF) == 0x002, "status=device identity");
  CHECK(w[3] == 0xDEADBEEF, "version");
  PASS();
}

void test_stream_config_request(void) {
  TEST("Stream: Config Request MIDI 2.0");
  uint32_t w[4];
  midi2_msg_stream_config_request(w, 0x02);
  CHECK(((w[0] >> 16) & 0x3FF) == 0x005, "status=config request");
  CHECK(((w[0] >> 8) & 0xFF) == 0x02, "protocol=MIDI2");
  PASS();
}

void test_stream_config_notify(void) {
  TEST("Stream: Config Notify MIDI 1.0");
  uint32_t w[4];
  midi2_msg_stream_config_notify(w, 0x01);
  CHECK(((w[0] >> 16) & 0x3FF) == 0x006, "status=config notify");
  CHECK(((w[0] >> 8) & 0xFF) == 0x01, "protocol=MIDI1");
  PASS();
}

void test_stream_fb_discovery(void) {
  TEST("Stream: FB Discovery all");
  uint32_t w[4];
  midi2_msg_stream_fb_discovery(w, 0xFF, 0x03);
  CHECK(((w[0] >> 16) & 0x3FF) == 0x010, "status=FB discovery");
  CHECK(((w[0] >> 8) & 0xFF) == 0xFF, "fb=all");
  CHECK((w[0] & 0xFF) == 0x03, "filter=info+name");
  PASS();
}

void test_stream_fb_info(void) {
  TEST("Stream: FB Info bidirectional");
  uint32_t w[4];
  midi2_msg_stream_fb_info(w, true, 0, 0x02, 0, 4, 2, false, 0x02);
  CHECK(((w[0] >> 16) & 0x3FF) == 0x011, "status=FB info");
  CHECK(w[0] & (1u << 15), "active");
  CHECK(((w[0] >> 8) & 0x7F) == 0, "fb_num=0");
  CHECK((w[0] & 0x03) == 0x02, "direction=bidir");
  CHECK(((w[1] >> 24) & 0x0F) == 0, "first_group=0");
  CHECK(((w[1] >> 16) & 0x0F) == 4, "num_groups=4");
  CHECK((w[1] & 0x03) == 0x02, "protocol=MIDI2");
  PASS();
}

void test_stream_clip(void) {
  TEST("Stream: Start/End of Clip");
  uint32_t w[4];
  midi2_msg_stream_start_of_clip(w);
  CHECK(((w[0] >> 16) & 0x3FF) == 0x020, "start of clip");
  midi2_msg_stream_end_of_clip(w);
  CHECK(((w[0] >> 16) & 0x3FF) == 0x021, "end of clip");
  PASS();
}

/* --- SysEx8 --- */

void test_sysex8_complete(void) {
  TEST("SysEx8: Complete 4 bytes, stream_id=1");
  uint32_t w[4];
  uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
  midi2_msg_sysex8_packet(w, 0, MIDI2_SYSEX8_COMPLETE, 1, data, 4);
  CHECK(midi2_msg_get_mt(w) == 0x05, "MT=Data128");
  CHECK(((w[0] >> 16) & 0x0F) == 5, "numBytes=5 (stream_id + 4 data)");
  CHECK(((w[0] >> 8) & 0xFF) == 1, "stream_id=1");
  CHECK((w[0] & 0xFF) == 0xAA, "data[0]");
  CHECK(((w[1] >> 24) & 0xFF) == 0xBB, "data[1]");
  CHECK(((w[1] >> 16) & 0xFF) == 0xCC, "data[2]");
  CHECK(((w[1] >> 8) & 0xFF) == 0xDD, "data[3]");
  PASS();
}

void test_sysex8_13bytes(void) {
  TEST("SysEx8: 13 bytes (max per packet)");
  uint32_t w[4];
  uint8_t data[13];
  int i;
  for (i = 0; i < 13; i++) data[i] = (uint8_t)(i + 1);
  midi2_msg_sysex8_packet(w, 0, MIDI2_SYSEX8_COMPLETE, 0, data, 13);
  CHECK(((w[0] >> 16) & 0x0F) == 14, "numBytes=14 (stream_id + 13 data)");
  CHECK((w[0] & 0xFF) == 0x01, "data[0]");
  CHECK((w[3] & 0xFF) == 0x0D, "data[12]");
  PASS();
}

/* --- MIDI 1.0 Byte Conversion --- */

void test_from_midi1(void) {
  TEST("from_midi1: Note On ch=0 note=60 vel=127");
  uint32_t w = midi2_msg_from_midi1(0, 0x90, 60, 127);
  CHECK(((w >> 28) & 0x0F) == 0x02, "MT=MIDI1 CV");
  CHECK(((w >> 16) & 0xFF) == 0x90, "status=0x90");
  CHECK(((w >> 8) & 0x7F) == 60, "data1=60");
  CHECK((w & 0x7F) == 127, "data2=127");
  PASS();
}

/* --- Main --- */

int main(void) {
  printf("\n=== midi2_msg.h Unit Tests ===\n\n");

  printf("[Value Scaling]\n");
  test_scale_7to16_zero();
  test_scale_7to16_max();
  test_scale_roundtrip_7_16_7();
  test_scale_roundtrip_7_32_7();
  test_scale_roundtrip_14_32_14();

  printf("\n[Word Count]\n");
  test_word_count();

  printf("\n[Channel Voice]\n");
  test_note_on();
  test_note_on_group_channel();
  test_note_off();
  test_cc();
  test_program_change_bank();
  test_program_change_no_bank();
  test_pitch_bend();
  test_rpn();

  printf("\n[Per-Note Expression]\n");
  test_per_note_pb();
  test_per_note_cc();

  printf("\n[System Messages]\n");
  test_system_timing_clock();
  test_system_spp();

  printf("\n[Flex Data]\n");
  test_tempo();
  test_time_sig();
  test_key_sig_major();
  test_key_sig_minor();

  printf("\n[SysEx7]\n");
  test_sysex7_3bytes();
  test_sysex7_6bytes();

  printf("\n[JR Timestamps]\n");
  test_jr_clock();
  test_jr_timestamp();

  printf("\n[Stream Messages]\n");
  test_stream_endpoint_discovery();
  test_stream_endpoint_info();
  test_stream_device_identity();
  test_stream_config_request();
  test_stream_config_notify();
  test_stream_fb_discovery();
  test_stream_fb_info();
  test_stream_clip();

  printf("\n[SysEx8]\n");
  test_sysex8_complete();
  test_sysex8_13bytes();

  printf("\n[MIDI 1.0 Conversion]\n");
  test_from_midi1();

  printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
