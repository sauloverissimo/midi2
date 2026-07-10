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
 */

#include "midi2_ci.h"
#include <string.h>

/* Message Version declared on every reply this convenience Responder emits.
 *
 * The responder implements MIDI-CI 1.2 in full: it advertises Process Inquiry
 * and Property Exchange (both 1.2 categories) and its replies carry the v2
 * message fields (Discovery Reply Output Path Id + Function Block, PE
 * Capability major/minor, NAK details). It therefore declares version 0x02
 * uniformly. Keeping a single constant guarantees two invariants:
 *   1. Advertising a 1.2 category (Process Inquiry 0x10) while claiming
 *      version 0x01 is unrepresentable (the bug the Workbench flagged with
 *      "Process Inquiry not allowed on Message Format Version 0x01 Devices").
 *   2. All replies share one version, so an Initiator never sees
 *      "MIDI-CI Message Format Version has Changed" between messages.
 * ci_cat stays configurable (midi2_ci_set_capabilities); the declared version
 * does not, because the lib always speaks the 1.2 message format. */
#define CI_RESPONDER_VERSION  MIDI2_CI_VERSION_2

/*--------------------------------------------------------------------+
 * Init
 *--------------------------------------------------------------------*/
void midi2_ci_init_ex(midi2_ci_state *state, uint32_t muid_seed,
                       uint8_t (*profiles)[5], uint8_t max_profiles,
                       midi2_ci_property *properties, uint8_t max_properties,
                       midi2_ci_subscriber *subscribers, uint8_t max_subscribers) {
  if (state == NULL) return;
  memset(state, 0, sizeof(midi2_ci_state));
  state->muid = muid_seed & 0x0FFFFFFF;
  state->profiles = profiles;
  state->profile_capacity = max_profiles;
  state->properties = properties;
  state->property_capacity = max_properties;
  state->subscribers = subscribers;
  state->subscriber_capacity = max_subscribers;
  /* Clear caller-provided subscriber slots so the `in_use` sentinel starts
   * zero. That field is a midi2 implementation detail, not part of the
   * caller contract, so init owns it. */
  if (subscribers != NULL && max_subscribers > 0) {
    memset(subscribers, 0, sizeof(midi2_ci_subscriber) * max_subscribers);
  }
  state->auto_invalidate_on_collision = true; /* v0.3.0+ default on */
  /* Advertise every CI Category the convenience responder actually handles:
   * Profile Config, Property Exchange, Process Inquiry (0x1C). (v0.6.1+) */
  state->ci_cat = MIDI2_CI_CAT_PROFILE_CONFIG
                | MIDI2_CI_CAT_PROPERTY_EXCHANGE
                | MIDI2_CI_CAT_PROCESS_INQUIRY;
}

void midi2_ci_init(midi2_ci_state *state, uint32_t muid_seed,
                     uint8_t (*profiles)[5], uint8_t max_profiles,
                     midi2_ci_property *properties, uint8_t max_properties) {
  midi2_ci_init_ex(state, muid_seed,
                    profiles, max_profiles,
                    properties, max_properties,
                    NULL, 0);
}

void midi2_ci_set_identity(midi2_ci_state *state,
                             uint32_t manufacturer_id, uint16_t family_id,
                             uint16_t model_id, uint32_t version_id) {
  if (state == NULL) return;
  state->manufacturer_id = manufacturer_id;
  state->family_id = family_id;
  state->model_id = model_id;
  state->version_id = version_id;
}

void midi2_ci_set_capabilities(midi2_ci_state *state, uint8_t ci_cat) {
  if (state == NULL) return;
  state->ci_cat = ci_cat;
}

void midi2_ci_set_write_fn(midi2_ci_state *state,
                              midi2_proc_write_fn write_fn, void *context) {
  if (state == NULL) return;
  state->write_fn = write_fn;
  state->write_context = context;
}

void midi2_ci_set_rng(midi2_ci_state *state,
                         midi2_ci_rng_fn rng, void *context) {
  if (state == NULL) return;
  state->rng         = rng;
  state->rng_context = context;
}

void midi2_ci_set_nak_on_unknown(midi2_ci_state *state, bool enabled) {
  if (state == NULL) return;
  state->nak_on_unknown = enabled;
}

void midi2_ci_set_auto_invalidate_on_collision(midi2_ci_state *state, bool enabled) {
  if (state == NULL) return;
  state->auto_invalidate_on_collision = enabled;
}

uint32_t midi2_ci_new_muid(midi2_ci_state *state) {
  uint32_t m;
  uint8_t tries = 0;
  if (state == NULL) return 0u;
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
  if (state == NULL) return MIDI2_CI_ERR_NULL;
  if (state->profiles == NULL) return MIDI2_CI_ERR_NULL;
  if (state->profile_count >= state->profile_capacity) return MIDI2_CI_ERR_FULL;
  memcpy(state->profiles[state->profile_count], profile_id, 5);
  state->profile_count++;
  return MIDI2_CI_OK;
}

int midi2_ci_remove_profile(midi2_ci_state *state, const uint8_t profile_id[5]) {
  uint8_t i;
  if (state == NULL) return MIDI2_CI_ERR_NULL;
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
  if (state == NULL) return MIDI2_CI_ERR_NULL;
  if (state->properties == NULL) return MIDI2_CI_ERR_NULL;
  if (state->property_count >= state->property_capacity) return MIDI2_CI_ERR_FULL;
  state->properties[state->property_count].name = name;
  state->properties[state->property_count].static_value = value;
  state->properties[state->property_count].getter = NULL;
  state->properties[state->property_count].setter = NULL;
  state->properties[state->property_count].subscribable = false; /* v0.3.0+ */
  state->property_count++;
  return MIDI2_CI_OK;
}

int midi2_ci_add_property_dynamic(midi2_ci_state *state,
                                     const char *name,
                                     midi2_ci_pe_getter getter,
                                     midi2_ci_pe_setter setter) {
  if (state == NULL) return MIDI2_CI_ERR_NULL;
  if (state->properties == NULL) return MIDI2_CI_ERR_NULL;
  if (state->property_count >= state->property_capacity) return MIDI2_CI_ERR_FULL;
  state->properties[state->property_count].name = name;
  state->properties[state->property_count].static_value = NULL;
  state->properties[state->property_count].getter = getter;
  state->properties[state->property_count].setter = setter;
  state->properties[state->property_count].subscribable = false; /* v0.3.0+ */
  state->property_count++;
  return MIDI2_CI_OK;
}

int midi2_ci_remove_property(midi2_ci_state *state, const char *name) {
  uint8_t i;
  if (state == NULL) return MIDI2_CI_ERR_NULL;
  if (name == NULL) return MIDI2_CI_ERR_NULL;
  for (i = 0; i < state->property_count; i++) {
    if (state->properties[i].name != NULL
        && strcmp(state->properties[i].name, name) == 0) {
      uint8_t j;
      for (j = i; j + 1 < state->property_count; j++) {
        state->properties[j] = state->properties[j + 1];
      }
      state->property_count--;
      return MIDI2_CI_OK;
    }
  }
  return MIDI2_CI_ERR_NOT_FOUND;
}

void midi2_ci_reset_profiles(midi2_ci_state *state) {
  if (state == NULL) return;
  state->profile_count = 0;
}

void midi2_ci_reset_properties(midi2_ci_state *state) {
  if (state == NULL) return;
  state->property_count = 0;
}

/*--------------------------------------------------------------------+
 * Subscribe / Notify (v0.3.0)
 *
 * Registry is caller-provided (state->subscribers). Each slot carries
 * a stable 36-char copy of the resource name so the responder does
 * not depend on app-owned string lifetimes.
 *--------------------------------------------------------------------*/
static int find_property_idx(const midi2_ci_state *state, const char *name) {
  uint8_t i;
  if (state == NULL || name == NULL) return -1;
  for (i = 0; i < state->property_count; i++) {
    if (state->properties[i].name != NULL
        && strcmp(state->properties[i].name, name) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static int find_subscriber_idx(const midi2_ci_state *state, uint32_t muid,
                                const char *name) {
  uint8_t i;
  if (state == NULL || state->subscribers == NULL || name == NULL) return -1;
  for (i = 0; i < state->subscriber_capacity; i++) {
    if (!state->subscribers[i].in_use) continue;
    if (state->subscribers[i].caller_muid != muid) continue;
    if (strncmp(state->subscribers[i].name_copy, name, 36) != 0) continue;
    return (int)i;
  }
  return -1;
}

int midi2_ci_pe_set_subscribable(midi2_ci_state *state, const char *name,
                                  bool subscribable) {
  int idx;
  if (state == NULL || name == NULL) return MIDI2_CI_ERR_NULL;
  idx = find_property_idx(state, name);
  if (idx < 0) return MIDI2_CI_ERR_NOT_FOUND;
  state->properties[idx].subscribable = subscribable;
  return MIDI2_CI_OK;
}

int midi2_ci_subscribe_add(midi2_ci_state *state, uint32_t caller_muid,
                            const char *resource_name) {
  uint8_t i;
  int pi;
  size_t n;
  if (state == NULL || resource_name == NULL) return MIDI2_CI_ERR_NULL;
  if (state->subscribers == NULL) return MIDI2_CI_ERR_FULL;
  pi = find_property_idx(state, resource_name);
  if (pi < 0) return MIDI2_CI_ERR_NOT_FOUND;
  if (!state->properties[pi].subscribable) return MIDI2_CI_ERR_NOT_FOUND;
  if (find_subscriber_idx(state, caller_muid, resource_name) >= 0) {
    return MIDI2_CI_OK; /* idempotent duplicate */
  }
  for (i = 0; i < state->subscriber_capacity; i++) {
    if (state->subscribers[i].in_use) continue;
    state->subscribers[i].caller_muid = caller_muid;
    n = strlen(resource_name);
    if (n > 36) n = 36;
    memcpy(state->subscribers[i].name_copy, resource_name, n);
    state->subscribers[i].name_copy[n] = '\0';
    state->subscribers[i].in_use = 1;
    state->subscriber_count++;
    return MIDI2_CI_OK;
  }
  return MIDI2_CI_ERR_FULL;
}

int midi2_ci_subscribe_remove(midi2_ci_state *state, uint32_t caller_muid,
                               const char *resource_name) {
  int idx;
  if (state == NULL || resource_name == NULL) return MIDI2_CI_ERR_NULL;
  idx = find_subscriber_idx(state, caller_muid, resource_name);
  if (idx < 0) return MIDI2_CI_ERR_NOT_FOUND;
  state->subscribers[idx].in_use = 0;
  state->subscribers[idx].caller_muid = 0;
  state->subscribers[idx].name_copy[0] = '\0';
  state->subscriber_count--;
  return MIDI2_CI_OK;
}

uint8_t midi2_ci_get_subscriber_count(const midi2_ci_state *state) {
  return (state == NULL) ? 0u : state->subscriber_count;
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
 * PE Notify fan-out (v0.3.0)
 *
 * Walks the subscriber registry and, for every slot matching the
 * resource name, emits a PE Notify frame carrying a minimal JSON
 * header `{"resource":"<name>"}`. The actual property value is not
 * embedded; consumers issue a PE Get to fetch the new value. Matches
 * the common M2-103 pattern for PE where Notify signals invalidation.
 *--------------------------------------------------------------------*/
int midi2_ci_notify_property_changed(midi2_ci_state *state,
                                      const char *resource_name) {
  /* JSON header template `{"resource":"<name>"}`. <name> is bounded by
   * MIDI2_CI_RESOURCE_NAME_MAX (36 per M2-105) and stored in the
   * subscriber's name_copy slot, so the worst-case header is
   * 13 (prefix) + 36 (name) + 2 (suffix) = 51 bytes. Buffer of 64 is
   * comfortable. No <stdio.h> dependency. */
  static const char HDR_PREFIX[] = "{\"resource\":\"";
  static const char HDR_SUFFIX[] = "\"}";
  uint8_t i;
  int pi;
  if (state == NULL || resource_name == NULL) return MIDI2_CI_ERR_NULL;
  pi = find_property_idx(state, resource_name);
  if (pi < 0) return MIDI2_CI_ERR_NOT_FOUND;
  if (state->write_fn == NULL || state->subscribers == NULL) return MIDI2_CI_OK;

  for (i = 0; i < state->subscriber_capacity; i++) {
    if (!state->subscribers[i].in_use) continue;
    if (strncmp(state->subscribers[i].name_copy, resource_name, 36) != 0) continue;

    uint8_t frame[128];
    uint8_t hdr[64];
    uint16_t hdr_n = 0;
    size_t name_len = strlen(state->subscribers[i].name_copy);
    if (name_len > 36u) name_len = 36u;

    memcpy(hdr + hdr_n, HDR_PREFIX, sizeof HDR_PREFIX - 1u);
    hdr_n = (uint16_t)(hdr_n + (sizeof HDR_PREFIX - 1u));
    memcpy(hdr + hdr_n, state->subscribers[i].name_copy, name_len);
    hdr_n = (uint16_t)(hdr_n + name_len);
    memcpy(hdr + hdr_n, HDR_SUFFIX, sizeof HDR_SUFFIX - 1u);
    hdr_n = (uint16_t)(hdr_n + (sizeof HDR_SUFFIX - 1u));

    uint16_t frame_n = midi2_ci_build_pe_notify(
        frame, CI_RESPONDER_VERSION,
        state->muid,
        state->subscribers[i].caller_muid,
        0 /* request_id */,
        hdr, hdr_n,
        1, 1,
        NULL, 0);
    if (frame_n == 0) continue;
    ci_send(state, 0 /* group */, frame, frame_n);
  }
  return MIDI2_CI_OK;
}

/*--------------------------------------------------------------------+
 * Discovery Reply -- uses midi2_ci_build_discovery_reply
 *--------------------------------------------------------------------*/
static bool ci_handle_discovery(midi2_ci_state *state, uint8_t group,
                                  const uint8_t *data, uint16_t length) {
  if (length < 13 || state->manufacturer_id == 0) return false;

  uint32_t src_muid = midi2_ci_get_src_muid(data);

  uint8_t reply[32];
  uint16_t reply_len = midi2_ci_build_discovery_reply(
      reply, CI_RESPONDER_VERSION, state->muid, src_muid,
      state->manufacturer_id, state->family_id, state->model_id,
      state->version_id, state->ci_cat, 512, 0, 0x7F);

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
      reply, CI_RESPONDER_VERSION, state->muid, src_muid,
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
      reply, CI_RESPONDER_VERSION, state->muid, src_muid,
      /*max_simultaneous*/ 1,
      /*pe_ver_major*/     1,
      /*pe_ver_minor*/     0);

  ci_send(state, group, reply, reply_len);
}

/*--------------------------------------------------------------------+
 * Property Exchange reply helpers (M2-105-UM)
 *
 * Every PE reply header is a JSON object carrying at least a "status".
 * An empty header makes an Initiator (e.g. the MIDI 2.0 Workbench) NAK with
 * "the first header property is not resource, status or command".
 *--------------------------------------------------------------------*/
static const char PE_HDR_OK[]  = "{\"status\":200}";
static const char PE_HDR_400[] = "{\"status\":400}";
static const char PE_HDR_404[] = "{\"status\":404}";

/* Assembled PE reply and body bounds, sized to the 512-byte Receivable Max
 * SysEx the responder advertises so a full DeviceInfo (manufacturer/family/
 * model/version plus the four ID arrays, ~200 bytes) round-trips without
 * truncation. REPLY_MAX must exceed BODY_MAX by the ~36-byte CI+PE framing. */
#define CI_PE_REPLY_MAX 512
#define CI_PE_BODY_MAX  448

/* Extract the "resource" string value from a PE inquiry header JSON. Returns a
 * pointer into hdr with *out_len set, or NULL if absent. Minimal scanner: no
 * escape handling (PE resource names are plain identifiers per M2-105). */
static const char *ci_pe_resource(const uint8_t *hdr, uint16_t hdr_len,
                                     uint16_t *out_len) {
  static const char KEY[] = "\"resource\"";
  const uint16_t klen = (uint16_t)(sizeof(KEY) - 1);
  uint16_t i;
  *out_len = 0;
  if (hdr == NULL || hdr_len < klen) return NULL;
  for (i = 0; (uint16_t)(i + klen) <= hdr_len; i++) {
    if (memcmp(hdr + i, KEY, klen) != 0) continue;
    i = (uint16_t)(i + klen);
    while (i < hdr_len && (hdr[i] == ' ' || hdr[i] == ':')) i++;
    if (i >= hdr_len || hdr[i] != '"') return NULL;
    i++;  /* opening quote */
    {
      uint16_t start = i;
      while (i < hdr_len && hdr[i] != '"') i++;
      *out_len = (uint16_t)(i - start);
      return (const char *)(hdr + start);
    }
  }
  return NULL;
}

/* Find a registered property whose name matches the requested resource. */
static const midi2_ci_property *ci_pe_find_property(const midi2_ci_state *state,
                                                       const char *res,
                                                       uint16_t res_len) {
  uint8_t i;
  for (i = 0; i < state->property_count; i++) {
    const char *name = state->properties[i].name;
    if (name != NULL && (uint16_t)strlen(name) == res_len
        && memcmp(name, res, res_len) == 0) {
      return &state->properties[i];
    }
  }
  return NULL;
}

/* Build the built-in ResourceList body: a JSON array of {"resource":"NAME"}
 * for every registered property. Emits only entries that fit whole, always
 * closing the array, so the result is valid JSON even when it overflows (a
 * truncated but well-formed list rather than an empty or half-written body).
 * Returns bytes written (>= 2, i.e. at least "[]"), or 0 if max < 2. */
static uint16_t ci_pe_build_resource_list(const midi2_ci_state *state,
                                            uint8_t *out, uint16_t max) {
  static const char PRE[] = "{\"resource\":\"";
  const uint16_t prelen = (uint16_t)(sizeof(PRE) - 1);
  uint16_t p = 0;
  uint8_t emitted = 0;
  uint8_t i;
  if (max < 2) return 0;  /* no room even for "[]" */
  out[p++] = '[';
  for (i = 0; i < state->property_count; i++) {
    const char *name = state->properties[i].name;
    uint16_t namelen, need, k;
    if (name == NULL) continue;
    namelen = (uint16_t)strlen(name);
    /* comma? + {"resource":" + name + "} , plus 1 reserved for closing ']'. */
    need = (uint16_t)((emitted > 0 ? 1u : 0u) + prelen + namelen + 2u + 1u);
    if ((uint16_t)(p + need) > max) break;  /* stop at the last whole entry */
    if (emitted > 0) out[p++] = ',';
    for (k = 0; k < prelen; k++)   out[p++] = (uint8_t)PRE[k];
    for (k = 0; k < namelen; k++)  out[p++] = (uint8_t)name[k];
    out[p++] = '"'; out[p++] = '}';
    emitted++;
  }
  out[p++] = ']';
  return p;
}

/* If body is a JSON array, count its top-level elements; returns 1 and sets
 * *count. Returns 0 for a non-array body (e.g. the DeviceInfo object). String-
 * and nesting-aware, so commas inside strings or nested objects/arrays are not
 * counted. An empty array "[]" yields count 0. */
static int ci_pe_array_count(const uint8_t *body, uint16_t len,
                               uint16_t *count) {
  uint16_t i = 0, n;
  int depth = 0, in_str = 0;
  *count = 0;
  while (i < len && (body[i] == ' ' || body[i] == '\t'
                     || body[i] == '\r' || body[i] == '\n')) i++;
  if (i >= len || body[i] != '[') return 0;
  for (i++; i < len && (body[i] == ' ' || body[i] == '\t'
                        || body[i] == '\r' || body[i] == '\n'); i++) {}
  if (i < len && body[i] == ']') return 1;  /* empty array -> 0 */
  n = 1;                                     /* at least one element */
  for (; i < len; i++) {
    uint8_t c = body[i];
    if (in_str) {
      if (c == '\\') { i++; continue; }      /* skip escaped char */
      if (c == '"') in_str = 0;
      continue;
    }
    if (c == '"') in_str = 1;
    else if (c == '{' || c == '[') depth++;
    else if (c == '}') depth--;
    else if (c == ']') { if (depth == 0) break; depth--; }
    else if (c == ',' && depth == 0) n++;
  }
  *count = n;
  return 1;
}

/* Build the success header: {"status":200} or, for a list resource,
 * {"status":200,"totalCount":N}. totalCount is required by M2-105 for
 * paginable resources (the Workbench warns without it). Returns bytes written;
 * buf must hold at least CI_PE_OK_HDR_MAX bytes. */
#define CI_PE_OK_HDR_MAX 40
static uint16_t ci_pe_ok_header(uint8_t *buf, int has_count, uint16_t count) {
  static const char HEAD[]  = "{\"status\":200";
  static const char TOTAL[] = ",\"totalCount\":";
  uint16_t p = 0, k;
  for (k = 0; k < (uint16_t)(sizeof(HEAD) - 1); k++) buf[p++] = (uint8_t)HEAD[k];
  if (has_count) {
    uint8_t tmp[5];
    uint8_t t = 0;
    for (k = 0; k < (uint16_t)(sizeof(TOTAL) - 1); k++) buf[p++] = (uint8_t)TOTAL[k];
    if (count == 0) { buf[p++] = '0'; }
    else {
      while (count > 0) { tmp[t++] = (uint8_t)('0' + (count % 10)); count /= 10; }
      while (t > 0)     { buf[p++] = tmp[--t]; }
    }
  }
  buf[p++] = '}';
  return p;
}

/*--------------------------------------------------------------------+
 * PE Get handler -- uses midi2_ci_build_pe_get_reply
 *
 * Matches the requested resource by name. Built-in "ResourceList" enumerates
 * the registered resources. Unknown resource -> {"status":404}. Every reply
 * carries a non-empty header; list resources also carry "totalCount".
 *--------------------------------------------------------------------*/
static void ci_handle_pe_get(midi2_ci_state *state, uint8_t group,
                               const uint8_t *data, uint16_t length) {
  /* Respond even with zero registered properties: a ResourceList Get still
   * gets an empty array, a named Get gets 404. The responder advertises PE,
   * so it must always answer. */
  if (length < 16) return;

  uint32_t src_muid   = midi2_ci_get_src_muid(data);
  uint8_t  request_id = midi2_ci_get_pe_request_id(data);
  uint16_t hdr_len    = midi2_ci_get_pe_header_len(data);
  const uint8_t *inq  = (length >= (uint16_t)(16 + hdr_len)) ? (data + 16) : NULL;

  uint16_t res_len = 0;
  const char *res = ci_pe_resource(inq, hdr_len, &res_len);

  uint8_t reply[CI_PE_REPLY_MAX];
  uint16_t reply_len;

  /* Built-in ResourceList: enumerate the registered resources. An
   * app-registered "ResourceList" property takes precedence (served by the
   * named lookup below), so devices can publish entries that carry a schema
   * for custom X- resources, as M2-105 requires. */
  if (res != NULL && res_len == 12 && memcmp(res, "ResourceList", 12) == 0
      && !ci_pe_find_property(state, res, res_len)) {
    uint8_t body[CI_PE_BODY_MAX];
    uint16_t body_len = ci_pe_build_resource_list(state, body, sizeof(body));
    uint8_t hdr[CI_PE_OK_HDR_MAX];
    uint16_t total = 0;
    (void)ci_pe_array_count(body, body_len, &total);
    reply_len = midi2_ci_build_pe_get_reply(
        reply, CI_RESPONDER_VERSION, state->muid, src_muid, request_id,
        hdr, ci_pe_ok_header(hdr, 1, total),
        1, 1, body, body_len);
    ci_send(state, group, reply, reply_len);
    return;
  }

  /* Named resource lookup. */
  if (res != NULL) {
    uint8_t i;
    for (i = 0; i < state->property_count; i++) {
      const char *name = state->properties[i].name;
      const char *value;
      uint16_t val_len;
      if (name == NULL || (uint16_t)strlen(name) != res_len
          || memcmp(name, res, res_len) != 0) continue;

      value = state->properties[i].getter
          ? state->properties[i].getter(name, state->context)
          : state->properties[i].static_value;
      if (value == NULL) break;  /* resource exists but has no value -> 404 */

      uint8_t hdr[CI_PE_OK_HDR_MAX];
      uint16_t total = 0;
      int is_list;
      val_len = (uint16_t)strlen(value);
      if (val_len > CI_PE_BODY_MAX) val_len = CI_PE_BODY_MAX;

      /* List resources (array body) carry totalCount; objects (DeviceInfo) do
       * not. Count on the possibly-truncated body so it matches what is sent. */
      is_list = ci_pe_array_count((const uint8_t *)value, val_len, &total);
      reply_len = midi2_ci_build_pe_get_reply(
          reply, CI_RESPONDER_VERSION, state->muid, src_muid, request_id,
          hdr, ci_pe_ok_header(hdr, is_list, total),
          1, 1, (const uint8_t *)value, val_len);
      ci_send(state, group, reply, reply_len);
      return;
    }
  }

  /* Unknown or unspecified resource -> status 404. */
  reply_len = midi2_ci_build_pe_get_reply(
      reply, CI_RESPONDER_VERSION, state->muid, src_muid, request_id,
      (const uint8_t *)PE_HDR_404, (uint16_t)(sizeof(PE_HDR_404) - 1),
      1, 1, NULL, 0);
  ci_send(state, group, reply, reply_len);
}

/*--------------------------------------------------------------------+
 * PE Set handler -- uses midi2_ci_build_pe_set_reply
 *
 * Matches the requested resource by name and passes the request body to that
 * resource's setter. Status reflects reality: 200 on success, 404 for an
 * unknown or non-settable resource, 400 when the setter rejects the value.
 *--------------------------------------------------------------------*/
static void ci_handle_pe_set(midi2_ci_state *state, uint8_t group,
                               const uint8_t *data, uint16_t length) {
  if (length < 16) return;

  uint32_t src_muid   = midi2_ci_get_src_muid(data);
  uint8_t  request_id = midi2_ci_get_pe_request_id(data);
  uint16_t hdr_len    = midi2_ci_get_pe_header_len(data);
  const uint8_t *inq  = (length >= (uint16_t)(16 + hdr_len)) ? (data + 16) : NULL;

  uint16_t res_len = 0;
  const char *res = ci_pe_resource(inq, hdr_len, &res_len);

  const char *status  = PE_HDR_404;  /* default: no matching settable resource */
  uint16_t    status_len = (uint16_t)(sizeof(PE_HDR_404) - 1);

  if (res != NULL) {
    uint8_t i;
    for (i = 0; i < state->property_count; i++) {
      const char *name = state->properties[i].name;
      if (name == NULL || (uint16_t)strlen(name) != res_len
          || memcmp(name, res, res_len) != 0) continue;

      if (state->properties[i].setter == NULL) break;  /* exists, read-only -> 404 */

      /* Extract the (single-chunk) request body and pass it to the setter as a
       * NUL-terminated string. Body follows header + num_chunks + this_chunk +
       * body_len (M2-105 PE data layout). */
      {
        uint16_t off = (uint16_t)(16 + hdr_len);
        char valbuf[CI_PE_BODY_MAX + 1];
        uint16_t body_len = 0;
        const uint8_t *body = NULL;
        bool ok;

        if (length >= (uint16_t)(off + 6)) {
          body_len = midi2_ci_read_14(&data[off + 4]);
          body = &data[off + 6];
          if (length < (uint16_t)(off + 6 + body_len)) body_len = 0;
        }
        if (body_len > CI_PE_BODY_MAX) body_len = CI_PE_BODY_MAX;
        if (body_len > 0) memcpy(valbuf, body, body_len);
        valbuf[body_len] = '\0';

        ok = state->properties[i].setter(name, valbuf, state->context);
        if (ok) { status = PE_HDR_OK;  status_len = (uint16_t)(sizeof(PE_HDR_OK)  - 1); }
        else    { status = PE_HDR_400; status_len = (uint16_t)(sizeof(PE_HDR_400) - 1); }
      }
      break;
    }
  }

  {
    uint8_t reply[64];
    uint16_t reply_len = midi2_ci_build_pe_set_reply(
        reply, CI_RESPONDER_VERSION, state->muid, src_muid,
        request_id, (const uint8_t *)status, status_len);
    ci_send(state, group, reply, reply_len);
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
  if (state->rng == NULL) return;
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
  if (state->rng == NULL) return;
  if (peer_src_muid != state->muid) return;
  uint32_t old = state->muid;
  midi2_ci_new_muid(state);
  if (state->write_fn == NULL) return;
  if (!state->auto_invalidate_on_collision) return;
  uint8_t buf[24];
  uint16_t len = midi2_ci_build_invalidate_muid(
      buf, CI_RESPONDER_VERSION, state->muid, old);
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
  if (state->write_fn == NULL) return;
  uint32_t src_muid = midi2_ci_get_src_muid(data);
  uint8_t device_id = midi2_ci_get_device_id(data);
  uint8_t buf[32];
  uint16_t len = midi2_ci_build_nak(
      buf, CI_RESPONDER_VERSION, state->muid, src_muid, device_id,
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
      reply, CI_RESPONDER_VERSION, state->muid, src_muid, 0x01);

  ci_send(state, group, reply, reply_len);
}

/*--------------------------------------------------------------------+
 * Process incoming SysEx
 *--------------------------------------------------------------------*/
bool midi2_ci_process_sysex(midi2_ci_state *state,
                               uint8_t group, const uint8_t *data, uint16_t length) {
  if (state == NULL) return false;
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
