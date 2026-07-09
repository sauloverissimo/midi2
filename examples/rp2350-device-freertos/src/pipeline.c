/* FreeRTOS pipeline for rp2350-device-freertos.
 *
 * usb_task (high prio) is the SOLE owner of every tud_midi2_* call: it services
 * tud_task_ext with a 1 ms finite timeout, drains tx_queue to the device on
 * every wake, and on tud_midi2_rx_cb frames inbound words per-message into
 * rx_queue. midi_task (low prio) blocks on rx_queue with a scene-tick timeout,
 * dispatches inbound, and emits the catalog cycle to tx_queue.
 *
 * Everything is statically allocated (configSUPPORT_STATIC_ALLOCATION). */
#include "pipeline.h"
#include "catalog.h"
#include "midi2_msg.h"
#include "tusb.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* ---- sizing: queue depth + endpoint FIFO (tusb_config.h) = burst budget ---- */
#define PIPE_QUEUE_DEPTH   64
#define USB_TASK_PRIO      (tskIDLE_PRIORITY + 3)
#define MIDI_TASK_PRIO     (tskIDLE_PRIORITY + 1)
#define USB_TASK_STACK     512
#define MIDI_TASK_STACK    512
#define SCENE_TICK_MS      50

/* ---- static storage ---- */
static StaticQueue_t s_rxq_ctrl, s_txq_ctrl;
static uint8_t s_rxq_store[PIPE_QUEUE_DEPTH * sizeof(ump_msg_t)];
static uint8_t s_txq_store[PIPE_QUEUE_DEPTH * sizeof(ump_msg_t)];
static QueueHandle_t rx_queue, tx_queue;

static StaticTask_t s_usb_tcb, s_midi_tcb;
static StackType_t  s_usb_stack[USB_TASK_STACK], s_midi_stack[MIDI_TASK_STACK];

static volatile uint32_t s_rx_drops, s_tx_drops;

uint32_t pipeline_rx_drops(void) { return s_rx_drops; }
uint32_t pipeline_tx_drops(void) { return s_tx_drops; }

void pipeline_tx(const ump_msg_t *m) {
  if (xQueueSend(tx_queue, m, 0) != pdTRUE) s_tx_drops++;   /* counted, never masked */
}

/*--------------------------------------------------------------------+
 * usb_task: sole owner of tud_midi2_*
 *--------------------------------------------------------------------*/
static void usb_task(void *arg) {
  (void) arg;
  for (;;) {
    tud_task_ext(1, false);            /* 1 ms finite timeout: wakes on USB event OR timeout */
    ump_msg_t m;
    while (xQueueReceive(tx_queue, &m, 0) == pdTRUE) {
      tud_midi2_n_ump_write(0, m.w, m.n);
    }
  }
}

/* RX callback runs in tud_task context (i.e. inside usb_task). Frame per-message:
 * tud_midi2_n_ump_read does not respect UMP message boundaries. */
void tud_midi2_rx_cb(uint8_t itf) {
  (void) itf;
  for (;;) {
    uint32_t w0;
    if (tud_midi2_n_ump_read(0, &w0, 1) != 1) break;
    ump_msg_t m;
    m.n = midi2_msg_word_count(midi2_msg_get_mt(&w0));
    if (m.n == 0 || m.n > 4) m.n = 1;
    m.w[0] = w0;
    m.w[1] = m.w[2] = m.w[3] = 0;
    for (uint8_t i = 1; i < m.n; i++) {
      if (tud_midi2_n_ump_read(0, &m.w[i], 1) != 1) { m.n = i; break; }
    }
    /* rx_cb runs in tud_task (usb_task) context, not an ISR: plain send. */
    if (xQueueSend(rx_queue, &m, 0) != pdTRUE) s_rx_drops++;
  }
}

/*--------------------------------------------------------------------+
 * midi_task: catalog cycle + inbound triggers
 *--------------------------------------------------------------------*/
static uint32_t s_cycle_idx;
static bool s_cycle_paused;

static void handle_inbound(const ump_msg_t *in) {
  /* group-15 sentinel control surface (device advertises only group 0). */
  if (midi2_msg_get_mt(in->w) == MIDI2_MT_MIDI2_CV && midi2_msg_get_group(in->w) == 15) {
    uint8_t status = (uint8_t)(midi2_msg_get_status(in->w) & 0xF0);
    uint8_t note   = midi2_msg_get_note(in->w);
    if (status == MIDI2_STATUS_NOTE_ON) {
      catalog_msg_t c;
      if (midi2_catalog_build(note % midi2_catalog_count(), &c)) {
        ump_msg_t m; m.n = c.n;
        for (uint8_t i = 0; i < c.n; i++) m.w[i] = c.w[i];
        pipeline_tx(&m);
      }
    } else if (status == MIDI2_STATUS_CC) {
      uint8_t index = note;   /* CC index sits in the note field for MT4 CC */
      if (index == 120) s_cycle_paused = true;
      else if (index == 121) s_cycle_paused = false;
    }
  }
}

static void midi_task(void *arg) {
  (void) arg;
  for (;;) {
    ump_msg_t in;
    if (xQueueReceive(rx_queue, &in, pdMS_TO_TICKS(SCENE_TICK_MS)) == pdTRUE) {
      handle_inbound(&in);
      continue;
    }
    /* timeout: advance the catalog cycle unless paused, with a JR heartbeat */
    if (s_cycle_paused || !tud_midi2_n_mounted(0)) continue;

    ump_msg_t hb; hb.n = 1; hb.w[0] = midi2_msg_jr_timestamp(0);
    pipeline_tx(&hb);

    catalog_msg_t c;
    if (midi2_catalog_build(s_cycle_idx, &c)) {
      ump_msg_t m; m.n = c.n;
      for (uint8_t i = 0; i < c.n; i++) m.w[i] = c.w[i];
      pipeline_tx(&m);
    }
    s_cycle_idx = (s_cycle_idx + 1) % midi2_catalog_count();
  }
}

void pipeline_start(void) {
  rx_queue = xQueueCreateStatic(PIPE_QUEUE_DEPTH, sizeof(ump_msg_t), s_rxq_store, &s_rxq_ctrl);
  tx_queue = xQueueCreateStatic(PIPE_QUEUE_DEPTH, sizeof(ump_msg_t), s_txq_store, &s_txq_ctrl);

  xTaskCreateStatic(usb_task,  "usb",  USB_TASK_STACK,  NULL, USB_TASK_PRIO,  s_usb_stack,  &s_usb_tcb);
  xTaskCreateStatic(midi_task, "midi", MIDI_TASK_STACK, NULL, MIDI_TASK_PRIO, s_midi_stack, &s_midi_tcb);
}
