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

/*
 * midi2_ci.c - MIDI-CI convenience responder implementation
 *
 * Part of midi2 - Portable MIDI 2.0 library (C99)
 * https://github.com/sauloverissimo/midi2
 *
 * Spec: MIDI-CI (M2-101-UM v1.2, Jun 2023)
 * Version: 0.2.3
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

void midi2_ci_set_rng(midi2_ci_state *state,
                         midi2_ci_rng_fn rng, void *context) {
  state->rng         = rng;
  state->rng_context = context;
}

void midi2_ci_set_nak_on_unknown(midi2_ci_state *state, bool enabled) {
  state->nak_on_unknown = enabled;
}

uint32_t midi2_ci_new_muid(midi2_ci_state *state) {
  uint32_t m;
  uint8_t tries = 0;
  do {
    if (state->rng) {
      m = state->rng(state->rng_context) & 0x0FFFFFFFu;
    } else {
      /* Fallback: perturb current MUID. Better than returning a reserved
       * value. Real devices should always install an RNG. */
      m = (state->muid * 1103515245u + 12345u) & 0x0FFFFFFFu;
    }
    if (++tries > 8) break; /* avoid pathological loop */
  } while (m == 0u || m == 0x0FFFFFFFu);
  if (m == 0u || m == 0x0FFFFFFFu) m = 0x12345678u; /* hard fallback */
  state->muid = m;
  return m;
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
 * Discovery Reply -- uses midi2_ci_build_discovery_reply
 *--------------------------------------------------------------------*/
static bool ci_handle_discovery(midi2_ci_state *state, uint8_t group,
                                  const uint8_t *data, uint16_t length) {
  if (length < 13 || state->manufacturer_id == 0) return false;

  uint32_t src_muid = midi2_ci_get_src_muid(data);
  /* Declare every CI Category the convenience responder actually handles.
   * The handlers for Profile Inquiry, PE, and PI Capability all live here;
   * Discovery must advertise them so Initiators know to send the related
   * inquiries. Bug present through v0.2.3: Process Inquiry was handled but
   * not announced. */
  uint8_t ci_cat = MIDI2_CI_CAT_PROFILE_CONFIG
                 | MIDI2_CI_CAT_PROPERTY_EXCHANGE
                 | MIDI2_CI_CAT_PROCESS_INQUIRY;

  uint8_t reply[32];
  uint16_t reply_len = midi2_ci_build_discovery_reply(
      reply, MIDI2_CI_VERSION_1, state->muid, src_muid,
      state->manufacturer_id, state->family_id, state->model_id,
      state->version_id, ci_cat, 512, 0, 0x7F);

  ci_send(state, group, reply, reply_len);
  return true;
}

/*--------------------------------------------------------------------+
 * Profile Inquiry Reply -- uses midi2_ci_build_profile_inquiry_reply
 *--------------------------------------------------------------------*/
static void ci_handle_profile_inquiry(midi2_ci_state *state, uint8_t group,
                                        const uint8_t *data, uint16_t length) {
  if (length < 13) return;

  uint32_t src_muid = midi2_ci_get_src_muid(data);

  uint8_t reply[256];
  uint16_t reply_len = midi2_ci_build_profile_inquiry_reply(
      reply, MIDI2_CI_VERSION_1, state->muid, src_muid,
      midi2_ci_get_device_id(data),
      (const uint8_t (*)[5])state->profiles, state->profile_count,
      NULL, 0);

  ci_send(state, group, reply, reply_len);
}

/*--------------------------------------------------------------------+
 * PE Capability Reply -- uses midi2_ci_build_pe_capability_reply
 *
 * Parallels Profile Inquiry and PI Capability: without this, an
 * Initiator asking "do you do PE?" gets no answer and never tries
 * PE GET/SET. Advertises max_simultaneous=1 and PE v1.0 (basic).
 *--------------------------------------------------------------------*/
static void ci_handle_pe_capability(midi2_ci_state *state, uint8_t group,
                                      const uint8_t *data, uint16_t length) {
  if (length < 13) return;

  uint32_t src_muid = midi2_ci_get_src_muid(data);

  uint8_t reply[24];
  uint16_t reply_len = midi2_ci_build_pe_capability_reply(
      reply, MIDI2_CI_VERSION_1, state->muid, src_muid,
      /*max_simultaneous*/ 1,
      /*pe_ver_major*/     1,
      /*pe_ver_minor*/     0);

  ci_send(state, group, reply, reply_len);
}

/*--------------------------------------------------------------------+
 * PE Get handler -- uses midi2_ci_build_pe_get_reply
 *--------------------------------------------------------------------*/
static void ci_handle_pe_get(midi2_ci_state *state, uint8_t group,
                               const uint8_t *data, uint16_t length) {
  if (length < 14 || state->property_count == 0) return;

  uint32_t src_muid = midi2_ci_get_src_muid(data);
  uint8_t request_id = midi2_ci_get_pe_request_id(data);

  uint8_t i;
  for (i = 0; i < state->property_count; i++) {
    const char *value = NULL;
    if (state->properties[i].getter) {
      value = state->properties[i].getter(state->properties[i].name, state->context);
    } else {
      value = state->properties[i].static_value;
    }

    if (value != NULL) {
      uint16_t val_len = 0;
      while (value[val_len] && val_len < 30) val_len++;

      uint8_t reply[64];
      uint16_t reply_len = midi2_ci_build_pe_get_reply(
          reply, MIDI2_CI_VERSION_1, state->muid, src_muid,
          request_id, NULL, 0, 1, 1,
          (const uint8_t *)value, val_len);

      ci_send(state, group, reply, reply_len);
      return;
    }
  }
}

/*--------------------------------------------------------------------+
 * PE Set handler -- uses midi2_ci_build_pe_set_reply
 *--------------------------------------------------------------------*/
static void ci_handle_pe_set(midi2_ci_state *state, uint8_t group,
                               const uint8_t *data, uint16_t length) {
  if (length < 14 || state->property_count == 0) return;

  uint32_t src_muid = midi2_ci_get_src_muid(data);
  uint8_t request_id = midi2_ci_get_pe_request_id(data);
  uint8_t i;

  for (i = 0; i < state->property_count; i++) {
    if (state->properties[i].setter) {
      bool ok = state->properties[i].setter(state->properties[i].name, "", state->context);

      uint8_t reply[32];
      uint16_t reply_len = midi2_ci_build_pe_set_reply(
          reply, MIDI2_CI_VERSION_1, state->muid, src_muid,
          request_id, NULL, 0);
      (void)ok; /* simplified: always send reply */

      ci_send(state, group, reply, reply_len);
      return;
    }
  }
}

/*--------------------------------------------------------------------+
 * Invalidate MUID handler (M2-101-UM §3.5 + Appendix E)
 *
 * If the message's target MUID matches ours, regenerate it via the
 * installed RNG. Without an RNG the request is silently ignored (v0.2.3
 * behavior preserved).
 *--------------------------------------------------------------------*/
static void ci_handle_invalidate_muid(midi2_ci_state *state, uint8_t group,
                                         const uint8_t *data, uint16_t length) {
  (void)group;
  if (length < 17) return;
  if (!state->rng) return;
  uint32_t target = midi2_ci_get_target_muid(data);
  if (target != state->muid) return;
  midi2_ci_new_muid(state);
}

/*--------------------------------------------------------------------+
 * MUID collision detection (M2-101-UM Appendix E §2)
 *
 * Any inbound CI message whose src_muid matches ours means a peer is
 * using our MUID. Resolution: pick a new MUID and broadcast Invalidate
 * for the old value. No-op without an RNG.
 *--------------------------------------------------------------------*/
static void ci_check_muid_collision(midi2_ci_state *state, uint8_t group,
                                       uint32_t peer_src_muid) {
  if (!state->rng) return;
  if (peer_src_muid != state->muid) return;
  uint32_t old = state->muid;
  midi2_ci_new_muid(state);
  if (!state->write_fn) return;
  uint8_t buf[24];
  uint16_t len = midi2_ci_build_invalidate_muid(
      buf, MIDI2_CI_VERSION_1, state->muid, old);
  ci_send(state, group, buf, len);
}

/*--------------------------------------------------------------------+
 * NAK builder (M2-101-UM Appendix E)
 *
 * Build a Sub-ID#2 0x7F NAK with status NOT_SUPPORTED for a given
 * original sub-id. Used when nak_on_unknown is enabled.
 *--------------------------------------------------------------------*/
static void ci_send_nak_not_supported(midi2_ci_state *state, uint8_t group,
                                         const uint8_t *data,
                                         uint8_t orig_sub_id) {
  if (!state->write_fn) return;
  uint32_t src_muid = midi2_ci_get_src_muid(data);
  uint8_t device_id = midi2_ci_get_device_id(data);
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_nak(
      buf, MIDI2_CI_VERSION_2, state->muid, src_muid, device_id,
      orig_sub_id, MIDI2_CI_NAK_NOT_SUPPORTED, 0,
      NULL, 0, NULL);
  ci_send(state, group, buf, len);
}

/*--------------------------------------------------------------------+
 * Process Inquiry handler -- uses midi2_ci_build_pi_capability_reply
 *--------------------------------------------------------------------*/
static void ci_handle_process_inquiry(midi2_ci_state *state, uint8_t group,
                                        const uint8_t *data, uint16_t length) {
  if (length < 13) return;

  uint32_t src_muid = midi2_ci_get_src_muid(data);

  uint8_t reply[16];
  uint16_t reply_len = midi2_ci_build_pi_capability_reply(
      reply, MIDI2_CI_VERSION_1, state->muid, src_muid, 0x01);

  ci_send(state, group, reply, reply_len);
}

/*--------------------------------------------------------------------+
 * Process incoming SysEx
 *--------------------------------------------------------------------*/
bool midi2_ci_process_sysex(midi2_ci_state *state,
                               uint8_t group, const uint8_t *data, uint16_t length) {
  if (!midi2_ci_is_ci(data, length)) return false;

  /* Every inbound CI message is an opportunity to detect MUID collisions.
   * Do this before dispatching so a reply (if any) carries the new MUID. */
  ci_check_muid_collision(state, group, midi2_ci_get_src_muid(data));

  uint8_t sub_id = midi2_ci_get_sub_id(data);
  switch (sub_id) {
    case MIDI2_CI_DISCOVERY:
      return ci_handle_discovery(state, group, data, length);

    case MIDI2_CI_INVALIDATE_MUID:
      ci_handle_invalidate_muid(state, group, data, length);
      return true;

    case MIDI2_CI_PROFILE_INQUIRY:
      ci_handle_profile_inquiry(state, group, data, length);
      return true;

    case MIDI2_CI_PE_CAPABILITY:
      ci_handle_pe_capability(state, group, data, length);
      return true;

    case MIDI2_CI_PE_GET:
      ci_handle_pe_get(state, group, data, length);
      return true;

    case MIDI2_CI_PE_SET:
      ci_handle_pe_set(state, group, data, length);
      return true;

    case MIDI2_CI_PI_CAPABILITY:
      ci_handle_process_inquiry(state, group, data, length);
      return true;

    default:
      /* Appendix E: "Be able to send a NAK message when appropriate."
       * When nak_on_unknown is set, reply with Sub-ID#2 0x7F
       * status NOT_SUPPORTED. Otherwise the v0.2.3 behavior (silent
       * fall-through to return false) is preserved. */
      if (state->nak_on_unknown) {
        ci_send_nak_not_supported(state, group, data, sub_id);
        return true;
      }
      return false;
  }
}
