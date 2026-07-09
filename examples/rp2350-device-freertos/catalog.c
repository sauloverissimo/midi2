/* Deterministic UMP catalog for the rp2350-device-freertos showcase.
 *
 * One entry per distinct message type, covering every DEFINED UMP message
 * type category of M2-104-UM v1.1.2: Utility (0x0), System (0x1), MIDI 1.0
 * Channel Voice (0x2), SysEx7 / Data64 (0x3), MIDI 2.0 Channel Voice incl.
 * per-note (0x4), SysEx8 + Mixed Data Set / Data128 (0x5), Flex Data (0xD),
 * UMP Stream (0xF). MT 0x6..0xC and 0xE are reserved in v1.1.2 and therefore
 * intentionally absent.
 *
 * Pure C99. No FreeRTOS, no TinyUSB: this file is host-unit-testable and is
 * the source of truth for the recipe's `## Spec coverage` table. */
#include "catalog.h"
#include "midi2_msg.h"

/* Group 0 is the only advertised group (Function Block numGroups=1). */
#define CAT_GROUP   0
#define CAT_CHANNEL 0

/* Sample payloads (educational manufacturer prefix 0x7D). */
static const uint8_t k_sysex7[]  = { 0x7D, 0x11, 0x22, 0x33 };
static const uint8_t k_sysex8[]  = { 0x7D, 0x44, 0x55, 0x66, 0x77 };
static const uint8_t k_mds[]     = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02 };
static const uint8_t k_composer[] = { 'B', 'a', 'c', 'h' };
static const uint8_t k_lyrics[]   = { 'l', 'a' };

/* MIDI-CI / device identity constants, shared with the CI responder. */
#define CAT_MFR_ID   0x007D0000u   /* {0x7D,0x00,0x00} educational prefix */
#define CAT_FAMILY   0x0001u
#define CAT_MODEL    0x0001u
#define CAT_VERSION  0x00010000u

uint32_t midi2_catalog_count(void) { return 58; }

uint8_t midi2_catalog_build(uint32_t idx, catalog_msg_t *out) {
  uint32_t *w = out->w;
  w[0] = w[1] = w[2] = w[3] = 0;

  switch (idx) {
    /* ---- MT 0x0 Utility (1 word) ---- */
    case  0: w[0] = midi2_msg_noop();                       out->n = 1; break;
    case  1: w[0] = midi2_msg_jr_clock(0x1234);             out->n = 1; break;
    case  2: w[0] = midi2_msg_jr_timestamp(0x1234);         out->n = 1; break;
    case  3: w[0] = midi2_msg_dctpq(960);                   out->n = 1; break;
    case  4: w[0] = midi2_msg_delta_clockstamp(240);        out->n = 1; break;

    /* ---- MT 0x1 System Real-Time & Common (1 word) ---- */
    case  5: w[0] = midi2_msg_system_mtc(CAT_GROUP, 0x10);  out->n = 1; break;
    case  6: w[0] = midi2_msg_system_song_position(CAT_GROUP, 100); out->n = 1; break;
    case  7: w[0] = midi2_msg_system_song_select(CAT_GROUP, 3); out->n = 1; break;
    case  8: w[0] = midi2_msg_system_tune_request(CAT_GROUP);   out->n = 1; break;
    case  9: w[0] = midi2_msg_system_timing_clock(CAT_GROUP);   out->n = 1; break;
    case 10: w[0] = midi2_msg_system_start(CAT_GROUP);          out->n = 1; break;
    case 11: w[0] = midi2_msg_system_continue(CAT_GROUP);       out->n = 1; break;
    case 12: w[0] = midi2_msg_system_stop(CAT_GROUP);           out->n = 1; break;
    case 13: w[0] = midi2_msg_system_active_sensing(CAT_GROUP); out->n = 1; break;
    case 14: w[0] = midi2_msg_system_reset(CAT_GROUP);          out->n = 1; break;

    /* ---- MT 0x2 MIDI 1.0 Channel Voice (1 word) ---- */
    case 15: w[0] = midi2_msg_from_midi1(CAT_GROUP, 0x80 | CAT_CHANNEL, 60, 0x40); out->n = 1; break;
    case 16: w[0] = midi2_msg_from_midi1(CAT_GROUP, 0x90 | CAT_CHANNEL, 60, 0x50); out->n = 1; break;
    case 17: w[0] = midi2_msg_from_midi1(CAT_GROUP, 0xA0 | CAT_CHANNEL, 60, 0x30); out->n = 1; break;
    case 18: w[0] = midi2_msg_from_midi1(CAT_GROUP, 0xB0 | CAT_CHANNEL, 74, 0x64); out->n = 1; break;
    case 19: w[0] = midi2_msg_from_midi1(CAT_GROUP, 0xC0 | CAT_CHANNEL, 5, 0);     out->n = 1; break;
    case 20: w[0] = midi2_msg_from_midi1(CAT_GROUP, 0xD0 | CAT_CHANNEL, 0x55, 0);  out->n = 1; break;
    case 21: w[0] = midi2_msg_from_midi1(CAT_GROUP, 0xE0 | CAT_CHANNEL, 0x00, 0x40); out->n = 1; break;

    /* ---- MT 0x3 SysEx7 / Data64 (2 words), all four statuses ---- */
    case 22: midi2_msg_sysex7_packet(w, CAT_GROUP, MIDI2_SYSEX7_COMPLETE, k_sysex7, 4); out->n = 2; break;
    case 23: midi2_msg_sysex7_packet(w, CAT_GROUP, MIDI2_SYSEX7_START,    k_sysex7, 4); out->n = 2; break;
    case 24: midi2_msg_sysex7_packet(w, CAT_GROUP, MIDI2_SYSEX7_CONTINUE, k_sysex7, 4); out->n = 2; break;
    case 25: midi2_msg_sysex7_packet(w, CAT_GROUP, MIDI2_SYSEX7_END,      k_sysex7, 4); out->n = 2; break;

    /* ---- MT 0x4 MIDI 2.0 Channel Voice (2 words), including per-note ---- */
    case 26: midi2_msg_reg_per_note_ctrl(w, CAT_GROUP, CAT_CHANNEL, 60, 7, 0x40000000u); out->n = 2; break; /* RPNC   0x0 */
    case 27: midi2_msg_asn_per_note_ctrl(w, CAT_GROUP, CAT_CHANNEL, 60, 3, 0x20000000u); out->n = 2; break; /* APNC   0x1 */
    case 28: midi2_msg_rpn(w, CAT_GROUP, CAT_CHANNEL, 0, 0, 0x40000000u);       out->n = 2; break;          /* RPN    0x2 */
    case 29: midi2_msg_nrpn(w, CAT_GROUP, CAT_CHANNEL, 1, 2, 0x30000000u);      out->n = 2; break;          /* NRPN   0x3 */
    case 30: midi2_msg_rel_rpn(w, CAT_GROUP, CAT_CHANNEL, 0, 0, 0x00001000u);   out->n = 2; break;          /* relRPN 0x4 */
    case 31: midi2_msg_rel_nrpn(w, CAT_GROUP, CAT_CHANNEL, 1, 2, 0xFFFFF000u);  out->n = 2; break;          /* relNRPN0x5 */
    case 32: midi2_msg_per_note_pb(w, CAT_GROUP, CAT_CHANNEL, 60, 0x80000000u); out->n = 2; break;          /* PNPB   0x6 */
    case 33: midi2_msg_note_off(w, CAT_GROUP, CAT_CHANNEL, 60, 0x4000, 0x00, 0); out->n = 2; break;         /* NoteOff 0x8, 16-bit vel */
    case 34: midi2_msg_note_on(w, CAT_GROUP, CAT_CHANNEL, 60, 0xC000, 0x00, 0);  out->n = 2; break;         /* NoteOn  0x9, attr none */
    case 35: midi2_msg_note_on(w, CAT_GROUP, CAT_CHANNEL, 62, 0xC000, 0x03, 0x1234); out->n = 2; break;     /* NoteOn  0x9, attr pitch7.9 */
    case 36: midi2_msg_poly_pressure(w, CAT_GROUP, CAT_CHANNEL, 60, 0x80000000u); out->n = 2; break;        /* PolyPr  0xA */
    case 37: midi2_msg_cc(w, CAT_GROUP, CAT_CHANNEL, 74, 0x40000000u);          out->n = 2; break;          /* CC     0xB */
    case 38: midi2_msg_program(w, CAT_GROUP, CAT_CHANNEL, 5, true, 0, 12);      out->n = 2; break;          /* Prog   0xC, bank valid */
    case 39: midi2_msg_chan_pressure(w, CAT_GROUP, CAT_CHANNEL, 0x60000000u);   out->n = 2; break;          /* ChanPr 0xD */
    case 40: midi2_msg_pitch_bend(w, CAT_GROUP, CAT_CHANNEL, 0x80000000u);      out->n = 2; break;          /* PB     0xE */
    case 41: midi2_msg_per_note_mgmt(w, CAT_GROUP, CAT_CHANNEL, 60, true, true); out->n = 2; break;         /* PNMgmt 0xF, detach+reset */

    /* ---- MT 0x5 SysEx8 + Mixed Data Set / Data128 (4 words) ----
     * Included for spec coverage. SysEx7/Data64 above is the practical demo
     * transport; SysEx8 has no market adoption. */
    case 42: midi2_msg_sysex8_packet(w, CAT_GROUP, MIDI2_SYSEX8_COMPLETE, 0, k_sysex8, 5); out->n = 4; break;
    case 43: midi2_msg_sysex8_packet(w, CAT_GROUP, MIDI2_SYSEX8_START,    0, k_sysex8, 5); out->n = 4; break;
    case 44: midi2_msg_sysex8_packet(w, CAT_GROUP, MIDI2_SYSEX8_CONTINUE, 0, k_sysex8, 5); out->n = 4; break;
    case 45: midi2_msg_sysex8_packet(w, CAT_GROUP, MIDI2_SYSEX8_END,      0, k_sysex8, 5); out->n = 4; break;
    case 46: midi2_msg_mds_header(w, CAT_GROUP, 1, sizeof k_mds, 1, 1, 0x007D, 0xFFFF, 0, 0); out->n = 4; break;
    case 47: midi2_msg_mds_payload(w, CAT_GROUP, 1, k_mds, sizeof k_mds); out->n = 4; break;

    /* ---- MT 0xD Flex Data (4 words) ---- */
    case 48: midi2_msg_tempo(w, CAT_GROUP, 50000);                 out->n = 4; break; /* 500000 us/qn = 120 BPM in 10ns units */
    case 49: midi2_msg_time_sig(w, CAT_GROUP, 4, 2, 8);            out->n = 4; break; /* 4/4 */
    case 50: midi2_msg_metronome(w, CAT_GROUP, 24, 4, 0, 0, 0, 0); out->n = 4; break;
    case 51: midi2_msg_key_sig(w, CAT_GROUP, 2, false);           out->n = 4; break; /* 2 sharps, major */
    case 52: midi2_msg_chord_name(w, CAT_GROUP, 1, CAT_CHANNEL,
                                  2, 3, 0x01, 0,0, 0,0, 0,0, 0,0,
                                  0, 0, 0x00, 0,0, 0,0);            out->n = 4; break; /* C major */
    case 53: midi2_msg_flex_text(w, CAT_GROUP, 0, 1, 0,
                                 MIDI2_FLEX_BANK_METADATA, MIDI2_FLEX_TEXT_COMPOSER_NAME,
                                 k_composer, sizeof k_composer);    out->n = 4; break;
    case 54: midi2_msg_flex_text(w, CAT_GROUP, 0, 1, 0,
                                 MIDI2_FLEX_BANK_PERF_TEXT, MIDI2_FLEX_PERF_LYRICS,
                                 k_lyrics, sizeof k_lyrics);        out->n = 4; break;

    /* ---- MT 0xF UMP Stream (4 words), app-owned only ----
     * Endpoint / Function Block / Stream Config discovery is owned by the
     * TinyUSB built-in responder, not this catalog. */
    case 55: midi2_msg_stream_device_identity(w, CAT_MFR_ID, CAT_FAMILY, CAT_MODEL, CAT_VERSION); out->n = 4; break;
    case 56: midi2_msg_stream_start_of_clip(w); out->n = 4; break;
    case 57: midi2_msg_stream_end_of_clip(w);   out->n = 4; break;

    default: out->n = 0; break;
  }
  return out->n;
}
