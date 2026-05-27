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
  TEST("word count for all 16 MTs per UMP 1.1.2 sec 2.1.4");
  /* Defined MTs */
  CHECK(midi2_msg_word_count(0x00) == 1, "MT 0x0 Utility");
  CHECK(midi2_msg_word_count(0x01) == 1, "MT 0x1 System RT/Common");
  CHECK(midi2_msg_word_count(0x02) == 1, "MT 0x2 MIDI 1.0 CV");
  CHECK(midi2_msg_word_count(0x03) == 2, "MT 0x3 Data 64 (SysEx7)");
  CHECK(midi2_msg_word_count(0x04) == 2, "MT 0x4 MIDI 2.0 CV");
  CHECK(midi2_msg_word_count(0x05) == 4, "MT 0x5 Data 128");
  CHECK(midi2_msg_word_count(0x0D) == 4, "MT 0xD Flex Data");
  CHECK(midi2_msg_word_count(0x0F) == 4, "MT 0xF UMP Stream");
  /* Reserved MTs (spec-allocated sizes, future-proof against unknown streams) */
  CHECK(midi2_msg_word_count(0x06) == 1, "MT 0x6 reserved 32-bit");
  CHECK(midi2_msg_word_count(0x07) == 1, "MT 0x7 reserved 32-bit");
  CHECK(midi2_msg_word_count(0x08) == 2, "MT 0x8 reserved 64-bit");
  CHECK(midi2_msg_word_count(0x09) == 2, "MT 0x9 reserved 64-bit");
  CHECK(midi2_msg_word_count(0x0A) == 2, "MT 0xA reserved 64-bit");
  CHECK(midi2_msg_word_count(0x0B) == 3, "MT 0xB reserved 96-bit");
  CHECK(midi2_msg_word_count(0x0C) == 3, "MT 0xC reserved 96-bit");
  CHECK(midi2_msg_word_count(0x0E) == 4, "MT 0xE reserved 128-bit");
  PASS();
}

/* --- Channel Voice --- */

void test_note_on(void) {
  TEST("Note On: group=0, ch=0, note=60, vel=0xC000");
  uint32_t w[2];
  midi2_msg_note_on(w, 0, 0, 60, 0xC000, 0, 0);
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
  midi2_msg_note_on(w, 3, 9, 36, 0xFFFF, 0, 0);
  CHECK(midi2_msg_get_group(w) == 3, "group");
  CHECK(midi2_msg_get_channel(w) == 9, "channel");
  CHECK(midi2_msg_get_note(w) == 36, "note");
  CHECK(midi2_msg_get_velocity(w) == 0xFFFF, "velocity");
  PASS();
}

void test_note_off(void) {
  TEST("Note Off: group=0, ch=0, note=60");
  uint32_t w[2];
  midi2_msg_note_off(w, 0, 0, 60, 0, 0, 0);
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
  TEST("Program Change: prog=5, bank=1/2, bank_valid in byte 4 LSB");
  uint32_t w[2];
  midi2_msg_program(w, 0, 0, 5, true, 1, 2);
  CHECK((midi2_msg_get_status(w) & 0xF0) == 0xC0, "status");
  /* bank_valid (B) is byte 4 option flags bit 0, NOT byte 5 MSB. */
  CHECK((w[0] & 0x01) != 0, "bank valid bit set in byte 4 option flags");
  CHECK((w[1] & (1u << 31)) == 0, "byte 5 MSB is reserved (zero), not bank_valid");
  CHECK(((w[1] >> 24) & 0x7F) == 5, "program");
  CHECK(((w[1] >> 8) & 0x7F) == 1, "bankMSB");
  CHECK((w[1] & 0x7F) == 2, "bankLSB");
  PASS();
}

void test_program_change_no_bank(void) {
  TEST("Program Change: prog=10, no bank, byte 4 LSB clear");
  uint32_t w[2];
  midi2_msg_program(w, 0, 0, 10, false, 0, 0);
  CHECK((w[0] & 0x01) == 0, "bank valid bit clear in byte 4");
  CHECK((w[1] & (1u << 31)) == 0, "byte 5 MSB reserved/zero");
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

/* --- System Real-Time + System Common named wrappers (v0.3.0) --- */

void test_system_wrapper_tune_request(void) {
  TEST("system_tune_request: status 0xF6, group preserved");
  uint32_t w = midi2_msg_system_tune_request(2);
  CHECK(((w >> 28) & 0x0F) == 0x01, "MT=System");
  CHECK(((w >> 24) & 0x0F) == 2, "group");
  CHECK(((w >> 16) & 0xFF) == 0xF6, "status=Tune Request");
  PASS();
}

void test_system_wrapper_timing_clock(void) {
  TEST("system_timing_clock: status 0xF8");
  uint32_t w = midi2_msg_system_timing_clock(0);
  CHECK(((w >> 16) & 0xFF) == 0xF8, "status=Timing Clock");
  PASS();
}

void test_system_wrapper_start(void) {
  TEST("system_start: status 0xFA");
  uint32_t w = midi2_msg_system_start(0);
  CHECK(((w >> 16) & 0xFF) == 0xFA, "status=Start");
  PASS();
}

void test_system_wrapper_continue(void) {
  TEST("system_continue: status 0xFB");
  uint32_t w = midi2_msg_system_continue(0);
  CHECK(((w >> 16) & 0xFF) == 0xFB, "status=Continue");
  PASS();
}

void test_system_wrapper_stop(void) {
  TEST("system_stop: status 0xFC");
  uint32_t w = midi2_msg_system_stop(0);
  CHECK(((w >> 16) & 0xFF) == 0xFC, "status=Stop");
  PASS();
}

void test_system_wrapper_active_sensing(void) {
  TEST("system_active_sensing: status 0xFE");
  uint32_t w = midi2_msg_system_active_sensing(0);
  CHECK(((w >> 16) & 0xFF) == 0xFE, "status=Active Sensing");
  PASS();
}

void test_system_wrapper_reset(void) {
  TEST("system_reset: status 0xFF");
  uint32_t w = midi2_msg_system_reset(0);
  CHECK(((w >> 16) & 0xFF) == 0xFF, "status=System Reset");
  PASS();
}

void test_system_wrapper_mtc(void) {
  TEST("system_mtc: status 0xF1, time_code masked to 7 bits");
  uint32_t w = midi2_msg_system_mtc(0, 0x42);
  CHECK(((w >> 16) & 0xFF) == 0xF1, "status=MTC QF");
  CHECK(((w >> 8) & 0x7F) == 0x42, "time code value");
  PASS();
}

void test_system_wrapper_song_select(void) {
  TEST("system_song_select: status 0xF3, song masked to 7 bits");
  uint32_t w = midi2_msg_system_song_select(0, 5);
  CHECK(((w >> 16) & 0xFF) == 0xF3, "status=Song Select");
  CHECK(((w >> 8) & 0x7F) == 5, "song number");
  PASS();
}

void test_system_wrapper_song_position(void) {
  TEST("system_song_position: status 0xF2, 14-bit split LSB/MSB");
  uint32_t w = midi2_msg_system_song_position(0, 0x2040);
  CHECK(((w >> 16) & 0xFF) == 0xF2, "status=SPP");
  CHECK(((w >> 8) & 0x7F) == 0x40, "LSB (low 7 bits of 0x2040)");
  CHECK((w & 0x7F) == 0x40, "MSB (high 7 bits of 0x2040 = 0x40)");
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
  TEST("Flex: Time Signature 6/8 with 8 thirty-seconds per 24 MIDI clocks");
  uint32_t w[4];
  midi2_msg_time_sig(w, 0, 6, 3, 8);
  CHECK(((w[1] >> 24) & 0xFF) == 6, "numerator");
  CHECK(((w[1] >> 16) & 0xFF) == 3, "denominator");
  CHECK(((w[1] >> 8) & 0xFF) == 8, "num_32nd_notes (SMF compat field)");
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
  TEST("JR Clock: Groupless, timestamp=1000");
  uint32_t w = midi2_msg_jr_clock(1000);
  CHECK(((w >> 28) & 0x0F) == 0x00, "MT=Utility");
  CHECK(((w >> 24) & 0x0F) == 0, "group field reserved/zero (spec v1.1.2 Groupless)");
  CHECK(((w >> 20) & 0x0F) == 0x01, "status=JR Clock");
  CHECK((w & 0xFFFF) == 1000, "timestamp=1000");
  PASS();
}

void test_jr_timestamp(void) {
  TEST("JR Timestamp: Groupless, timestamp=0xFFFF");
  uint32_t w = midi2_msg_jr_timestamp(0xFFFF);
  CHECK(((w >> 24) & 0x0F) == 0, "group field reserved/zero (spec v1.1.2 Groupless)");
  CHECK(((w >> 20) & 0x0F) == 0x02, "status=JR Timestamp");
  CHECK((w & 0xFFFF) == 0xFFFF, "timestamp=max");
  PASS();
}

void test_noop(void) {
  TEST("NOOP: all-zero payload, status 0x0");
  uint32_t w = midi2_msg_noop();
  CHECK(((w >> 28) & 0x0F) == 0x00, "MT=Utility");
  CHECK(((w >> 24) & 0x0F) == 0, "group field reserved/zero");
  CHECK(((w >> 20) & 0x0F) == 0x00, "status=NOOP (0x0)");
  CHECK((w & 0x000FFFFF) == 0, "data 20 bits all zero");
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
  midi2_msg_stream_config_request(w, 0x02, /*rx_jr*/ false, /*tx_jr*/ false);
  CHECK(((w[0] >> 16) & 0x3FF) == 0x005, "status=config request");
  CHECK(((w[0] >> 8) & 0xFF) == 0x02, "protocol=MIDI2");
  CHECK((w[0] & 0x03) == 0, "JR bits clear");
  PASS();
}

void test_stream_config_request_jr_bits(void) {
  TEST("Stream: Config Request asks for TX JR enable");
  uint32_t w[4];
  midi2_msg_stream_config_request(w, 0x02, /*rx_jr*/ true, /*tx_jr*/ true);
  CHECK((w[0] & 0x03) == 0x03, "both JR bits set");
  PASS();
}

void test_stream_config_notify(void) {
  TEST("Stream: Config Notify MIDI 1.0");
  uint32_t w[4];
  midi2_msg_stream_config_notify(w, 0x01, /*rx_jr*/ false, /*tx_jr*/ false);
  CHECK(((w[0] >> 16) & 0x3FF) == 0x006, "status=config notify");
  CHECK(((w[0] >> 8) & 0xFF) == 0x01, "protocol=MIDI1");
  CHECK((w[0] & 0x03) == 0x00, "JR bits clear");
  PASS();
}

void test_stream_config_notify_jr_bits(void) {
  TEST("Stream: Config Notify MIDI 2.0 with JR TX enabled");
  uint32_t w[4];
  midi2_msg_stream_config_notify(w, 0x02, /*rx_jr*/ false, /*tx_jr*/ true);
  CHECK(((w[0] >> 8) & 0xFF) == 0x02, "protocol=MIDI2");
  CHECK((w[0] & 0x01) != 0, "tx_jr_enable bit set");
  CHECK((w[0] & 0x02) == 0, "rx_jr_enable bit clear");

  midi2_msg_stream_config_notify(w, 0x02, /*rx_jr*/ true, /*tx_jr*/ true);
  CHECK((w[0] & 0x03) == 0x03, "both JR bits set");
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
  TEST("Stream: FB Info bidirectional, no SysEx8");
  uint32_t w[4];
  midi2_msg_stream_fb_info(w, true, 0, /*direction*/ 0x02, /*ui_hint*/ 0x00,
                           0, 4, 2, /*max_sysex8_streams*/ 0, 0x02);
  CHECK(((w[0] >> 16) & 0x3FF) == 0x011, "status=FB info");
  CHECK(w[0] & (1u << 15), "active");
  CHECK(((w[0] >> 8) & 0x7F) == 0, "fb_num=0");
  CHECK((w[0] & 0x03) == 0x02, "direction=bidir");
  CHECK(((w[0] >> 4) & 0x03) == 0x00, "ui_hint=undeclared");
  CHECK(((w[1] >> 24) & 0x0F) == 0, "first_group=0");
  CHECK(((w[1] >> 16) & 0x0F) == 4, "num_groups=4");
  CHECK(((w[1] >> 2) & 0x3F) == 0, "max_sysex8_streams=0");
  CHECK((w[1] & 0x03) == 0x02, "protocol=MIDI2");
  PASS();
}

void test_stream_fb_info_ui_hint(void) {
  TEST("Stream: FB Info with UI Hint Sender+Receiver");
  uint32_t w[4];
  midi2_msg_stream_fb_info(w, true, 0, /*direction*/ 0x03, /*ui_hint*/ 0x03,
                           0, 1, 2, 0, 0x02);
  CHECK((w[0] & 0x03) == 0x03, "direction=bidirectional");
  CHECK(((w[0] >> 4) & 0x03) == 0x03, "ui_hint=Sender+Receiver");

  // Validate ui_hint and direction live in independent bit lanes.
  midi2_msg_stream_fb_info(w, true, 0, /*direction*/ 0x01, /*ui_hint*/ 0x02,
                           0, 1, 2, 0, 0x02);
  CHECK((w[0] & 0x03) == 0x01, "direction=Receiver");
  CHECK(((w[0] >> 4) & 0x03) == 0x02, "ui_hint=Sender");
  PASS();
}

void test_stream_fb_info_sysex8_streams(void) {
  TEST("Stream: FB Info max_sysex8_streams full range");
  uint32_t w[4];
  midi2_msg_stream_fb_info(w, true, 0, 0x02, 0x03, 0, 1, 2,
                           /*max_sysex8_streams*/ 0x3F, 0x02);
  CHECK(((w[1] >> 2) & 0x3F) == 0x3F, "max=63");

  midi2_msg_stream_fb_info(w, true, 0, 0x02, 0x03, 0, 1, 2,
                           /*max_sysex8_streams*/ 1, 0x02);
  CHECK(((w[1] >> 2) & 0x3F) == 1, "max=1");
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

/* --- Delta Clockstamp (new) --- */

void test_dctpq(void) {
  TEST("DCTPQ: 480 ticks per quarter note");
  uint32_t w = midi2_msg_dctpq(480);
  CHECK(((w >> 28) & 0x0F) == 0x00, "MT=Utility");
  CHECK(((w >> 20) & 0x0F) == 0x03, "status=DCTPQ");
  CHECK((w & 0xFFFF) == 480, "tpq=480");
  PASS();
}

void test_delta_clockstamp(void) {
  TEST("Delta Clockstamp: 100000 ticks");
  uint32_t w = midi2_msg_delta_clockstamp(100000);
  CHECK(((w >> 28) & 0x0F) == 0x00, "MT=Utility");
  CHECK(((w >> 20) & 0x0F) == 0x04, "status nibble=DC");
  CHECK((w & 0x000FFFFF) == 100000, "ticks=100000");
  PASS();
}

void test_delta_clockstamp_max(void) {
  TEST("Delta Clockstamp: max 20-bit (1048575)");
  uint32_t w = midi2_msg_delta_clockstamp(0xFFFFF);
  CHECK((w & 0x000FFFFF) == 0xFFFFF, "ticks=max");
  PASS();
}

void test_delta_clockstamp_overflow(void) {
  TEST("Delta Clockstamp: overflow masked to 20 bits");
  uint32_t w = midi2_msg_delta_clockstamp(0x1FFFFF);
  CHECK((w & 0x000FFFFF) == 0xFFFFF, "masked to 20 bits");
  PASS();
}

/* --- Registered/Assignable Per-Note Controllers (new) --- */

void test_reg_per_note_ctrl(void) {
  TEST("Reg Per-Note Ctrl: note=60, idx=3, val=0x80000000");
  uint32_t w[2];
  midi2_msg_reg_per_note_ctrl(w, 2, 5, 60, 3, 0x80000000);
  CHECK(midi2_msg_get_mt(w) == 0x04, "MT=CV2");
  CHECK(midi2_msg_get_group(w) == 2, "group=2");
  CHECK((midi2_msg_get_status(w) & 0xF0) == 0x00, "status=0x00");
  CHECK(midi2_msg_get_channel(w) == 5, "channel=5");
  CHECK(midi2_msg_get_note(w) == 60, "note=60");
  CHECK((w[0] & 0xFF) == 3, "index=3");
  CHECK(w[1] == 0x80000000, "value");
  PASS();
}

void test_asn_per_note_ctrl(void) {
  TEST("Asn Per-Note Ctrl: note=48, idx=7, val=0x40000000");
  uint32_t w[2];
  midi2_msg_asn_per_note_ctrl(w, 0, 0, 48, 7, 0x40000000);
  CHECK((midi2_msg_get_status(w) & 0xF0) == 0x10, "status=0x10");
  CHECK(midi2_msg_get_note(w) == 48, "note=48");
  CHECK((w[0] & 0xFF) == 7, "index=7");
  CHECK(w[1] == 0x40000000, "value");
  PASS();
}

/* --- Relative RPN/NRPN (new) --- */

void test_rel_rpn(void) {
  TEST("Relative RPN: bank=0, idx=7, val=0xFFFFFF00");
  uint32_t w[2];
  midi2_msg_rel_rpn(w, 0, 0, 0, 7, 0xFFFFFF00);
  CHECK(midi2_msg_get_mt(w) == 0x04, "MT=CV2");
  CHECK((midi2_msg_get_status(w) & 0xF0) == 0x40, "status=0x40");
  CHECK(midi2_msg_get_note(w) == 0, "bank=0");
  CHECK((w[0] & 0xFF) == 7, "index=7");
  CHECK(w[1] == 0xFFFFFF00, "value (negative relative)");
  PASS();
}

void test_rel_nrpn(void) {
  TEST("Relative NRPN: bank=1, idx=2, val=0x00000100");
  uint32_t w[2];
  midi2_msg_rel_nrpn(w, 0, 0, 1, 2, 0x00000100);
  CHECK((midi2_msg_get_status(w) & 0xF0) == 0x50, "status=0x50");
  CHECK(w[1] == 0x00000100, "value (positive relative)");
  PASS();
}

/* --- Mixed Data Set (new) --- */

void test_mds_header(void) {
  TEST("MDS Header: mds_id=1, 256 bytes, chunk 1/3");
  uint32_t w[4];
  midi2_msg_mds_header(w, 0, 1, 256, 3, 1, 0x007D, 0xFFFF, 0x0001, 0x0002);
  CHECK(midi2_msg_get_mt(w) == 0x05, "MT=Data128");
  CHECK(((w[0] >> 20) & 0x0F) == 0x08, "status nibble=8 (header)");
  CHECK(((w[0] >> 16) & 0x0F) == 1, "mds_id=1");
  CHECK((w[0] & 0xFFFF) == 256, "num_bytes=256");
  CHECK(((w[1] >> 16) & 0xFFFF) == 3, "num_chunks=3");
  CHECK((w[1] & 0xFFFF) == 1, "this_chunk=1");
  CHECK(((w[2] >> 16) & 0xFFFF) == 0x007D, "mfr_id=0x007D");
  CHECK((w[2] & 0xFFFF) == 0xFFFF, "device_id=all");
  CHECK(((w[3] >> 16) & 0xFFFF) == 0x0001, "sub_id1");
  CHECK((w[3] & 0xFFFF) == 0x0002, "sub_id2");
  PASS();
}

void test_mds_payload(void) {
  TEST("MDS Payload: 14 bytes sequential");
  uint32_t w[4];
  uint8_t data[14];
  int i;
  for (i = 0; i < 14; i++) data[i] = (uint8_t)(0xA0 + i);
  midi2_msg_mds_payload(w, 0, 1, data, 14);
  CHECK(midi2_msg_get_mt(w) == 0x05, "MT=Data128");
  CHECK(((w[0] >> 20) & 0x0F) == 0x09, "status nibble=9 (payload)");
  CHECK(((w[0] >> 16) & 0x0F) == 1, "mds_id=1");
  /* data[0] at w[0][15:8], data[1] at w[0][7:0] */
  CHECK(((w[0] >> 8) & 0xFF) == 0xA0, "data[0]");
  CHECK((w[0] & 0xFF) == 0xA1, "data[1]");
  /* data[2] at w[1][31:24] */
  CHECK(((w[1] >> 24) & 0xFF) == 0xA2, "data[2]");
  /* data[13] at w[3][7:0] */
  CHECK((w[3] & 0xFF) == 0xAD, "data[13]");
  PASS();
}

/* --- Metronome (new) --- */

void test_metronome(void) {
  TEST("Flex: Metronome 4/4, 24 clocks, accent every 4th");
  uint32_t w[4];
  midi2_msg_metronome(w, 0, 24, 4, 0, 0, 2, 0);
  CHECK(midi2_msg_get_mt(w) == 0x0D, "MT=Flex");
  CHECK((w[0] & 0xFF) == 0x02, "status=metronome");
  CHECK(((w[1] >> 24) & 0xFF) == 24, "primary_clicks=24");
  CHECK(((w[1] >> 16) & 0xFF) == 4, "accent_1=4");
  CHECK(((w[1] >> 8) & 0xFF) == 0, "accent_2=0");
  CHECK((w[1] & 0xFF) == 0, "accent_3=0");
  CHECK(((w[2] >> 24) & 0xFF) == 2, "subdiv_1=2");
  CHECK(((w[2] >> 16) & 0xFF) == 0, "subdiv_2=0");
  PASS();
}

/* --- Chord Name (new) --- */

void test_chord_name_bb_minor(void) {
  TEST("Flex: Chord Bb minor (spec example)");
  uint32_t w[4];
  /* Bb minor: tonic_sf=-1(0xF), tonic=B(2), type=minor(0x07), no alts, bass=same */
  midi2_msg_chord_name(w, 0, 1, 0,
                          -1, 2, 0x07,
                          0, 0, 0, 0, 0, 0, 0, 0,
                          -8, 0, 0x00,
                          0, 0, 0, 0);
  CHECK(midi2_msg_get_mt(w) == 0x0D, "MT=Flex");
  CHECK((w[0] & 0xFF) == 0x06, "status=chord");
  CHECK(((w[1] >> 28) & 0x0F) == 0x0F, "tonic_sf=-1");
  CHECK(((w[1] >> 24) & 0x0F) == 2, "tonic=B");
  CHECK(((w[1] >> 16) & 0xFF) == 0x07, "type=minor");
  PASS();
}


/* --- Key Signature Full (new) --- */

void test_key_sig_full(void) {
  TEST("Flex: Key Sig Full -- C major, tonic=C, group-level");
  uint32_t w[4];
  midi2_msg_key_sig_full(w, 0, 1, 0, 0, 3, 0);  /* C=3, major=0 */
  CHECK(midi2_msg_get_mt(w) == 0x0D, "MT=Flex");
  CHECK((w[0] & 0xFF) == 0x05, "status=key_sig");
  CHECK(((w[0] >> 8) & 0xFF) == 0x00, "bank=setup");
  CHECK(((w[1] >> 28) & 0x0F) == 0, "sharps_flats=0");
  CHECK(((w[1] >> 24) & 0x0F) == 3, "tonic=C");
  CHECK(((w[1] >> 22) & 0x03) == 0, "key_type=major");
  PASS();
}

void test_key_sig_full_minor(void) {
  TEST("Flex: Key Sig Full -- A minor, tonic=A, channel 5");
  uint32_t w[4];
  midi2_msg_key_sig_full(w, 0, 0, 5, 0, 1, 1);  /* A=1, minor=1 */
  CHECK(((w[0] >> 20) & 0x03) == 0, "address=channel");
  CHECK(((w[0] >> 16) & 0x0F) == 5, "channel=5");
  CHECK(((w[1] >> 24) & 0x0F) == 1, "tonic=A");
  CHECK(((w[1] >> 22) & 0x03) == 1, "key_type=minor");
  PASS();
}

/* --- Flex Text (new) --- */

void test_flex_text_copyright(void) {
  TEST("Flex: Text copyright notice (complete)");
  uint32_t w[4];
  const uint8_t text[] = "2026 AMEI";
  midi2_msg_flex_text(w, 0, 0, 1, 0, MIDI2_FLEX_BANK_METADATA,
                         MIDI2_FLEX_TEXT_COPYRIGHT, text, 9);
  CHECK(midi2_msg_get_mt(w) == 0x0D, "MT=Flex");
  CHECK(((w[0] >> 22) & 0x03) == 0, "format=complete");
  CHECK(((w[0] >> 8) & 0xFF) == 0x01, "bank=metadata");
  CHECK((w[0] & 0xFF) == 0x04, "status=copyright");
  CHECK(((w[1] >> 24) & 0xFF) == '2', "text[0]='2'");
  CHECK(((w[1] >> 16) & 0xFF) == '0', "text[1]='0'");
  CHECK(((w[1] >> 8) & 0xFF) == '2', "text[2]='2'");
  CHECK((w[1] & 0xFF) == '6', "text[3]='6'");
  CHECK(((w[2] >> 24) & 0xFF) == ' ', "text[4]=' '");
  PASS();
}

void test_flex_text_lyrics(void) {
  TEST("Flex: Lyrics (performance text, bank 0x02)");
  uint32_t w[4];
  const uint8_t text[] = "la";
  midi2_msg_flex_text(w, 0, 0, 0, 0, MIDI2_FLEX_BANK_PERF_TEXT,
                         MIDI2_FLEX_PERF_LYRICS, text, 2);
  CHECK(((w[0] >> 8) & 0xFF) == 0x02, "bank=perf_text");
  CHECK((w[0] & 0xFF) == 0x01, "status=lyrics");
  CHECK(((w[1] >> 24) & 0xFF) == 'l', "text[0]='l'");
  CHECK(((w[1] >> 16) & 0xFF) == 'a', "text[1]='a'");
  PASS();
}

/* --- Stream Text Notifications (new) --- */

void test_stream_endpoint_name(void) {
  TEST("Stream: Endpoint Name 'Test'");
  uint32_t w[4];
  midi2_msg_stream_endpoint_name(w, 0, (const uint8_t *)"Test", 4);
  CHECK(midi2_msg_get_mt(w) == 0x0F, "MT=Stream");
  CHECK(((w[0] >> 16) & 0x3FF) == 0x003, "status=endpoint name");
  CHECK(((w[0] >> 8) & 0xFF) == 'T', "name[0]='T'");
  CHECK((w[0] & 0xFF) == 'e', "name[1]='e'");
  CHECK(((w[1] >> 24) & 0xFF) == 's', "name[2]='s'");
  CHECK(((w[1] >> 16) & 0xFF) == 't', "name[3]='t'");
  PASS();
}

void test_stream_product_id(void) {
  TEST("Stream: Product Instance ID 'ABC'");
  uint32_t w[4];
  midi2_msg_stream_product_id(w, 0, (const uint8_t *)"ABC", 3);
  CHECK(((w[0] >> 16) & 0x3FF) == 0x004, "status=product id");
  CHECK(((w[0] >> 8) & 0xFF) == 'A', "id[0]='A'");
  CHECK((w[0] & 0xFF) == 'B', "id[1]='B'");
  CHECK(((w[1] >> 24) & 0xFF) == 'C', "id[2]='C'");
  PASS();
}

void test_stream_fb_name(void) {
  TEST("Stream: FB Name 'Piano' fb=1");
  uint32_t w[4];
  midi2_msg_stream_fb_name(w, 0, 1, (const uint8_t *)"Piano", 5);
  CHECK(((w[0] >> 16) & 0x3FF) == 0x012, "status=FB name");
  CHECK(((w[0] >> 8) & 0xFF) == 1, "fb_num=1");
  CHECK((w[0] & 0xFF) == 'P', "name[0]='P'");
  CHECK(((w[1] >> 24) & 0xFF) == 'i', "name[1]='i'");
  CHECK(((w[1] >> 16) & 0xFF) == 'a', "name[2]='a'");
  CHECK(((w[1] >> 8) & 0xFF) == 'n', "name[3]='n'");
  CHECK((w[1] & 0xFF) == 'o', "name[4]='o'");
  PASS();
}

/* --- Main --- */

/* --- MT 0x2 -> MT 0x4 Translation --- */

void test_mt2_to_mt4_note_on(void) {
  TEST("mt2->mt4: Note On ch=0 note=60 vel=100");
  uint32_t mt2 = midi2_msg_from_midi1(0, 0x90, 60, 100);
  uint32_t mt4[2];
  CHECK(midi2_msg_mt2_to_mt4(mt2, mt4), "translated");
  CHECK(midi2_msg_get_mt(mt4) == MIDI2_MT_MIDI2_CV, "MT=0x4");
  CHECK(midi2_msg_get_note(mt4) == 60, "note=60");
  uint16_t vel = midi2_msg_get_velocity(mt4);
  CHECK(vel == midi2_msg_scale_up_7to16(100), "velocity scaled");
  PASS();
}

void test_mt2_to_mt4_note_on_vel0(void) {
  TEST("mt2->mt4: Note On vel=0 -> Note Off");
  uint32_t mt2 = midi2_msg_from_midi1(0, 0x90, 60, 0);
  uint32_t mt4[2];
  CHECK(midi2_msg_mt2_to_mt4(mt2, mt4), "translated");
  uint8_t status = (mt4[0] >> 16) & 0xF0;
  CHECK(status == 0x80, "status=Note Off");
  CHECK(midi2_msg_get_note(mt4) == 60, "note=60");
  PASS();
}

void test_mt2_to_mt4_cc(void) {
  TEST("mt2->mt4: CC#7 val=100 -> 32-bit scaled");
  uint32_t mt2 = midi2_msg_from_midi1(0, 0xB0, 7, 100);
  uint32_t mt4[2];
  CHECK(midi2_msg_mt2_to_mt4(mt2, mt4), "translated");
  CHECK(midi2_msg_get_mt(mt4) == MIDI2_MT_MIDI2_CV, "MT=0x4");
  uint32_t val = mt4[1];
  CHECK(val == midi2_msg_scale_up_7to32(100), "value scaled");
  PASS();
}

void test_mt2_to_mt4_program(void) {
  TEST("mt2->mt4: Program Change prog=5, no bank");
  uint32_t mt2 = midi2_msg_from_midi1(0, 0xC0, 5, 0);
  uint32_t mt4[2];
  CHECK(midi2_msg_mt2_to_mt4(mt2, mt4), "translated");
  /* bank_valid bit should be 0 */
  CHECK((mt4[0] & 0x01) == 0, "bank_valid=false");
  uint8_t prog = (mt4[1] >> 24) & 0x7F;
  CHECK(prog == 5, "program=5");
  PASS();
}

void test_mt2_to_mt4_chan_pressure(void) {
  TEST("mt2->mt4: Channel Pressure val=80 -> 32-bit");
  uint32_t mt2 = midi2_msg_from_midi1(0, 0xD0, 80, 0);
  uint32_t mt4[2];
  CHECK(midi2_msg_mt2_to_mt4(mt2, mt4), "translated");
  CHECK(mt4[1] == midi2_msg_scale_up_7to32(80), "value scaled");
  PASS();
}

void test_mt2_to_mt4_pitch_bend(void) {
  TEST("mt2->mt4: Pitch Bend center (0x00,0x40) -> 32-bit");
  /* MIDI 1.0 pitch bend: data1=LSB, data2=MSB. Center = 0x2000 = (0x40 << 7) | 0x00 */
  uint32_t mt2 = midi2_msg_from_midi1(0, 0xE0, 0x00, 0x40);
  uint32_t mt4[2];
  CHECK(midi2_msg_mt2_to_mt4(mt2, mt4), "translated");
  uint32_t val = mt4[1];
  uint32_t expected = midi2_msg_scale_up_14to32(0x2000);
  CHECK(val == expected, "center scaled correctly");
  PASS();
}

void test_mt2_to_mt4_poly_pressure(void) {
  TEST("mt2->mt4: Poly Pressure note=48 val=100");
  uint32_t mt2 = midi2_msg_from_midi1(0, 0xA0, 48, 100);
  uint32_t mt4[2];
  CHECK(midi2_msg_mt2_to_mt4(mt2, mt4), "translated");
  CHECK(midi2_msg_get_note(mt4) == 48, "note=48");
  CHECK(mt4[1] == midi2_msg_scale_up_7to32(100), "value scaled");
  PASS();
}

void test_mt2_to_mt4_roundtrip(void) {
  TEST("mt2->mt4: roundtrip velocity 7->16->7");
  uint8_t v;
  for (v = 1; v <= 127; v++) {
    uint32_t mt2 = midi2_msg_from_midi1(0, 0x90, 60, v);
    uint32_t mt4[2];
    midi2_msg_mt2_to_mt4(mt2, mt4);
    uint16_t vel16 = midi2_msg_get_velocity(mt4);
    uint8_t back = midi2_msg_scale_down_16to7(vel16);
    if (back != v) {
      char msg[64];
      sprintf(msg, "roundtrip failed: %d -> %u -> %d", v, vel16, back);
      FAIL(msg);
      return;
    }
  }
  PASS();
}

void test_mt2_to_mt4_wrong_mt(void) {
  TEST("mt2->mt4: wrong MT returns false");
  uint32_t w[2];
  midi2_msg_note_on(w, 0, 0, 60, 0xC000, 0, 0); /* MT 0x4 */
  uint32_t out[2];
  CHECK(!midi2_msg_mt2_to_mt4(w[0], out), "MT 0x4 rejected");
  CHECK(!midi2_msg_mt2_to_mt4(0, out), "zero rejected");
  PASS();
}

/* --- Group Re-stamp (v0.3.0) --- */

void test_set_group_mt4(void) {
  TEST("set_group: MT 0x4 rewrites group nibble");
  uint32_t w = 0x40903C7Fu; /* MT 0x4, group 0 */
  midi2_msg_set_group(&w, 5);
  CHECK(((w >> 24) & 0x0F) == 5, "expected group 5");
  CHECK(((w >> 28) & 0x0F) == 0x4, "MT preserved");
  PASS();
}

void test_set_group_mt0_unchanged(void) {
  TEST("set_group: MT 0x0 utility word unchanged");
  uint32_t w = 0x00112233u;
  midi2_msg_set_group(&w, 7);
  CHECK(w == 0x00112233u, "utility word must not be touched");
  PASS();
}

void test_set_group_mt_stream_unchanged(void) {
  TEST("set_group: MT 0xF stream word unchanged");
  uint32_t w = 0xF0010000u;
  uint32_t before = w;
  midi2_msg_set_group(&w, 3);
  CHECK(w == before, "stream word must not be touched");
  PASS();
}

void test_set_group_mt3_sysex7(void) {
  TEST("set_group: MT 0x3 SysEx7 rewrites group nibble");
  uint32_t w = 0x30123456u; /* MT 0x3 group 0 */
  midi2_msg_set_group(&w, 0xF);
  CHECK(((w >> 24) & 0x0F) == 0xF, "expected group 0xF");
  CHECK(((w >> 28) & 0x0F) == 0x3, "MT preserved");
  CHECK((w & 0x00FFFFFFu) == 0x00123456u, "lower 24 bits preserved");
  PASS();
}

/* --- MT 0x4 -> MT 0x2 downgrade (v0.3.0) --- */

void test_mt4_to_mt2_note_on_max_velocity(void) {
  TEST("mt4_to_mt2: Note On vel 0xFFFF -> MT 0x2 vel 0x7F");
  uint32_t in[2];
  midi2_msg_note_on(in, 0, 0, 60, 0xFFFFu, 0, 0);
  uint32_t out = 0;
  uint32_t n = midi2_msg_mt4_to_mt2(in, &out);
  CHECK(n == 1, "one MT2 word produced");
  CHECK(((out >> 28) & 0x0F) == 0x02, "MT=MIDI 1.0 CV");
  CHECK(((out >> 16) & 0xF0) == 0x90, "status = Note On");
  CHECK((out & 0x7F) == 0x7F, "velocity scaled down to 0x7F");
  PASS();
}

void test_mt4_to_mt2_cc_max(void) {
  TEST("mt4_to_mt2: CC value 32-bit max -> 0x7F");
  uint32_t in[2];
  midi2_msg_cc(in, 2, 3, 7, 0xFFFFFFFFu);
  uint32_t out = 0;
  CHECK(midi2_msg_mt4_to_mt2(in, &out) == 1, "one MT2 word");
  CHECK(((out >> 24) & 0x0F) == 2, "group preserved");
  CHECK(((out >> 16) & 0xFF) == 0xB3, "status CC ch 3");
  CHECK(((out >> 8) & 0x7F) == 7, "CC#7");
  CHECK((out & 0x7F) == 0x7F, "value 0x7F");
  PASS();
}

void test_mt4_to_mt2_pitch_bend_center(void) {
  TEST("mt4_to_mt2: PB 32-bit center -> 14-bit midpoint 0x2000");
  uint32_t in[2];
  midi2_msg_pitch_bend(in, 0, 0, 0x80000000u);
  uint32_t out = 0;
  CHECK(midi2_msg_mt4_to_mt2(in, &out) == 1, "one word");
  uint16_t pb14 = (uint16_t)(((out >> 8) & 0x7F)
                            | ((out & 0x7F) << 7));
  CHECK(pb14 == 0x2000u, "center 14-bit PB");
  PASS();
}

void test_mt4_to_mt2_drops_rpn(void) {
  TEST("mt4_to_mt2: RPN dropped (no MIDI 1.0 single-word form)");
  uint32_t in[2];
  midi2_msg_rpn(in, 0, 0, 0, 0, 0x12345678u);
  uint32_t out = 0;
  CHECK(midi2_msg_mt4_to_mt2(in, &out) == 0, "RPN dropped");
  PASS();
}

void test_mt4_to_mt2_program(void) {
  TEST("mt4_to_mt2: Program 0x05 preserved, bank dropped");
  uint32_t in[2];
  midi2_msg_program(in, 0, 0, 0x05, false, 0, 0);
  uint32_t out = 0;
  CHECK(midi2_msg_mt4_to_mt2(in, &out) == 1, "one word");
  CHECK(((out >> 16) & 0xFF) == 0xC0, "status Program ch 0");
  CHECK(((out >> 8) & 0x7F) == 0x05, "program 0x05");
  PASS();
}

void test_mt4_to_mt2_rejects_non_mt4(void) {
  TEST("mt4_to_mt2: MT0 utility word dropped");
  uint32_t in[2] = {0x00000000u, 0u};
  uint32_t out = 0;
  CHECK(midi2_msg_mt4_to_mt2(in, &out) == 0, "non-MT4 dropped");
  PASS();
}

/* --- USB MIDI 1.0 cable event -> UMP (v0.3.0) --- */

static uint32_t pack_cable(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  return  (uint32_t)b0
       | ((uint32_t)b1 <<  8)
       | ((uint32_t)b2 << 16)
       | ((uint32_t)b3 << 24);
}

void test_cable_event_note_on(void) {
  TEST("cable_event: CIN 0x9 NoteOn -> MT 0x2");
  uint32_t packed = pack_cable(0x09, 0x90, 0x3C, 0x7F);
  uint32_t ump = 0;
  CHECK(midi2_msg_cable_event_to_ump(packed, 5, &ump), "converts");
  CHECK(((ump >> 28) & 0x0F) == 0x02, "MT MIDI 1.0 CV");
  CHECK(((ump >> 24) & 0x0F) == 5, "group 5 (caller-supplied)");
  CHECK(((ump >> 16) & 0xFF) == 0x90, "status preserved");
  CHECK(((ump >>  8) & 0x7F) == 0x3C, "note preserved");
  CHECK((ump        & 0x7F) == 0x7F, "velocity preserved");
  PASS();
}

void test_cable_event_cc(void) {
  TEST("cable_event: CIN 0xB CC with cable 3");
  uint32_t packed = pack_cable((3 << 4) | 0xB, 0xB0, 0x07, 0x64);
  uint32_t ump = 0;
  CHECK(midi2_msg_cable_event_to_ump(packed, 0, &ump), "converts");
  CHECK(((ump >> 16) & 0xFF) == 0xB0, "status CC");
  CHECK(((ump >>  8) & 0x7F) == 0x07, "CC#7");
  CHECK((ump        & 0x7F) == 0x64, "value 100");
  PASS();
}

void test_cable_event_program(void) {
  TEST("cable_event: CIN 0xC Program 2-byte");
  uint32_t packed = pack_cable(0x0C, 0xC0, 0x42, 0x00);
  uint32_t ump = 0;
  CHECK(midi2_msg_cable_event_to_ump(packed, 0, &ump), "converts");
  CHECK(((ump >> 16) & 0xFF) == 0xC0, "program change");
  CHECK(((ump >>  8) & 0x7F) == 0x42, "program 0x42");
  PASS();
}

void test_cable_event_reserved_cin(void) {
  TEST("cable_event: reserved CIN 0x0 returns false");
  uint32_t packed = pack_cable(0x00, 0x00, 0x00, 0x00);
  uint32_t ump = 0xDEADBEEFu;
  CHECK(!midi2_msg_cable_event_to_ump(packed, 0, &ump),
        "reserved CIN rejected");
  CHECK(ump == 0xDEADBEEFu, "output untouched on failure");
  PASS();
}

void test_cable_event_sysex_cin(void) {
  TEST("cable_event: SysEx fragment CIN 0x4 returns false (deferred to midi2_conv)");
  uint32_t packed = pack_cable(0x04, 0xF0, 0x7E, 0x00);
  uint32_t ump = 0;
  CHECK(!midi2_msg_cable_event_to_ump(packed, 0, &ump),
        "SysEx fragment CIN deferred");
  PASS();
}

void test_cable_event_null_safe(void) {
  TEST("cable_event: NULL output returns false");
  uint32_t packed = pack_cable(0x09, 0x90, 0x3C, 0x7F);
  CHECK(!midi2_msg_cable_event_to_ump(packed, 0, NULL), "NULL rejected");
  PASS();
}

/* --- NULL paths (boundary conversion functions) --- */

void test_mt4_to_mt2_null_input(void) {
  TEST("mt4_to_mt2: NULL mt4_words returns 0");
  uint32_t out_word = 0xDEADBEEFu;
  CHECK(midi2_msg_mt4_to_mt2(NULL, &out_word) == 0, "NULL input rejected");
  CHECK(out_word == 0xDEADBEEFu, "out_word untouched on rejection");
  PASS();
}

void test_mt4_to_mt2_null_output(void) {
  TEST("mt4_to_mt2: NULL out_word returns 0");
  uint32_t mt4[2];
  midi2_msg_note_on(mt4, 0, 0, 60, 0xC000, 0, 0);
  CHECK(midi2_msg_mt4_to_mt2(mt4, NULL) == 0, "NULL output rejected");
  PASS();
}

void test_mt2_to_mt4_null_output(void) {
  TEST("mt2_to_mt4: NULL out returns false");
  uint32_t mt2 = 0x20903C7Fu; /* MT 0x2 Note On */
  CHECK(!midi2_msg_mt2_to_mt4(mt2, NULL), "NULL output rejected");
  PASS();
}

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

  printf("\n[System Wrappers (named, v0.3.0)]\n");
  test_system_wrapper_tune_request();
  test_system_wrapper_timing_clock();
  test_system_wrapper_start();
  test_system_wrapper_continue();
  test_system_wrapper_stop();
  test_system_wrapper_active_sensing();
  test_system_wrapper_reset();
  test_system_wrapper_mtc();
  test_system_wrapper_song_select();
  test_system_wrapper_song_position();

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
  test_noop();

  printf("\n[Stream Messages]\n");
  test_stream_endpoint_discovery();
  test_stream_endpoint_info();
  test_stream_device_identity();
  test_stream_config_request();
  test_stream_config_request_jr_bits();
  test_stream_config_notify();
  test_stream_config_notify_jr_bits();
  test_stream_fb_discovery();
  test_stream_fb_info();
  test_stream_fb_info_ui_hint();
  test_stream_fb_info_sysex8_streams();
  test_stream_clip();

  printf("\n[SysEx8]\n");
  test_sysex8_complete();
  test_sysex8_13bytes();

  printf("\n[MIDI 1.0 Conversion]\n");
  test_from_midi1();

  printf("\n[Delta Clockstamp]\n");
  test_dctpq();
  test_delta_clockstamp();
  test_delta_clockstamp_max();
  test_delta_clockstamp_overflow();

  printf("\n[Per-Note Controllers]\n");
  test_reg_per_note_ctrl();
  test_asn_per_note_ctrl();

  printf("\n[Relative RPN/NRPN]\n");
  test_rel_rpn();
  test_rel_nrpn();

  printf("\n[Mixed Data Set]\n");
  test_mds_header();
  test_mds_payload();

  printf("\n[Metronome]\n");
  test_metronome();

  printf("\n[Chord Name]\n");
  test_chord_name_bb_minor();

  printf("\n[Key Signature Full]\n");
  test_key_sig_full();
  test_key_sig_full_minor();

  printf("\n[Flex Text]\n");
  test_flex_text_copyright();
  test_flex_text_lyrics();

  printf("\n[Stream Text Notifications]\n");
  test_stream_endpoint_name();
  test_stream_product_id();
  test_stream_fb_name();

  printf("\n[Group Re-stamp]\n");
  test_set_group_mt4();
  test_set_group_mt0_unchanged();
  test_set_group_mt_stream_unchanged();
  test_set_group_mt3_sysex7();

  printf("\n[MT 0x4 -> MT 0x2 Downgrade]\n");
  test_mt4_to_mt2_note_on_max_velocity();
  test_mt4_to_mt2_cc_max();
  test_mt4_to_mt2_pitch_bend_center();
  test_mt4_to_mt2_drops_rpn();
  test_mt4_to_mt2_program();
  test_mt4_to_mt2_rejects_non_mt4();

  printf("\n[USB Cable Event -> UMP]\n");
  test_cable_event_note_on();
  test_cable_event_cc();
  test_cable_event_program();
  test_cable_event_reserved_cin();
  test_cable_event_sysex_cin();
  test_cable_event_null_safe();

  printf("\n[NULL Paths (boundary conversions)]\n");
  test_mt4_to_mt2_null_input();
  test_mt4_to_mt2_null_output();
  test_mt2_to_mt4_null_output();

  printf("\n[MT 0x2 -> MT 0x4 Translation]\n");
  test_mt2_to_mt4_note_on();
  test_mt2_to_mt4_note_on_vel0();
  test_mt2_to_mt4_cc();
  test_mt2_to_mt4_program();
  test_mt2_to_mt4_chan_pressure();
  test_mt2_to_mt4_pitch_bend();
  test_mt2_to_mt4_poly_pressure();
  test_mt2_to_mt4_roundtrip();
  test_mt2_to_mt4_wrong_mt();

  printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
