/* MIDI-CI responder for rp2350-device-freertos.
 *
 * Wraps midi2_ci_state: Discovery, one Profile, and Property Exchange
 * (a static DeviceInfo property). Mirrors the Zephyr sibling's CI bootstrap.
 * Replies are built by the core and emitted via pipeline_tx_words. */
#include "ci_responder.h"
#include "pipeline.h"
#include "midi2_ci.h"

#include "pico/rand.h"

/* Educational identity (shared with the catalog's Device Identity entry). */
#define CI_MFR      0x007D0000u   /* {0x7D, 0x00, 0x00} educational prefix */
#define CI_FAMILY   0x0001u
#define CI_MODEL    0x0001u
#define CI_VERSION  0x00010000u

static const uint8_t k_profile_id[5] = { 0x7D, 0x00, 0x00, 0x01, 0x00 };

static midi2_ci_state    g_ci;
static uint8_t           g_ci_profiles[4][5];
static midi2_ci_property g_ci_props[4];

/* Platform RNG for MUID (regeneration on Invalidate MUID / collision). */
static uint32_t ci_rng(void *ctx) {
  (void) ctx;
  return get_rand_32();
}

/* CI reply sink: the core hands us a run of UMP words (one or more SysEx7
 * packets); reframe and enqueue each as its own UMP message. */
static uint32_t ci_write_fn(const uint32_t *words, uint32_t count, void *ctx) {
  (void) ctx;
  pipeline_tx_words(words, count);
  return count;
}

void ci_responder_init(void) {
  midi2_ci_init(&g_ci, get_rand_32(),
                g_ci_profiles, 4,
                g_ci_props,    4);
  midi2_ci_set_rng(&g_ci, ci_rng, NULL);
  midi2_ci_set_write_fn(&g_ci, ci_write_fn, NULL);
  midi2_ci_set_identity(&g_ci, CI_MFR, CI_FAMILY, CI_MODEL, CI_VERSION);
  midi2_ci_set_nak_on_unknown(&g_ci, true);

  midi2_ci_add_profile(&g_ci, k_profile_id);
  midi2_ci_add_property_static(&g_ci, "DeviceInfo",
      "{\"manufacturer\":\"github.com/sauloverissimo\","
       "\"family\":\"rp2350-device-freertos\","
       "\"model\":\"RP2350FreeRTOSBench\","
       "\"version\":\"0.6.0\"}");
}

void ci_responder_feed_sysex7(uint8_t group, const uint8_t *data, uint16_t len) {
  (void) midi2_ci_process_sysex(&g_ci, group, data, len);
}
