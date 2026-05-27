/*
 * midi2_ci_msg.h unit tests
 * Compile: gcc -std=c99 -I../src test_midi2_ci_msg.c -o test_midi2_ci_msg
 */

#include <stdio.h>
#include <string.h>
#include "midi2_ci_msg.h"

static int passed = 0;
static int failed = 0;

#define TEST(name) printf("  %-55s ", name)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/*--------------------------------------------------------------------+
 * MUID encoding
 *--------------------------------------------------------------------*/
void test_muid_roundtrip(void) {
  TEST("MUID: roundtrip encode/decode 0x0ABCDEF");
  uint8_t buf[4];
  midi2_ci_write_muid(buf, 0x0ABCDEF);
  uint32_t v = midi2_ci_read_muid(buf);
  CHECK(v == 0x0ABCDEF, "mismatch");
  PASS();
}

void test_muid_broadcast(void) {
  TEST("MUID: broadcast = 0x0FFFFFFF");
  uint8_t buf[4];
  midi2_ci_write_muid(buf, MIDI2_CI_BROADCAST_MUID);
  CHECK(buf[0] == 0x7F && buf[1] == 0x7F && buf[2] == 0x7F && buf[3] == 0x7F, "all 0x7F");
  uint32_t v = midi2_ci_read_muid(buf);
  CHECK(v == MIDI2_CI_BROADCAST_MUID, "roundtrip");
  PASS();
}

void test_14bit_roundtrip(void) {
  TEST("14-bit: roundtrip 0, 127, 128, 16383");
  uint8_t buf[2];
  uint16_t vals[] = {0, 127, 128, 16383};
  int i;
  for (i = 0; i < 4; i++) {
    midi2_ci_write_14(buf, vals[i]);
    uint16_t v = midi2_ci_read_14(buf);
    if (v != vals[i]) { FAIL("mismatch"); return; }
  }
  PASS();
}

/*--------------------------------------------------------------------+
 * Common header
 *--------------------------------------------------------------------*/
void test_header_build_parse(void) {
  TEST("Header: build and parse fields");
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_header(buf, 0x7F, 0x70, MIDI2_CI_VERSION_2,
                                           0x1234567, 0x0FFFFFFF);
  CHECK(len == 13, "header = 13 bytes");
  CHECK(buf[0] == 0x7E, "universal sysex");
  CHECK(buf[1] == 0x7F, "device_id = function block");
  CHECK(buf[2] == 0x0D, "sub-id#1 = MIDI-CI");
  CHECK(buf[3] == 0x70, "sub-id#2 = discovery");
  CHECK(buf[4] == 0x02, "version = 2");
  CHECK(midi2_ci_get_device_id(buf) == 0x7F, "parse device_id");
  CHECK(midi2_ci_get_sub_id(buf) == 0x70, "parse sub_id");
  CHECK(midi2_ci_get_version(buf) == 0x02, "parse version");
  CHECK(midi2_ci_get_src_muid(buf) == 0x1234567, "parse src_muid");
  CHECK(midi2_ci_get_dst_muid(buf) == MIDI2_CI_BROADCAST_MUID, "parse dst_muid");
  PASS();
}

void test_is_ci(void) {
  TEST("is_ci: detect CI message");
  uint8_t buf[16] = {0x7E, 0x7F, 0x0D, 0x70};
  CHECK(midi2_ci_is_ci(buf, 13), "valid CI");
  CHECK(!midi2_ci_is_ci(buf, 12), "too short");
  buf[0] = 0x7D;
  CHECK(!midi2_ci_is_ci(buf, 13), "wrong byte 0");
  PASS();
}

/*--------------------------------------------------------------------+
 * Discovery (0x70)
 *--------------------------------------------------------------------*/
void test_discovery_build(void) {
  TEST("Discovery: build v2, parse all fields");
  uint8_t buf[64];
  /* mfr 0x002109 = 3-byte SysEx ID (all bytes <= 0x7F) */
  uint16_t len = midi2_ci_build_discovery(buf, MIDI2_CI_VERSION_2, 0x1234567,
                                              0x002109, 0x0001, 0x0002, 0x00010000,
                                              MIDI2_CI_CAT_PROFILE_CONFIG | MIDI2_CI_CAT_PROPERTY_EXCHANGE,
                                              512, 0);
  CHECK(len == 30, "v2 discovery = 30 bytes");
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_DISCOVERY, "sub_id");
  CHECK(midi2_ci_get_dst_muid(buf) == MIDI2_CI_BROADCAST_MUID, "dst = broadcast");
  CHECK(midi2_ci_get_mfr_id(buf) == 0x002109, "mfr_id");
  CHECK(midi2_ci_get_family(buf) == 0x0001, "family");
  CHECK(midi2_ci_get_model(buf) == 0x0002, "model");
  CHECK(midi2_ci_get_sw_rev(buf) == 0x00010000, "sw_rev");
  CHECK(midi2_ci_get_ci_category(buf) == 0x0C, "ci_category");
  CHECK(midi2_ci_get_max_sysex(buf) == 512, "max_sysex=512");
  PASS();
}

void test_discovery_v1(void) {
  TEST("Discovery: v1 shorter (no output_path_id)");
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_discovery(buf, MIDI2_CI_VERSION_1, 0x1234567,
                                              0x00AABB, 0x0001, 0x0002, 0x00010000,
                                              0x0C, 512, 0);
  CHECK(len == 29, "v1 discovery = 29 bytes");
  PASS();
}

/*--------------------------------------------------------------------+
 * Discovery Reply (0x71)
 *--------------------------------------------------------------------*/
void test_discovery_reply(void) {
  TEST("Discovery Reply: build v2, function_block=0x7F");
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_discovery_reply(buf, MIDI2_CI_VERSION_2,
                                                     0x7654321, 0x1234567,
                                                     0x002109, 0x0001, 0x0002, 0x00010000,
                                                     0x0C, 512, 0, 0x7F);
  CHECK(len == 31, "v2 reply = 31 bytes");
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_DISCOVERY_REPLY, "sub_id");
  CHECK(midi2_ci_get_src_muid(buf) == 0x7654321, "src_muid");
  CHECK(midi2_ci_get_dst_muid(buf) == 0x1234567, "dst_muid");
  CHECK(buf[len - 1] == 0x7F, "function_block = 0x7F");
  PASS();
}

/*--------------------------------------------------------------------+
 * Endpoint Info (0x72/0x73)
 *--------------------------------------------------------------------*/
void test_endpoint_info(void) {
  TEST("Endpoint Info: inquiry + reply");
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_endpoint_info(buf, MIDI2_CI_VERSION_2,
                                                    0x1234567, 0x7654321, 0x00);
  CHECK(len == 14, "inquiry = 14 bytes");
  CHECK(buf[13] == 0x00, "status = 0x00");

  len = midi2_ci_build_endpoint_info_reply(buf, MIDI2_CI_VERSION_2,
                                               0x7654321, 0x1234567, 0x00,
                                               (const uint8_t *)"SN123", 5);
  CHECK(len == 21, "reply with 5-byte data = 21 bytes");
  CHECK(buf[13] == 0x00, "status = 0x00");
  CHECK(midi2_ci_read_14(&buf[14]) == 5, "info_len = 5");
  CHECK(buf[16] == 'S', "data[0] = 'S'");
  PASS();
}

/*--------------------------------------------------------------------+
 * Invalidate MUID (0x7E)
 *--------------------------------------------------------------------*/
void test_invalidate_muid(void) {
  TEST("Invalidate MUID: build and parse target");
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_invalidate_muid(buf, MIDI2_CI_VERSION_2,
                                                      0x1234567, 0x7654321);
  CHECK(len == 17, "invalidate = 17 bytes");
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_INVALIDATE_MUID, "sub_id");
  CHECK(midi2_ci_get_dst_muid(buf) == MIDI2_CI_BROADCAST_MUID, "dst = broadcast");
  CHECK(midi2_ci_get_target_muid(buf) == 0x7654321, "target_muid");
  PASS();
}

/*--------------------------------------------------------------------+
 * NAK (0x7F)
 *--------------------------------------------------------------------*/
void test_nak_v2(void) {
  TEST("NAK v2: status=not_supported, with text");
  uint8_t buf[64];
  uint8_t details[5] = {0x7E, 0x01, 0x00, 0x00, 0x00};
  uint16_t len = midi2_ci_build_nak(buf, MIDI2_CI_VERSION_2,
                                         0x1234567, 0x7654321, 0x7F,
                                         MIDI2_CI_PE_GET, MIDI2_CI_NAK_NOT_SUPPORTED, 0x00,
                                         details, 4, (const uint8_t *)"nope");
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_NAK, "sub_id");
  CHECK(midi2_ci_get_orig_sub_id(buf) == MIDI2_CI_PE_GET, "orig_sub_id");
  CHECK(midi2_ci_get_nak_status_code(buf) == MIDI2_CI_NAK_NOT_SUPPORTED, "status_code");
  CHECK(midi2_ci_get_nak_status_data(buf) == 0x00, "status_data");
  CHECK(buf[16] == 0x7E, "details[0]");
  CHECK(len > 13, "has payload");
  PASS();
}

void test_nak_v1(void) {
  TEST("NAK v1: header only (no extended fields)");
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_nak(buf, MIDI2_CI_VERSION_1,
                                         0x1234567, 0x7654321, 0x7F,
                                         0x34, 0x01, 0x00, NULL, 0, NULL);
  CHECK(len == 13, "v1 NAK = header only");
  PASS();
}

/*--------------------------------------------------------------------+
 * ACK (0x7D)
 *--------------------------------------------------------------------*/
void test_ack(void) {
  TEST("ACK: ok, no text");
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_ack(buf, MIDI2_CI_VERSION_2,
                                         0x1234567, 0x7654321, 0x7F,
                                         MIDI2_CI_PE_SET, MIDI2_CI_ACK_OK, 0x00,
                                         NULL, 0, NULL);
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_ACK, "sub_id");
  CHECK(midi2_ci_get_orig_sub_id(buf) == MIDI2_CI_PE_SET, "orig = PE Set");
  CHECK(len == 23, "ack with empty details and no text");
  PASS();
}

/*--------------------------------------------------------------------+
 * Profile Inquiry (0x20) + Reply (0x21)
 *--------------------------------------------------------------------*/
void test_profile_inquiry(void) {
  TEST("Profile Inquiry: header only");
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_profile_inquiry(buf, MIDI2_CI_VERSION_2,
                                                      0x1234567, 0x7654321, 0x7F);
  CHECK(len == 13, "inquiry = header only");
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PROFILE_INQUIRY, "sub_id");
  PASS();
}

void test_profile_inquiry_reply(void) {
  TEST("Profile Reply: 2 enabled, 1 disabled");
  const uint8_t enabled[2][5] = {{0x7E, 0x01, 0x00, 0x00, 0x00}, {0x7E, 0x02, 0x00, 0x00, 0x00}};
  const uint8_t disabled[1][5] = {{0x7E, 0x03, 0x00, 0x00, 0x00}};
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_profile_inquiry_reply(buf, MIDI2_CI_VERSION_2,
                                                            0x7654321, 0x1234567, 0x7F,
                                                            enabled, 2, disabled, 1);
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PROFILE_INQUIRY_REPLY, "sub_id");
  CHECK(midi2_ci_get_enabled_count(buf) == 2, "enabled_count = 2");
  /* First enabled profile at offset 15 (13 + 2 bytes count) */
  CHECK(buf[15] == 0x7E && buf[16] == 0x01, "enabled[0] = {7E,01,...}");
  /* Disabled count at offset 15 + 2*5 = 25 */
  CHECK(midi2_ci_read_14(&buf[25]) == 1, "disabled_count = 1");
  CHECK(len == 13 + 2 + 10 + 2 + 5, "total length");
  PASS();
}

/*--------------------------------------------------------------------+
 * Set Profile On/Off (0x22/0x23)
 *--------------------------------------------------------------------*/
void test_set_profile_on(void) {
  TEST("Set Profile On: v2 with num_channels");
  uint8_t prof[5] = {0x7E, 0x01, 0x00, 0x00, 0x00};
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_set_profile_on(buf, MIDI2_CI_VERSION_2,
                                                      0x1234567, 0x7654321, 0x00,
                                                      prof, 3);
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_SET_PROFILE_ON, "sub_id");
  CHECK(midi2_ci_get_device_id(buf) == 0x00, "device_id = channel 0");
  CHECK(len == 13 + 5 + 2, "header + profile + num_channels");
  PASS();
}

void test_set_profile_off(void) {
  TEST("Set Profile Off: v2 with reserved bytes");
  uint8_t prof[5] = {0x7E, 0x01, 0x00, 0x00, 0x00};
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_set_profile_off(buf, MIDI2_CI_VERSION_2,
                                                       0x1234567, 0x7654321, 0x7F, prof);
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_SET_PROFILE_OFF, "sub_id");
  CHECK(len == 13 + 5 + 2, "header + profile + reserved");
  PASS();
}

/*--------------------------------------------------------------------+
 * Profile Enabled/Disabled/Added/Removed Reports
 *--------------------------------------------------------------------*/
void test_profile_enabled(void) {
  TEST("Profile Enabled Report: broadcast dst");
  uint8_t prof[5] = {0x7E, 0x01, 0x00, 0x00, 0x00};
  uint8_t buf[32];
  midi2_ci_build_profile_enabled(buf, MIDI2_CI_VERSION_2, 0x1234567, 0x7F, prof, 1);
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PROFILE_ENABLED, "sub_id");
  CHECK(midi2_ci_get_dst_muid(buf) == MIDI2_CI_BROADCAST_MUID, "dst = broadcast");
  PASS();
}

void test_profile_added_removed(void) {
  TEST("Profile Added/Removed: broadcast, no channels field");
  uint8_t prof[5] = {0x7E, 0x05, 0x00, 0x00, 0x00};
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_profile_added(buf, MIDI2_CI_VERSION_2, 0x1234567, 0x7F, prof);
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PROFILE_ADDED, "sub_id = added");
  CHECK(len == 13 + 5, "header + profile only");

  len = midi2_ci_build_profile_removed(buf, MIDI2_CI_VERSION_2, 0x1234567, 0x7F, prof);
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PROFILE_REMOVED, "sub_id = removed");
  CHECK(len == 13 + 5, "header + profile only");
  PASS();
}

/*--------------------------------------------------------------------+
 * Profile Details (0x28/0x29)
 *--------------------------------------------------------------------*/
void test_profile_details(void) {
  TEST("Profile Details: inquiry + reply with data");
  uint8_t prof[5] = {0x7E, 0x01, 0x00, 0x00, 0x00};
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_profile_details(buf, MIDI2_CI_VERSION_2,
                                                       0x1234567, 0x7654321, 0x7F, prof, 0x01);
  CHECK(len == 13 + 5 + 1, "inquiry = header + profile + target");

  uint8_t data[] = {0x01, 0x02, 0x03};
  len = midi2_ci_build_profile_details_reply(buf, MIDI2_CI_VERSION_2,
                                                    0x7654321, 0x1234567, 0x7F, prof,
                                                    0x01, data, 3);
  CHECK(len == 13 + 5 + 1 + 2 + 3, "reply = header + profile + target + len + data");
  PASS();
}

/*--------------------------------------------------------------------+
 * Profile Specific Data (0x2F)
 *--------------------------------------------------------------------*/
void test_profile_specific_data(void) {
  TEST("Profile Specific Data: no cap on length");
  uint8_t prof[5] = {0x7E, 0x01, 0x00, 0x00, 0x00};
  uint8_t buf[1024];
  uint8_t data[600];
  memset(data, 0x42, 600);
  uint16_t len = midi2_ci_build_profile_specific_data(buf, MIDI2_CI_VERSION_2,
                                                             0x1234567, 0x7654321, 0x7F,
                                                             prof, data, 600);
  CHECK(len == 13 + 5 + 4 + 600, "no cap: full 600 bytes");
  CHECK(buf[13 + 5 + 4] == 0x42, "data starts correctly");
  CHECK(buf[13 + 5 + 4 + 599] == 0x42, "data[599] present");
  PASS();
}

/*--------------------------------------------------------------------+
 * PE Capabilities (0x30/0x31)
 *--------------------------------------------------------------------*/
void test_pe_capability(void) {
  TEST("PE Capability: inquiry + reply v2");
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_pe_capability(buf, MIDI2_CI_VERSION_2,
                                                    0x1234567, 0x7654321, 3, 0, 0);
  CHECK(len == 16, "v2 = 13 + 1 + 2");
  CHECK(buf[13] == 3, "max_simultaneous = 3");
  CHECK(buf[14] == 0, "pe_ver_major = 0");
  CHECK(buf[15] == 0, "pe_ver_minor = 0");

  len = midi2_ci_build_pe_capability_reply(buf, MIDI2_CI_VERSION_2,
                                                  0x7654321, 0x1234567, 2, 0, 0);
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PE_CAPABILITY_REPLY, "sub_id");
  CHECK(buf[13] == 2, "max_simultaneous = 2");
  PASS();
}

/*--------------------------------------------------------------------+
 * PE Get/Set/Subscribe/Notify (generic pe_data)
 *--------------------------------------------------------------------*/
void test_pe_get(void) {
  TEST("PE Get: header only, no body");
  uint8_t buf[64];
  const uint8_t hdr[] = "{\"resource\":\"DeviceInfo\"}";
  uint16_t hdr_len = 24;
  uint16_t len = midi2_ci_build_pe_get(buf, MIDI2_CI_VERSION_2,
                                            0x1234567, 0x7654321, 1, hdr, hdr_len);
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PE_GET, "sub_id");
  CHECK(midi2_ci_get_pe_request_id(buf) == 1, "request_id = 1");
  CHECK(midi2_ci_get_pe_header_len(buf) == hdr_len, "header_len");
  /* num_chunks at offset 14 + 2 + hdr_len */
  uint16_t nc_off = (uint16_t)(14 + 2 + hdr_len);
  CHECK(midi2_ci_read_14(&buf[nc_off]) == 1, "num_chunks = 1");
  CHECK(midi2_ci_read_14(&buf[nc_off + 2]) == 1, "this_chunk = 1");
  CHECK(midi2_ci_read_14(&buf[nc_off + 4]) == 0, "body_len = 0");
  (void)len;
  PASS();
}

void test_pe_get_reply_chunked(void) {
  TEST("PE Get Reply: chunk 2 of 3 with body");
  uint8_t buf[64];
  const uint8_t body[] = "partial data";
  uint16_t len = midi2_ci_build_pe_get_reply(buf, MIDI2_CI_VERSION_2,
                                                    0x7654321, 0x1234567, 1,
                                                    NULL, 0,  /* no header in chunk 2 */
                                                    3, 2,     /* chunk 2 of 3 */
                                                    body, 12);
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PE_GET_REPLY, "sub_id");
  /* header_len = 0, num_chunks = 3, this_chunk = 2, body_len = 12 */
  CHECK(midi2_ci_read_14(&buf[14]) == 0, "header_len = 0");
  CHECK(midi2_ci_read_14(&buf[16]) == 3, "num_chunks = 3");
  CHECK(midi2_ci_read_14(&buf[18]) == 2, "this_chunk = 2");
  CHECK(midi2_ci_read_14(&buf[20]) == 12, "body_len = 12");
  CHECK(buf[22] == 'p', "body[0] = 'p'");
  (void)len;
  PASS();
}

void test_pe_set(void) {
  TEST("PE Set: header + body, single chunk");
  uint8_t buf[128];
  const uint8_t hdr[] = "{\"resource\":\"X\"}";
  const uint8_t body[] = "{\"value\":42}";
  uint16_t len = midi2_ci_build_pe_set(buf, MIDI2_CI_VERSION_2,
                                             0x1234567, 0x7654321, 1,
                                             hdr, 16, 1, 1, body, 12);
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PE_SET, "sub_id");
  CHECK(len > 40, "reasonable length");
  PASS();
}

void test_pe_subscribe(void) {
  TEST("PE Subscribe: builds correctly");
  uint8_t buf[64];
  const uint8_t hdr[] = "{\"resource\":\"X\"}";
  uint16_t len = midi2_ci_build_pe_subscribe(buf, MIDI2_CI_VERSION_2,
                                                    0x1234567, 0x7654321, 1,
                                                    hdr, 16, 1, 1, NULL, 0);
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PE_SUBSCRIBE, "sub_id");
  CHECK(len > 13, "has payload");
  PASS();
}

void test_pe_notify(void) {
  TEST("PE Notify: builds correctly");
  uint8_t buf[64];
  uint16_t len = midi2_ci_build_pe_notify(buf, MIDI2_CI_VERSION_2,
                                                0x1234567, 0x7654321, 1,
                                                (const uint8_t *)"{}", 2,
                                                1, 1, NULL, 0);
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PE_NOTIFY, "sub_id");
  CHECK(len > 13, "has payload");
  PASS();
}

/*--------------------------------------------------------------------+
 * Process Inquiry (0x40-0x44)
 *--------------------------------------------------------------------*/
void test_pi_capability(void) {
  TEST("PI Capability: inquiry + reply");
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_pi_capability(buf, MIDI2_CI_VERSION_2,
                                                     0x1234567, 0x7654321);
  CHECK(len == 13, "inquiry = header only");
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PI_CAPABILITY, "sub_id");

  len = midi2_ci_build_pi_capability_reply(buf, MIDI2_CI_VERSION_2,
                                                  0x7654321, 0x1234567, 0x01);
  CHECK(len == 14, "reply = header + 1 byte features");
  CHECK(buf[13] == 0x01, "features = MIDI Message Report");
  PASS();
}

void test_pi_midi_report(void) {
  TEST("PI MIDI Report: inquiry with bitmaps");
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_pi_midi_report(buf, MIDI2_CI_VERSION_2,
                                                       0x1234567, 0x7654321, 0x7F,
                                                       0x01, 0x07, 0x00, 0x3F, 0x1F);
  CHECK(len == 18, "inquiry = header + 5 fields");
  CHECK(midi2_ci_get_pi_msg_data_control(buf) == 0x01, "msg_data_control");
  CHECK(midi2_ci_get_pi_system_bitmap(buf) == 0x07, "system_bitmap");
  CHECK(midi2_ci_get_pi_channel_ctrl_bitmap(buf) == 0x3F, "channel_ctrl");
  CHECK(midi2_ci_get_pi_note_data_bitmap(buf) == 0x1F, "note_data");
  PASS();
}

void test_pi_midi_report_reply(void) {
  TEST("PI MIDI Report Reply: bitmaps");
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_pi_midi_report_reply(buf, MIDI2_CI_VERSION_2,
                                                              0x7654321, 0x1234567, 0x7F,
                                                              0x03, 0x00, 0x1F, 0x07);
  CHECK(len == 17, "reply = header + 4 fields");
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PI_MIDI_REPORT_REPLY, "sub_id");
  PASS();
}

void test_pi_midi_report_end(void) {
  TEST("PI MIDI Report End: header only");
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_pi_midi_report_end(buf, MIDI2_CI_VERSION_2,
                                                            0x7654321, 0x1234567, 0x7F);
  CHECK(len == 13, "end = header only");
  CHECK(midi2_ci_get_sub_id(buf) == MIDI2_CI_PI_MIDI_REPORT_END, "sub_id");
  PASS();
}

/*--------------------------------------------------------------------+
 * Roundtrip: build -> parse -> verify
 *--------------------------------------------------------------------*/
void test_roundtrip_discovery(void) {
  TEST("Roundtrip: build discovery, parse all fields");
  uint8_t buf[64];
  /* All values use valid 7-bit bytes for SysEx encoding */
  /* family/model max 14-bit (16383), mfr bytes all <= 0x7F, sw_rev/max_sysex 28-bit */
  midi2_ci_build_discovery(buf, MIDI2_CI_VERSION_2, 0x0ABCDEF,
                               0x002109, 0x1234, 0x3FFF, 0x01234567,
                               0x1C, 1024, 3);
  CHECK(midi2_ci_get_src_muid(buf) == 0x0ABCDEF, "src_muid roundtrip");
  CHECK(midi2_ci_get_mfr_id(buf) == 0x002109, "mfr_id roundtrip");
  CHECK(midi2_ci_get_family(buf) == 0x1234, "family roundtrip");
  CHECK(midi2_ci_get_model(buf) == 0x3FFF, "model roundtrip");
  CHECK(midi2_ci_get_sw_rev(buf) == 0x01234567, "sw_rev roundtrip");
  CHECK(midi2_ci_get_ci_category(buf) == 0x1C, "ci_category roundtrip");
  CHECK(midi2_ci_get_max_sysex(buf) == 1024, "max_sysex roundtrip");
  PASS();
}

/*--------------------------------------------------------------------+
 * NULL paths (gate function defensive)
 *--------------------------------------------------------------------*/

void test_is_ci_null_safe(void) {
  TEST("is_ci: NULL d returns false regardless of length");
  CHECK(!midi2_ci_is_ci(NULL, 0), "NULL d, len=0");
  CHECK(!midi2_ci_is_ci(NULL, 12), "NULL d, len<13");
  CHECK(!midi2_ci_is_ci(NULL, 13), "NULL d, len=13 (would-be UB pre-fix)");
  CHECK(!midi2_ci_is_ci(NULL, 1024), "NULL d, large len");
  PASS();
}

/*--------------------------------------------------------------------+
 * Main
 *--------------------------------------------------------------------*/
int main(void) {
  printf("\n=== midi2_ci_msg.h Unit Tests ===\n\n");

  printf("[MUID Encoding]\n");
  test_muid_roundtrip();
  test_muid_broadcast();
  test_14bit_roundtrip();

  printf("\n[Common Header]\n");
  test_header_build_parse();
  test_is_ci();

  printf("\n[Management]\n");
  test_discovery_build();
  test_discovery_v1();
  test_discovery_reply();
  test_endpoint_info();
  test_invalidate_muid();
  test_nak_v2();
  test_nak_v1();
  test_ack();

  printf("\n[Profile Configuration]\n");
  test_profile_inquiry();
  test_profile_inquiry_reply();
  test_set_profile_on();
  test_set_profile_off();
  test_profile_enabled();
  test_profile_added_removed();
  test_profile_details();
  test_profile_specific_data();

  printf("\n[Property Exchange]\n");
  test_pe_capability();
  test_pe_get();
  test_pe_get_reply_chunked();
  test_pe_set();
  test_pe_subscribe();
  test_pe_notify();

  printf("\n[Process Inquiry]\n");
  test_pi_capability();
  test_pi_midi_report();
  test_pi_midi_report_reply();
  test_pi_midi_report_end();

  printf("\n[Roundtrip]\n");
  test_roundtrip_discovery();

  printf("\n[NULL Paths]\n");
  test_is_ci_null_safe();

  printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
