#include <avr/io.h>
#include <avr/eeprom.h>
#include "ci_responder.h"
#include <midi2duino.h>
#include "midi2_ci.h"

/* Same educational identity as the stream responder. NOTE: the CI builder
 * serializes the manufacturer LSB-first (wire Byte 1 = value bits 0..7),
 * the OPPOSITE of midi2_msg_stream_device_identity. 0x7D must sit in the
 * low byte here to reach wire Byte 1. */
#define CI_MFR      0x0000007DUL   /* wire: {0x7D, 0x00, 0x00} */
#define CI_FAMILY   0x0001
#define CI_MODEL    0x0002   /* distinct from the baremetal recipe (0x0001) */
#define CI_VERSION  0x00010000UL

static midi2_ci_state    g_ci;
static uint8_t           g_profiles[1][5];   /* slots exist, none added */
static midi2_ci_property g_props[1];

/* xorshift32 for the 28-bit MUID on a part with no TRNG. The seed mixes an
 * EEPROM boot counter (monotonic across power cycles, so two boots can never
 * repeat a seed) with Timer1 phase and the USB frame number (jitter). */
static uint32_t  g_rng_state;
static uint32_t EEMEM ee_boot_count;

static uint32_t ci_rng(void *ctx) {
    (void)ctx;
    if (g_rng_state == 0) {
        uint32_t boots = eeprom_read_dword(&ee_boot_count);
        if (boots == 0xFFFFFFFFUL)
            boots = 0;                     /* fresh EEPROM */
        eeprom_update_dword(&ee_boot_count, boots + 1);
        g_rng_state = ((boots + 1) * 0x9E3779B9UL)   /* golden-ratio spread */
                    ^ (((uint32_t)UDFNUMH << 24) | ((uint32_t)UDFNUML << 16) | TCNT1);
        if (g_rng_state == 0)
            g_rng_state = 1;
    }
    g_rng_state ^= g_rng_state << 13;
    g_rng_state ^= g_rng_state >> 17;
    g_rng_state ^= g_rng_state << 5;
    return g_rng_state;
}

static uint32_t ci_write_fn(const uint32_t *words, uint32_t count, void *ctx) {
    (void)ctx;
    /* Queue the whole SysEx7 packet atomically: a partial write would leave
     * half a UMP at the head of the ring and corrupt the USB output. */
    return midi2duino_write(words, (uint8_t)count) ? count : 0;
}

void ci_responder_init(void) {
    midi2_ci_init(&g_ci, ci_rng(0), g_profiles, 1, g_props, 1);
    midi2_ci_set_rng(&g_ci, ci_rng, 0);
    midi2_ci_set_write_fn(&g_ci, ci_write_fn, 0);
    midi2_ci_set_identity(&g_ci, CI_MFR, CI_FAMILY, CI_MODEL, CI_VERSION);
    midi2_ci_set_capabilities(&g_ci, 0x00);   /* Discovery only, honest */
    midi2_ci_set_nak_on_unknown(&g_ci, true);
}

void ci_responder_feed_sysex7(uint8_t group, const uint8_t *data, uint16_t len) {
    (void)midi2_ci_process_sysex(&g_ci, group, data, len);
}
