/* MIDI-CI responder: Discovery + automatic NAK, nothing else advertised.
 * Replies are SysEx7 UMP packets emitted through midi2duino_write_word().
 */
#ifndef CI_RESPONDER_H
#define CI_RESPONDER_H

#include <stdint.h>

void ci_responder_init(void);
void ci_responder_feed_sysex7(uint8_t group, const uint8_t *data, uint16_t len);

#endif
