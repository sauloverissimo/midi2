/*
 * midi2.h amalgamated header tests
 * Validates that the single-header build produces identical behavior
 * to the multi-module build across all 7 modules.
 *
 * Compile: gcc -std=c99 -I../src test_midi2_amalgam.c -o test_midi2_amalgam
 */

#define MIDI2_IMPLEMENTATION
#include "midi2.h"

#include <stdio.h>

static int passed = 0;
static int failed = 0;

#define TEST(name) printf("  %-55s ", name)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ---- midi2_msg (static inline) ---- */

void test_msg_note_on(void) {
  TEST("msg: note_on builds correct UMP words");
  uint32_t w[2];
  midi2_msg_note_on(w, 0, 0, 60, 0xFFFF, 0, 0);
  CHECK(w[0] == 0x40903C00, "w0 mismatch");
  CHECK(w[1] == 0xFFFF0000, "w1 mismatch");
  PASS();
}

void test_msg_note_off(void) {
  TEST("msg: note_off builds correct UMP words");
  uint32_t w[2];
  midi2_msg_note_off(w, 0, 0, 60, 0x8000, 0, 0);
  CHECK(w[0] == 0x40803C00, "w0 mismatch");
  CHECK(w[1] == 0x80000000, "w1 mismatch");
  PASS();
}

void test_msg_cc(void) {
  TEST("msg: cc builds correct UMP words");
  uint32_t w[2];
  midi2_msg_cc(w, 0, 0, 7, 0x80000000);
  CHECK((w[0] & 0xFFF00000) == 0x40B00000, "status mismatch");
  CHECK(((w[0] >> 8) & 0x7F) == 7, "index mismatch");
  CHECK(w[1] == 0x80000000, "value mismatch");
  PASS();
}

void test_msg_scale_roundtrip(void) {
  TEST("msg: scale 7->16->7 round-trip");
  for (uint8_t v = 0; v < 128; v++) {
    uint16_t up = midi2_msg_scale_up_7to16(v);
    uint8_t down = midi2_msg_scale_down_16to7(up);
    CHECK(down == v, "round-trip failed");
  }
  PASS();
}

void test_msg_pitch_bend(void) {
  TEST("msg: pitch_bend builds correct words");
  uint32_t w[2];
  midi2_msg_pitch_bend(w, 0, 0, 0x80000000);
  CHECK((w[0] & 0xFFF00000) == 0x40E00000, "status mismatch");
  CHECK(w[1] == 0x80000000, "value mismatch");
  PASS();
}

void test_msg_program_change(void) {
  TEST("msg: program_change builds correct words");
  uint32_t w[2];
  midi2_msg_program(w, 0, 0, 42, false, 0, 0);
  CHECK((w[0] & 0xFFF00000) == 0x40C00000, "status mismatch");
  CHECK(((w[1] >> 24) & 0x7F) == 42, "program mismatch");
  PASS();
}

void test_msg_sysex7(void) {
  TEST("msg: sysex7_packet packs correctly");
  uint32_t w[2];
  uint8_t data[] = {0x7E, 0x7F, 0x09, 0x01};
  midi2_msg_sysex7_packet(w, 0, MIDI2_SYSEX7_COMPLETE, data, 4);
  CHECK(((w[0] >> 20) & 0x0F) == 0x0, "status should be complete");
  CHECK(((w[0] >> 16) & 0x0F) == 4, "length should be 4");
  PASS();
}

void test_msg_word_count(void) {
  TEST("msg: word_count returns correct sizes");
  CHECK(midi2_msg_word_count(0x0) == 1, "MT0 should be 1 word");
  CHECK(midi2_msg_word_count(0x2) == 1, "MT2 should be 1 word");
  CHECK(midi2_msg_word_count(0x4) == 2, "MT4 should be 2 words");
  CHECK(midi2_msg_word_count(0x5) == 4, "MT5 should be 4 words");
  PASS();
}

/* ---- midi2_ci_msg (static inline) ---- */

void test_ci_msg_discovery(void) {
  TEST("ci_msg: build discovery request");
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_discovery(buf, 0x02,
    0x01020304, 0x000102, 0x0100, 0x0200, 0x01020304,
    0x00, 512, 0x00);
  CHECK(len > 0, "build failed");
  PASS();
}

void test_ci_msg_profile_inquiry(void) {
  TEST("ci_msg: build profile inquiry");
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_profile_inquiry(buf, 0x02,
    0x01020304, 0x05060708, 0x7F);
  CHECK(len > 0, "build failed");
  PASS();
}

/* ---- midi2_dispatch (stateful) ---- */

static uint8_t dp_note = 0;
static uint16_t dp_vel = 0;

static void on_note_on(uint8_t group, uint8_t channel, uint8_t note,
                       uint16_t velocity, uint8_t attr_type,
                       uint16_t attr_data, void *ctx) {
  (void)group; (void)channel; (void)attr_type; (void)attr_data; (void)ctx;
  dp_note = note;
  dp_vel = velocity;
}

void test_dispatch_note_on(void) {
  TEST("dispatch: note_on callback fires");
  midi2_dispatch dp;
  midi2_dispatch_init(&dp);
  dp.on_note_on = on_note_on;
  uint32_t w[2];
  midi2_msg_note_on(w, 0, 0, 60, 0xC000, 0, 0);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(dp_note == 60, "note mismatch");
  CHECK(dp_vel == 0xC000, "velocity mismatch");
  PASS();
}

static uint8_t dp_cc_idx = 0;
static uint32_t dp_cc_val = 0;

static void on_cc(uint8_t group, uint8_t channel, uint8_t index,
                  uint32_t value, void *ctx) {
  (void)group; (void)channel; (void)ctx;
  dp_cc_idx = index;
  dp_cc_val = value;
}

void test_dispatch_cc(void) {
  TEST("dispatch: cc callback fires");
  midi2_dispatch dp;
  midi2_dispatch_init(&dp);
  dp.on_cc = on_cc;
  uint32_t w[2];
  midi2_msg_cc(w, 0, 0, 7, 0x40000000);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(dp_cc_idx == 7, "index mismatch");
  CHECK(dp_cc_val == 0x40000000, "value mismatch");
  PASS();
}

void test_dispatch_null_safe(void) {
  TEST("dispatch: null callbacks don't crash");
  midi2_dispatch dp;
  midi2_dispatch_init(&dp);
  uint32_t w[2];
  midi2_msg_note_on(w, 0, 0, 60, 0xFFFF, 0, 0);
  midi2_dispatch_feed(w, 2, &dp);
  PASS();
}

/* ---- midi2_proc (stateful) ---- */

static uint32_t proc_last[4];
static uint8_t proc_wc = 0;

static void proc_cb(const uint32_t *words, uint8_t word_count, void *ctx) {
  (void)ctx;
  for (int i = 0; i < word_count; i++) proc_last[i] = words[i];
  proc_wc = word_count;
}

void test_proc_passthrough(void) {
  TEST("proc: passes UMP through with all groups enabled");
  midi2_proc_state ps;
  uint8_t sx7[32], sx8[32];
  midi2_proc_init(&ps, sx7, sizeof(sx7), sx8, sizeof(sx8));
  ps.group_mask = 0xFFFF;
  ps.on_ump = proc_cb;
  uint32_t w[2];
  midi2_msg_note_on(w, 0, 0, 60, 0xFFFF, 0, 0);
  midi2_proc_feed(&ps, w, 2);
  CHECK(proc_wc == 2, "word count mismatch");
  CHECK(proc_last[0] == w[0], "w0 mismatch");
  PASS();
}

void test_proc_group_filter(void) {
  TEST("proc: filters out disabled group");
  midi2_proc_state ps;
  uint8_t sx7[32], sx8[32];
  midi2_proc_init(&ps, sx7, sizeof(sx7), sx8, sizeof(sx8));
  ps.group_mask = 0x0002;  /* only group 1 */
  ps.on_ump = proc_cb;
  proc_wc = 0;
  uint32_t w[2];
  midi2_msg_note_on(w, 0, 0, 60, 0xFFFF, 0, 0);  /* group 0 */
  midi2_proc_feed(&ps, w, 2);
  CHECK(proc_wc == 0, "should have been filtered");
  PASS();
}

/* ---- midi2_conv (stateful) ---- */

void test_conv_note_on(void) {
  TEST("conv: MIDI 1.0 note_on bytes -> UMP");
  midi2_conv_state cs;
  midi2_conv_init(&cs, 0);
  bool r;
  r = midi2_conv_feed(&cs, 0x90);
  CHECK(!r, "not complete yet");
  r = midi2_conv_feed(&cs, 60);
  CHECK(!r, "not complete yet");
  r = midi2_conv_feed(&cs, 100);
  CHECK(r, "should be complete");
  CHECK(cs.ump_words == 1, "should produce 1 word");
  CHECK(((cs.ump[0] >> 20) & 0x0F) == 0x9, "should be note on");
  PASS();
}

void test_conv_running_status(void) {
  TEST("conv: running status sends second note");
  midi2_conv_state cs;
  midi2_conv_init(&cs, 0);
  midi2_conv_feed(&cs, 0x90);
  midi2_conv_feed(&cs, 60);
  midi2_conv_feed(&cs, 100);
  /* running status: no status byte, just data */
  midi2_conv_feed(&cs, 72);
  bool r = midi2_conv_feed(&cs, 80);
  CHECK(r, "should be complete");
  CHECK(((cs.ump[0] >> 8) & 0x7F) == 72, "note should be 72");
  PASS();
}

/* ---- midi2_ci_dispatch (stateful) ---- */

static bool ci_dp_fired = false;
static uint32_t ci_dp_source = 0;

static void ci_on_discovery(midi2_ci_header hdr, uint32_t mfr_id,
                            uint16_t family, uint16_t model,
                            uint32_t sw_rev, uint8_t ci_category,
                            uint32_t max_sysex, uint8_t output_path_id,
                            void *ctx) {
  (void)mfr_id; (void)family; (void)model; (void)sw_rev;
  (void)ci_category; (void)max_sysex; (void)output_path_id; (void)ctx;
  ci_dp_fired = true;
  ci_dp_source = hdr.src_muid;
}

void test_ci_dispatch_discovery(void) {
  TEST("ci_dispatch: discovery callback fires");
  midi2_ci_dispatch cdp;
  midi2_ci_dispatch_init(&cdp);
  cdp.on_discovery = ci_on_discovery;
  ci_dp_fired = false;

  uint8_t buf[64];
  uint16_t len = midi2_ci_build_discovery(buf, 0x02,
    0x01020304, 0x000102, 0x0100, 0x0200, 0x01020304,
    0x00, 512, 0x00);
  /* feed without F0/F7 framing -- ci_dispatch expects raw CI payload */
  midi2_ci_dispatch_feed(&cdp, 0, buf, len);
  CHECK(ci_dp_fired, "callback should have fired");
  CHECK(ci_dp_source == 0x01020304, "source MUID mismatch");
  PASS();
}

/* ---- midi2_ci (convenience responder) ---- */

void test_ci_init(void) {
  TEST("ci: init without crash");
  uint8_t profiles[4][5];
  midi2_ci_property props[2];
  memset(props, 0, sizeof(props));
  midi2_ci_state cis;
  midi2_ci_init(&cis, 12345, profiles, 4, props, 2);
  CHECK(cis.muid != 0, "MUID should be set");
  PASS();
}

/* ---- Amalgam-specific: verify stb-style pattern ---- */

void test_header_only_inline(void) {
  TEST("amalgam: static inline works without IMPLEMENTATION");
  /* This TU has IMPLEMENTATION, but the point is midi2_msg_* are
     static inline and would compile in any TU regardless. */
  uint32_t w[2];
  midi2_msg_note_on(w, 15, 15, 127, 0xFFFF, 0, 0);
  CHECK(((w[0] >> 24) & 0x0F) == 15, "group should be 15");
  PASS();
}

/* ---- v0.3.0 surface smoke tests ---- */

void test_v030_msg_set_group(void) {
  TEST("amalgam v0.3.0: midi2_msg_set_group rewrites MT4 group");
  uint32_t w[2];
  midi2_msg_note_on(w, 0, 0, 60, 0xFFFF, 0, 0);
  midi2_msg_set_group(&w[0], 7);
  CHECK(((w[0] >> 24) & 0x0F) == 7, "group rewritten");
  PASS();
}

void test_v030_msg_cable_event(void) {
  TEST("amalgam v0.3.0: midi2_msg_cable_event_to_ump");
  uint32_t packed = (uint32_t)0x09 | (0x90u << 8) | (0x3Cu << 16) | (0x7Fu << 24);
  uint32_t ump = 0;
  CHECK(midi2_msg_cable_event_to_ump(packed, 5, &ump), "converts");
  CHECK(((ump >> 28) & 0x0F) == 0x02, "MT2");
  CHECK(((ump >> 24) & 0x0F) == 5, "group preserved");
  PASS();
}

void test_v030_msg_mt4_to_mt2(void) {
  TEST("amalgam v0.3.0: midi2_msg_mt4_to_mt2 downgrades Note On");
  uint32_t in[2];
  midi2_msg_note_on(in, 0, 0, 60, 0xFFFFu, 0, 0);
  uint32_t out = 0;
  CHECK(midi2_msg_mt4_to_mt2(in, &out) == 1, "1 word");
  CHECK(((out >> 28) & 0x0F) == 0x02, "MT MIDI 1.0 CV");
  CHECK((out & 0x7F) == 0x7F, "velocity scaled to 0x7F");
  PASS();
}

/* Capture first word of a UMP emission; matches midi2_proc_write_fn. */
static uint32_t v030_first_word;
static int      v030_emit_count;
static uint32_t v030_capture(const uint32_t *w, uint32_t n, void *ctx) {
  (void)ctx;
  if (n > 0 && v030_emit_count == 0) v030_first_word = w[0];
  v030_emit_count++;
  return n;
}

void test_v030_proc_send_endpoint_name(void) {
  TEST("amalgam v0.3.0: midi2_proc_send_endpoint_name emits Stream MT F");
  v030_first_word = 0;
  v030_emit_count = 0;
  midi2_proc_send_endpoint_name("Hi", v030_capture, NULL);
  CHECK(v030_emit_count == 1, "1 UMP packet");
  CHECK(((v030_first_word >> 28) & 0x0F) == 0x0F, "MT=Stream");
  CHECK(((v030_first_word >> 16) & 0x3FF) == 0x003, "status=Endpoint Name");
  PASS();
}

void test_v030_ci_subscribe_add(void) {
  TEST("amalgam v0.3.0: midi2_ci subscribe_add / notify API linked");
  midi2_ci_state s;
  midi2_ci_property props[1];
  midi2_ci_subscriber subs[2];
  memset(props, 0, sizeof(props));
  midi2_ci_init_ex(&s, 0xABCD, NULL, 0, props, 1, subs, 2);
  midi2_ci_add_property_static(&s, "X", "v");
  CHECK(midi2_ci_pe_set_subscribable(&s, "X", true) == MIDI2_CI_OK, "flag");
  CHECK(midi2_ci_subscribe_add(&s, 0x100, "X") == MIDI2_CI_OK, "add");
  CHECK(midi2_ci_get_subscriber_count(&s) == 1, "count 1");
  PASS();
}

/* ---- main ---- */

int main(void) {
  printf("\n[midi2.h amalgamated header test]\n\n");

  printf("[midi2_msg]\n");
  test_msg_note_on();
  test_msg_note_off();
  test_msg_cc();
  test_msg_scale_roundtrip();
  test_msg_pitch_bend();
  test_msg_program_change();
  test_msg_sysex7();
  test_msg_word_count();

  printf("\n[midi2_ci_msg]\n");
  test_ci_msg_discovery();
  test_ci_msg_profile_inquiry();

  printf("\n[midi2_dispatch]\n");
  test_dispatch_note_on();
  test_dispatch_cc();
  test_dispatch_null_safe();

  printf("\n[midi2_proc]\n");
  test_proc_passthrough();
  test_proc_group_filter();

  printf("\n[midi2_conv]\n");
  test_conv_note_on();
  test_conv_running_status();

  printf("\n[midi2_ci_dispatch]\n");
  test_ci_dispatch_discovery();

  printf("\n[midi2_ci]\n");
  test_ci_init();

  printf("\n[amalgam]\n");
  test_header_only_inline();

  printf("\n[v0.3.0 surface]\n");
  test_v030_msg_set_group();
  test_v030_msg_cable_event();
  test_v030_msg_mt4_to_mt2();
  test_v030_proc_send_endpoint_name();
  test_v030_ci_subscribe_add();

  printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
