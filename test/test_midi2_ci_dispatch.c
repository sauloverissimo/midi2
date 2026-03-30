/*
 * midi2_ci_dispatch unit tests
 * Compile: gcc -std=c99 -I../src test_midi2_ci_dispatch.c ../src/midi2_ci_dispatch.c -o test_midi2_ci_dispatch
 */

#include <stdio.h>
#include <string.h>
#include "midi2_ci_dispatch.h"

static int passed = 0;
static int failed = 0;

#define TEST(name) printf("  %-55s ", name)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* Capture context */
static struct {
  int called;
  midi2_ci_header hdr;
  uint32_t mfr_id;
  uint16_t family, model;
  uint32_t sw_rev, max_sysex;
  uint8_t ci_cat, output_path, function_block;
  uint32_t target_muid;
  uint8_t orig_sub_id, status_code;
  uint8_t profile_id[5];
  uint16_t num_channels;
  uint16_t enabled_count, disabled_count;
  uint8_t request_id;
  uint16_t header_len, body_len;
  uint16_t num_chunks, this_chunk;
  uint8_t max_simultaneous, pe_maj, pe_min;
  uint8_t supported_features;
  uint8_t msg_data_control, system_bm, chan_ctrl_bm, note_data_bm;
  uint8_t sub_id;
  uint8_t status;
  uint16_t info_len;
} ctx;

static void reset(void) { memset(&ctx, 0, sizeof(ctx)); }

/* Management callbacks */
static void cb_discovery(midi2_ci_header h, uint32_t mfr, uint16_t fam, uint16_t mod,
                            uint32_t sw, uint8_t cat, uint32_t mx, uint8_t op, void *c) {
  (void)c; ctx.called = 1; ctx.hdr = h; ctx.mfr_id = mfr; ctx.family = fam;
  ctx.model = mod; ctx.sw_rev = sw; ctx.ci_cat = cat; ctx.max_sysex = mx; ctx.output_path = op;
}
static void cb_discovery_reply(midi2_ci_header h, uint32_t mfr, uint16_t fam, uint16_t mod,
                                  uint32_t sw, uint8_t cat, uint32_t mx, uint8_t op, uint8_t fb, void *c) {
  (void)c; ctx.called = 1; ctx.hdr = h; ctx.mfr_id = mfr; ctx.family = fam;
  ctx.model = mod; ctx.sw_rev = sw; ctx.ci_cat = cat; ctx.max_sysex = mx;
  ctx.output_path = op; ctx.function_block = fb;
}
static void cb_endpoint_info(midi2_ci_header h, uint8_t st, void *c) {
  (void)c; ctx.called = 1; ctx.hdr = h; ctx.status = st;
}
static void cb_endpoint_info_reply(midi2_ci_header h, uint8_t st, const uint8_t *d, uint16_t l, void *c) {
  (void)c; (void)d; ctx.called = 1; ctx.hdr = h; ctx.status = st; ctx.info_len = l;
}
static void cb_invalidate(midi2_ci_header h, uint32_t target, void *c) {
  (void)c; ctx.called = 1; ctx.hdr = h; ctx.target_muid = target;
}
static void cb_nak(midi2_ci_header h, uint8_t orig, uint8_t sc, uint8_t sd,
                      const uint8_t *det, uint16_t ml, const uint8_t *mt, void *c) {
  (void)c; (void)det; (void)ml; (void)mt; (void)sd;
  ctx.called = 1; ctx.hdr = h; ctx.orig_sub_id = orig; ctx.status_code = sc;
}
static void cb_ack(midi2_ci_header h, uint8_t orig, uint8_t sc, uint8_t sd,
                      const uint8_t *det, uint16_t ml, const uint8_t *mt, void *c) {
  (void)c; (void)det; (void)ml; (void)mt; (void)sd;
  ctx.called = 1; ctx.hdr = h; ctx.orig_sub_id = orig; ctx.status_code = sc;
}

/* Profile callbacks */
static void cb_profile_inquiry(midi2_ci_header h, void *c) {
  (void)c; ctx.called = 1; ctx.hdr = h;
}
static void cb_profile_inquiry_reply(midi2_ci_header h, uint16_t en, const uint8_t *ed,
                                        uint16_t dis, const uint8_t *dd, void *c) {
  (void)c; (void)ed; (void)dd;
  ctx.called = 1; ctx.hdr = h; ctx.enabled_count = en; ctx.disabled_count = dis;
}
static void cb_set_profile(midi2_ci_header h, const uint8_t *pid, uint16_t nch, void *c) {
  (void)c; ctx.called = 1; ctx.hdr = h; memcpy(ctx.profile_id, pid, 5); ctx.num_channels = nch;
}
static void cb_profile_report(midi2_ci_header h, const uint8_t *pid, uint16_t nch, void *c) {
  (void)c; ctx.called = 1; ctx.hdr = h; memcpy(ctx.profile_id, pid, 5); ctx.num_channels = nch;
}
static void cb_profile_added(midi2_ci_header h, const uint8_t *pid, void *c) {
  (void)c; ctx.called = 1; ctx.hdr = h; memcpy(ctx.profile_id, pid, 5);
}

/* PE callbacks */
static void cb_pe_caps(midi2_ci_header h, uint8_t ms, uint8_t maj, uint8_t min, void *c) {
  (void)c; ctx.called = 1; ctx.hdr = h; ctx.max_simultaneous = ms; ctx.pe_maj = maj; ctx.pe_min = min;
}
static void cb_pe_data(midi2_ci_header h, uint8_t rid, const uint8_t *hd, uint16_t hl,
                          uint16_t nc, uint16_t tc, const uint8_t *bd, uint16_t bl, void *c) {
  (void)c; (void)hd; (void)bd;
  ctx.called = 1; ctx.hdr = h; ctx.request_id = rid; ctx.header_len = hl;
  ctx.num_chunks = nc; ctx.this_chunk = tc; ctx.body_len = bl;
}

/* PI callbacks */
static void cb_pi_caps(midi2_ci_header h, void *c) { (void)c; ctx.called = 1; ctx.hdr = h; }
static void cb_pi_caps_reply(midi2_ci_header h, uint8_t f, void *c) {
  (void)c; ctx.called = 1; ctx.hdr = h; ctx.supported_features = f;
}
static void cb_pi_report(midi2_ci_header h, uint8_t mdc, uint8_t sb, uint8_t ccb, uint8_t ndb, void *c) {
  (void)c; ctx.called = 1; ctx.hdr = h; ctx.msg_data_control = mdc;
  ctx.system_bm = sb; ctx.chan_ctrl_bm = ccb; ctx.note_data_bm = ndb;
}
static void cb_pi_report_reply(midi2_ci_header h, uint8_t sb, uint8_t ccb, uint8_t ndb, void *c) {
  (void)c; ctx.called = 1; ctx.hdr = h; ctx.system_bm = sb; ctx.chan_ctrl_bm = ccb; ctx.note_data_bm = ndb;
}
static void cb_pi_end(midi2_ci_header h, void *c) { (void)c; ctx.called = 1; ctx.hdr = h; }
static void cb_unknown(midi2_ci_header h, uint8_t sid, const uint8_t *d, uint16_t l, void *c) {
  (void)c; (void)d; (void)l; ctx.called = 1; ctx.hdr = h; ctx.sub_id = sid;
}

/* Wire all callbacks */
static midi2_ci_dispatch make_dp(void) {
  midi2_ci_dispatch dp;
  midi2_ci_dispatch_init(&dp);
  dp.on_discovery = cb_discovery;
  dp.on_discovery_reply = cb_discovery_reply;
  dp.on_endpoint_info = cb_endpoint_info;
  dp.on_endpoint_info_reply = cb_endpoint_info_reply;
  dp.on_invalidate_muid = cb_invalidate;
  dp.on_nak = cb_nak;
  dp.on_ack = cb_ack;
  dp.on_profile_inquiry = cb_profile_inquiry;
  dp.on_profile_inquiry_reply = cb_profile_inquiry_reply;
  dp.on_set_profile_on = cb_set_profile;
  dp.on_set_profile_off = cb_set_profile;
  dp.on_profile_enabled = cb_profile_report;
  dp.on_profile_disabled = cb_profile_report;
  dp.on_profile_added = cb_profile_added;
  dp.on_profile_removed = cb_profile_added;
  dp.on_pe_capability = cb_pe_caps;
  dp.on_pe_capability_reply = cb_pe_caps;
  dp.on_pe_get = cb_pe_data;
  dp.on_pe_get_reply = cb_pe_data;
  dp.on_pe_set = cb_pe_data;
  dp.on_pe_set_reply = cb_pe_data;
  dp.on_pe_subscribe = cb_pe_data;
  dp.on_pe_subscribe_reply = cb_pe_data;
  dp.on_pe_notify = cb_pe_data;
  dp.on_pi_capability = cb_pi_caps;
  dp.on_pi_capability_reply = cb_pi_caps_reply;
  dp.on_pi_midi_report = cb_pi_report;
  dp.on_pi_midi_report_reply = cb_pi_report_reply;
  dp.on_pi_midi_report_end = cb_pi_end;
  dp.on_unknown = cb_unknown;
  return dp;
}

/*--------------------------------------------------------------------+
 * Tests
 *--------------------------------------------------------------------*/

void test_ci_dp_discovery(void) {
  TEST("CI dispatch: Discovery v2");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[64];
  midi2_ci_build_discovery(buf, 0x02, 0x1234567, 0x002109, 0x0001, 0x0002, 0x00010000, 0x0C, 512, 3);
  bool ok = midi2_ci_dispatch_feed(&dp, 0, buf, 30);
  CHECK(ok && ctx.called, "dispatched");
  CHECK(ctx.hdr.src_muid == 0x1234567, "src_muid");
  CHECK(ctx.mfr_id == 0x002109, "mfr_id");
  CHECK(ctx.family == 0x0001, "family");
  CHECK(ctx.model == 0x0002, "model");
  CHECK(ctx.ci_cat == 0x0C, "ci_cat");
  CHECK(ctx.max_sysex == 512, "max_sysex");
  CHECK(ctx.output_path == 3, "output_path");
  PASS();
}

void test_ci_dp_discovery_reply(void) {
  TEST("CI dispatch: Discovery Reply with FB");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[64];
  midi2_ci_build_discovery_reply(buf, 0x02, 0x7654321, 0x1234567, 0x002109, 0x0001, 0x0002, 0x00010000, 0x0C, 512, 0, 0x02);
  midi2_ci_dispatch_feed(&dp, 0, buf, 31);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.function_block == 0x02, "function_block");
  PASS();
}

void test_ci_dp_endpoint_info(void) {
  TEST("CI dispatch: Endpoint Info inquiry + reply");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[64];
  midi2_ci_build_endpoint_info(buf, 0x02, 0x1234567, 0x7654321, 0x00);
  midi2_ci_dispatch_feed(&dp, 0, buf, 14);
  CHECK(ctx.called && ctx.status == 0x00, "inquiry status=0");

  reset();
  uint16_t len = midi2_ci_build_endpoint_info_reply(buf, 0x02, 0x7654321, 0x1234567, 0x00, (const uint8_t*)"SN1", 3);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called && ctx.info_len == 3, "reply info_len=3");
  PASS();
}

void test_ci_dp_invalidate_muid(void) {
  TEST("CI dispatch: Invalidate MUID");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[32];
  midi2_ci_build_invalidate_muid(buf, 0x02, 0x1234567, 0x7654321);
  midi2_ci_dispatch_feed(&dp, 0, buf, 17);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.target_muid == 0x7654321, "target");
  PASS();
}

void test_ci_dp_nak(void) {
  TEST("CI dispatch: NAK v2 not_supported");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_nak(buf, 0x02, 0x1234567, 0x7654321, 0x7F, 0x34, 0x01, 0x00, NULL, 0, NULL);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.orig_sub_id == 0x34, "orig=PE Get");
  CHECK(ctx.status_code == 0x01, "not_supported");
  PASS();
}

void test_ci_dp_ack(void) {
  TEST("CI dispatch: ACK ok");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_ack(buf, 0x02, 0x1234567, 0x7654321, 0x7F, 0x36, 0x00, 0x00, NULL, 0, NULL);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.orig_sub_id == 0x36, "orig=PE Set");
  CHECK(ctx.status_code == 0x00, "ok");
  PASS();
}

void test_ci_dp_profile_inquiry(void) {
  TEST("CI dispatch: Profile Inquiry");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[32];
  midi2_ci_build_profile_inquiry(buf, 0x02, 0x1234567, 0x7654321, 0x7F);
  midi2_ci_dispatch_feed(&dp, 0, buf, 13);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.hdr.device_id == 0x7F, "device_id=FB");
  PASS();
}

void test_ci_dp_profile_reply(void) {
  TEST("CI dispatch: Profile Inquiry Reply 2en+1dis");
  midi2_ci_dispatch dp = make_dp();
  reset();
  const uint8_t en[2][5] = {{0x7E, 1, 0, 0, 0}, {0x7E, 2, 0, 0, 0}};
  const uint8_t dis[1][5] = {{0x7E, 3, 0, 0, 0}};
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_profile_inquiry_reply(buf, 0x02, 0x7654321, 0x1234567, 0x7F, en, 2, dis, 1);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.enabled_count == 2, "en=2");
  CHECK(ctx.disabled_count == 1, "dis=1");
  PASS();
}

void test_ci_dp_set_profile_on(void) {
  TEST("CI dispatch: Set Profile On");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t prof[5] = {0x7E, 1, 0, 0, 0};
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_set_profile_on(buf, 0x02, 0x1234567, 0x7654321, 0x00, prof, 3);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.profile_id[1] == 1, "profile");
  CHECK(ctx.num_channels == 3, "channels=3");
  PASS();
}

void test_ci_dp_profile_enabled(void) {
  TEST("CI dispatch: Profile Enabled Report");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t prof[5] = {0x7E, 5, 0, 0, 0};
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_profile_enabled(buf, 0x02, 0x1234567, 0x7F, prof, 1);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.profile_id[1] == 5, "profile");
  PASS();
}

void test_ci_dp_profile_added(void) {
  TEST("CI dispatch: Profile Added Report");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t prof[5] = {0x7E, 7, 0, 0, 0};
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_profile_added(buf, 0x02, 0x1234567, 0x7F, prof);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.profile_id[1] == 7, "profile");
  PASS();
}

void test_ci_dp_pe_capability(void) {
  TEST("CI dispatch: PE Capability");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[32];
  midi2_ci_build_pe_capability(buf, 0x02, 0x1234567, 0x7654321, 3, 0, 0);
  midi2_ci_dispatch_feed(&dp, 0, buf, 16);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.max_simultaneous == 3, "max_sim=3");
  PASS();
}

void test_ci_dp_pe_get(void) {
  TEST("CI dispatch: PE Get with header");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[64];
  const uint8_t hdr[] = "{\"resource\":\"X\"}";
  uint16_t len = midi2_ci_build_pe_get(buf, 0x02, 0x1234567, 0x7654321, 5, hdr, 16);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.request_id == 5, "req_id=5");
  CHECK(ctx.header_len == 16, "hdr_len=16");
  CHECK(ctx.num_chunks == 1, "chunks=1");
  CHECK(ctx.body_len == 0, "body=0");
  PASS();
}

void test_ci_dp_pe_get_reply_chunked(void) {
  TEST("CI dispatch: PE Get Reply chunk 2/3");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_pe_get_reply(buf, 0x02, 0x7654321, 0x1234567, 1, NULL, 0, 3, 2, (const uint8_t*)"data", 4);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.num_chunks == 3, "chunks=3");
  CHECK(ctx.this_chunk == 2, "this=2");
  CHECK(ctx.body_len == 4, "body=4");
  PASS();
}

void test_ci_dp_pi_capability(void) {
  TEST("CI dispatch: PI Capability");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[32];
  midi2_ci_build_pi_capability(buf, 0x02, 0x1234567, 0x7654321);
  midi2_ci_dispatch_feed(&dp, 0, buf, 13);
  CHECK(ctx.called, "dispatched");
  PASS();
}

void test_ci_dp_pi_report(void) {
  TEST("CI dispatch: PI MIDI Report");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[32];
  midi2_ci_build_pi_midi_report(buf, 0x02, 0x1234567, 0x7654321, 0x7F, 0x01, 0x07, 0x00, 0x3F, 0x1F);
  midi2_ci_dispatch_feed(&dp, 0, buf, 18);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.msg_data_control == 0x01, "mdc");
  CHECK(ctx.system_bm == 0x07, "sys");
  CHECK(ctx.chan_ctrl_bm == 0x3F, "chan");
  CHECK(ctx.note_data_bm == 0x1F, "note");
  PASS();
}

void test_ci_dp_pi_end(void) {
  TEST("CI dispatch: PI MIDI Report End");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[32];
  midi2_ci_build_pi_midi_report_end(buf, 0x02, 0x7654321, 0x1234567, 0x7F);
  midi2_ci_dispatch_feed(&dp, 0, buf, 13);
  CHECK(ctx.called, "dispatched");
  PASS();
}

void test_ci_dp_set_profile_off(void) {
  TEST("CI dispatch: Set Profile Off (nch always 0)");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t prof[5] = {0x7E, 2, 0, 0, 0};
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_set_profile_off(buf, 0x02, 0x1234567, 0x7654321, 0x7F, prof);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.profile_id[1] == 2, "profile");
  CHECK(ctx.num_channels == 0, "nch=0 for off");
  PASS();
}

void test_ci_dp_profile_disabled(void) {
  TEST("CI dispatch: Profile Disabled Report");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t prof[5] = {0x7E, 3, 0, 0, 0};
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_profile_disabled(buf, 0x02, 0x1234567, 0x7F, prof, 0);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.profile_id[1] == 3, "profile");
  PASS();
}

void test_ci_dp_profile_removed(void) {
  TEST("CI dispatch: Profile Removed Report");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t prof[5] = {0x7E, 4, 0, 0, 0};
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_profile_removed(buf, 0x02, 0x1234567, 0x7F, prof);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.profile_id[1] == 4, "profile");
  PASS();
}

void test_ci_dp_profile_details(void) {
  TEST("CI dispatch: Profile Details Inquiry + Reply");
  midi2_ci_dispatch dp = make_dp();
  dp.on_profile_details = (midi2_ci_dp_profile_details_cb)cb_profile_added; /* simplified */
  reset();
  uint8_t prof[5] = {0x7E, 1, 0, 0, 0};
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_profile_details(buf, 0x02, 0x1234567, 0x7654321, 0x7F, prof, 0x01);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "details dispatched");
  PASS();
}

void test_ci_dp_profile_specific(void) {
  TEST("CI dispatch: Profile Specific Data");
  midi2_ci_dispatch dp = make_dp();
  dp.on_profile_specific_data = (midi2_ci_dp_profile_specific_cb)cb_profile_added; /* simplified */
  reset();
  uint8_t prof[5] = {0x7E, 1, 0, 0, 0};
  uint8_t buf[64];
  uint8_t data[10]; memset(data, 0x42, 10);
  uint16_t len = midi2_ci_build_profile_specific_data(buf, 0x02, 0x1234567, 0x7654321, 0x7F, prof, data, 10);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "specific data dispatched");
  PASS();
}

void test_ci_dp_pe_set(void) {
  TEST("CI dispatch: PE Set with body");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_pe_set(buf, 0x02, 0x1234567, 0x7654321, 2, (const uint8_t*)"{}", 2, 1, 1, (const uint8_t*)"val", 3);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.request_id == 2, "req_id=2");
  CHECK(ctx.body_len == 3, "body=3");
  PASS();
}

void test_ci_dp_pe_notify(void) {
  TEST("CI dispatch: PE Notify");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_pe_notify(buf, 0x02, 0x1234567, 0x7654321, 1, (const uint8_t*)"{}", 2, 1, 1, NULL, 0);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  PASS();
}

void test_ci_dp_pi_report_reply(void) {
  TEST("CI dispatch: PI MIDI Report Reply");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_pi_midi_report_reply(buf, 0x02, 0x7654321, 0x1234567, 0x7F, 0x03, 0x00, 0x1F, 0x07);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.system_bm == 0x03, "sys");
  CHECK(ctx.chan_ctrl_bm == 0x1F, "chan");
  CHECK(ctx.note_data_bm == 0x07, "note");
  PASS();
}

void test_ci_dp_nak_v1(void) {
  TEST("CI dispatch: NAK v1 header-only");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_nak(buf, 0x01, 0x1234567, 0x7654321, 0x7F, 0, 0, 0, NULL, 0, NULL);
  midi2_ci_dispatch_feed(&dp, 0, buf, len);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.orig_sub_id == 0, "v1: orig=0");
  CHECK(ctx.status_code == 0, "v1: status=0");
  PASS();
}

void test_ci_dp_null_safe(void) {
  TEST("CI dispatch: NULL callbacks don't crash");
  midi2_ci_dispatch dp;
  midi2_ci_dispatch_init(&dp);
  uint8_t buf[64];
  midi2_ci_build_discovery(buf, 0x02, 0x1234567, 0x002109, 1, 2, 0, 0x0C, 512, 0);
  midi2_ci_dispatch_feed(&dp, 0, buf, 30); /* should not crash */
  PASS();
}

void test_ci_dp_not_ci(void) {
  TEST("CI dispatch: non-CI SysEx returns false");
  midi2_ci_dispatch dp = make_dp();
  uint8_t buf[] = {0x7E, 0x7F, 0x06, 0x01};  /* Device Inquiry, not CI */
  bool ok = midi2_ci_dispatch_feed(&dp, 0, buf, 4);
  CHECK(!ok, "not CI");
  PASS();
}

void test_ci_dp_unknown(void) {
  TEST("CI dispatch: unknown sub-id -> on_unknown");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[16];
  midi2_ci_build_header(buf, 0x7F, 0x55, 0x02, 0x1234567, 0x7654321);
  midi2_ci_dispatch_feed(&dp, 0, buf, 13);
  CHECK(ctx.called, "unknown fired");
  CHECK(ctx.sub_id == 0x55, "sub_id=0x55");
  PASS();
}

void test_ci_dp_group_preserved(void) {
  TEST("CI dispatch: UMP group preserved in header");
  midi2_ci_dispatch dp = make_dp();
  reset();
  uint8_t buf[32];
  midi2_ci_build_profile_inquiry(buf, 0x02, 0x1234567, 0x7654321, 0x7F);
  midi2_ci_dispatch_feed(&dp, 5, buf, 13);
  CHECK(ctx.called, "dispatched");
  CHECK(ctx.hdr.group == 5, "group=5");
  PASS();
}

int main(void) {
  printf("\n=== midi2_ci_dispatch Unit Tests ===\n\n");

  printf("[Management]\n");
  test_ci_dp_discovery();
  test_ci_dp_discovery_reply();
  test_ci_dp_endpoint_info();
  test_ci_dp_invalidate_muid();
  test_ci_dp_nak();
  test_ci_dp_ack();

  printf("\n[Profile Configuration]\n");
  test_ci_dp_profile_inquiry();
  test_ci_dp_profile_reply();
  test_ci_dp_set_profile_on();
  test_ci_dp_profile_enabled();
  test_ci_dp_profile_added();

  printf("\n[Property Exchange]\n");
  test_ci_dp_pe_capability();
  test_ci_dp_pe_get();
  test_ci_dp_pe_get_reply_chunked();

  printf("\n[Process Inquiry]\n");
  test_ci_dp_pi_capability();
  test_ci_dp_pi_report();
  test_ci_dp_pi_end();

  printf("\n[Additional Profile]\n");
  test_ci_dp_set_profile_off();
  test_ci_dp_profile_disabled();
  test_ci_dp_profile_removed();
  test_ci_dp_profile_details();
  test_ci_dp_profile_specific();

  printf("\n[Additional PE]\n");
  test_ci_dp_pe_set();
  test_ci_dp_pe_notify();

  printf("\n[Additional PI]\n");
  test_ci_dp_pi_report_reply();

  printf("\n[Version Handling]\n");
  test_ci_dp_nak_v1();

  printf("\n[Edge Cases]\n");
  test_ci_dp_null_safe();
  test_ci_dp_not_ci();
  test_ci_dp_unknown();
  test_ci_dp_group_preserved();

  printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
