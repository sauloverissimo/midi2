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

static uint32_t write_buf[512];
static uint32_t write_pos;

static uint32_t capture_write(const uint32_t *words, uint32_t count, void *ctx) {
  uint32_t i;
  (void)ctx;
  for (i = 0; i < count && write_pos < 512; i++) {
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

/* True if the byte range [hay, hay+n) contains the ASCII needle. */
static bool blob_contains(const uint8_t *hay, uint16_t n, const char *needle) {
  uint16_t nl = (uint16_t)strlen(needle);
  uint16_t i;
  if (nl == 0 || nl > n) return false;
  for (i = 0; (uint16_t)(i + nl) <= n; i++) {
    if (memcmp(&hay[i], needle, nl) == 0) return true;
  }
  return false;
}

/* PE Set setter recorder: captures the name/value the handler passes and
 * returns a configurable result. */
static char g_set_name[40];
static char g_set_value[64];
static bool g_set_return = true;
static bool record_setter(const char *name, const char *value, void *ctx) {
  (void)ctx;
  g_set_name[0] = '\0';
  g_set_value[0] = '\0';
  if (name)  { strncpy(g_set_name,  name,  sizeof(g_set_name)  - 1); g_set_name[sizeof(g_set_name)   - 1] = '\0'; }
  if (value) { strncpy(g_set_value, value, sizeof(g_set_value) - 1); g_set_value[sizeof(g_set_value) - 1] = '\0'; }
  return g_set_return;
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

void test_remove_property(void) {
  TEST("remove_property: removes and shifts remaining entries (v0.3.0)");
  midi2_ci_state s;
  midi2_ci_init(&s, 0, test_profiles, 8, test_props, 4);
  CHECK(midi2_ci_add_property_static(&s, "A", "1") == MIDI2_CI_OK, "add A");
  CHECK(midi2_ci_add_property_static(&s, "B", "2") == MIDI2_CI_OK, "add B");
  CHECK(midi2_ci_add_property_static(&s, "C", "3") == MIDI2_CI_OK, "add C");
  CHECK(s.property_count == 3, "three properties added");
  CHECK(midi2_ci_remove_property(&s, "B") == MIDI2_CI_OK, "remove B");
  CHECK(s.property_count == 2, "count drops after remove");
  CHECK(strcmp(s.properties[0].name, "A") == 0, "A still at index 0");
  CHECK(strcmp(s.properties[1].name, "C") == 0, "C shifted into index 1");
  CHECK(midi2_ci_remove_property(&s, "zzz") == MIDI2_CI_ERR_NOT_FOUND,
        "missing name returns NOT_FOUND");
  PASS();
}

void test_reset_profiles_and_properties(void) {
  TEST("reset_profiles / reset_properties: clear registries (v0.3.0)");
  midi2_ci_state s;
  midi2_ci_init(&s, 0, test_profiles, 8, test_props, 4);
  uint8_t pid[] = {0x00, 0x21, 0x09, 0x00, 0x01};
  midi2_ci_add_profile(&s, pid);
  midi2_ci_add_profile(&s, pid);
  midi2_ci_add_property_static(&s, "DeviceName", "TestSynth");
  CHECK(s.profile_count == 2, "two profiles added");
  CHECK(s.property_count == 1, "one property added");

  midi2_ci_reset_profiles(&s);
  CHECK(s.profile_count == 0, "profile_count cleared");
  CHECK(s.property_count == 1, "reset_profiles leaves properties intact");

  midi2_ci_reset_properties(&s);
  CHECK(s.property_count == 0, "property_count cleared");
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
  TEST("PE Get: returns the named static property value with status 200");
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x00AABB, 0x0001, 0x0002, 0x00010000);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_add_property_static(&s, "DeviceName", "TestSynth");
  reset();

  /* Build a PE Get for the registered "DeviceName" resource. */
  const char inq[] = "{\"resource\":\"DeviceName\"}";
  uint8_t request[64];
  uint16_t req_len = midi2_ci_build_pe_get(request, MIDI2_CI_VERSION_2,
      0x0000001, s.muid, 0x01, (const uint8_t *)inq, (uint16_t)(sizeof(inq) - 1));

  bool handled = midi2_ci_process_sysex(&s, 0, request, req_len);
  CHECK(handled, "PE Get handled");
  CHECK(write_pos > 0, "response written");

  uint8_t resp[64];
  uint16_t resp_len = extract_sysex7_data(resp, 64);
  CHECK(resp_len > 16, "response has data");
  CHECK(resp[3] == 0x35, "PE Get Reply sub-ID");
  uint16_t hdr_len = midi2_ci_read_14(&resp[14]);
  CHECK(blob_contains(&resp[16], hdr_len, "\"status\":200"), "status 200 header");
  uint16_t body_off = (uint16_t)(16 + hdr_len + 6);
  CHECK(blob_contains(&resp[body_off], (uint16_t)(resp_len - body_off), "TestSynth"),
        "body carries the requested property value");
  PASS();
}

/* v0.6.1 PE conformance: every successful PE Get Reply must carry a
 * {"status":200} header. An empty header made the Workbench NAK with
 * "The first header property is not resource, status or command". */
static void test_pe_get_reply_has_status_header(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_add_property_static(&s, "DeviceInfo", "{\"manufacturer\":\"test\"}");
  reset();

  const char inq[] = "{\"resource\":\"DeviceInfo\"}";
  uint8_t req[64];
  uint16_t req_len = midi2_ci_build_pe_get(req, MIDI2_CI_VERSION_2,
      0x0000001, s.muid, 0x01, (const uint8_t *)inq, (uint16_t)(sizeof(inq) - 1));
  midi2_ci_process_sysex(&s, 0, req, req_len);

  uint8_t resp[128];
  uint16_t n = extract_sysex7_data(resp, sizeof(resp));
  TEST("PE Get Reply carries a status:200 header");
  CHECK(n > 16, "reply present");
  CHECK(resp[3] == 0x35, "PE Get Reply sub-ID");
  uint16_t hdr_len = midi2_ci_read_14(&resp[14]);
  CHECK(hdr_len > 0, "header is not empty");
  CHECK(blob_contains(&resp[16], hdr_len, "\"status\":200"), "header declares status 200");
  PASS();
}

/* v0.6.1 PE conformance: the reply must match the requested resource. The
 * DeviceInfo value comes back, not properties[0] by accident. */
static void test_pe_get_matches_requested_resource(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_add_property_static(&s, "ChannelList", "[{\"title\":\"ch1\"}]");
  midi2_ci_add_property_static(&s, "DeviceInfo",  "{\"manufacturer\":\"acme\"}");
  reset();

  const char inq[] = "{\"resource\":\"DeviceInfo\"}";
  uint8_t req[64];
  uint16_t req_len = midi2_ci_build_pe_get(req, MIDI2_CI_VERSION_2,
      0x0000001, s.muid, 0x01, (const uint8_t *)inq, (uint16_t)(sizeof(inq) - 1));
  midi2_ci_process_sysex(&s, 0, req, req_len);

  uint8_t resp[128];
  uint16_t n = extract_sysex7_data(resp, sizeof(resp));
  TEST("PE Get returns the requested resource, not properties[0]");
  CHECK(n > 16, "reply present");
  uint16_t hdr_len = midi2_ci_read_14(&resp[14]);
  uint16_t body_off = (uint16_t)(16 + hdr_len + 6); /* +num_chunks,this_chunk,body_len */
  CHECK(blob_contains(&resp[body_off], (uint16_t)(n - body_off), "acme"),
        "body carries DeviceInfo (acme), not ChannelList");
  PASS();
}

/* v0.6.1 PE conformance: an unknown resource is answered with status:404,
 * never a wrong 200 body. */
static void test_pe_get_unknown_resource_404(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_add_property_static(&s, "DeviceInfo", "{\"manufacturer\":\"acme\"}");
  reset();

  const char inq[] = "{\"resource\":\"Nonexistent\"}";
  uint8_t req[64];
  uint16_t req_len = midi2_ci_build_pe_get(req, MIDI2_CI_VERSION_2,
      0x0000001, s.muid, 0x01, (const uint8_t *)inq, (uint16_t)(sizeof(inq) - 1));
  midi2_ci_process_sysex(&s, 0, req, req_len);

  uint8_t resp[128];
  uint16_t n = extract_sysex7_data(resp, sizeof(resp));
  TEST("PE Get for unknown resource replies status:404");
  CHECK(n > 16, "reply present");
  CHECK(resp[3] == 0x35, "PE Get Reply sub-ID");
  uint16_t hdr_len = midi2_ci_read_14(&resp[14]);
  CHECK(blob_contains(&resp[16], hdr_len, "\"status\":404"), "header declares status 404");
  PASS();
}

/* M2-105 conformance: a Get reply for a paginable list resource carries
 * totalCount in the header (the Workbench warns without it), while an object
 * resource such as DeviceInfo does not. */
static void test_pe_get_list_has_totalcount(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_add_property_static(&s, "ChannelList",
      "[{\"title\":\"a\",\"channel\":1},{\"title\":\"b\",\"channel\":2},"
      "{\"title\":\"c\",\"channel\":3}]");
  midi2_ci_add_property_static(&s, "DeviceInfo", "{\"manufacturer\":\"acme\"}");

  const char list_inq[] = "{\"resource\":\"ChannelList\"}";
  uint8_t req[64];
  uint16_t req_len;
  uint8_t resp[256];
  uint16_t n, hdr_len;

  reset();
  req_len = midi2_ci_build_pe_get(req, MIDI2_CI_VERSION_2, 0x0000001, s.muid,
      0x01, (const uint8_t *)list_inq, (uint16_t)(sizeof(list_inq) - 1));
  midi2_ci_process_sysex(&s, 0, req, req_len);
  n = extract_sysex7_data(resp, sizeof(resp));
  TEST("PE Get list resource header carries totalCount");
  CHECK(n > 16, "reply present");
  hdr_len = midi2_ci_read_14(&resp[14]);
  CHECK(blob_contains(&resp[16], hdr_len, "\"status\":200"), "status 200");
  CHECK(blob_contains(&resp[16], hdr_len, "\"totalCount\":3"),
        "totalCount equals the array element count");
  PASS();

  const char obj_inq[] = "{\"resource\":\"DeviceInfo\"}";
  reset();
  req_len = midi2_ci_build_pe_get(req, MIDI2_CI_VERSION_2, 0x0000001, s.muid,
      0x01, (const uint8_t *)obj_inq, (uint16_t)(sizeof(obj_inq) - 1));
  midi2_ci_process_sysex(&s, 0, req, req_len);
  n = extract_sysex7_data(resp, sizeof(resp));
  TEST("PE Get object resource header omits totalCount");
  CHECK(n > 16, "reply present");
  hdr_len = midi2_ci_read_14(&resp[14]);
  CHECK(!blob_contains(&resp[16], hdr_len, "totalCount"),
        "object resource has no totalCount");
  PASS();
}

/* v0.6.1 PE conformance: the built-in ResourceList enumerates registered
 * resources so an Initiator can discover what to GET. */
static void test_pe_get_resource_list(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_add_property_static(&s, "DeviceInfo",  "{\"manufacturer\":\"acme\"}");
  midi2_ci_add_property_static(&s, "ChannelList", "[{\"title\":\"ch1\"}]");
  reset();

  const char inq[] = "{\"resource\":\"ResourceList\"}";
  uint8_t req[64];
  uint16_t req_len = midi2_ci_build_pe_get(req, MIDI2_CI_VERSION_2,
      0x0000001, s.muid, 0x01, (const uint8_t *)inq, (uint16_t)(sizeof(inq) - 1));
  midi2_ci_process_sysex(&s, 0, req, req_len);

  uint8_t resp[256];
  uint16_t n = extract_sysex7_data(resp, sizeof(resp));
  TEST("PE Get ResourceList enumerates registered resources");
  CHECK(n > 16, "reply present");
  uint16_t hdr_len = midi2_ci_read_14(&resp[14]);
  CHECK(blob_contains(&resp[16], hdr_len, "\"status\":200"), "status 200");
  uint16_t body_off = (uint16_t)(16 + hdr_len + 6);
  uint16_t body_n = (uint16_t)(n - body_off);
  CHECK(blob_contains(&resp[body_off], body_n, "DeviceInfo"),  "lists DeviceInfo");
  CHECK(blob_contains(&resp[body_off], body_n, "ChannelList"), "lists ChannelList");
  PASS();
}

/* Review fix #4: a device that advertises PE but has zero registered
 * properties must still answer PE Get (ResourceList -> "[]"), not drop it. */
static void test_pe_get_zero_properties_replies(void) {
  midi2_ci_state s;
  midi2_ci_property props[2];
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, props, 2);  /* count stays 0 */
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  reset();

  const char inq[] = "{\"resource\":\"ResourceList\"}";
  uint8_t req[64];
  uint16_t req_len = midi2_ci_build_pe_get(req, MIDI2_CI_VERSION_2,
      0x0000001, s.muid, 0x01, (const uint8_t *)inq, (uint16_t)(sizeof(inq) - 1));
  midi2_ci_process_sysex(&s, 0, req, req_len);

  uint8_t resp[64];
  uint16_t n = extract_sysex7_data(resp, sizeof(resp));
  TEST("PE Get ResourceList with zero properties replies empty array");
  CHECK(n > 16, "reply present (not dropped)");
  CHECK(resp[3] == 0x35, "PE Get Reply sub-ID");
  uint16_t hdr_len = midi2_ci_read_14(&resp[14]);
  CHECK(blob_contains(&resp[16], hdr_len, "\"status\":200"), "status 200");
  uint16_t body_off = (uint16_t)(16 + hdr_len + 6);
  CHECK(blob_contains(&resp[body_off], (uint16_t)(n - body_off), "[]"), "empty resource array");
  PASS();
}

/* Review fix #1: when the resource array overflows the body buffer it must
 * stay valid JSON (whole entries + closing bracket), never an empty body. */
static void test_pe_get_resource_list_overflow_valid(void) {
  midi2_ci_state s;
  midi2_ci_property props[16];
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, props, 16);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  /* 12 long names (~60 chars each) so the array overflows CI_PE_BODY_MAX. */
  static const char *names[12] = {
    "ResourceNameThatIsDeliberatelyQuiteLongToForceOverflow0001",
    "ResourceNameThatIsDeliberatelyQuiteLongToForceOverflow0002",
    "ResourceNameThatIsDeliberatelyQuiteLongToForceOverflow0003",
    "ResourceNameThatIsDeliberatelyQuiteLongToForceOverflow0004",
    "ResourceNameThatIsDeliberatelyQuiteLongToForceOverflow0005",
    "ResourceNameThatIsDeliberatelyQuiteLongToForceOverflow0006",
    "ResourceNameThatIsDeliberatelyQuiteLongToForceOverflow0007",
    "ResourceNameThatIsDeliberatelyQuiteLongToForceOverflow0008",
    "ResourceNameThatIsDeliberatelyQuiteLongToForceOverflow0009",
    "ResourceNameThatIsDeliberatelyQuiteLongToForceOverflow0010",
    "ResourceNameThatIsDeliberatelyQuiteLongToForceOverflow0011",
    "ResourceNameThatIsDeliberatelyQuiteLongToForceOverflow0012",
  };
  int i;
  for (i = 0; i < 12; i++) midi2_ci_add_property_static(&s, names[i], "{}");
  reset();

  const char inq[] = "{\"resource\":\"ResourceList\"}";
  uint8_t req[64];
  uint16_t req_len = midi2_ci_build_pe_get(req, MIDI2_CI_VERSION_2,
      0x0000001, s.muid, 0x01, (const uint8_t *)inq, (uint16_t)(sizeof(inq) - 1));
  midi2_ci_process_sysex(&s, 0, req, req_len);

  uint8_t resp[640];
  uint16_t n = extract_sysex7_data(resp, sizeof(resp));
  TEST("PE Get ResourceList overflow stays valid JSON, never empty");
  CHECK(n > 16, "reply present");
  uint16_t hdr_len = midi2_ci_read_14(&resp[14]);
  uint16_t body_off  = (uint16_t)(16 + hdr_len + 6);
  uint16_t body_len  = midi2_ci_read_14(&resp[body_off - 2]);
  CHECK(body_len >= 2, "body not empty");
  CHECK(resp[body_off] == '[', "body starts with '['");
  CHECK(resp[body_off + body_len - 1] == ']', "body ends with ']'");
  PASS();
}

/* Review fix #2: PE Set matches the requested resource by name and passes the
 * real request body value to the setter, not "" against properties[0]. */
static void test_pe_set_matches_resource_and_value(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_add_property_static(&s, "DeviceInfo", "{\"x\":1}");          /* no setter, first */
  midi2_ci_add_property_dynamic(&s, "Config", NULL, record_setter);      /* settable, second */
  g_set_return = true;
  g_set_name[0] = g_set_value[0] = '\0';
  reset();

  const char hdr[]  = "{\"resource\":\"Config\"}";
  const char body[] = "{\"gain\":42}";
  uint8_t req[96];
  uint16_t req_len = midi2_ci_build_pe_set(req, MIDI2_CI_VERSION_2,
      0x0000001, s.muid, 0x01,
      (const uint8_t *)hdr, (uint16_t)(sizeof(hdr) - 1), 1, 1,
      (const uint8_t *)body, (uint16_t)(sizeof(body) - 1));
  midi2_ci_process_sysex(&s, 0, req, req_len);

  uint8_t resp[64];
  uint16_t n = extract_sysex7_data(resp, sizeof(resp));
  TEST("PE Set matches resource by name and passes real body value");
  CHECK(n > 16, "reply present");
  CHECK(resp[3] == 0x37, "PE Set Reply sub-ID");
  CHECK(strcmp(g_set_name, "Config") == 0, "setter got the requested resource");
  CHECK(strcmp(g_set_value, "{\"gain\":42}") == 0, "setter got the real body value");
  uint16_t hdr_len = midi2_ci_read_14(&resp[14]);
  CHECK(blob_contains(&resp[16], hdr_len, "\"status\":200"), "status 200 on success");
  PASS();
}

/* Review fix #2: PE Set for an unknown resource must not claim success. */
static void test_pe_set_unknown_resource_404(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_add_property_dynamic(&s, "Config", NULL, record_setter);
  reset();

  const char hdr[]  = "{\"resource\":\"Missing\"}";
  const char body[] = "{}";
  uint8_t req[96];
  uint16_t req_len = midi2_ci_build_pe_set(req, MIDI2_CI_VERSION_2,
      0x0000001, s.muid, 0x01,
      (const uint8_t *)hdr, (uint16_t)(sizeof(hdr) - 1), 1, 1,
      (const uint8_t *)body, (uint16_t)(sizeof(body) - 1));
  midi2_ci_process_sysex(&s, 0, req, req_len);

  uint8_t resp[64];
  uint16_t n = extract_sysex7_data(resp, sizeof(resp));
  TEST("PE Set for unknown resource replies status:404");
  CHECK(n > 16, "reply present");
  CHECK(resp[3] == 0x37, "PE Set Reply sub-ID");
  uint16_t hdr_len = midi2_ci_read_14(&resp[14]);
  CHECK(blob_contains(&resp[16], hdr_len, "\"status\":404"), "status 404");
  PASS();
}

/* Review fix #2: a setter returning false must not be reported as 200. */
static void test_pe_set_setter_failure_not_200(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_add_property_dynamic(&s, "Config", NULL, record_setter);
  g_set_return = false;  /* setter rejects the write */
  reset();

  const char hdr[]  = "{\"resource\":\"Config\"}";
  const char body[] = "{\"bad\":true}";
  uint8_t req[96];
  uint16_t req_len = midi2_ci_build_pe_set(req, MIDI2_CI_VERSION_2,
      0x0000001, s.muid, 0x01,
      (const uint8_t *)hdr, (uint16_t)(sizeof(hdr) - 1), 1, 1,
      (const uint8_t *)body, (uint16_t)(sizeof(body) - 1));
  midi2_ci_process_sysex(&s, 0, req, req_len);
  g_set_return = true;  /* restore for other tests */

  uint8_t resp[64];
  uint16_t n = extract_sysex7_data(resp, sizeof(resp));
  TEST("PE Set setter failure is not reported as status:200");
  CHECK(n > 16, "reply present");
  uint16_t hdr_len = midi2_ci_read_14(&resp[14]);
  CHECK(!blob_contains(&resp[16], hdr_len, "\"status\":200"), "not status 200 on failure");
  PASS();
}

/* v0.6.1: an app-registered "ResourceList" property overrides the built-in
 * enumeration, so devices with custom (X-) resources can publish entries
 * that carry a schema, as M2-105 requires for manufacturer resources. */
static void test_pe_get_resource_list_app_override(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  static const char custom[] =
    "[{\"resource\":\"X-Custom\",\"schema\":{\"type\":\"object\"}}]";
  midi2_ci_add_property_static(&s, "ResourceList", custom);
  midi2_ci_add_property_static(&s, "DeviceInfo", "{}");
  reset();

  const char inq[] = "{\"resource\":\"ResourceList\"}";
  uint8_t req[64];
  uint16_t req_len = midi2_ci_build_pe_get(req, MIDI2_CI_VERSION_2,
      0x0000001, s.muid, 0x01, (const uint8_t *)inq, (uint16_t)(sizeof(inq) - 1));
  midi2_ci_process_sysex(&s, 0, req, req_len);

  uint8_t resp[128];
  uint16_t n = extract_sysex7_data(resp, sizeof(resp));
  TEST("PE Get ResourceList prefers an app-registered property");
  CHECK(n > 16, "reply present");
  uint16_t hdr_len = midi2_ci_read_14(&resp[14]);
  uint16_t body_off = (uint16_t)(16 + hdr_len + 6);
  CHECK(blob_contains(&resp[body_off], (uint16_t)(n - body_off), "X-Custom"),
        "custom entry served");
  CHECK(blob_contains(&resp[body_off], (uint16_t)(n - body_off), "schema"),
        "schema field survives");
  CHECK(!blob_contains(&resp[body_off], (uint16_t)(n - body_off), "DeviceInfo"),
        "built-in enumeration not used");
  PASS();
}

/* Review follow-up: a realistic DeviceInfo (~200 bytes, with ID arrays) must
 * round-trip through PE Get without being truncated mid-JSON. */
static void test_pe_get_large_value_not_truncated(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  /* 205-byte DeviceInfo-shaped value ending in a distinctive marker. */
  static const char big[] =
    "{\"manufacturerId\":[125,0,0],\"familyId\":[1,0],\"modelId\":[1,0],"
    "\"versionId\":[0,0,4,0],\"manufacturer\":\"github.com/sauloverissimo\","
    "\"family\":\"Some Longer Family Name Here\",\"model\":\"A Model\","
    "\"version\":\"1.0.0\",\"tail\":\"END_MARKER\"}";
  midi2_ci_add_property_static(&s, "DeviceInfo", big);
  reset();

  const char inq[] = "{\"resource\":\"DeviceInfo\"}";
  uint8_t req[64];
  uint16_t req_len = midi2_ci_build_pe_get(req, MIDI2_CI_VERSION_2,
      0x0000001, s.muid, 0x01, (const uint8_t *)inq, (uint16_t)(sizeof(inq) - 1));
  midi2_ci_process_sysex(&s, 0, req, req_len);

  uint8_t resp[400];
  uint16_t n = extract_sysex7_data(resp, sizeof(resp));
  TEST("PE Get does not truncate a ~200-byte DeviceInfo value");
  CHECK(n > 16, "reply present");
  uint16_t hdr_len  = midi2_ci_read_14(&resp[14]);
  uint16_t body_off = (uint16_t)(16 + hdr_len + 6);
  uint16_t body_len = midi2_ci_read_14(&resp[body_off - 2]);
  CHECK(body_len == (uint16_t)(sizeof(big) - 1), "full value length echoed");
  CHECK(blob_contains(&resp[body_off], body_len, "END_MARKER"), "tail marker survives");
  CHECK(resp[body_off + body_len - 1] == '}', "body ends with closing brace");
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

/* v0.6.1: the convenience Responder implements MIDI-CI 1.2 (Process Inquiry,
 * PE, v2 message fields), so every reply must declare Message Version 0x02.
 * A Discovery Reply advertising Process Inquiry (0x10) while claiming version
 * 0x01 is invalid; the MIDI 2.0 Workbench flags it. The reply must also carry
 * the v2 trailing fields (Output Path Id + Function Block, Table 8). */
static void test_discovery_reply_declares_v2(void) {
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
  TEST("discovery reply declares version 0x02 with v2 fields");
  CHECK(n >= 31, "reply includes v2 trailing fields (31 bytes)");
  CHECK(body[4] == MIDI2_CI_VERSION_2, "message version == 0x02 (MIDI-CI 1.2)");
  /* Invariant: advertising Process Inquiry requires version >= 0x02. */
  CHECK(!(body[24] & 0x10) || body[4] >= MIDI2_CI_VERSION_2,
        "PI advertised => version >= 0x02");
  CHECK(body[29] == 0x00, "output path id == 0");
  CHECK(body[30] == 0x7F, "function block == 0x7F");
  PASS();
}

/* v0.6.1: ci_cat is configurable via midi2_ci_set_capabilities and flows into
 * the Discovery Reply, replacing the previously hardcoded 0x1C. */
static void test_set_capabilities_flows_to_reply(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x12345678, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_set_capabilities(&s,
      MIDI2_CI_CAT_PROFILE_CONFIG | MIDI2_CI_CAT_PROPERTY_EXCHANGE); /* 0x0C */
  reset();

  uint8_t req[30] = {0};
  req[0]=0x7E; req[1]=0x7F; req[2]=0x0D; req[3]=0x70; req[4]=0x01;
  midi2_ci_process_sysex(&s, 0, req, 30);

  uint8_t body[64];
  uint16_t n = extract_sysex7_data(body, sizeof(body));
  TEST("set_capabilities flows into discovery reply ci_cat");
  CHECK(n >= 25, "reply present");
  CHECK(body[24] == (MIDI2_CI_CAT_PROFILE_CONFIG | MIDI2_CI_CAT_PROPERTY_EXCHANGE),
        "ci_cat reflects configured value (0x0C)");
  PASS();
}

/* v0.6.1: all responder replies declare the same Message Version (0x02).
 * Mixing versions across replies makes an Initiator raise
 * "MIDI-CI Message Format Version has Changed". Guard the Profile Inquiry
 * Reply, which historically declared 0x01 while Discovery/NAK drifted. */
static void test_reply_version_consistency(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x11111111, test_profiles, 8, test_props, 4);
  midi2_ci_set_identity(&s, 0x00AABB, 0x0001, 0x0002, 0x00010000);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  uint8_t p1[] = {0x00, 0x21, 0x09, 0x00, 0x01};
  midi2_ci_add_profile(&s, p1);
  reset();

  uint8_t request[16];
  uint16_t req_len = midi2_ci_build_profile_inquiry(request, MIDI2_CI_VERSION_1,
                                                        0x0000001, s.muid, 0x7F);
  midi2_ci_process_sysex(&s, 0, request, req_len);

  uint8_t resp[64];
  uint16_t resp_len = extract_sysex7_data(resp, 64);
  TEST("profile inquiry reply declares version 0x02 (consistency)");
  CHECK(resp_len >= 5, "reply present");
  CHECK(resp[4] == MIDI2_CI_VERSION_2, "profile inquiry reply version == 0x02");
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

/* Helper: scan captured SysEx for an Invalidate MUID frame (sub-id 0x7E)
 * carrying the given target MUID. Returns true if found. Searches the
 * raw SysEx bytes captured by extract_sysex7_data. */
static bool saw_invalidate_muid_for(uint32_t target_muid) {
  uint8_t body[128];
  uint16_t n = extract_sysex7_data(body, sizeof body);
  uint16_t i;
  for (i = 0; i + 17 <= n; i++) {
    /* CI header minimum: 0x7E, dev_id, 0x0D, sub_id, ver, src×4, dst×4 */
    if (body[i] != 0x7E) continue;
    if (body[i + 2] != 0x0D) continue;
    if (body[i + 3] != 0x7E /* Invalidate MUID */) continue;
    /* After 13-byte common header, 4-byte target MUID (7-bit packed) */
    uint32_t t = (uint32_t)body[i + 13]
               | ((uint32_t)body[i + 14] << 7)
               | ((uint32_t)body[i + 15] << 14)
               | ((uint32_t)body[i + 16] << 21);
    if (t == target_muid) return true;
  }
  return false;
}

static void test_collision_broadcasts_invalidate_muid(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x00ABCDEF, NULL, 0, NULL, 0);
  _rng_value = 0x03333333u;
  midi2_ci_set_rng(&s, fake_rng, NULL);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  reset();

  uint32_t old = s.muid;
  uint8_t req[30] = {0};
  req[0]=0x7E; req[1]=0x7F; req[2]=0x0D; req[3]=0x70; req[4]=0x01;
  req[5]=old & 0x7F; req[6]=(old>>7)&0x7F; req[7]=(old>>14)&0x7F; req[8]=(old>>21)&0x7F;
  req[9]=0x7F; req[10]=0x7F; req[11]=0x7F; req[12]=0x7F;

  midi2_ci_process_sysex(&s, 0, req, 30);

  TEST("collision broadcasts Invalidate MUID by default (v0.3.0)");
  CHECK(s.muid != old, "MUID regenerated");
  CHECK(s.auto_invalidate_on_collision == true, "flag default on");
  CHECK(saw_invalidate_muid_for(old), "Invalidate MUID frame carries old MUID");
  PASS();
}

static void test_collision_broadcast_can_be_disabled(void) {
  midi2_ci_state s;
  midi2_ci_init(&s, 0x00ABCDEF, NULL, 0, NULL, 0);
  _rng_value = 0x04444444u;
  midi2_ci_set_rng(&s, fake_rng, NULL);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_set_identity(&s, 0x7D, 1, 1, 1);
  midi2_ci_set_auto_invalidate_on_collision(&s, false);
  reset();

  uint32_t old = s.muid;
  uint8_t req[30] = {0};
  req[0]=0x7E; req[1]=0x7F; req[2]=0x0D; req[3]=0x70; req[4]=0x01;
  req[5]=old & 0x7F; req[6]=(old>>7)&0x7F; req[7]=(old>>14)&0x7F; req[8]=(old>>21)&0x7F;
  req[9]=0x7F; req[10]=0x7F; req[11]=0x7F; req[12]=0x7F;

  midi2_ci_process_sysex(&s, 0, req, 30);

  TEST("collision Invalidate broadcast silenced when flag disabled");
  CHECK(s.muid != old, "MUID still regenerates");
  CHECK(!saw_invalidate_muid_for(old), "no Invalidate MUID frame emitted for old");
  PASS();
}

/* --- Subscribe / Notify (v0.3.0) --- */

static midi2_ci_subscriber test_subs[4];

static void test_init_ex_zeroes_subscriber_count(void) {
  TEST("init_ex: subscribers array bound and count zeroed");
  midi2_ci_state s;
  midi2_ci_init_ex(&s, 0xBEEF, test_profiles, 8, test_props, 4,
                    test_subs, 4);
  CHECK(s.subscribers == test_subs, "subscribers pointer bound");
  CHECK(s.subscriber_capacity == 4, "capacity stored");
  CHECK(s.subscriber_count == 0, "count zero");
  PASS();
}

static void test_classic_init_leaves_no_subscribers(void) {
  TEST("legacy midi2_ci_init delegates to init_ex with NULL subscribers");
  midi2_ci_state s;
  midi2_ci_init(&s, 0xCAFE, test_profiles, 8, test_props, 4);
  CHECK(s.subscribers == NULL, "no subscriber array");
  CHECK(s.subscriber_capacity == 0, "capacity zero");
  CHECK(s.subscriber_count == 0, "count zero");
  PASS();
}

static void test_pe_set_subscribable_toggle(void) {
  TEST("pe_set_subscribable: flips property flag at runtime");
  midi2_ci_state s;
  midi2_ci_init_ex(&s, 0xFEED, NULL, 0, test_props, 4, test_subs, 4);
  midi2_ci_add_property_static(&s, "Temp", "22");
  CHECK(!s.properties[0].subscribable, "flag default false");
  CHECK(midi2_ci_pe_set_subscribable(&s, "Temp", true) == MIDI2_CI_OK, "on ok");
  CHECK(s.properties[0].subscribable, "flag flipped on");
  CHECK(midi2_ci_pe_set_subscribable(&s, "Temp", false) == MIDI2_CI_OK, "off ok");
  CHECK(!s.properties[0].subscribable, "flag flipped off");
  CHECK(midi2_ci_pe_set_subscribable(&s, "missing", true) == MIDI2_CI_ERR_NOT_FOUND,
        "unknown property");
  PASS();
}

static void test_subscribe_add_remove_basic(void) {
  TEST("subscribe_add/remove: basic lifecycle with 2 subscribers");
  midi2_ci_state s;
  midi2_ci_init_ex(&s, 0x1234, NULL, 0, test_props, 4, test_subs, 4);
  midi2_ci_add_property_static(&s, "Temp", "22");
  midi2_ci_pe_set_subscribable(&s, "Temp", true);

  CHECK(midi2_ci_subscribe_add(&s, 0xAAA, "Temp") == MIDI2_CI_OK, "add #1");
  CHECK(midi2_ci_subscribe_add(&s, 0xBBB, "Temp") == MIDI2_CI_OK, "add #2");
  CHECK(midi2_ci_get_subscriber_count(&s) == 2, "count 2");

  /* Duplicate is idempotent */
  CHECK(midi2_ci_subscribe_add(&s, 0xAAA, "Temp") == MIDI2_CI_OK, "duplicate ok");
  CHECK(midi2_ci_get_subscriber_count(&s) == 2, "still 2");

  CHECK(midi2_ci_subscribe_remove(&s, 0xAAA, "Temp") == MIDI2_CI_OK, "remove #1");
  CHECK(midi2_ci_get_subscriber_count(&s) == 1, "count 1");
  CHECK(midi2_ci_subscribe_remove(&s, 0x999, "Temp") == MIDI2_CI_ERR_NOT_FOUND,
        "unknown muid NOT_FOUND");
  PASS();
}

static void test_subscribe_rejects_non_subscribable(void) {
  TEST("subscribe_add: property not marked subscribable returns NOT_FOUND");
  midi2_ci_state s;
  midi2_ci_init_ex(&s, 0x2222, NULL, 0, test_props, 4, test_subs, 4);
  midi2_ci_add_property_static(&s, "X", "v");
  /* NOT calling pe_set_subscribable: property stays non-subscribable */
  CHECK(midi2_ci_subscribe_add(&s, 0x1, "X") == MIDI2_CI_ERR_NOT_FOUND,
        "rejected");
  CHECK(midi2_ci_subscribe_add(&s, 0x1, "missing") == MIDI2_CI_ERR_NOT_FOUND,
        "unknown prop");
  PASS();
}

static void test_subscribe_full(void) {
  TEST("subscribe_add: returns ERR_FULL at capacity");
  midi2_ci_subscriber subs2[2];
  midi2_ci_state s;
  midi2_ci_init_ex(&s, 0x3333, NULL, 0, test_props, 4, subs2, 2);
  midi2_ci_add_property_static(&s, "X", "v");
  midi2_ci_pe_set_subscribable(&s, "X", true);
  CHECK(midi2_ci_subscribe_add(&s, 0x1, "X") == MIDI2_CI_OK, "#1");
  CHECK(midi2_ci_subscribe_add(&s, 0x2, "X") == MIDI2_CI_OK, "#2");
  CHECK(midi2_ci_subscribe_add(&s, 0x3, "X") == MIDI2_CI_ERR_FULL, "#3 full");
  PASS();
}

static void test_subscribe_requires_subscribers_array(void) {
  TEST("subscribe_add: state without subscribers array returns ERR_FULL");
  midi2_ci_state s;
  midi2_ci_init(&s, 0x4444, NULL, 0, test_props, 4);
  midi2_ci_add_property_static(&s, "X", "v");
  midi2_ci_pe_set_subscribable(&s, "X", true);
  CHECK(midi2_ci_subscribe_add(&s, 0x1, "X") == MIDI2_CI_ERR_FULL,
        "no subs array means FULL");
  CHECK(midi2_ci_get_subscriber_count(&s) == 0, "count untouched");
  PASS();
}

static void test_notify_property_changed_emits(void) {
  TEST("notify_property_changed: emits PE Notify to each subscriber");
  midi2_ci_state s;
  midi2_ci_init_ex(&s, 0x5555, NULL, 0, test_props, 4, test_subs, 4);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_add_property_static(&s, "Temp", "22");
  midi2_ci_pe_set_subscribable(&s, "Temp", true);
  midi2_ci_subscribe_add(&s, 0x100, "Temp");
  midi2_ci_subscribe_add(&s, 0x200, "Temp");
  reset();

  CHECK(midi2_ci_notify_property_changed(&s, "Temp") == MIDI2_CI_OK, "returns OK");
  CHECK(write_pos > 0, "UMP emitted");

  /* Scan captured SysEx for PE Notify (sub-ID 0x3F) -- should see 2 frames. */
  uint8_t body[512];
  uint16_t n = extract_sysex7_data(body, sizeof body);
  int notifies = 0;
  uint16_t i;
  for (i = 0; i + 13 <= n; i++) {
    if (body[i] == 0x7E && body[i + 2] == 0x0D && body[i + 3] == 0x3F)
      notifies++;
  }
  CHECK(notifies == 2, "two PE Notify frames emitted");
  PASS();
}

static void test_notify_unknown_property(void) {
  TEST("notify_property_changed: unknown name returns ERR_NOT_FOUND");
  midi2_ci_state s;
  midi2_ci_init_ex(&s, 0x6666, NULL, 0, test_props, 4, test_subs, 4);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  reset();
  CHECK(midi2_ci_notify_property_changed(&s, "nope") == MIDI2_CI_ERR_NOT_FOUND,
        "NOT_FOUND");
  CHECK(write_pos == 0, "no UMP emitted");
  PASS();
}

static void test_notify_without_subscribers_is_ok(void) {
  TEST("notify_property_changed: zero subscribers is OK with no emit");
  midi2_ci_state s;
  midi2_ci_init_ex(&s, 0x7777, NULL, 0, test_props, 4, test_subs, 4);
  midi2_ci_set_write_fn(&s, capture_write, NULL);
  midi2_ci_add_property_static(&s, "X", "v");
  midi2_ci_pe_set_subscribable(&s, "X", true);
  reset();
  CHECK(midi2_ci_notify_property_changed(&s, "X") == MIDI2_CI_OK, "OK");
  CHECK(write_pos == 0, "no emit without subscribers");
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

/* --- NULL paths (boundary defensive checks) --- */

void test_ci_init_null_safe(void) {
  TEST("ci_init/init_ex: NULL state is no-op (no crash)");
  midi2_ci_init(NULL, 0, NULL, 0, NULL, 0);
  midi2_ci_init_ex(NULL, 0, NULL, 0, NULL, 0, NULL, 0);
  PASS();
}

void test_ci_setters_null_safe(void) {
  TEST("ci setters: NULL state are no-op (no crash)");
  midi2_ci_set_identity(NULL, 0, 0, 0, 0);
  midi2_ci_set_write_fn(NULL, NULL, NULL);
  midi2_ci_set_rng(NULL, NULL, NULL);
  midi2_ci_set_nak_on_unknown(NULL, true);
  PASS();
}

void test_ci_new_muid_null_safe(void) {
  TEST("ci_new_muid: NULL state returns 0u (reserved sentinel)");
  CHECK(midi2_ci_new_muid(NULL) == 0u, "NULL state -> 0u");
  PASS();
}

void test_ci_profile_property_null_safe(void) {
  TEST("ci profile/property mutators: NULL state returns ERR_NULL");
  uint8_t profile[5] = {0};
  CHECK(midi2_ci_add_profile(NULL, profile) == MIDI2_CI_ERR_NULL, "add_profile NULL");
  CHECK(midi2_ci_remove_profile(NULL, profile) == MIDI2_CI_ERR_NULL, "remove_profile NULL");
  CHECK(midi2_ci_add_property_static(NULL, "x", "y") == MIDI2_CI_ERR_NULL, "add_property_static NULL");
  CHECK(midi2_ci_add_property_dynamic(NULL, "x", NULL, NULL) == MIDI2_CI_ERR_NULL, "add_property_dynamic NULL");
  PASS();
}

void test_ci_process_sysex_null_safe(void) {
  TEST("ci_process_sysex: NULL state returns false");
  uint8_t data[16] = {0xF0, 0x7E, 0x00, 0x0D, 0x70, 0x01};
  CHECK(!midi2_ci_process_sysex(NULL, 0, data, sizeof(data)), "NULL state -> false");
  PASS();
}

/* Track 3: NULL state/name returns MIDI2_CI_ERR_NULL consistently in the five
 * functions that previously returned MIDI2_CI_ERR_NOT_FOUND for NULL. */
void test_ci_null_arg_returns_err_null(void) {
  TEST("Track 3: NULL state returns ERR_NULL in 5 fns");
  CHECK(midi2_ci_remove_property(NULL, "x") == MIDI2_CI_ERR_NULL, "remove_property NULL state");
  CHECK(midi2_ci_pe_set_subscribable(NULL, "x", true) == MIDI2_CI_ERR_NULL, "pe_set_subscribable NULL state");
  CHECK(midi2_ci_subscribe_add(NULL, 1, "x") == MIDI2_CI_ERR_NULL, "subscribe_add NULL state");
  CHECK(midi2_ci_subscribe_remove(NULL, 1, "x") == MIDI2_CI_ERR_NULL, "subscribe_remove NULL state");
  CHECK(midi2_ci_notify_property_changed(NULL, "x") == MIDI2_CI_ERR_NULL, "notify NULL state");
  PASS();
}

void test_ci_null_name_vs_missing(void) {
  TEST("Track 3: NULL name -> ERR_NULL; missing name -> ERR_NOT_FOUND");
  midi2_ci_state s;
  midi2_ci_init(&s, 0, test_profiles, 8, test_props, 4);
  CHECK(midi2_ci_remove_property(&s, NULL) == MIDI2_CI_ERR_NULL, "remove_property NULL name");
  CHECK(midi2_ci_notify_property_changed(&s, NULL) == MIDI2_CI_ERR_NULL, "notify NULL name");
  CHECK(midi2_ci_remove_property(&s, "nope") == MIDI2_CI_ERR_NOT_FOUND, "missing name still NOT_FOUND");
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
  test_remove_property();
  test_reset_profiles_and_properties();

  printf("\n[Discovery]\n");
  test_discovery_reply();

  printf("\n[Profile Inquiry]\n");
  test_profile_inquiry_reply();

  printf("\n[Property Exchange]\n");
  test_pe_get_static();
  test_pe_get_reply_has_status_header();
  test_pe_get_matches_requested_resource();
  test_pe_get_unknown_resource_404();
  test_pe_get_list_has_totalcount();
  test_pe_get_resource_list();
  test_pe_get_zero_properties_replies();
  test_pe_get_resource_list_overflow_valid();
  test_pe_set_matches_resource_and_value();
  test_pe_set_unknown_resource_404();
  test_pe_set_setter_failure_not_200();
  test_pe_get_resource_list_app_override();
  test_pe_get_large_value_not_truncated();

  printf("\n[Edge Cases]\n");
  test_non_ci_returns_false();
  test_ci_without_identity_ignored();

  printf("\n[v0.2.4 compliance features]\n");
  test_discovery_reply_ci_cat_includes_pi();
  test_discovery_reply_declares_v2();
  test_set_capabilities_flows_to_reply();
  test_reply_version_consistency();
  test_pe_capability_autoreply();
  test_new_muid_avoids_reserved();
  test_invalidate_muid_regen_with_rng();
  test_invalidate_muid_ignore_other_target();
  test_muid_collision_triggers_regen();
  test_collision_broadcasts_invalidate_muid();
  test_collision_broadcast_can_be_disabled();
  test_nak_on_unknown_opt_in();

  printf("\n[Subscribe / Notify (v0.3.0)]\n");
  test_init_ex_zeroes_subscriber_count();
  test_classic_init_leaves_no_subscribers();
  test_pe_set_subscribable_toggle();
  test_subscribe_add_remove_basic();
  test_subscribe_rejects_non_subscribable();
  test_subscribe_full();
  test_subscribe_requires_subscribers_array();
  test_notify_property_changed_emits();
  test_notify_unknown_property();
  test_notify_without_subscribers_is_ok();

  printf("\n[NULL Paths]\n");
  test_ci_init_null_safe();
  test_ci_setters_null_safe();
  test_ci_new_muid_null_safe();
  test_ci_profile_property_null_safe();
  test_ci_process_sysex_null_safe();
  test_ci_null_arg_returns_err_null();
  test_ci_null_name_vs_missing();

  printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
