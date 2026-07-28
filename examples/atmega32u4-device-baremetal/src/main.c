/* atmega32u4-device-baremetal: USB MIDI 2.0 device on ATmega32U4, bare metal.
 *
 * Enumerates with both USB-MIDI alternate settings (alt 0 = MIDI 1.0,
 * alt 1 = MIDI 2.0/UMP), plays a 4-note riff and echoes back everything
 * it receives. The super-loop is the whole scheduler.
 */
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <LUFA/Drivers/USB/USB.h>
#include "midi2lufa_descriptors.h"
#include "midi2lufa.h"
#include "stream_responder.h"
#include "ci_responder.h"
#include "catalog.h"
#include "midi2_dispatch.h"
#include "midi2_proc.h"
#include "midi2_msg.h"
#include "board.h"

static midi2_dispatch    g_dp;
static midi2_proc_state  g_proc;
static uint8_t           g_sysex7_buf[160];   /* MIDI-CI reassembly */

/* ---- millisecond clock, polled (Timer1, no ISR) ----------------------- */
static uint16_t g_ms;

static void clock_init(void) {
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);   /* CTC, /64: 250 kHz */
    OCR1A  = 249;                                  /* 1 ms per compare  */
}

static void clock_task(void) {
    if (TIFR1 & _BV(OCF1A)) {
        TIFR1 = _BV(OCF1A);
        g_ms++;
    }
}

/* ---- MIDI 1.0 event packets ------------------------------------------ */
/* In alt 0 the transport still moves 4-byte words; each word is one
 * USB-MIDI 1.0 event packet, packed little-endian. */
static bool midi1_send(uint8_t cin, uint8_t b0, uint8_t b1, uint8_t b2) {
    uint32_t w = (uint32_t)cin | ((uint32_t)b0 << 8)
               | ((uint32_t)b1 << 16) | ((uint32_t)b2 << 24);
    return midi2lufa_write_word(w);
}

/* echo: drain received packets straight back out */
static void echo_task(void) {
    uint32_t w;
    while (midi2lufa_read(&w)) {
        (void)midi2lufa_write_word(w);
        LED_TOGGLE();
    }
}

/* riff: C4 E4 G4 C5, one note every 500 ms, note off at 250 ms */
static const uint8_t riff_notes[4] = { 60, 64, 67, 72 };

/* one note event, on the wire format of the active alt setting */
static void riff_send(bool note_on, uint8_t note) {
    if (midi2lufa_alt() == 0) {
        midi1_send(note_on ? 0x09 : 0x08, note_on ? 0x90 : 0x80,
                   note, note_on ? 100 : 0);
    } else {
        /* MT4 MIDI 2.0 channel voice, group 0, 16-bit velocity */
        uint32_t w[2];
        w[0] = (UINT32_C(0x4) << 28) | ((uint32_t)(note_on ? 0x90 : 0x80) << 16)
             | ((uint32_t)note << 8);
        w[1] = note_on ? (UINT32_C(0xC800) << 16) : 0;
        midi2lufa_write(w, 2);
    }
}

static void riff_task(void) {
    static uint16_t last;
    static uint8_t  step;
    static bool     on;
    uint16_t elapsed = (uint16_t)(g_ms - last);

    if (!on && elapsed >= 500) {
        riff_send(true, riff_notes[step & 3]);
        on = true; last = g_ms;
        LED_TOGGLE();
    } else if (on && elapsed >= 250) {
        riff_send(false, riff_notes[step & 3]);
        on = false; step++;
        last = (uint16_t)(g_ms - 250);   /* keep the 500 ms note grid */
    }
}

/* ---- alt 1 catalog cycle ------------------------------------------------
 * Emits one M2-104 catalog entry every 500 ms; group-15 control surface:
 * NoteOn = fire that entry, CC 120/121 = pause/resume, CC 119 = burst
 * (value+1 full catalog sweeps back to back, paced only by the TX ring). */
static uint16_t cat_last;
static uint8_t  cat_idx;
static bool     cat_paused;
static uint8_t  burst_sweeps;

static void catalog_emit(uint8_t idx) {
    catalog_msg_t m;
    if (midi2_catalog_build(idx, &m))
        midi2lufa_write(m.w, m.n);
}

static void catalog_task(void) {
    while (burst_sweeps) {
        catalog_msg_t m;
        midi2_catalog_build(cat_idx, &m);
        if (!midi2lufa_write(m.w, m.n))
            return;                    /* ring full: resume next loop */
        if (++cat_idx >= (uint8_t)midi2_catalog_count()) {
            cat_idx = 0;
            burst_sweeps--;
        }
    }
    if (cat_paused)
        return;
    if ((uint16_t)(g_ms - cat_last) >= 500) {
        catalog_emit(cat_idx);
        cat_idx = (uint8_t)((cat_idx + 1u) % midi2_catalog_count());
        cat_last = g_ms;
        LED_TOGGLE();
    }
}

/* group-15 sentinel control surface, MT2 or MT4 form */
static bool control_surface(const uint32_t *w) {
    uint8_t mt = midi2_msg_get_mt(w);
    if ((mt != MIDI2_MT_MIDI1_CV && mt != MIDI2_MT_MIDI2_CV) ||
        midi2_msg_get_group(w) != 15)
        return false;

    uint8_t status = (uint8_t)(midi2_msg_get_status(w) & 0xF0);
    uint8_t index  = (uint8_t)((w[0] >> 8) & 0x7F);

    if (status == 0x90) {
        catalog_emit((uint8_t)(index % midi2_catalog_count()));
    } else if (status == 0xB0) {
        uint8_t v7 = (mt == MIDI2_MT_MIDI2_CV) ? (uint8_t)((w[1] >> 25) & 0x7F)
                                               : (uint8_t)(w[0] & 0x7F);
        if (index == 120)      cat_paused = true;
        else if (index == 121) cat_paused = false;
        else if (index == 119) burst_sweeps = (uint8_t)(v7 + 1);
    }
    return true;
}

/* ---- alt 1 message routing -------------------------------------------- */
/* midi2_proc on_ump: stream messages go to the dispatch (responder),
 * group 15 drives the catalog, everything else echoes. SysEx7 never lands
 * here; proc reassembles it and delivers through on_sysex7 below. */
static void on_ump(const uint32_t *words, uint8_t word_count, void *ctx) {
    (void)ctx;
    if (midi2_msg_get_mt(words) == MIDI2_MT_STREAM)
        midi2_dispatch_feed(words, word_count, &g_dp);
    else if (!control_surface(words))
        midi2lufa_write(words, word_count);   /* echo */
}

static void on_sysex7(uint8_t group, const uint8_t *data, uint16_t len, void *ctx) {
    (void)ctx;
    ci_responder_feed_sysex7(group, data, len);
}

/* assemble complete UMPs from the RX ring and feed the processor */
static void ump_task(void) {
    static uint32_t msg[4];
    static uint8_t  have;
    uint32_t w;

    for (;;) {
        if (have == 0) {
            if (!midi2lufa_read(&msg[0]))
                return;
            have = 1;
        }
        uint8_t mt   = midi2_msg_get_mt(msg);
        uint8_t need = midi2_msg_word_count(mt);
        while (have < need) {
            if (!midi2lufa_read(&w))
                return;               /* rest of the message next round */
            msg[have++] = w;
        }
        /* Data128 (SysEx8 / MDS) is echoed raw: midi2_proc consumes it for
         * reassembly, which is off here, so it never reaches on_ump. */
        if (mt == MIDI2_MT_DATA128)
            midi2lufa_write(msg, need);
        else
            midi2_proc_feed(&g_proc, msg, need);
        LED_TOGGLE();
        have = 0;
    }
}

/* ---- LUFA events ------------------------------------------------------ */
void EVENT_USB_Device_ConfigurationChanged(void) {
    midi2lufa_configure_endpoints();
}

void EVENT_USB_Device_ControlRequest(void) {
    midi2lufa_control_request();
}

int main(void) {
    MCUSR = 0;
    wdt_disable();
    clock_prescale_set(clock_div_1);

    LED_INIT();
    LED_OFF();
    clock_init();
    midi2_dispatch_init(&g_dp);
    stream_responder_attach(&g_dp);
    midi2_proc_init(&g_proc, g_sysex7_buf, sizeof g_sysex7_buf, NULL, 0);
    g_proc.on_ump    = on_ump;
    g_proc.on_sysex7 = on_sysex7;
    ci_responder_init();
    USB_Init();
    GlobalInterruptEnable();   /* LUFA needs the USB general ISR */

    for (;;) {
        clock_task();
        if (USB_DeviceState == DEVICE_STATE_Configured) {
            if (midi2lufa_alt() == 0) {
                riff_task();
                echo_task();
                            } else {
                midi2lufa_task();
                ump_task();
                catalog_task();
            }
        }
        USB_USBTask();
    }
}
