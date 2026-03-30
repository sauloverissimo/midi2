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
} midi2_ci_property;

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
} midi2_ci_state;

/*--------------------------------------------------------------------+
 * Functions
 *--------------------------------------------------------------------*/

/** Initialize state with caller-provided storage.
 *  @param state         State struct (caller-allocated)
 *  @param muid_seed     Random or unique value for MUID generation (28-bit)
 *  @param profiles      Caller's profile array, or NULL if no profiles needed
 *  @param max_profiles  Capacity of profiles array
 *  @param properties    Caller's property array, or NULL if no properties needed
 *  @param max_properties Capacity of properties array */
void midi2_ci_init(midi2_ci_state *state, uint32_t muid_seed,
                     uint8_t (*profiles)[5], uint8_t max_profiles,
                     midi2_ci_property *properties, uint8_t max_properties);

/** Configure device identity */
void midi2_ci_set_identity(midi2_ci_state *state,
                             uint32_t manufacturer_id, uint16_t family_id,
                             uint16_t model_id, uint32_t version_id);

/** Set the write function (how CI sends SysEx responses) */
void midi2_ci_set_write_fn(midi2_ci_state *state,
                              midi2_proc_write_fn write_fn, void *context);

/** Add a profile. Returns MIDI2_CI_OK or MIDI2_CI_ERR_FULL. */
int midi2_ci_add_profile(midi2_ci_state *state, const uint8_t profile_id[5]);

/** Remove a profile. Returns MIDI2_CI_OK or MIDI2_CI_ERR_NOT_FOUND. */
int midi2_ci_remove_profile(midi2_ci_state *state, const uint8_t profile_id[5]);

/** Add a static property. Returns MIDI2_CI_OK or MIDI2_CI_ERR_FULL. */
int midi2_ci_add_property_static(midi2_ci_state *state,
                                    const char *name, const char *value);

/** Add a dynamic property with getter/setter. Returns MIDI2_CI_OK or MIDI2_CI_ERR_FULL. */
int midi2_ci_add_property_dynamic(midi2_ci_state *state,
                                     const char *name,
                                     midi2_ci_pe_getter getter,
                                     midi2_ci_pe_setter setter);

/** Process incoming SysEx that might be MIDI-CI.
 *  Returns true if the message was handled (CI), false if not.
 *  Automatically sends Discovery Reply, Profile Inquiry Reply, PE responses.
 *  @param group   UMP group the SysEx arrived on (responses go to same group)
 *  @param data    Reassembled SysEx content (no F0/F7)
 *  @param length  Length of data */
bool midi2_ci_process_sysex(midi2_ci_state *state,
                               uint8_t group, const uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* MIDI2_CI_H */
