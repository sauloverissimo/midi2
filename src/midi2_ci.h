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
 * midi2_ci.h - MIDI-CI convenience responder
 *
 * Part of midi2 - Portable MIDI 2.0 library (C99)
 * https://github.com/sauloverissimo/midi2
 *
 * Spec: MIDI-CI (M2-101-UM v1.2, Jun 2023)
 * Version: 0.3.0
 */

#ifndef MIDI2_CI_H
#define MIDI2_CI_H

#include <stdint.h>
#include <stdbool.h>
#include "midi2_proc.h"
#include "midi2_ci_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------+
 * Error codes
 *--------------------------------------------------------------------*/
enum {
  MIDI2_CI_OK          =  0,
  MIDI2_CI_ERR_FULL    = -1,   /**< Storage is full (profiles or properties) */
  MIDI2_CI_ERR_NOT_FOUND = -2, /**< Item not found */
  MIDI2_CI_ERR_NULL    = -3,   /**< NULL pointer argument */
};

/* Sub-IDs and constants are now in midi2_ci_msg.h.
 * Legacy alias for backward compatibility: */
#define MIDI2_CI_DISCOVERY_REQUEST  MIDI2_CI_DISCOVERY

/*--------------------------------------------------------------------+
 * Callback types
 *--------------------------------------------------------------------*/

/* Property Exchange getter: returns value string for given property name */
typedef const char* (*midi2_ci_pe_getter)(const char *name, void *context);

/* Property Exchange setter: sets value, returns true on success */
typedef bool (*midi2_ci_pe_setter)(const char *name, const char *value, void *context);

/*--------------------------------------------------------------------+
 * Property descriptor (used in caller-provided array)
 *--------------------------------------------------------------------*/
typedef struct {
  const char         *name;
  const char         *static_value;  /**< used if getter is NULL */
  midi2_ci_pe_getter  getter;
  midi2_ci_pe_setter  setter;
  bool                subscribable;  /**< v0.3.0+: eligible for PE Subscribe */
} midi2_ci_property;

/*--------------------------------------------------------------------+
 * Subscriber registry entry (caller-provided array, v0.3.0+)
 *
 * name_copy holds up to 36 chars per M2-105 PE resource name limit
 * plus NUL terminator, so the responder keeps a stable reference even
 * if the app frees the resource name string passed to subscribe_add.
 *--------------------------------------------------------------------*/
typedef struct {
  uint32_t caller_muid;
  char     name_copy[37];
  uint8_t  in_use;  /**< 0 = free slot; non-zero = active */
} midi2_ci_subscriber;

/*--------------------------------------------------------------------+
 * State struct (user-allocated)
 *
 * The caller provides storage for profiles and properties by pointing
 * to pre-allocated arrays. This allows any capacity without compile-time
 * limits. Example:
 *
 *   uint8_t my_profiles[4][5];
 *   midi2_ci_property my_props[2];
 *   midi2_ci_state ci;
 *   midi2_ci_init(&ci, seed, my_profiles, 4, my_props, 2);
 *--------------------------------------------------------------------*/
/*--------------------------------------------------------------------+
 * RNG callback (v0.2.4+)
 *
 * Platform-specific randomness source. When set via midi2_ci_set_rng(),
 * the convenience responder automatically handles MUID regeneration on
 * Invalidate MUID and peer MUID collisions. Only the lower 28 bits of
 * the returned value are used. Reserved values 0x00000000 and
 * 0x0FFFFFFF (broadcast) are automatically avoided by midi2_ci_new_muid.
 *--------------------------------------------------------------------*/
typedef uint32_t (*midi2_ci_rng_fn)(void *context);

typedef struct {
  /* Device identity (configured by user) */
  uint32_t manufacturer_id;   /**< 3-byte SysEx Manufacturer ID in lower 24 bits */
  uint16_t family_id;
  uint16_t model_id;
  uint32_t version_id;

  /* MUID (set at init) */
  uint32_t muid;

  /* Profiles: caller-provided storage */
  uint8_t (*profiles)[5];     /**< pointer to caller's profile array (each 5 bytes) */
  uint8_t  profile_capacity;  /**< max profiles (size of caller's array) */
  uint8_t  profile_count;     /**< current count */

  /* Properties: caller-provided storage */
  midi2_ci_property *properties;  /**< pointer to caller's property array */
  uint8_t  property_capacity;
  uint8_t  property_count;

  /* Write function (how to send SysEx responses back) */
  midi2_proc_write_fn write_fn;
  void               *write_context;

  /* User context for PE callbacks */
  void               *context;

  /* RNG for MUID regeneration (v0.2.4+). NULL = no auto-regen. */
  midi2_ci_rng_fn    rng;
  void              *rng_context;

  /* When true, process_sysex replies with a NAK (Sub-ID#2 0x7F) for any
   * CI sub-id not handled by the convenience responder (v0.2.4+).
   * Default false to preserve v0.2.3 behavior. */
  bool               nak_on_unknown;

  /* When true, the convenience responder broadcasts an Invalidate MUID
   * frame for the old MUID whenever it regenerates because an inbound
   * src_muid collided with ours. M2-101-UM Appendix E 2. Default: true
   * (v0.3.0+). Implementation was already present in v0.2.4 but always
   * on; this flag gates it. */
  bool               auto_invalidate_on_collision;

  /* Subscribers: caller-provided storage (v0.3.0+). NULL when the state
   * was built with the legacy midi2_ci_init (no subscribe/notify). */
  midi2_ci_subscriber *subscribers;
  uint8_t              subscriber_capacity;
  uint8_t              subscriber_count;
} midi2_ci_state;

/*--------------------------------------------------------------------+
 * Functions
 *--------------------------------------------------------------------*/

/** Initialize state with caller-provided storage.
 *  Delegates to midi2_ci_init_ex(..., NULL, 0), so the subscriber
 *  registry is absent and subscribe/notify APIs return ERR_FULL.
 *  @param state         State struct (caller-allocated). Safe to pass NULL
 *                       (function is no-op).
 *  @param muid_seed     Random or unique value for MUID generation (28-bit)
 *  @param profiles      Caller's profile array, or NULL if no profiles needed
 *  @param max_profiles  Capacity of profiles array
 *  @param properties    Caller's property array, or NULL if no properties needed
 *  @param max_properties Capacity of properties array */
void midi2_ci_init(midi2_ci_state *state, uint32_t muid_seed,
                     uint8_t (*profiles)[5], uint8_t max_profiles,
                     midi2_ci_property *properties, uint8_t max_properties);

/** Extended initializer that also wires a subscriber-registry array
 *  for PE Subscribe / Notify. Pass NULL / 0 for the subscribers
 *  argument to match midi2_ci_init semantics. Safe to call with
 *  NULL state (function is no-op).
 *  (v0.3.0+) */
void midi2_ci_init_ex(midi2_ci_state *state, uint32_t muid_seed,
                       uint8_t (*profiles)[5], uint8_t max_profiles,
                       midi2_ci_property *properties, uint8_t max_properties,
                       midi2_ci_subscriber *subscribers, uint8_t max_subscribers);

/** Configure device identity. Safe to call with NULL state (no-op). */
void midi2_ci_set_identity(midi2_ci_state *state,
                             uint32_t manufacturer_id, uint16_t family_id,
                             uint16_t model_id, uint32_t version_id);

/** Set the write function (how CI sends SysEx responses).
 *  Safe to call with NULL state (no-op). */
void midi2_ci_set_write_fn(midi2_ci_state *state,
                              midi2_proc_write_fn write_fn, void *context);

/** Install a platform RNG so the convenience responder can regenerate the
 *  MUID on Invalidate MUID messages and on peer MUID collisions. Without
 *  this, both situations are silently ignored (v0.2.3 behavior).
 *  The callback is invoked from within process_sysex; it must be re-entrant
 *  and should return quickly. Only the lower 28 bits matter.
 *  Safe to call with NULL state (no-op). (v0.2.4+) */
void midi2_ci_set_rng(midi2_ci_state *state,
                         midi2_ci_rng_fn rng, void *context);

/** Enable/disable automatic NAK (Sub-ID#2 0x7F, status 0x01 NOT_SUPPORTED)
 *  replies for CI sub-ids the convenience responder does not handle.
 *  M2-101-UM Appendix E requires a device to "Be able to send a NAK message
 *  when appropriate". Default: false (v0.2.3 compatible).
 *  Safe to call with NULL state (no-op). (v0.2.4+) */
void midi2_ci_set_nak_on_unknown(midi2_ci_state *state, bool enabled);

/** Enable/disable automatic broadcast of an Invalidate MUID frame for the
 *  old MUID whenever the convenience responder regenerates due to an
 *  inbound collision. Default: true.
 *  Safe to call with NULL state (no-op). (v0.3.0+) */
void midi2_ci_set_auto_invalidate_on_collision(midi2_ci_state *state, bool enabled);

/** Generate a fresh 28-bit MUID using the configured RNG, avoiding the
 *  reserved values 0x00000000 and 0x0FFFFFFF (broadcast). If no RNG is
 *  set, falls back to perturbing the current MUID. Returns the new MUID
 *  and also stores it into state->muid. Returns 0u (reserved sentinel)
 *  if state is NULL. (v0.2.4+) */
uint32_t midi2_ci_new_muid(midi2_ci_state *state);

/** Add a profile. Returns MIDI2_CI_OK, MIDI2_CI_ERR_FULL, or
 *  MIDI2_CI_ERR_NULL (state is NULL). */
int midi2_ci_add_profile(midi2_ci_state *state, const uint8_t profile_id[5]);

/** Remove a profile. Returns MIDI2_CI_OK, MIDI2_CI_ERR_NOT_FOUND, or
 *  MIDI2_CI_ERR_NULL (state is NULL). */
int midi2_ci_remove_profile(midi2_ci_state *state, const uint8_t profile_id[5]);

/** Add a static property. Returns MIDI2_CI_OK, MIDI2_CI_ERR_FULL, or
 *  MIDI2_CI_ERR_NULL (state is NULL). */
int midi2_ci_add_property_static(midi2_ci_state *state,
                                    const char *name, const char *value);

/** Add a dynamic property with getter/setter. Returns MIDI2_CI_OK,
 *  MIDI2_CI_ERR_FULL, or MIDI2_CI_ERR_NULL (state is NULL). */
int midi2_ci_add_property_dynamic(midi2_ci_state *state,
                                     const char *name,
                                     midi2_ci_pe_getter getter,
                                     midi2_ci_pe_setter setter);

/** Remove a property by name. Remaining properties are shifted left to
 *  preserve contiguous storage. Returns MIDI2_CI_OK or
 *  MIDI2_CI_ERR_NOT_FOUND. Symmetric with midi2_ci_remove_profile.
 *  Safe to call with NULL state or NULL name (returns
 *  MIDI2_CI_ERR_NOT_FOUND). (v0.3.0+) */
int midi2_ci_remove_property(midi2_ci_state *state, const char *name);

/** Clear all registered profiles (count-only reset; storage contents are
 *  left intact for caller inspection or reuse).
 *  Safe to call with NULL state (no-op). (v0.3.0+) */
void midi2_ci_reset_profiles(midi2_ci_state *state);

/** Clear all registered properties (count-only reset; storage contents
 *  are left intact for caller inspection or reuse).
 *  Safe to call with NULL state (no-op). (v0.3.0+) */
void midi2_ci_reset_properties(midi2_ci_state *state);

/** Toggle the subscribable flag on a registered property at runtime.
 *  Returns MIDI2_CI_OK or MIDI2_CI_ERR_NOT_FOUND.
 *  Safe to call with NULL state or NULL name (returns
 *  MIDI2_CI_ERR_NOT_FOUND). (v0.3.0+) */
int midi2_ci_pe_set_subscribable(midi2_ci_state *state,
                                  const char *name, bool subscribable);

/** Register a subscriber (caller_muid) for the named PE resource. The
 *  property must be registered and marked subscribable. Duplicate
 *  (muid, name) pairs are idempotent and return OK.
 *  Safe to call with NULL state or NULL resource_name (returns
 *  MIDI2_CI_ERR_NOT_FOUND).
 *  @return MIDI2_CI_OK, MIDI2_CI_ERR_NOT_FOUND (property unknown or
 *          not subscribable), or MIDI2_CI_ERR_FULL (no subscriber
 *          capacity, including the case of midi2_ci_init without a
 *          subscribers array). (v0.3.0+) */
int midi2_ci_subscribe_add(midi2_ci_state *state, uint32_t caller_muid,
                            const char *resource_name);

/** Remove a subscriber from the named resource.
 *  Safe to call with NULL state or NULL resource_name (returns
 *  MIDI2_CI_ERR_NOT_FOUND, via indirect find_subscriber_idx guard).
 *  @return MIDI2_CI_OK or MIDI2_CI_ERR_NOT_FOUND. (v0.3.0+) */
int midi2_ci_subscribe_remove(midi2_ci_state *state, uint32_t caller_muid,
                               const char *resource_name);

/** Fan out a PE Notify frame to every subscriber of the named resource.
 *  Returns MIDI2_CI_OK even when the subscriber list is empty, or
 *  MIDI2_CI_ERR_NOT_FOUND when the property is unknown. Emission uses
 *  the state's write_fn (same path as Discovery / PE Reply).
 *  Safe to call with NULL state or NULL resource_name (returns
 *  MIDI2_CI_ERR_NOT_FOUND). (v0.3.0+) */
int midi2_ci_notify_property_changed(midi2_ci_state *state,
                                      const char *resource_name);

/** Return the current number of active subscribers across all
 *  resources. Returns 0 if state is NULL. (v0.3.0+) */
uint8_t midi2_ci_get_subscriber_count(const midi2_ci_state *state);

/** Process incoming SysEx that might be MIDI-CI.
 *  Returns true if the message was handled (CI), false if not.
 *  Automatically sends Discovery Reply, Profile Inquiry Reply, PE responses.
 *
 *  LIMITATIONS (simplified convenience responder):
 *  - PE Get always returns the first property with a non-NULL value,
 *    regardless of which property was requested (no JSON header parsing).
 *  - PE Set calls the first setter with an empty value string.
 *  - All replies use MIDI-CI Message Version 1 (no v2 extended fields).
 *  - For full PE/Profile control, use midi2_ci_dispatch directly.
 *
 *  @param state   CI state struct. Safe to pass NULL (returns false).
 *  @param group   UMP group the SysEx arrived on (responses go to same group)
 *  @param data    Reassembled SysEx content (no F0/F7)
 *  @param length  Length of data */
bool midi2_ci_process_sysex(midi2_ci_state *state,
                               uint8_t group, const uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* MIDI2_CI_H */
