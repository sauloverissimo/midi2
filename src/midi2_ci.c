/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Saulo Verissimo
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "midi2_ci.h"
#include <string.h>

/*--------------------------------------------------------------------+
 * Init
 *--------------------------------------------------------------------*/
void midi2_ci_init(midi2_ci_state *state, uint32_t muid_seed,
                     uint8_t (*profiles)[5], uint8_t max_profiles,
                     midi2_ci_property *properties, uint8_t max_properties) {
  memset(state, 0, sizeof(midi2_ci_state));
  state->muid = muid_seed & 0x0FFFFFFF;
  state->profiles = profiles;
  state->profile_capacity = max_profiles;
  state->properties = properties;
  state->property_capacity = max_properties;
}

void midi2_ci_set_identity(midi2_ci_state *state,
                             uint32_t manufacturer_id, uint16_t family_id,
                             uint16_t model_id, uint32_t version_id) {
  state->manufacturer_id = manufacturer_id;
  state->family_id = family_id;
  state->model_id = model_id;
  state->version_id = version_id;
}

void midi2_ci_set_write_fn(midi2_ci_state *state,
                              midi2_proc_write_fn write_fn, void *context) {
  state->write_fn = write_fn;
  state->write_context = context;
}

/*--------------------------------------------------------------------+
 * Profiles
 *--------------------------------------------------------------------*/
int midi2_ci_add_profile(midi2_ci_state *state, const uint8_t profile_id[5]) {
  if (state->profiles == NULL) return MIDI2_CI_ERR_NULL;
  if (state->profile_count >= state->profile_capacity) return MIDI2_CI_ERR_FULL;
  memcpy(state->profiles[state->profile_count], profile_id, 5);
  state->profile_count++;
  return MIDI2_CI_OK;
}

int midi2_ci_remove_profile(midi2_ci_state *state, const uint8_t profile_id[5]) {
  uint8_t i;
  for (i = 0; i < state->profile_count; i++) {
    if (memcmp(state->profiles[i], profile_id, 5) == 0) {
      uint8_t j;
      for (j = i; j < state->profile_count - 1; j++) {
        memcpy(state->profiles[j], state->profiles[j + 1], 5);
      }
      state->profile_count--;
      return MIDI2_CI_OK;
    }
  }
  return MIDI2_CI_ERR_NOT_FOUND;
}

/*--------------------------------------------------------------------+
 * Properties
 *--------------------------------------------------------------------*/
int midi2_ci_add_property_static(midi2_ci_state *state,
                                    const char *name, const char *value) {
  if (state->properties == NULL) return MIDI2_CI_ERR_NULL;
  if (state->property_count >= state->property_capacity) return MIDI2_CI_ERR_FULL;
  state->properties[state->property_count].name = name;
  state->properties[state->property_count].static_value = value;
  state->properties[state->property_count].getter = NULL;
  state->properties[state->property_count].setter = NULL;
  state->property_count++;
  return MIDI2_CI_OK;
}

int midi2_ci_add_property_dynamic(midi2_ci_state *state,
                                     const char *name,
                                     midi2_ci_pe_getter getter,
                                     midi2_ci_pe_setter setter) {
  if (state->properties == NULL) return MIDI2_CI_ERR_NULL;
  if (state->property_count >= state->property_capacity) return MIDI2_CI_ERR_FULL;
  state->properties[state->property_count].name = name;
  state->properties[state->property_count].static_value = NULL;
  state->properties[state->property_count].getter = getter;
  state->properties[state->property_count].setter = setter;
  state->property_count++;
  return MIDI2_CI_OK;
}

/*--------------------------------------------------------------------+
 * Internal: send SysEx via write function
 *--------------------------------------------------------------------*/
static void ci_send(midi2_ci_state *state, uint8_t group,
                     const uint8_t *data, uint16_t length) {
  if (state->write_fn) {
    midi2_proc_send_sysex7(group, data, length, state->write_fn, state->write_context);
  }
}

/*--------------------------------------------------------------------+
 * Internal: extract source MUID from CI message (bytes 4-7, 7-bit encoding)
 *--------------------------------------------------------------------*/
static uint32_t ci_get_source_muid(const uint8_t *data) {
  return (uint32_t)data[4]
       | ((uint32_t)data[5] << 7)
       | ((uint32_t)data[6] << 14)
       | ((uint32_t)data[7] << 21);
}

/*--------------------------------------------------------------------+
 * Internal: write MUID into buffer at offset (7-bit encoding, 4 bytes)
 *--------------------------------------------------------------------*/
static void ci_write_muid(uint8_t *buf, uint32_t muid) {
  buf[0] = (uint8_t)((muid >> 0) & 0x7F);
  buf[1] = (uint8_t)((muid >> 7) & 0x7F);
  buf[2] = (uint8_t)((muid >> 14) & 0x7F);
  buf[3] = (uint8_t)((muid >> 21) & 0x7F);
}

/*--------------------------------------------------------------------+
 * Discovery Reply
 *--------------------------------------------------------------------*/
static bool ci_handle_discovery(midi2_ci_state *state, uint8_t group,
                                  const uint8_t *data, uint16_t length) {
  if (length < 13 || state->manufacturer_id == 0) return false;

  uint32_t src_muid = ci_get_source_muid(data);
  uint8_t reply[29];

  reply[0] = 0x7E;                                        /* Universal SysEx */
  reply[1] = 0x7F;                                        /* Device ID (all) */
  reply[2] = 0x0D;                                        /* MIDI-CI */
  reply[3] = MIDI2_CI_DISCOVERY_REPLY;

  ci_write_muid(&reply[4], state->muid);                  /* Our MUID */
  ci_write_muid(&reply[8], src_muid);                     /* Destination MUID */

  reply[12] = (uint8_t)((state->manufacturer_id >> 0) & 0x7F);
  reply[13] = (uint8_t)((state->manufacturer_id >> 8) & 0x7F);
  reply[14] = (uint8_t)((state->manufacturer_id >> 16) & 0x7F);

  reply[15] = (uint8_t)((state->family_id >> 0) & 0x7F);
  reply[16] = (uint8_t)((state->family_id >> 8) & 0x7F);

  reply[17] = (uint8_t)((state->model_id >> 0) & 0x7F);
  reply[18] = (uint8_t)((state->model_id >> 8) & 0x7F);

  reply[19] = (uint8_t)((state->version_id >> 0) & 0x7F);
  reply[20] = (uint8_t)((state->version_id >> 7) & 0x7F);
  reply[21] = (uint8_t)((state->version_id >> 14) & 0x7F);
  reply[22] = (uint8_t)((state->version_id >> 21) & 0x7F);

  reply[23] = 0x0C;          /* CI category: bit2=Profile Config, bit3=PE */
  reply[24] = 0x00;          /* Max SysEx size (512) */
  reply[25] = 0x04;
  reply[26] = 0x00;
  reply[27] = 0x00;
  reply[28] = 0x00;          /* Output path ID */

  ci_send(state, group, reply, 29);
  return true;
}

/*--------------------------------------------------------------------+
 * Profile Inquiry Reply
 *--------------------------------------------------------------------*/
static void ci_handle_profile_inquiry(midi2_ci_state *state, uint8_t group,
                                        const uint8_t *data, uint16_t length) {
  if (length < 8) return;

  uint32_t src_muid = ci_get_source_muid(data);
  /* enabled profiles + 2 bytes disabled count (always 0 for now) */
  uint16_t reply_len = (uint16_t)(14 + state->profile_count * 5 + 2);
  uint8_t reply[256]; /* max ~48 profiles */
  if (reply_len > sizeof(reply)) return;

  reply[0] = 0x7E;
  reply[1] = 0x7F;
  reply[2] = 0x0D;
  reply[3] = MIDI2_CI_PROFILE_INQUIRY_REPLY;

  ci_write_muid(&reply[4], state->muid);
  ci_write_muid(&reply[8], src_muid);

  reply[12] = state->profile_count & 0x7F;   /* enabled count (low) */
  reply[13] = 0;                              /* enabled count (high) */

  {
    uint8_t i;
    for (i = 0; i < state->profile_count; i++) {
      memcpy(&reply[14 + i * 5], state->profiles[i], 5);
    }
  }

  /* Disabled profiles count (per MIDI-CI 1.2: must be present, even if 0) */
  {
    uint16_t disabled_offset = (uint16_t)(14 + state->profile_count * 5);
    reply[disabled_offset] = 0x00;
    reply[disabled_offset + 1] = 0x00;
  }

  ci_send(state, group, reply, reply_len);
}

/*--------------------------------------------------------------------+
 * PE Get handler (basic: looks up property by name, returns value)
 *--------------------------------------------------------------------*/
static void ci_handle_pe_get(midi2_ci_state *state, uint8_t group,
                               const uint8_t *data, uint16_t length) {
  if (length < 14 || state->property_count == 0) return;

  uint32_t src_muid = ci_get_source_muid(data);
  uint8_t request_id = data[12];

  /* Extract property name from PE header (simplified: assumes name follows
   * immediately after the CI header at a known offset). Full PE parsing
   * requires JSON header decoding which is out of scope for embedded.
   * This basic implementation matches by scanning properties list. */

  /* For basic PE, we respond with all properties if no specific name match.
   * This is a simplified responder -- full PE with JSON headers and chunking
   * should use AM_MIDI2.0Lib or a dedicated PE library. */
  uint8_t i;
  for (i = 0; i < state->property_count; i++) {
    const char *value = NULL;
    if (state->properties[i].getter) {
      value = state->properties[i].getter(state->properties[i].name, state->context);
    } else {
      value = state->properties[i].static_value;
    }

    if (value != NULL) {
      /* Build PE Get Reply with the value */
      uint8_t reply[64];
      uint16_t val_len = 0;
      while (value[val_len] && val_len < 30) val_len++;

      reply[0] = 0x7E;
      reply[1] = 0x7F;
      reply[2] = 0x0D;
      reply[3] = MIDI2_CI_PE_GET_REPLY;
      ci_write_muid(&reply[4], state->muid);
      ci_write_muid(&reply[8], src_muid);
      reply[12] = request_id;
      /* Simplified: header length = 0, body = raw value bytes */
      reply[13] = 0x00;  /* header length low */
      reply[14] = 0x00;  /* header length high */
      reply[15] = 0x01;  /* number of chunks = 1 */
      reply[16] = 0x01;  /* this chunk = 1 */
      reply[17] = (uint8_t)(val_len & 0x7F);  /* body length low */
      reply[18] = (uint8_t)((val_len >> 7) & 0x7F);  /* body length high */
      {
        uint16_t j;
        for (j = 0; j < val_len; j++) {
          reply[19 + j] = (uint8_t)value[j];
        }
      }
      ci_send(state, group, reply, (uint16_t)(19 + val_len));
      return;  /* Return first matching property */
    }
  }
}

/*--------------------------------------------------------------------+
 * PE Set handler (basic: calls setter for first matching property)
 *--------------------------------------------------------------------*/
static void ci_handle_pe_set(midi2_ci_state *state, uint8_t group,
                               const uint8_t *data, uint16_t length) {
  if (length < 14 || state->property_count == 0) return;

  uint32_t src_muid = ci_get_source_muid(data);
  uint8_t request_id = data[12];
  uint8_t i;

  /* Simplified: try to call the first property setter that exists */
  for (i = 0; i < state->property_count; i++) {
    if (state->properties[i].setter) {
      /* Extract value from body (simplified: skip header, read raw bytes) */
      bool ok = state->properties[i].setter(state->properties[i].name, "", state->context);

      /* Build PE Set Reply */
      uint8_t reply[16];
      reply[0] = 0x7E;
      reply[1] = 0x7F;
      reply[2] = 0x0D;
      reply[3] = MIDI2_CI_PE_SET_REPLY;
      ci_write_muid(&reply[4], state->muid);
      ci_write_muid(&reply[8], src_muid);
      reply[12] = request_id;
      reply[13] = ok ? 0x00 : 0x01;  /* status: 0=ok, 1=error */
      reply[14] = 0x00;
      reply[15] = 0x00;
      ci_send(state, group, reply, 16);
      return;
    }
  }
}

/*--------------------------------------------------------------------+
 * Process Inquiry handler (reports supported message types)
 *--------------------------------------------------------------------*/
static void ci_handle_process_inquiry(midi2_ci_state *state, uint8_t group,
                                        const uint8_t *data, uint16_t length) {
  if (length < 8) return;

  uint32_t src_muid = ci_get_source_muid(data);

  /* Process Inquiry Reply (sub-ID 0x21): report MIDI message capabilities */
  uint8_t reply[16];
  reply[0] = 0x7E;
  reply[1] = 0x7F;
  reply[2] = 0x0D;
  reply[3] = 0x21;  /* Process Inquiry Reply */
  ci_write_muid(&reply[4], state->muid);
  ci_write_muid(&reply[8], src_muid);
  reply[12] = 0x01;  /* supported: MIDI 2.0 Channel Voice */
  reply[13] = 0x00;
  reply[14] = 0x00;
  reply[15] = 0x00;
  ci_send(state, group, reply, 16);
}

/*--------------------------------------------------------------------+
 * Process incoming SysEx
 *--------------------------------------------------------------------*/
static bool ci_check_header(const uint8_t *data, uint16_t length) {
  /* MIDI-CI Universal SysEx: 7E <deviceID> 0D <subID> ... */
  return (length >= 4 && data[0] == 0x7E && data[2] == 0x0D);
}

bool midi2_ci_process_sysex(midi2_ci_state *state,
                               uint8_t group, const uint8_t *data, uint16_t length) {
  if (!ci_check_header(data, length)) return false;

  switch (data[3]) {
    case MIDI2_CI_DISCOVERY_REQUEST:
      return ci_handle_discovery(state, group, data, length);

    case MIDI2_CI_PROFILE_INQUIRY:
      ci_handle_profile_inquiry(state, group, data, length);
      return true;

    case MIDI2_CI_PE_GET:
      ci_handle_pe_get(state, group, data, length);
      return true;

    case MIDI2_CI_PE_SET:
      ci_handle_pe_set(state, group, data, length);
      return true;

    case 0x20:  /* Process Inquiry */
      ci_handle_process_inquiry(state, group, data, length);
      return true;

    default:
      return false;
  }
}
