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

  /* Build a Discovery Request */
  uint8_t request[14];
  request[0] = 0x7E;         /* Universal SysEx */
  request[1] = 0x7F;         /* device ID */
  request[2] = 0x0D;         /* MIDI-CI */
  request[3] = 0x70;         /* Discovery Request */
  /* Source MUID = 0x0ABCDEF */
  request[4] = 0x6F;         /* bits 0-6 */
  request[5] = 0x37;         /* bits 7-13 */
  request[6] = 0x2F;         /* bits 14-20 */
  request[7] = 0x05;         /* bits 21-27 */
  /* Destination MUID = broadcast */
  request[8] = 0x7F;
  request[9] = 0x7F;
  request[10] = 0x7F;
  request[11] = 0x7F;
  /* Manufacturer + rest */
  request[12] = 0x00;
  request[13] = 0x00;

  bool handled = midi2_ci_process_sysex(&s, 0, request, 14);
  CHECK(handled, "message was handled");
  CHECK(write_pos > 0, "response was written");

  /* Extract response data */
  uint8_t resp[64];
  uint16_t resp_len = extract_sysex7_data(resp, 64);
  CHECK(resp_len >= 29, "response >= 29 bytes");
  CHECK(resp[0] == 0x7E, "Universal SysEx");
  CHECK(resp[2] == 0x0D, "MIDI-CI");
  CHECK(resp[3] == 0x71, "Discovery Reply sub-ID");

  /* Check our MUID in response (bytes 4-7) */
  uint32_t our_muid = (uint32_t)resp[4] | ((uint32_t)resp[5] << 7)
                     | ((uint32_t)resp[6] << 14) | ((uint32_t)resp[7] << 21);
  CHECK(our_muid == (0x12345678 & 0x0FFFFFFF), "our MUID in reply");

  /* Check manufacturer ID */
  uint32_t mfr = (uint32_t)resp[12] | ((uint32_t)resp[13] << 8) | ((uint32_t)resp[14] << 16);
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

  /* Build Profile Inquiry */
  uint8_t request[13];
  request[0] = 0x7E;
  request[1] = 0x7F;
  request[2] = 0x0D;
  request[3] = 0x20;         /* Profile Inquiry (per M2-101-UM) */
  request[4] = 0x01;         /* source MUID */
  request[5] = 0x00;
  request[6] = 0x00;
  request[7] = 0x00;
  request[8] = 0x7F;         /* dest MUID (broadcast) */
  request[9] = 0x7F;
  request[10] = 0x7F;
  request[11] = 0x7F;
  request[12] = 0x00;

  bool handled = midi2_ci_process_sysex(&s, 0, request, 13);
  CHECK(handled, "message was handled");
  CHECK(write_pos > 0, "response was written");

  uint8_t resp[64];
  uint16_t resp_len = extract_sysex7_data(resp, 64);
  CHECK(resp_len >= 24, "response has profiles");
  CHECK(resp[3] == 0x21, "Profile Inquiry Reply sub-ID (per M2-101-UM)");
  CHECK(resp[12] == 2, "2 profiles enabled");

  /* Check first profile bytes */
  CHECK(resp[14] == 0x00, "profile 1 byte 0");
  CHECK(resp[15] == 0x21, "profile 1 byte 1");

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

  printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
