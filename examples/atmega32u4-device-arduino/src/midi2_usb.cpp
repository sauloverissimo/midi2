/* Midi2Usb implementation: the C99 core wiring and the UMP send helpers. */
#include "midi2_usb.h"
#include <midi2duino.h>

extern "C" {
#include "stream_responder.h"
#include "ci_responder.h"
#include "midi2_dispatch.h"
#include "midi2_proc.h"
#include "midi2_msg.h"
}

/* Single Function Block, group 0. */
#define GRP 0

static midi2_dispatch   g_dp;
static midi2_proc_state g_proc;
static uint8_t          g_sysex7_buf[160];   /* MIDI-CI reassembly */

/* Stream messages drive the responder; everything else echoes back.
 * SysEx7 never lands here: the processor reassembles it and delivers it
 * through on_sysex7 to the MIDI-CI responder. */
static void on_ump(const uint32_t *words, uint8_t word_count, void *ctx) {
    (void)ctx;
    if (midi2_msg_get_mt(words) == MIDI2_MT_STREAM)
        midi2_dispatch_feed(words, word_count, &g_dp);
    else
        midi2duino_write(words, word_count);
}

static void on_sysex7(uint8_t group, const uint8_t *data, uint16_t len, void *ctx) {
    (void)ctx;
    ci_responder_feed_sysex7(group, data, len);
}

void Midi2Usb::begin() {
    midi2_dispatch_init(&g_dp);
    stream_responder_attach(&g_dp);
    midi2_proc_init(&g_proc, g_sysex7_buf, sizeof g_sysex7_buf, NULL, 0);
    g_proc.on_ump    = on_ump;
    g_proc.on_sysex7 = on_sysex7;
    ci_responder_init();
}

void Midi2Usb::task() {
    midi2duino_task();

    /* Assemble complete UMPs from the RX ring and feed the processor. */
    static uint32_t msg[4];
    static uint8_t  have;
    uint32_t w;

    for (;;) {
        if (have == 0) {
            if (!midi2duino_read(&msg[0]))
                return;
            have = 1;
        }
        uint8_t mt   = midi2_msg_get_mt(msg);
        uint8_t need = midi2_msg_word_count(mt);
        while (have < need) {
            if (!midi2duino_read(&w))
                return;                 /* rest of the message next round */
            msg[have++] = w;
        }
        /* Data128 (SysEx8 / MDS) is echoed raw: midi2_proc consumes it for
         * reassembly, which is off here, so it never reaches on_ump. */
        if (mt == MIDI2_MT_DATA128)
            midi2duino_write(msg, need);
        else
            midi2_proc_feed(&g_proc, msg, need);
        have = 0;
    }
}

/* ---- send helpers ----------------------------------------------------- */
void Midi2Usb::sendNoteOn(uint8_t channel, uint8_t note, uint16_t velocity16) {
    uint32_t w[2];
    midi2_msg_note_on(w, GRP, channel, note, velocity16, 0, 0);
    midi2duino_write(w, 2);
}

void Midi2Usb::sendNoteOff(uint8_t channel, uint8_t note) {
    uint32_t w[2];
    midi2_msg_note_off(w, GRP, channel, note, 0, 0, 0);
    midi2duino_write(w, 2);
}

void Midi2Usb::sendControlChange(uint8_t channel, uint8_t index, uint32_t value) {
    uint32_t w[2];
    midi2_msg_cc(w, GRP, channel, index, value);
    midi2duino_write(w, 2);
}

void Midi2Usb::sendProgramChange(uint8_t channel, uint8_t program) {
    uint32_t w[2];
    midi2_msg_program(w, GRP, channel, program, false, 0, 0);
    midi2duino_write(w, 2);
}

void Midi2Usb::sendPitchBend(uint8_t channel, uint32_t value) {
    uint32_t w[2];
    midi2_msg_pitch_bend(w, GRP, channel, value);
    midi2duino_write(w, 2);
}

void Midi2Usb::sendChannelPressure(uint8_t channel, uint32_t value) {
    uint32_t w[2];
    midi2_msg_chan_pressure(w, GRP, channel, value);
    midi2duino_write(w, 2);
}

void Midi2Usb::sendPolyPressure(uint8_t channel, uint8_t note, uint32_t value) {
    uint32_t w[2];
    midi2_msg_poly_pressure(w, GRP, channel, note, value);
    midi2duino_write(w, 2);
}

void Midi2Usb::sendPerNotePitchBend(uint8_t channel, uint8_t note, uint32_t value) {
    uint32_t w[2];
    midi2_msg_per_note_pb(w, GRP, channel, note, value);
    midi2duino_write(w, 2);
}

void Midi2Usb::sendTempo(uint32_t tenNsPerQuarter) {
    uint32_t w[4];
    midi2_msg_tempo(w, GRP, tenNsPerQuarter);
    midi2duino_write(w, 4);
}

void Midi2Usb::sendTimeSignature(uint8_t numerator, uint8_t denominator) {
    uint32_t w[4];
    midi2_msg_time_sig(w, GRP, numerator, denominator, 8);
    midi2duino_write(w, 4);
}

void Midi2Usb::sendSysEx7(const uint8_t *data, uint8_t len) {
    uint32_t w[2];
    midi2_msg_sysex7_packet(w, GRP, MIDI2_SYSEX7_COMPLETE, data, len);
    midi2duino_write(w, 2);
}

void Midi2Usb::sendSysEx8(uint8_t streamId, const uint8_t *data, uint8_t len) {
    uint32_t w[4];
    midi2_msg_sysex8_packet(w, GRP, MIDI2_SYSEX8_COMPLETE, streamId, data, len);
    midi2duino_write(w, 4);
}

void Midi2Usb::sendMds(uint8_t mdsId, const uint8_t *data, uint8_t len) {
    uint32_t w[4];
    /* one self-contained chunk: header then a single payload */
    midi2_msg_mds_header(w, GRP, mdsId, (uint16_t)len, 1, 1,
                         0x7D00 /* educational mfr */, 0xFFFF, 0, 0);
    midi2duino_write(w, 4);
    midi2_msg_mds_payload(w, GRP, mdsId, data, len);
    midi2duino_write(w, 4);
}
