// midi2 / atmega32u4-device-arduino
// USB MIDI 2.0 device on the ATmega32U4 (Arduino Leonardo / Pro Micro / Micro).
//
// The Arduino-friendly sibling of examples/atmega32u4-device-baremetal: same
// midi2 C99 core, same USB MIDI 2.0 class descriptors, but the USB transport is
// the midi2duino library (PluggableUSB, stock Arduino core) instead of LUFA, so
// it flashes with a normal Arduino upload (no manual double-tap reset).
//
// Requires: Arduino AVR Boards core (arduino:avr) and the midi2duino library
// (Library Manager, or github.com/sauloverissimo/midi2duino). The midi2 C99
// core and the responder glue live under src/ and compile with the sketch.
//
// USB identity (VID 0xCAFE, PID 0x40D1, Manufacturer "midi2.diy",
// Product "ATmega32U4 MIDI 2.0") comes from boards.local.txt / the build
// properties in the README. On the UMP wire: Endpoint Name "ATmega32U4 MIDI 2.0",
// one bidirectional Function Block "Main" on group 0.
//
// Always-UMP: the stock AVR core does not surface SET_INTERFACE, so this device
// always speaks MIDI 2.0 (UMP) and targets MIDI 2.0 hosts. See the README.

#include "src/midi2_usb.h"

Midi2Usb midi;

static uint32_t lastDemo = 0;
static uint8_t  demoNote = 60;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    midi.begin();
}

void loop() {
    midi.task();   // pump USB, answer Discovery / MIDI-CI, echo input

    uint32_t now = millis();
    if (now - lastDemo < 3000)
        return;
    lastDemo = now;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

    // A readable tour of the MIDI 2.0 surface, one message family per line.
    // Channel Voice 2.0 (32-bit resolution)
    midi.sendNoteOn(0, demoNote, 0xC000);
    delay(150);
    midi.sendNoteOff(0, demoNote);
    midi.sendControlChange(0, 74, 0x80000000);   // filter cutoff, half scale
    midi.sendPitchBend(0, 0xC0000000);
    midi.sendChannelPressure(0, 0x60000000);
    midi.sendProgramChange(0, 42);
    midi.sendPolyPressure(0, demoNote, 0x40000000);
    midi.sendPerNotePitchBend(0, demoNote, 0x90000000);  // MIDI 2.0 exclusive

    // Flex Data
    midi.sendTempo(50000000);        // 120 BPM in 10-ns ticks per quarter
    midi.sendTimeSignature(4, 2);    // 4/4

    // Data messages: SysEx7 + SysEx8 + Mixed Data Set (educational mfr 0x7D)
    static const uint8_t sx7[] = {0x7D, 0x01, 0x02, 0x03, 0x04, 0x05};
    midi.sendSysEx7(sx7, sizeof sx7);
    static const uint8_t sx8[] = {0x7D, 0x01, 0x02, 0x03, 0x04};
    midi.sendSysEx8(0, sx8, sizeof sx8);
    static const uint8_t mds[] = {0x4D, 0x44, 0x53, 0x21};   // "MDS!"
    midi.sendMds(1, mds, sizeof mds);

    demoNote = (demoNote >= 72) ? 60 : (uint8_t)(demoNote + 1);
}
