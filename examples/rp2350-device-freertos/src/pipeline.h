#ifndef RP2350_PIPELINE_H
#define RP2350_PIPELINE_H
#include <stdint.h>

/* One UMP message: 1..4 words in w[]. Same shape as catalog_msg_t. */
typedef struct {
  uint8_t  n;
  uint32_t w[4];
} ump_msg_t;

/* Create the static queues and the two tasks (usb_task + midi_task).
 * Call before vTaskStartScheduler(). */
void pipeline_start(void);

/* midi_task side: enqueue an outbound message. usb_task drains it on its
 * next 1 ms poll and calls tud_midi2_n_ump_write. Counted-drop on full. */
void pipeline_tx(const ump_msg_t *m);

/* Enqueue a run of UMP words (one or more whole messages), reframing each by
 * its MT word-count. Used by the MIDI-CI responder, whose replies are a stream
 * of SysEx7 packets. Counted-drop on full. */
void pipeline_tx_words(const uint32_t *words, uint32_t count);

/* Counted drops when a queue was full (never masked). */
uint32_t pipeline_rx_drops(void);
uint32_t pipeline_tx_drops(void);

#endif
