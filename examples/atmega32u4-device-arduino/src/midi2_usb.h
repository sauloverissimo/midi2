/* Midi2Usb: Arduino-friendly veneer over the midi2 C99 core.
 *
 * begin() wires the UMP Stream responder, the MIDI-CI responder and the
 * message processor onto the midi2duino transport. task()
 * pumps USB and feeds received UMP into the processor. The send* helpers build
 * MIDI 2.0 UMP with the core's midi2_msg_* builders and queue them for TX.
 *
 * Everything is fixed-callback C99 underneath, no std::function and no heap,
 * so the whole device fits an ATmega32U4 (2.5 KB SRAM). One bidirectional
 * Function Block ("Main") on group 0.
 */
#pragma once

#include <stdint.h>

class Midi2Usb {
public:
    /* Attach responders + processor and program the device identity. */
    void begin();

    /* Pump USB and route received UMP. Call every loop() iteration. */
    void task();

    /* ---- MIDI 2.0 Channel Voice (group 0, 32-bit resolution) ---------- */
    void sendNoteOn(uint8_t channel, uint8_t note, uint16_t velocity16);
    void sendNoteOff(uint8_t channel, uint8_t note);
    void sendControlChange(uint8_t channel, uint8_t index, uint32_t value);
    void sendProgramChange(uint8_t channel, uint8_t program);
    void sendPitchBend(uint8_t channel, uint32_t value);
    void sendChannelPressure(uint8_t channel, uint32_t value);
    void sendPolyPressure(uint8_t channel, uint8_t note, uint32_t value);
    void sendPerNotePitchBend(uint8_t channel, uint8_t note, uint32_t value);

    /* ---- Flex Data (group 0) ------------------------------------------ */
    void sendTempo(uint32_t tenNsPerQuarter);
    void sendTimeSignature(uint8_t numerator, uint8_t denominator);

    /* ---- Data messages (group 0) -------------------------------------- */
    void sendSysEx7(const uint8_t *data, uint8_t len);                 /* <= 6 bytes, single packet */
    void sendSysEx8(uint8_t streamId, const uint8_t *data, uint8_t len); /* <= 13 bytes, single packet */
    void sendMds(uint8_t mdsId, const uint8_t *data, uint8_t len);     /* header + one payload */
};
