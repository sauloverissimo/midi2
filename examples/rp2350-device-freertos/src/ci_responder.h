#ifndef RP2350_CI_RESPONDER_H
#define RP2350_CI_RESPONDER_H
#include <stdint.h>

/* MIDI-CI responder: Discovery + Profile Configuration + Property Exchange.
 * Replies are emitted through pipeline_tx_words (routed to the USB TX queue). */
void ci_responder_init(void);

/* Feed a reassembled SysEx7 byte stream (F0/F7 stripped) to the responder.
 * Called by the midi2_proc SysEx7-complete hook in pipeline.c. */
void ci_responder_feed_sysex7(uint8_t group, const uint8_t *data, uint16_t len);

#endif
