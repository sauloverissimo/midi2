/*
 * midi2_dispatch unit tests
 * Compile: gcc -std=c99 -I../src test_midi2_dispatch.c ../src/midi2_dispatch.c -o test_midi2_dispatch
 */

#include <stdio.h>
#include <string.h>
#include "midi2_dispatch.h"

static int passed = 0;
static int failed = 0;

#define TEST(name) printf("  %-55s ", name)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/*--------------------------------------------------------------------+
 * Test context: captures last callback data for verification
 *--------------------------------------------------------------------*/
static struct {
  int called;
  uint8_t group, channel, note, status, index, attr_type;
  uint16_t velocity, attr_data, timestamp;
  uint32_t value;
  bool bank_valid, detach, reset;
  uint8_t bank_msb, bank_lsb, program;
  /* Flex */
  uint32_t tempo;
  uint8_t num, den, n32;
  uint8_t primary_clicks, accent_1, subdiv_1;
  int8_t sharps_flats;
  uint8_t tonic, key_type;
  uint8_t address;
  /* Chord */
  uint8_t chord_type, tonic_note;
  int8_t tonic_sf;
  /* Text */
  uint8_t text_bank, text_status, text_format;
  uint8_t text_data[14];
  uint8_t text_len;
  /* Stream */
  uint16_t stream_status;
  uint8_t ump_maj, ump_min, filter;
  bool static_fb, m2_proto, m1_proto, rx_jr, tx_jr;
  uint8_t num_fb;
  uint32_t mfr_id, version_id;
  uint16_t family_id, model_id;
  uint8_t proto;
  /* FB */
  bool fb_active;
  uint8_t fb_num, fb_dir, fb_first, fb_ngrp;
  /* Clip */
  bool clip_start;
  /* MDS */
  uint8_t mds_id;
  uint16_t mds_num_bytes, mds_num_chunks, mds_this_chunk;
  uint16_t mds_mfr, mds_device;
  uint8_t mds_data[14];
  uint8_t mds_data_len;
  /* SysEx */
  uint8_t sysex_status;
  uint8_t sysex_data[13];
  uint8_t sysex_len;
  uint8_t sysex_stream_id;
  /* Unknown */
  uint8_t unknown_wc;
  /* DCTPQ / DC */
  uint16_t dctpq;
  uint32_t dc_ticks;
} ctx;

static void reset_ctx(void) { memset(&ctx, 0, sizeof(ctx)); }

/*--------------------------------------------------------------------+
 * Callback implementations
 *--------------------------------------------------------------------*/

/* Utility */
static void cb_noop(void *c) { (void)c; ctx.called = 1; }
static void cb_jr_clock(uint8_t g, uint16_t ts, void *c) { (void)c; ctx.called = 1; ctx.group = g; ctx.timestamp = ts; }
static void cb_jr_timestamp(uint8_t g, uint16_t ts, void *c) { (void)c; ctx.called = 1; ctx.group = g; ctx.timestamp = ts; }
static void cb_dctpq(uint16_t tpq, void *c) { (void)c; ctx.called = 1; ctx.dctpq = tpq; }
static void cb_dc(uint32_t ticks, void *c) { (void)c; ctx.called = 1; ctx.dc_ticks = ticks; }

/* System */
static void cb_system(uint8_t g, uint8_t st, uint8_t d1, uint8_t d2, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.status = st; ctx.note = d1; ctx.index = d2;
}

/* CV1 */
static void cb_cv1_note_on(uint8_t g, uint8_t ch, uint8_t n, uint8_t v, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.note = n; ctx.velocity = v;
}
static void cb_cv1_cc(uint8_t g, uint8_t ch, uint8_t idx, uint8_t v, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.index = idx; ctx.value = v;
}
static void cb_cv1_program(uint8_t g, uint8_t ch, uint8_t prog, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.note = prog;
}
static void cb_cv1_pressure(uint8_t g, uint8_t ch, uint8_t v, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.value = v;
}
static void cb_cv1_poly_pressure(uint8_t g, uint8_t ch, uint8_t n, uint8_t v, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.note = n; ctx.value = v;
}
static void cb_cv1_pitch_bend(uint8_t g, uint8_t ch, uint16_t v, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.velocity = v;
}

/* CV2 */
static void cb_note_on(uint8_t g, uint8_t ch, uint8_t n, uint16_t vel,
                          uint8_t at, uint16_t ad, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.note = n;
  ctx.velocity = vel; ctx.attr_type = at; ctx.attr_data = ad;
}
static void cb_note_off(uint8_t g, uint8_t ch, uint8_t n, uint16_t vel,
                           uint8_t at, uint16_t ad, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.note = n;
  ctx.velocity = vel; ctx.attr_type = at; ctx.attr_data = ad;
}
static void cb_cc(uint8_t g, uint8_t ch, uint8_t idx, uint32_t v, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.index = idx; ctx.value = v;
}
static void cb_program(uint8_t g, uint8_t ch, uint8_t p, bool bv,
                          uint8_t bm, uint8_t bl, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.program = p;
  ctx.bank_valid = bv; ctx.bank_msb = bm; ctx.bank_lsb = bl;
}
static void cb_pitch_bend(uint8_t g, uint8_t ch, uint32_t v, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.value = v;
}
static void cb_rpn(uint8_t g, uint8_t ch, uint8_t bank, uint8_t idx, uint32_t v, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.bank_msb = bank; ctx.index = idx; ctx.value = v;
}
static void cb_per_note_mgmt(uint8_t g, uint8_t ch, uint8_t n, bool d, bool r, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.note = n; ctx.detach = d; ctx.reset = r;
}
static void cb_per_note_ctrl(uint8_t g, uint8_t ch, uint8_t n, uint8_t idx, uint32_t v, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.note = n; ctx.index = idx; ctx.value = v;
}
static void cb_poly_pressure(uint8_t g, uint8_t ch, uint8_t n, uint32_t v, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.note = n; ctx.value = v;
}
static void cb_chan_pressure(uint8_t g, uint8_t ch, uint32_t v, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.channel = ch; ctx.value = v;
}

/* SysEx7 */
static void cb_sysex7(uint8_t g, uint8_t st, const uint8_t *d, uint8_t len, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.sysex_status = st;
  ctx.sysex_len = len; if (len > 6) len = 6; memcpy(ctx.sysex_data, d, len);
}

/* SysEx8 */
static void cb_sysex8(uint8_t g, uint8_t st, uint8_t sid, const uint8_t *d, uint8_t len, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.sysex_status = st;
  ctx.sysex_stream_id = sid; ctx.sysex_len = len;
  if (len > 13) len = 13;
  memcpy(ctx.sysex_data, d, len);
}

/* MDS */
static void cb_mds_header(uint8_t g, uint8_t id, uint16_t nb, uint16_t nc,
                             uint16_t tc, uint16_t mfr, uint16_t dev,
                             uint16_t s1, uint16_t s2, void *c) {
  (void)c; (void)s1; (void)s2;
  ctx.called = 1; ctx.group = g; ctx.mds_id = id;
  ctx.mds_num_bytes = nb; ctx.mds_num_chunks = nc; ctx.mds_this_chunk = tc;
  ctx.mds_mfr = mfr; ctx.mds_device = dev;
}
static void cb_mds_payload(uint8_t g, uint8_t id, const uint8_t *d, uint8_t len, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.mds_id = id;
  ctx.mds_data_len = len; if (len > 14) len = 14; memcpy(ctx.mds_data, d, len);
}

/* Flex */
static void cb_tempo(uint8_t g, uint32_t t, void *c) { (void)c; ctx.called = 1; ctx.group = g; ctx.tempo = t; }
static void cb_time_sig(uint8_t g, uint8_t n, uint8_t d, uint8_t n32, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.num = n; ctx.den = d; ctx.n32 = n32;
}
static void cb_metronome(uint8_t g, uint8_t pc, uint8_t a1, uint8_t a2, uint8_t a3,
                            uint8_t s1, uint8_t s2, void *c) {
  (void)c; (void)a2; (void)a3; (void)s2;
  ctx.called = 1; ctx.group = g; ctx.primary_clicks = pc; ctx.accent_1 = a1; ctx.subdiv_1 = s1;
}
static void cb_key_sig(uint8_t g, uint8_t addr, uint8_t ch, int8_t sf, uint8_t t, uint8_t kt, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.address = addr; ctx.channel = ch;
  ctx.sharps_flats = sf; ctx.tonic = t; ctx.key_type = kt;
}
static void cb_chord(uint8_t g, uint8_t addr, uint8_t ch,
                       int8_t tsf, uint8_t tn, uint8_t ct,
                       uint8_t a1t, uint8_t a1d, uint8_t a2t, uint8_t a2d,
                       uint8_t a3t, uint8_t a3d, uint8_t a4t, uint8_t a4d,
                       int8_t bsf, uint8_t bn, uint8_t bt,
                       uint8_t b1t, uint8_t b1d, uint8_t b2t, uint8_t b2d, void *c) {
  (void)c; (void)a1t; (void)a1d; (void)a2t; (void)a2d;
  (void)a3t; (void)a3d; (void)a4t; (void)a4d;
  (void)bsf; (void)bn; (void)bt; (void)b1t; (void)b1d; (void)b2t; (void)b2d;
  ctx.called = 1; ctx.group = g; ctx.address = addr; ctx.channel = ch;
  ctx.tonic_sf = tsf; ctx.tonic_note = tn; ctx.chord_type = ct;
}
static void cb_flex_text(uint8_t g, uint8_t fmt, uint8_t addr, uint8_t ch,
                            uint8_t bank, uint8_t st, const uint8_t *text, uint8_t len, void *c) {
  (void)c; ctx.called = 1; ctx.group = g; ctx.text_format = fmt; ctx.address = addr;
  ctx.channel = ch; ctx.text_bank = bank; ctx.text_status = st;
  ctx.text_len = len; if (len > 14) len = 14; memcpy(ctx.text_data, text, len);
}

/* Stream */
static void cb_endpoint_discovery(uint8_t maj, uint8_t min, uint8_t filter, void *c) {
  (void)c; ctx.called = 1; ctx.ump_maj = maj; ctx.ump_min = min; ctx.filter = filter;
}
static void cb_endpoint_info(uint8_t maj, uint8_t min, bool sfb, uint8_t nfb,
                                bool m2, bool m1, bool rxjr, bool txjr, void *c) {
  (void)c; ctx.called = 1; ctx.ump_maj = maj; ctx.ump_min = min;
  ctx.static_fb = sfb; ctx.num_fb = nfb; ctx.m2_proto = m2; ctx.m1_proto = m1;
  ctx.rx_jr = rxjr; ctx.tx_jr = txjr;
}
static void cb_device_identity(uint32_t mfr, uint16_t fam, uint16_t mod, uint32_t ver, void *c) {
  (void)c; ctx.called = 1; ctx.mfr_id = mfr; ctx.family_id = fam; ctx.model_id = mod; ctx.version_id = ver;
}
static void cb_stream_text(uint16_t st, uint8_t fmt, const uint8_t *d, uint8_t len, void *c) {
  (void)c; ctx.called = 1; ctx.stream_status = st; ctx.text_format = fmt;
  ctx.text_len = len; if (len > 14) len = 14; memcpy(ctx.text_data, d, len);
}
static void cb_fb_name(uint8_t fmt, uint8_t fb, const uint8_t *name, uint8_t len, void *c) {
  (void)c; ctx.called = 1; ctx.text_format = fmt; ctx.fb_num = fb;
  ctx.text_len = len; if (len > 13) len = 13; memcpy(ctx.text_data, name, len);
}
static void cb_config(uint8_t proto, bool rxjr, bool txjr, void *c) {
  (void)c; ctx.called = 1; ctx.proto = proto; ctx.rx_jr = rxjr; ctx.tx_jr = txjr;
}
static void cb_fb_discovery(uint8_t fb, uint8_t filter, void *c) {
  (void)c; ctx.called = 1; ctx.fb_num = fb; ctx.filter = filter;
}
static void cb_fb_info(bool act, uint8_t fb, uint8_t dir, uint8_t first, uint8_t ngrp,
                          uint8_t ci, uint8_t s8, uint8_t proto, void *c) {
  (void)c; (void)ci; (void)s8;
  ctx.called = 1; ctx.fb_active = act; ctx.fb_num = fb; ctx.fb_dir = dir;
  ctx.fb_first = first; ctx.fb_ngrp = ngrp; ctx.proto = proto;
}
static void cb_clip(bool start, void *c) { (void)c; ctx.called = 1; ctx.clip_start = start; }
static void cb_unknown(const uint32_t *w, uint8_t wc, void *c) {
  (void)c; (void)w; ctx.called = 1; ctx.unknown_wc = wc;
}

/*--------------------------------------------------------------------+
 * Helper: create dispatch with all callbacks wired
 *--------------------------------------------------------------------*/
static midi2_dispatch make_dp(void) {
  midi2_dispatch dp;
  midi2_dispatch_init(&dp);
  dp.context = NULL;
  /* Utility */
  dp.on_noop = cb_noop;
  dp.on_jr_clock = cb_jr_clock;
  dp.on_jr_timestamp = cb_jr_timestamp;
  dp.on_dctpq = cb_dctpq;
  dp.on_dc = cb_dc;
  /* System */
  dp.on_system = cb_system;
  /* CV1 */
  dp.on_cv1_note_on = cb_cv1_note_on;
  dp.on_cv1_note_off = cb_cv1_note_on; /* same signature */
  dp.on_cv1_cc = cb_cv1_cc;
  dp.on_cv1_program = cb_cv1_program;
  dp.on_cv1_chan_pressure = cb_cv1_pressure;
  dp.on_cv1_poly_pressure = cb_cv1_poly_pressure;
  dp.on_cv1_pitch_bend = cb_cv1_pitch_bend;
  /* CV2 */
  dp.on_note_on = cb_note_on;
  dp.on_note_off = cb_note_off;
  dp.on_cc = cb_cc;
  dp.on_program = cb_program;
  dp.on_pitch_bend = cb_pitch_bend;
  dp.on_rpn = cb_rpn;
  dp.on_nrpn = cb_rpn;
  dp.on_rel_rpn = cb_rpn;
  dp.on_rel_nrpn = cb_rpn;
  dp.on_per_note_mgmt = cb_per_note_mgmt;
  dp.on_reg_per_note = cb_per_note_ctrl;
  dp.on_asn_per_note = cb_per_note_ctrl;
  dp.on_poly_pressure = cb_poly_pressure;
  dp.on_chan_pressure = cb_chan_pressure;
  dp.on_per_note_pb = (midi2_dp_per_note_pb_cb)cb_poly_pressure; /* same signature */
  /* SysEx */
  dp.on_sysex7 = cb_sysex7;
  dp.on_sysex8 = cb_sysex8;
  dp.on_mds_header = cb_mds_header;
  dp.on_mds_payload = cb_mds_payload;
  /* Flex */
  dp.on_tempo = cb_tempo;
  dp.on_time_sig = cb_time_sig;
  dp.on_metronome = cb_metronome;
  dp.on_key_sig = cb_key_sig;
  dp.on_chord = cb_chord;
  dp.on_flex_text = cb_flex_text;
  /* Stream */
  dp.on_endpoint_discovery = cb_endpoint_discovery;
  dp.on_endpoint_info = cb_endpoint_info;
  dp.on_device_identity = cb_device_identity;
  dp.on_stream_text = cb_stream_text;
  dp.on_fb_name = cb_fb_name;
  dp.on_config_request = cb_config;
  dp.on_config_notify = cb_config;
  dp.on_fb_discovery = cb_fb_discovery;
  dp.on_fb_info = cb_fb_info;
  dp.on_clip = cb_clip;
  dp.on_unknown = cb_unknown;
  return dp;
}

/*--------------------------------------------------------------------+
 * Tests: Utility (MT 0x0)
 *--------------------------------------------------------------------*/
void test_dp_jr_clock(void) {
  TEST("dispatch: JR Clock group=1 ts=5000");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w = midi2_msg_jr_clock(1, 5000);
  midi2_dispatch_feed(&w, 1, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.group == 1, "group");
  CHECK(ctx.timestamp == 5000, "timestamp");
  PASS();
}

void test_dp_dctpq(void) {
  TEST("dispatch: DCTPQ tpq=480");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w = midi2_msg_dctpq(480);
  midi2_dispatch_feed(&w, 1, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.dctpq == 480, "tpq");
  PASS();
}

void test_dp_dc(void) {
  TEST("dispatch: Delta Clockstamp ticks=100000");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w = midi2_msg_delta_clockstamp(100000);
  midi2_dispatch_feed(&w, 1, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.dc_ticks == 100000, "ticks");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: System (MT 0x1)
 *--------------------------------------------------------------------*/
void test_dp_system(void) {
  TEST("dispatch: System Timing Clock group=2");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w = midi2_msg_system(2, 0xF8);
  midi2_dispatch_feed(&w, 1, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.group == 2, "group");
  CHECK(ctx.status == 0xF8, "status");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: CV1 (MT 0x2)
 *--------------------------------------------------------------------*/
void test_dp_cv1_note_on(void) {
  TEST("dispatch: CV1 Note On group=0 ch=5 note=60 vel=100");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w = midi2_msg_from_midi1(0, 0x95, 60, 100);
  midi2_dispatch_feed(&w, 1, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.group == 0, "group");
  CHECK(ctx.channel == 5, "channel");
  CHECK(ctx.note == 60, "note");
  CHECK(ctx.velocity == 100, "velocity");
  PASS();
}

void test_dp_cv1_pitch_bend(void) {
  TEST("dispatch: CV1 Pitch Bend ch=0 value=8192");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w = midi2_msg_from_midi1(0, 0xE0, 0x00, 0x40);
  midi2_dispatch_feed(&w, 1, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.velocity == 8192, "pitch bend center");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: CV2 (MT 0x4)
 *--------------------------------------------------------------------*/
void test_dp_note_on(void) {
  TEST("dispatch: Note On group=2 ch=9 note=36 vel=0xC000 attr=3:0x1234");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_note_on(w, 2, 9, 36, 0xC000, (3 << 8) | 0x34);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.group == 2, "group");
  CHECK(ctx.channel == 9, "channel");
  CHECK(ctx.note == 36, "note");
  CHECK(ctx.velocity == 0xC000, "velocity");
  PASS();
}

void test_dp_cc(void) {
  TEST("dispatch: CC index=74 value=0x80000000");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_cc(w, 0, 0, 74, 0x80000000);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.index == 74, "index");
  CHECK(ctx.value == 0x80000000, "value");
  PASS();
}

void test_dp_program(void) {
  TEST("dispatch: Program Change prog=5 bank=1/2");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_program(w, 0, 3, 5, true, 1, 2);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.channel == 3, "channel");
  CHECK(ctx.program == 5, "program");
  CHECK(ctx.bank_valid, "bank valid");
  CHECK(ctx.bank_msb == 1, "bank MSB");
  CHECK(ctx.bank_lsb == 2, "bank LSB");
  PASS();
}

void test_dp_rpn(void) {
  TEST("dispatch: RPN bank=0 index=7 value=0x02000000");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_rpn(w, 0, 0, 0, 7, 0x02000000);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.bank_msb == 0, "bank");
  CHECK(ctx.index == 7, "index");
  CHECK(ctx.value == 0x02000000, "value");
  PASS();
}

void test_dp_per_note_mgmt(void) {
  TEST("dispatch: Per-Note Mgmt note=60 detach=true reset=false");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_per_note_mgmt(w, 0, 0, 60, true, false);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.note == 60, "note");
  CHECK(ctx.detach == true, "detach");
  CHECK(ctx.reset == false, "reset");
  PASS();
}

void test_dp_reg_per_note(void) {
  TEST("dispatch: Reg Per-Note Ctrl note=48 idx=3 val=0x80000000");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_reg_per_note_ctrl(w, 0, 0, 48, 3, 0x80000000);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.note == 48, "note");
  CHECK(ctx.index == 3, "index");
  CHECK(ctx.value == 0x80000000, "value");
  PASS();
}

void test_dp_poly_pressure(void) {
  TEST("dispatch: Poly Pressure note=60 val=0x40000000");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_poly_pressure(w, 0, 0, 60, 0x40000000);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.note == 60, "note");
  CHECK(ctx.value == 0x40000000, "value");
  PASS();
}

void test_dp_chan_pressure(void) {
  TEST("dispatch: Channel Pressure val=0xFFFFFFFF");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_chan_pressure(w, 0, 0, 0xFFFFFFFF);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.value == 0xFFFFFFFF, "value");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: SysEx7 (MT 0x3)
 *--------------------------------------------------------------------*/
void test_dp_sysex7(void) {
  TEST("dispatch: SysEx7 complete 3 bytes");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  uint8_t data[] = {0x7E, 0x7F, 0x09};
  midi2_msg_sysex7_packet(w, 0, MIDI2_SYSEX7_COMPLETE, data, 3);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.sysex_len == 3, "len=3");
  CHECK(ctx.sysex_data[0] == 0x7E, "data[0]");
  CHECK(ctx.sysex_data[1] == 0x7F, "data[1]");
  CHECK(ctx.sysex_data[2] == 0x09, "data[2]");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: SysEx8 / MDS (MT 0x5)
 *--------------------------------------------------------------------*/
void test_dp_sysex8(void) {
  TEST("dispatch: SysEx8 complete 4 bytes stream_id=1");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
  midi2_msg_sysex8_packet(w, 0, MIDI2_SYSEX8_COMPLETE, 1, data, 4);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.sysex_stream_id == 1, "stream_id=1");
  CHECK(ctx.sysex_len == 4, "len=4");
  CHECK(ctx.sysex_data[0] == 0xAA, "data[0]");
  CHECK(ctx.sysex_data[3] == 0xDD, "data[3]");
  PASS();
}

void test_dp_mds_header(void) {
  TEST("dispatch: MDS Header mds_id=1 chunk 1/3");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_mds_header(w, 0, 1, 256, 3, 1, 0x007D, 0xFFFF, 0, 0);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.mds_id == 1, "mds_id");
  CHECK(ctx.mds_num_bytes == 256, "num_bytes");
  CHECK(ctx.mds_num_chunks == 3, "num_chunks");
  CHECK(ctx.mds_this_chunk == 1, "this_chunk");
  CHECK(ctx.mds_mfr == 0x007D, "mfr_id");
  CHECK(ctx.mds_device == 0xFFFF, "device_id");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: Flex Data (MT 0xD)
 *--------------------------------------------------------------------*/
void test_dp_tempo(void) {
  TEST("dispatch: Tempo 120 BPM (5000000)");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_tempo(w, 0, 5000000);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.tempo == 5000000, "tempo");
  PASS();
}

void test_dp_time_sig(void) {
  TEST("dispatch: Time Sig 6/8");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_time_sig(w, 0, 6, 3);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.num == 6, "numerator");
  CHECK(ctx.den == 3, "denominator");
  PASS();
}

void test_dp_key_sig(void) {
  TEST("dispatch: Key Sig D major (2 sharps, tonic=D)");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_key_sig_full(w, 0, 1, 0, 2, 4, 0); /* D=4, major=0 */
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.sharps_flats == 2, "sharps=2");
  CHECK(ctx.tonic == 4, "tonic=D");
  CHECK(ctx.key_type == 0, "major");
  PASS();
}

void test_dp_chord(void) {
  TEST("dispatch: Chord Bb minor");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_chord_name(w, 0, 1, 0,
                          -1, 2, 0x07,
                          0, 0, 0, 0, 0, 0, 0, 0,
                          -8, 0, 0x00, 0, 0, 0, 0);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.tonic_sf == -1, "tonic_sf=-1");
  CHECK(ctx.tonic_note == 2, "tonic=B");
  CHECK(ctx.chord_type == 0x07, "type=minor");
  PASS();
}

void test_dp_flex_text(void) {
  TEST("dispatch: Flex text copyright 'Test'");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  const uint8_t text[] = "Test";
  midi2_msg_flex_text(w, 0, 0, 1, 0, MIDI2_FLEX_BANK_METADATA,
                         MIDI2_FLEX_TEXT_COPYRIGHT, text, 4);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.text_bank == 0x01, "bank=metadata");
  CHECK(ctx.text_status == 0x04, "status=copyright");
  CHECK(ctx.text_len == 4, "len=4");
  CHECK(ctx.text_data[0] == 'T', "T");
  CHECK(ctx.text_data[3] == 't', "t");
  PASS();
}

void test_dp_metronome(void) {
  TEST("dispatch: Metronome 24 clicks, accent 4");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_metronome(w, 0, 24, 4, 0, 0, 2, 0);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.primary_clicks == 24, "clicks");
  CHECK(ctx.accent_1 == 4, "accent");
  CHECK(ctx.subdiv_1 == 2, "subdiv");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: Stream (MT 0xF)
 *--------------------------------------------------------------------*/
void test_dp_endpoint_discovery(void) {
  TEST("dispatch: Endpoint Discovery v1.1 filter=0x1F");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_stream_endpoint_discovery(w, 1, 1, 0x1F);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.ump_maj == 1, "major");
  CHECK(ctx.ump_min == 1, "minor");
  CHECK(ctx.filter == 0x1F, "filter");
  PASS();
}

void test_dp_endpoint_info(void) {
  TEST("dispatch: Endpoint Info static, 2 FB, M1+M2");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_stream_endpoint_info(w, 1, 1, true, 2, true, true, false, false);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.static_fb, "static FB");
  CHECK(ctx.num_fb == 2, "num_fb");
  CHECK(ctx.m2_proto, "MIDI 2.0");
  CHECK(ctx.m1_proto, "MIDI 1.0");
  PASS();
}

void test_dp_device_identity(void) {
  TEST("dispatch: Device Identity mfr=0xAABB ver=0xDEAD");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_stream_device_identity(w, 0x00AABB, 0x1234, 0x5678, 0xDEADBEEF);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.mfr_id == 0x00AABB, "mfr_id");
  CHECK(ctx.family_id == 0x1234, "family");
  CHECK(ctx.model_id == 0x5678, "model");
  CHECK(ctx.version_id == 0xDEADBEEF, "version");
  PASS();
}

void test_dp_endpoint_name(void) {
  TEST("dispatch: Endpoint Name 'Test'");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_stream_endpoint_name(w, 0, (const uint8_t *)"Test", 4);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.stream_status == MIDI2_STREAM_ENDPOINT_NAME, "status");
  CHECK(ctx.text_data[0] == 'T', "T");
  CHECK(ctx.text_data[3] == 't', "t");
  PASS();
}

void test_dp_config_request(void) {
  TEST("dispatch: Config Request MIDI 2.0");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_stream_config_request(w, 0x02);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.proto == 0x02, "protocol=MIDI2");
  PASS();
}

void test_dp_fb_info(void) {
  TEST("dispatch: FB Info active, bidir, groups 0-3");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_stream_fb_info(w, true, 0, 0x02, 0, 4, 2, false, 0x02);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.fb_active, "active");
  CHECK(ctx.fb_num == 0, "fb_num");
  CHECK(ctx.fb_dir == 0x02, "bidir");
  CHECK(ctx.fb_first == 0, "first_group");
  CHECK(ctx.fb_ngrp == 4, "num_groups");
  PASS();
}

void test_dp_clip(void) {
  TEST("dispatch: Start/End of Clip");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_stream_start_of_clip(w);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called && ctx.clip_start == true, "start");
  reset_ctx();
  midi2_msg_stream_end_of_clip(w);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called && ctx.clip_start == false, "end");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: additional Utility (MT 0x0)
 *--------------------------------------------------------------------*/
void test_dp_noop(void) {
  TEST("dispatch: NOOP");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w = 0x00000000; /* MT=0, status=0 */
  midi2_dispatch_feed(&w, 1, &dp);
  CHECK(ctx.called, "callback fired");
  PASS();
}

void test_dp_jr_timestamp(void) {
  TEST("dispatch: JR Timestamp group=3 ts=0xFFFF");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w = midi2_msg_jr_timestamp(3, 0xFFFF);
  midi2_dispatch_feed(&w, 1, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.group == 3, "group");
  CHECK(ctx.timestamp == 0xFFFF, "timestamp");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: additional CV1 (MT 0x2)
 *--------------------------------------------------------------------*/
void test_dp_cv1_note_off(void) {
  TEST("dispatch: CV1 Note Off ch=0 note=64 vel=80");
  midi2_dispatch dp = make_dp();
  dp.on_cv1_note_off = cb_cv1_note_on; /* same sig */
  reset_ctx();
  uint32_t w = midi2_msg_from_midi1(0, 0x80, 64, 80);
  midi2_dispatch_feed(&w, 1, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.note == 64, "note");
  CHECK(ctx.velocity == 80, "velocity");
  PASS();
}

void test_dp_cv1_cc(void) {
  TEST("dispatch: CV1 CC idx=7 val=100");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w = midi2_msg_from_midi1(0, 0xB0, 7, 100);
  midi2_dispatch_feed(&w, 1, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.index == 7, "index");
  CHECK(ctx.value == 100, "value");
  PASS();
}

void test_dp_cv1_program(void) {
  TEST("dispatch: CV1 Program Change prog=5");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w = midi2_msg_from_midi1(0, 0xC0, 5, 0);
  midi2_dispatch_feed(&w, 1, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.note == 5, "program");
  PASS();
}

void test_dp_cv1_chan_pressure(void) {
  TEST("dispatch: CV1 Channel Pressure val=100");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w = midi2_msg_from_midi1(0, 0xD0, 100, 0);
  midi2_dispatch_feed(&w, 1, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.value == 100, "value");
  PASS();
}

void test_dp_cv1_poly_pressure(void) {
  TEST("dispatch: CV1 Poly Pressure note=60 val=80");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w = midi2_msg_from_midi1(0, 0xA0, 60, 80);
  midi2_dispatch_feed(&w, 1, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.note == 60, "note");
  CHECK(ctx.value == 80, "value");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: additional CV2 (MT 0x4)
 *--------------------------------------------------------------------*/
void test_dp_note_off(void) {
  TEST("dispatch: Note Off group=0 ch=0 note=60 vel=0x8000");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_note_off(w, 0, 0, 60, 0x8000, 0);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.note == 60, "note");
  CHECK(ctx.velocity == 0x8000, "velocity");
  PASS();
}

void test_dp_pitch_bend(void) {
  TEST("dispatch: Pitch Bend center=0x80000000");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_pitch_bend(w, 0, 0, 0x80000000);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.value == 0x80000000, "value=center");
  PASS();
}

void test_dp_per_note_pb(void) {
  TEST("dispatch: Per-Note Pitch Bend note=60 val=0x80000000");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_per_note_pb(w, 0, 0, 60, 0x80000000);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.note == 60, "note");
  CHECK(ctx.value == 0x80000000, "value");
  PASS();
}

void test_dp_asn_per_note(void) {
  TEST("dispatch: Asn Per-Note Ctrl note=48 idx=7 val=0x40000000");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_asn_per_note_ctrl(w, 0, 0, 48, 7, 0x40000000);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.note == 48, "note");
  CHECK(ctx.index == 7, "index");
  CHECK(ctx.value == 0x40000000, "value");
  PASS();
}

void test_dp_nrpn(void) {
  TEST("dispatch: NRPN bank=1 idx=2 val=0x12345678");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_nrpn(w, 0, 0, 1, 2, 0x12345678);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.bank_msb == 1, "bank");
  CHECK(ctx.index == 2, "index");
  CHECK(ctx.value == 0x12345678, "value");
  PASS();
}

void test_dp_rel_rpn(void) {
  TEST("dispatch: Relative RPN bank=0 idx=7 val=0xFFFFFF00");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_rel_rpn(w, 0, 0, 0, 7, 0xFFFFFF00);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.value == 0xFFFFFF00, "value (negative)");
  PASS();
}

void test_dp_rel_nrpn(void) {
  TEST("dispatch: Relative NRPN bank=1 idx=3 val=0x00000100");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  midi2_msg_rel_nrpn(w, 0, 0, 1, 3, 0x00000100);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.value == 0x00000100, "value (positive)");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: SysEx7 status enum alignment
 *--------------------------------------------------------------------*/
void test_dp_sysex7_status_start(void) {
  TEST("dispatch: SysEx7 Start status matches enum");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2];
  uint8_t data[] = {0x01, 0x02, 0x03};
  midi2_msg_sysex7_packet(w, 0, MIDI2_SYSEX7_START, data, 3);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.sysex_status == MIDI2_SYSEX7_START, "status matches MIDI2_SYSEX7_START");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: SysEx8 status enum alignment
 *--------------------------------------------------------------------*/
void test_dp_sysex8_status(void) {
  TEST("dispatch: SysEx8 Complete status matches enum");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  uint8_t data[] = {0xAA};
  midi2_msg_sysex8_packet(w, 0, MIDI2_SYSEX8_COMPLETE, 0, data, 1);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.sysex_status == MIDI2_SYSEX8_COMPLETE, "status matches MIDI2_SYSEX8_COMPLETE");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: MDS Payload
 *--------------------------------------------------------------------*/
void test_dp_mds_payload(void) {
  TEST("dispatch: MDS Payload 14 bytes");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  uint8_t data[14];
  int i;
  for (i = 0; i < 14; i++) data[i] = (uint8_t)(0xA0 + i);
  midi2_msg_mds_payload(w, 0, 1, data, 14);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.mds_id == 1, "mds_id");
  CHECK(ctx.mds_data_len == 14, "len=14");
  CHECK(ctx.mds_data[0] == 0xA0, "data[0]");
  CHECK(ctx.mds_data[13] == 0xAD, "data[13]");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: Config with JR bits
 *--------------------------------------------------------------------*/
void test_dp_config_with_jr(void) {
  TEST("dispatch: Config Request MIDI2 with rx_jr=true");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  memset(w, 0, 16);
  /* Build manually: MT=0xF, format=0, status=0x005, protocol=0x02, rxjr=1, txjr=0 */
  w[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x005 << 16) | ((uint32_t)0x02 << 8) | 0x02;
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.proto == 0x02, "protocol=MIDI2");
  CHECK(ctx.rx_jr == true, "rx_jr=true");
  CHECK(ctx.tx_jr == false, "tx_jr=false");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: FB Name separate callback
 *--------------------------------------------------------------------*/
void test_dp_fb_name(void) {
  TEST("dispatch: FB Name 'Piano' fb=1 (separate callback)");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_stream_fb_name(w, 0, 1, (const uint8_t *)"Piano", 5);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.fb_num == 1, "fb_num=1");
  CHECK(ctx.text_len == 5, "len=5");
  CHECK(ctx.text_data[0] == 'P', "P");
  CHECK(ctx.text_data[4] == 'o', "o");
  PASS();
}

void test_dp_fb_discovery(void) {
  TEST("dispatch: FB Discovery all");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_stream_fb_discovery(w, 0xFF, 0x03);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.fb_num == 0xFF, "fb=all");
  CHECK(ctx.filter == 0x03, "filter=info+name");
  PASS();
}

void test_dp_product_id(void) {
  TEST("dispatch: Product Instance ID 'XYZ123'");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_stream_product_id(w, 0, (const uint8_t *)"XYZ123", 6);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.stream_status == MIDI2_STREAM_PRODUCT_INSTANCE_ID, "status");
  CHECK(ctx.text_data[0] == 'X', "X");
  CHECK(ctx.text_data[5] == '3', "3");
  PASS();
}

void test_dp_config_notify(void) {
  TEST("dispatch: Config Notify MIDI 1.0");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[4];
  midi2_msg_stream_config_notify(w, 0x01);
  midi2_dispatch_feed(w, 4, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.proto == 0x01, "protocol=MIDI1");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: NULL callbacks are skipped
 *--------------------------------------------------------------------*/
void test_dp_null_callback(void) {
  TEST("dispatch: NULL callback does not crash");
  midi2_dispatch dp;
  midi2_dispatch_init(&dp); /* all NULL */
  uint32_t w[4];
  midi2_msg_note_on(w, 0, 0, 60, 0xC000, 0);
  midi2_dispatch_feed(w, 2, &dp); /* should not crash */
  midi2_msg_tempo(w, 0, 5000000);
  midi2_dispatch_feed(w, 4, &dp); /* should not crash */
  midi2_msg_stream_endpoint_discovery(w, 1, 1, 0x1F);
  midi2_dispatch_feed(w, 4, &dp); /* should not crash */
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: Unknown MT goes to fallback
 *--------------------------------------------------------------------*/
void test_dp_unknown(void) {
  TEST("dispatch: unknown MT 0x6 -> on_unknown");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  uint32_t w[2] = { 0x60000000, 0x00000000 };
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "callback fired");
  CHECK(ctx.unknown_wc == 2, "word_count=2");
  PASS();
}

/*--------------------------------------------------------------------+
 * Tests: proc compatibility (feed signature)
 *--------------------------------------------------------------------*/
void test_dp_proc_compat(void) {
  TEST("dispatch: feed signature compatible with midi2_proc on_ump");
  midi2_dispatch dp = make_dp();
  reset_ctx();
  /* midi2_proc_ump_cb is: void (*)(const uint32_t*, uint8_t, void*)
   * midi2_dispatch_feed is: void (const uint32_t*, uint8_t, void*)
   * Should be assignable. Test by calling directly. */
  uint32_t w[2];
  midi2_msg_note_on(w, 0, 0, 60, 0xFFFF, 0);
  midi2_dispatch_feed(w, 2, &dp);
  CHECK(ctx.called, "dispatched via feed");
  CHECK(ctx.note == 60, "note=60");
  PASS();
}

/*--------------------------------------------------------------------+
 * Upscale MT 0x2 -> MT 0x4
 *--------------------------------------------------------------------*/

void test_dp_upscale_note_on(void) {
  TEST("upscale: MT 0x2 Note On -> MT 0x4 on_note_on callback");
  reset_ctx();
  midi2_dispatch dp;
  midi2_dispatch_init(&dp);
  dp.context = NULL;
  dp.upscale_mt2 = true;
  dp.on_note_on = cb_note_on;

  /* Feed a MT 0x2 Note On: ch=0, note=60, vel=100 */
  uint32_t mt2 = midi2_msg_from_midi1(0, 0x90, 60, 100);
  midi2_dispatch_feed(&mt2, 1, &dp);

  CHECK(ctx.called, "on_note_on called");
  CHECK(ctx.note == 60, "note=60");
  CHECK(ctx.velocity == midi2_msg_scale_up_7to16(100), "velocity scaled");
  PASS();
}

void test_dp_upscale_note_on_vel0(void) {
  TEST("upscale: MT 0x2 Note On vel=0 -> on_note_off");
  reset_ctx();
  midi2_dispatch dp;
  midi2_dispatch_init(&dp);
  dp.context = NULL;
  dp.upscale_mt2 = true;
  dp.on_note_off = cb_note_on; /* reuse: same signature captures note */

  uint32_t mt2 = midi2_msg_from_midi1(0, 0x90, 60, 0);
  midi2_dispatch_feed(&mt2, 1, &dp);

  CHECK(ctx.called, "on_note_off called");
  CHECK(ctx.note == 60, "note=60");
  PASS();
}

void test_dp_upscale_cc(void) {
  TEST("upscale: MT 0x2 CC#7 val=100 -> on_cc 32-bit");
  reset_ctx();
  midi2_dispatch dp;
  midi2_dispatch_init(&dp);
  dp.context = NULL;
  dp.upscale_mt2 = true;
  dp.on_cc = cb_cc;

  uint32_t mt2 = midi2_msg_from_midi1(0, 0xB0, 7, 100);
  midi2_dispatch_feed(&mt2, 1, &dp);

  CHECK(ctx.called, "on_cc called");
  CHECK(ctx.index == 7, "index=7");
  CHECK(ctx.value == midi2_msg_scale_up_7to32(100), "value scaled");
  PASS();
}

void test_dp_upscale_pitch_bend(void) {
  TEST("upscale: MT 0x2 Pitch Bend center -> on_pitch_bend 32-bit");
  reset_ctx();
  midi2_dispatch dp;
  midi2_dispatch_init(&dp);
  dp.context = NULL;
  dp.upscale_mt2 = true;
  dp.on_pitch_bend = cb_pitch_bend;

  /* Center: LSB=0x00, MSB=0x40 -> 14-bit 0x2000 */
  uint32_t mt2 = midi2_msg_from_midi1(0, 0xE0, 0x00, 0x40);
  midi2_dispatch_feed(&mt2, 1, &dp);

  CHECK(ctx.called, "on_pitch_bend called");
  CHECK(ctx.value == midi2_msg_scale_up_14to32(0x2000), "center scaled");
  PASS();
}

void test_dp_upscale_off_by_default(void) {
  TEST("upscale: off by default, MT 0x2 -> on_cv1_note_on");
  reset_ctx();
  midi2_dispatch dp;
  midi2_dispatch_init(&dp);
  dp.context = NULL;
  /* upscale_mt2 is false (default from init) */
  dp.on_cv1_note_on = cb_cv1_note_on;
  dp.on_note_on = cb_note_on;

  uint32_t mt2 = midi2_msg_from_midi1(0, 0x90, 60, 100);
  midi2_dispatch_feed(&mt2, 1, &dp);

  CHECK(ctx.called, "cv1 callback called");
  CHECK(ctx.note == 60, "note=60");
  CHECK(ctx.velocity == 100, "velocity=100 (7-bit, not scaled)");
  PASS();
}

/*--------------------------------------------------------------------+
 * Main
 *--------------------------------------------------------------------*/
int main(void) {
  printf("\n=== midi2_dispatch Unit Tests ===\n\n");

  printf("[Utility MT 0x0]\n");
  test_dp_noop();
  test_dp_jr_clock();
  test_dp_jr_timestamp();
  test_dp_dctpq();
  test_dp_dc();

  printf("\n[System MT 0x1]\n");
  test_dp_system();

  printf("\n[MIDI 1.0 CV MT 0x2]\n");
  test_dp_cv1_note_on();
  test_dp_cv1_note_off();
  test_dp_cv1_cc();
  test_dp_cv1_program();
  test_dp_cv1_chan_pressure();
  test_dp_cv1_poly_pressure();
  test_dp_cv1_pitch_bend();

  printf("\n[MIDI 2.0 CV MT 0x4]\n");
  test_dp_note_on();
  test_dp_note_off();
  test_dp_cc();
  test_dp_program();
  test_dp_pitch_bend();
  test_dp_rpn();
  test_dp_nrpn();
  test_dp_rel_rpn();
  test_dp_rel_nrpn();
  test_dp_per_note_pb();
  test_dp_per_note_mgmt();
  test_dp_reg_per_note();
  test_dp_asn_per_note();
  test_dp_poly_pressure();
  test_dp_chan_pressure();

  printf("\n[SysEx7 MT 0x3]\n");
  test_dp_sysex7();
  test_dp_sysex7_status_start();

  printf("\n[SysEx8 / MDS MT 0x5]\n");
  test_dp_sysex8();
  test_dp_sysex8_status();
  test_dp_mds_header();
  test_dp_mds_payload();

  printf("\n[Flex Data MT 0xD]\n");
  test_dp_tempo();
  test_dp_time_sig();
  test_dp_key_sig();
  test_dp_chord();
  test_dp_flex_text();
  test_dp_metronome();

  printf("\n[Stream MT 0xF]\n");
  test_dp_endpoint_discovery();
  test_dp_endpoint_info();
  test_dp_device_identity();
  test_dp_endpoint_name();
  test_dp_product_id();
  test_dp_config_request();
  test_dp_config_with_jr();
  test_dp_config_notify();
  test_dp_fb_discovery();
  test_dp_fb_info();
  test_dp_fb_name();
  test_dp_clip();

  printf("\n[Edge Cases]\n");
  test_dp_null_callback();
  test_dp_unknown();
  test_dp_proc_compat();

  printf("\n[Upscale MT 0x2 -> MT 0x4]\n");
  test_dp_upscale_note_on();
  test_dp_upscale_note_on_vel0();
  test_dp_upscale_cc();
  test_dp_upscale_pitch_bend();
  test_dp_upscale_off_by_default();

  printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
