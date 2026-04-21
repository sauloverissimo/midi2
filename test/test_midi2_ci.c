/*
 * midi2_ci unit tests
 * Compile: gcc -std=c99 -Wall -I../src test_midi2_ci.c ../src/midi2_proc.c ../src/midi2_ci.c -o test_midi2_ci
 */

#include <stdio.h>
#include <string.h>
#include "midi2_ci.h"

static int passed = 0;
static int failed = 0;

/* Shared storage for tests */
static uint8_t test_profiles[8][5];
static midi2_ci_property test_props[4];

#define TEST(name) printf("  %-55s ", name)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* --- Write capture --- */

static uint32_t write_buf[128];
static uint32_t write_pos;

static uint32_t capture_write(const uint32_t *words, uint32_t count, void *ctx) {
  uint32_t i;
  (void)ctx;
  for (i = 0; i < count && write_pos < 128; i++) {
    write_buf[write_pos++] = words[i];
  }
  return count;
}

/* Extract SysEx7 data bytes from captured UMP packets */
static uint16_t extract_sysex7_data(uint8_t *out, uint16_t max_out) {
  uint16_t pos = 0;
  uint32_t i;
  for (i = 0; i + 1 < write_pos; i += 2) {
    uint8_t num = (write_buf[i] >> 16) & 0x0F;
    if (num >= 1 && pos < max_out) out[pos++] = (write_buf[i] >> 8) & 0x7F;
    if (num >= 2 && pos < max_out) out[pos++] = (write_buf[i] >> 0) & 0x7F;
    if (num >= 3 && pos < max_out) out[pos++] = (write_buf[i+1] >> 24) & 0x7F;
    if (num >= 4 && pos < max_out) out[pos++] = (write_buf[i+1] >> 16) & 0x7F;
    if (num >= 5 && pos < max_out) out[pos++] = (write_buf[i+1] >> 8) & 0x7F;
    if (num >= 6 && pos < max_out) out[pos++] = (write_buf[i+1] >> 0) & 0x7F;
  }
  return pos;
}

static void reset(void) {
  memset(write_buf, 0, sizeof(write_buf));
  write_pos = 0;
}

/* --- Init tests --- */

void test_init(void) {
  TEST("init: muid set from seed");
  midi2_ci_state s;
  midi2_ci_init(&s, 0xABCDEF01, test_profiles, 8, test_props, 4);
  CHECK(s.muid == (0xABCDEF01 & 0x0FFFFFFF), "28-bit muid");
  CHECK(s.profile_count == 0, "no profiles");
  CHECK(s.property_count == 0, "no properties");
  PASS();
}

void test_set_identity(void) {
  TEST("set_identity: stores values");
  midi2_ci_state s;
  midi2_ci_init(&s, 0, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x00AABB, 0x0001, 0x0002, 0x00010000);
  CHECK(s.manufacturer_id == 0x00AABB, "manufacturer");
  CHECK(s.family_id == 0x0001, "family");
  CHECK(s.model_id == 0x0002, "model");
  CHECK(s.version_id == 0x00010000, "version");
  PASS();
}

/* --- Profile tests --- */

void test_add_profile(void) {
  TEST("add_profile: adds to list");
  midi2_ci_state s;
  midi2_ci_init(&s, 0, test_profiles, 8, test_props, 4);
  uint8_t p1[] = {0x00, 0x21, 0x09, 0x00, 0x01};
  midi2_ci_add_profile(&s, p1);
  CHECK(s.profile_count == 1, "count = 1");
  CHECK(memcmp(s.profiles[0], p1, 5) == 0, "profile matches");
  PASS();
}

void test_remove_profile(void) {
  TEST("remove_profile: removes from list");
  midi2_ci_state s;
  midi2_ci_init(&s, 0, test_profiles, 8, test_props, 4);
  uint8_t p1[] = {0x00, 0x21, 0x09, 0x00, 0x01};
  uint8_t p2[] = {0x00, 0x21, 0x09, 0x00, 0x02};
  midi2_ci_add_profile(&s, p1);
  midi2_ci_add_profile(&s, p2);
  CHECK(s.profile_count == 2, "count = 2");
  midi2_ci_remove_profile(&s, p1);
  CHECK(s.profile_count == 1, "count = 1");
  CHECK(memcmp(s.profiles[0], p2, 5) == 0, "p2 remains");
  PASS();
}

/* --- Property tests --- */

void test_add_property_static(void) {
  TEST("add_property_static: stores name and value");
  midi2_ci_state s;
  midi2_ci_init(&s, 0, test_profiles, 8, test_props, 4);
  midi2_ci_add_property_static(&s, "DeviceName", "TestSynth");
  CHECK(s.property_count == 1, "count = 1");
  CHECK(strcmp(s.properties[0].name, "DeviceName") == 0, "name");
  CHECK(strcmp(s.properties[0].static_value, "TestSynth") == 0, "value");
  CHECK(s.properties[0].getter == NULL, "no getter");
  PASS();
}

/* --- Discovery Reply test --- */

void test_discovery_reply(void) {
  TEST("discovery: receives request, sends reply");
  midi2_ci_state s;
  midi2_ci_init(&s, 0x12345678, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x00AABB, 0x0001, 0x0002, 0x00010000);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  reset();

  /* Build a Discovery Request using midi2_ci_build */
  uint8_t request[32];
  uint16_t req_len = midi2_ci_build_discovery(request, MIDI2_CI_VERSION_1,
                                                   0x0ABCDEF, 0x002109, 1, 1, 0, 0x0C, 128, 0);

  bool handled = midi2_ci_process_sysex(&s, 0, request, req_len);
  CHECK(handled, "message was handled");
  CHECK(write_pos > 0, "response was written");

  /* Extract response data */
  uint8_t resp[64];
  uint16_t resp_len = extract_sysex7_data(resp, 64);
  CHECK(resp_len >= 29, "response >= 29 bytes");
  CHECK(resp[0] == 0x7E, "Universal SysEx");
  CHECK(resp[2] == 0x0D, "MIDI-CI");
  CHECK(resp[3] == 0x71, "Discovery Reply sub-ID");

  /* Check our MUID in response (bytes 5-8, after version at byte 4) */
  uint32_t our_muid = midi2_ci_get_src_muid(resp);
  CHECK(our_muid == (0x12345678 & 0x0FFFFFFF), "our MUID in reply");

  /* Check manufacturer ID (at offset 13) */
  uint32_t mfr = midi2_ci_get_mfr_id(resp);
  CHECK((mfr & 0x7F7F7F) == (0x00AABB & 0x7F7F7F), "manufacturer ID");

  PASS();
}

/* --- Profile Inquiry Reply test --- */

void test_profile_inquiry_reply(void) {
  TEST("profile inquiry: receives request, sends reply with profiles");
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x00AABB, 0x0001, 0x0002, 0x00010000);
  midi2_ci_set_write_fn(&s, capture_write, NULL);

  uint8_t p1[] = {0x00, 0x21, 0x09, 0x00, 0x01};
  uint8_t p2[] = {0x00, 0x21, 0x09, 0x00, 0x02};
  midi2_ci_add_profile(&s, p1);
  midi2_ci_add_profile(&s, p2);
  reset();

  /* Build Profile Inquiry using midi2_ci_build */
  uint8_t request[16];
  uint16_t req_len = midi2_ci_build_profile_inquiry(request, MIDI2_CI_VERSION_1,
                                                         0x0000001, s.muid, 0x7F);

  bool handled = midi2_ci_process_sysex(&s, 0, request, req_len);
  CHECK(handled, "message was handled");
  CHECK(write_pos > 0, "response was written");

  uint8_t resp[64];
  uint16_t resp_len = extract_sysex7_data(resp, 64);
  CHECK(resp_len >= 24, "response has profiles");
  CHECK(midi2_ci_get_sub_id(resp) == MIDI2_CI_PROFILE_INQUIRY_REPLY, "Profile Inquiry Reply");
  CHECK(midi2_ci_get_enabled_count(resp) == 2, "2 profiles enabled");

  /* Check first profile bytes (at offset 15 = 13 header + 2 count) */
  CHECK(resp[15] == 0x00, "profile 1 byte 0");
  CHECK(resp[16] == 0x21, "profile 1 byte 1");

  PASS();
}

/* --- PE Get --- */

void test_pe_get_static(void) {
  TEST("PE Get: returns static property value");
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x00AABB, 0x0001, 0x0002, 0x00010000);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_add_property_static(&s, "DeviceName", "TestSynth");
  reset();

  /* Build PE Get request */
  uint8_t request[16];
  request[0] = 0x7E;
  request[1] = 0x7F;
  request[2] = 0x0D;
  request[3] = 0x34;         /* PE Get */
  request[4] = 0x01;         /* source MUID */
  request[5] = 0x00;
  request[6] = 0x00;
  request[7] = 0x00;
  request[8] = 0x7F;         /* dest MUID */
  request[9] = 0x7F;
  request[10] = 0x7F;
  request[11] = 0x7F;
  request[12] = 0x01;        /* request ID */
  request[13] = 0x00;
  request[14] = 0x00;
  request[15] = 0x00;

  bool handled = midi2_ci_process_sysex(&s, 0, request, 16);
  CHECK(handled, "PE Get handled");
  CHECK(write_pos > 0, "response written");

  uint8_t resp[64];
  uint16_t resp_len = extract_sysex7_data(resp, 64);
  CHECK(resp_len > 0, "response has data");
  CHECK(resp[3] == 0x35, "PE Get Reply sub-ID");
  PASS();
}

/* --- Non-CI SysEx passthrough --- */

void test_non_ci_returns_false(void) {
  TEST("non-CI SysEx: returns false");
  midi2_ci_state s;
  midi2_ci_init(&s, 0, test_profiles, 8, test_props, 4);

  uint8_t data[] = {0x41, 0x10, 0x42, 0x12, 0x00};  /* Roland SysEx, not CI */
  bool handled = midi2_ci_process_sysex(&s, 0, data, 5);
  CHECK(!handled, "not handled");
  PASS();
}

void test_ci_without_identity_ignored(void) {
  TEST("discovery with no identity set: returns false");
  midi2_ci_state s;
  midi2_ci_init(&s, 0x12345678, test_profiles, 8, test_props, 4);
  /* NOT calling midi2_ci_set_identity */
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  reset();

  uint8_t request[14];
  memset(request, 0, sizeof(request));
  request[0] = 0x7E;
  request[1] = 0x7F;
  request[2] = 0x0D;
  request[3] = 0x70;

  bool handled = midi2_ci_process_sysex(&s, 0, request, 14);
  CHECK(!handled, "not handled (no identity configured)");
  CHECK(write_pos == 0, "no response sent");
  PASS();
}

/* --- v0.2.4 new features --- */

/* Discovery reply now advertises PROCESS_INQUIRY (bit 4) in ci_cat. */
static void test_discovery_reply_ci_cat_includes_pi(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x12345678, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  reset();

  uint8_t req[30] = {0};
  req[0]=0x7E; req[1]=0x7F; req[2]=0x0D; req[3]=0x70; req[4]=0x01;
  midi2_ci_process_sysex(&s, 0, req, 30);

  uint8_t body[64];
  uint16_t n = extract_sysex7_data(body, sizeof(body));
  TEST("discovery reply advertises PROCESS_INQUIRY in ci_cat");
  CHECK(n >= 28, "reply present");
  /* Discovery Reply body layout (Table 8):
   *  [0..12]  common header (7E, dev_id, 0D, sub_id, ver, src_muid×4, dst_muid×4)
   *  [13..15] manufacturer id (3 bytes)
   *  [16..17] family (2 bytes)
   *  [18..19] model (2 bytes)
   *  [20..23] sw_rev (4 bytes)
   *  [24]     ci_category   <-- here
   *  [25..28] max_sysex (4 bytes)
   */
  uint8_t ci_cat = body[24];
  CHECK((ci_cat & 0x04) != 0, "CAT_PROFILE_CONFIG set");
  CHECK((ci_cat & 0x08) != 0, "CAT_PROPERTY_EXCHANGE set");
  CHECK((ci_cat & 0x10) != 0, "CAT_PROCESS_INQUIRY set");
  PASS();
}

/* PE Capability inquiry gets an auto-reply with Sub-ID#2 0x31. */
static void test_pe_capability_autoreply(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x12345678, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  reset();

  /* PE Capability Inquiry body */
  uint8_t req[16] = {0};
  req[0]=0x7E; req[1]=0x7F; req[2]=0x0D;
  req[3]=0x30; req[4]=0x01;
  midi2_ci_process_sysex(&s, 0, req, 16);

  uint8_t body[16];
  uint16_t n = extract_sysex7_data(body, sizeof(body));
  TEST("PE Capability -> PE Capability Reply (0x31)");
  CHECK(n >= 16, "reply bytes captured");
  CHECK(body[3] == 0x31, "Sub-ID#2 == PE_CAPABILITY_REPLY");
  CHECK(body[4] == MIDI2_CI_VERSION_2, "version == CI v1.2 (for major/minor field)");
  CHECK(body[13] == 1, "max_simultaneous == 1");
  CHECK(body[14] == 1, "pe_ver_major == 1");
  CHECK(body[15] == 0, "pe_ver_minor == 0");
  PASS();
}

static uint32_t _rng_value = 0;
static uint32_t fake_rng(void *ctx) { (void)ctx; return _rng_value; }

static void test_new_muid_avoids_reserved(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x12345678, NULL, 0, NULL, 0);
  midi2_ci_set_rng(&s, fake_rng, NULL);

  TEST("new_muid avoids zero (rng returns 0)");
  _rng_value = 0;
  uint32_t m = midi2_ci_new_muid(&s);
  CHECK(m != 0 && m != 0x0FFFFFFFu, "not reserved");
  PASS();

  TEST("new_muid avoids 0x0FFFFFFF");
  _rng_value = 0x0FFFFFFFu;
  m = midi2_ci_new_muid(&s);
  CHECK(m != 0 && m != 0x0FFFFFFFu, "not reserved");
  PASS();

  TEST("new_muid returns 28-bit value");
  _rng_value = 0xDEADBEEFu;
  m = midi2_ci_new_muid(&s);
  CHECK((m & 0xF0000000u) == 0, "upper nibble clear");
  PASS();
}

static void test_invalidate_muid_regen_with_rng(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x00ABCDEF, NULL, 0, NULL, 0);
  _rng_value = 0x01111111u;
  midi2_ci_set_rng(&s, fake_rng, NULL);

  uint32_t old = s.muid;
  uint8_t req[17] = {0};
  req[0]=0x7E; req[1]=0x7F; req[2]=0x0D; req[3]=0x7E; req[4]=0x01;
  req[5]=0x02;
  req[9]=0x7F; req[10]=0x7F; req[11]=0x7F; req[12]=0x7F;
  req[13] = old & 0x7F;
  req[14] = (old >> 7) & 0x7F;
  req[15] = (old >> 14) & 0x7F;
  req[16] = (old >> 21) & 0x7F;

  midi2_ci_process_sysex(&s, 0, req, 17);

  TEST("Invalidate MUID (target==ours) regenerates via RNG");
  CHECK(s.muid != old, "MUID changed");
  PASS();
}

static void test_invalidate_muid_ignore_other_target(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x00ABCDEF, NULL, 0, NULL, 0);
  _rng_value = 0x01111111u;
  midi2_ci_set_rng(&s, fake_rng, NULL);

  uint32_t old = s.muid;
  uint8_t req[17] = {0};
  req[0]=0x7E; req[1]=0x7F; req[2]=0x0D; req[3]=0x7E; req[4]=0x01;
  req[5]=0x02; req[9]=0x7F; req[10]=0x7F; req[11]=0x7F; req[12]=0x7F;
  req[13]=0x55; req[14]=0x0A;

  midi2_ci_process_sysex(&s, 0, req, 17);

  TEST("Invalidate MUID with other target is ignored");
  CHECK(s.muid == old, "MUID unchanged");
  PASS();
}

static void test_muid_collision_triggers_regen(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x00ABCDEF, NULL, 0, NULL, 0);
  _rng_value = 0x02222222u;
  midi2_ci_set_rng(&s, fake_rng, NULL);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  reset();

  uint32_t old = s.muid;
  uint8_t req[30] = {0};
  req[0]=0x7E; req[1]=0x7F; req[2]=0x0D; req[3]=0x70; req[4]=0x01;
  req[5]  = old & 0x7F;
  req[6]  = (old >> 7) & 0x7F;
  req[7]  = (old >> 14) & 0x7F;
  req[8]  = (old >> 21) & 0x7F;
  req[9]=0x7F; req[10]=0x7F; req[11]=0x7F; req[12]=0x7F;

  midi2_ci_process_sysex(&s, 0, req, 30);

  TEST("MUID collision triggers regen");
  CHECK(s.muid != old, "MUID changed");
  PASS();
}

static void test_nak_on_unknown_opt_in(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x12345678, NULL, 0, NULL, 0);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  reset();

  uint8_t req[14] = {0};
  req[0]=0x7E; req[1]=0x7F; req[2]=0x0D; req[3]=0x55; req[4]=0x02;

  bool h1 = midi2_ci_process_sysex(&s, 0, req, 14);
  TEST("nak_on_unknown=false: unknown sub_id is silent");
  CHECK(!h1 && write_pos == 0, "no reply sent");
  PASS();

  midi2_ci_set_nak_on_unknown(&s, true);
  reset();
  bool h2 = midi2_ci_process_sysex(&s, 0, req, 14);
  uint8_t body[32];
  uint16_t n = extract_sysex7_data(body, sizeof(body));
  TEST("nak_on_unknown=true: unknown sub_id gets NAK 0x7F");
  CHECK(h2, "handled");
  CHECK(n > 14, "NAK body captured");
  CHECK(body[3] == 0x7F, "Sub-ID#2 == NAK");
  PASS();
}

/* --- Main --- */

int main(void) {
  printf("\n=== midi2_ci Unit Tests ===\n\n");

  printf("[Init]\n");
  test_init();
  test_set_identity();

  printf("\n[Profiles]\n");
  test_add_profile();
  test_remove_profile();

  printf("\n[Properties]\n");
  test_add_property_static();

  printf("\n[Discovery]\n");
  test_discovery_reply();

  printf("\n[Profile Inquiry]\n");
  test_profile_inquiry_reply();

  printf("\n[Property Exchange]\n");
  test_pe_get_static();

  printf("\n[Edge Cases]\n");
  test_non_ci_returns_false();
  test_ci_without_identity_ignored();

  printf("\n[v0.2.4 compliance features]\n");
  test_discovery_reply_ci_cat_includes_pi();
  test_pe_capability_autoreply();
  test_new_muid_avoids_reserved();
  test_invalidate_muid_regen_with_rng();
  test_invalidate_muid_ignore_other_target();
  test_muid_collision_triggers_regen();
  test_nak_on_unknown_opt_in();

  printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
