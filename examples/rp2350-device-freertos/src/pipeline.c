/* FreeRTOS pipeline for rp2350-device-freertos.
 *
 * usb_task (high prio) is the SOLE owner of every tud_midi2_* call: it services
 * tud_task_ext with a 1 ms finite timeout, drains tx_queue to the device on
 * every wake, and on tud_midi2_rx_cb frames inbound words per-message into
 * rx_queue. midi_task (low prio) blocks on rx_queue with a scene-tick timeout,
 * feeds inbound through midi2_proc (SysEx7 reassembly -> MIDI-CI, plain UMP ->
 * triggers), and emits the catalog cycle to tx_queue.
 *
 * Everything is statically allocated (configSUPPORT_STATIC_ALLOCATION). */
#include "pipeline.h"
#include "catalog.h"
#include "ci_responder.h"
#include "midi2_msg.h"
#include "midi2_proc.h"
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
#define HEARTBEAT_TICKS    10          /* JR Timestamp every 10 scene ticks (~500 ms) */

/* ---- static storage ---- */
static StaticQueue_t s_rxq_ctrl, s_txq_ctrl;
static uint8_t s_rxq_store[PIPE_QUEUE_DEPTH * sizeof(ump_msg_t)];
static uint8_t s_txq_store[PIPE_QUEUE_DEPTH * sizeof(ump_msg_t)];
static QueueHandle_t rx_queue, tx_queue;

static StaticTask_t s_usb_tcb, s_midi_tcb;
static StackType_t  s_usb_stack[USB_TASK_STACK], s_midi_stack[MIDI_TASK_STACK];

static volatile uint32_t s_rx_drops, s_tx_drops;

/* midi2_proc: inbound SysEx7 reassembly + plain-UMP dispatch. */
static midi2_proc_state s_proc;
static uint8_t s_sysex7_buf[256];

uint32_t pipeline_rx_drops(void) { return s_rx_drops; }
uint32_t pipeline_tx_drops(void) { return s_tx_drops; }

void pipeline_tx(const ump_msg_t *m) {
  if (xQueueSend(tx_queue, m, 0) != pdTRUE) s_tx_drops++;   /* counted, never masked */
}

void pipeline_tx_words(const uint32_t *words, uint32_t count) {
  uint32_t i = 0;
  while (i < count) {
    ump_msg_t m;
    m.n = midi2_msg_word_count(midi2_msg_get_mt(&words[i]));
    if (m.n == 0 || m.n > 4) m.n = 1;
    if (i + m.n > count) m.n = (uint8_t)(count - i);
    m.w[0] = m.w[1] = m.w[2] = m.w[3] = 0;
    for (uint8_t k = 0; k < m.n; k++) m.w[k] = words[i + k];
    pipeline_tx(&m);
    i += m.n;
  }
}

/* Build catalog entry `idx` and enqueue it. */
static void emit_catalog(uint32_t idx) {
  catalog_msg_t c;
  if (midi2_catalog_build(idx, &c)) {
    ump_msg_t m;
    m.n = c.n;
    for (uint8_t i = 0; i < c.n; i++) m.w[i] = c.w[i];
    pipeline_tx(&m);
  }
}

/*--------------------------------------------------------------------+
 * usb_task: sole owner of tud_midi2_*
 *--------------------------------------------------------------------*/
static void usb_task(void *arg) {
  (void) arg;
  for (;;) {
    /* Process all pending USB events, non-blocking. tud_task_ext does NOT
     * reliably sleep for its timeout under this FreeRTOS OSAL (it busy-returns),
     * so an explicit yield is required: without it usb_task spins and starves
     * the lower-priority midi_task on the single-core scheduler. */
    tud_task_ext(0, false);
    ump_msg_t m;
    while (xQueueReceive(tx_queue, &m, 0) == pdTRUE) {
      /* n_ump_write returns words actually written; a full TX endpoint FIFO
       * makes it write fewer than requested. Count the shortfall so drops are
       * never masked (matches the rx_queue accounting). */
      uint32_t written = tud_midi2_n_ump_write(0, m.w, m.n);
      if (written < m.n) s_tx_drops++;
    }
    vTaskDelay(1);   /* yield ~1 ms: cap the poll rate and let midi_task run */
  }
}

/* RX callback runs in tud_task context (i.e. inside usb_task).
 *
 * tud_midi2_n_ump_read returns only WHOLE UMP packets and refuses to return a
 * packet larger than max_words (it breaks rather than splitting), so reading
 * with max_words=1 would return 0 for every 2+ word message and wedge the FIFO.
 * Read up to 4 words (the largest UMP) at a time, then reframe the returned
 * buffer into individual messages by each word's MT word-count. */
void tud_midi2_rx_cb(uint8_t itf) {
  (void) itf;
  uint32_t buf[4];
  uint32_t n;
  while ((n = tud_midi2_n_ump_read(0, buf, 4)) > 0) {
    uint32_t i = 0;
    while (i < n) {
      ump_msg_t m;
      m.n = midi2_msg_word_count(midi2_msg_get_mt(&buf[i]));
      if (m.n == 0 || m.n > 4) m.n = 1;
      if (i + m.n > n) m.n = (uint8_t)(n - i);   /* driver returns whole packets; guard anyway */
      m.w[0] = m.w[1] = m.w[2] = m.w[3] = 0;
      for (uint8_t k = 0; k < m.n; k++) m.w[k] = buf[i + k];
      /* rx_cb runs in tud_task (usb_task) context, not an ISR: plain send. */
      if (xQueueSend(rx_queue, &m, 0) != pdTRUE) s_rx_drops++;
      i += m.n;
    }
  }
}

/*--------------------------------------------------------------------+
 * Inbound handling via midi2_proc
 *--------------------------------------------------------------------*/
static uint32_t s_cycle_idx;
static bool s_cycle_paused;

/* midi2_proc on_ump hook: group-15 sentinel control surface (device advertises
 * only group 0). NoteOn fires a catalog index; CC 120/121 pause/resume. */
static void on_ump(const uint32_t *words, uint8_t word_count, void *ctx) {
  (void) word_count; (void) ctx;
  if (midi2_msg_get_mt(words) != MIDI2_MT_MIDI2_CV) return;
  if (midi2_msg_get_group(words) != 15) return;
  uint8_t status = (uint8_t)(midi2_msg_get_status(words) & 0xF0);
  uint8_t index  = midi2_msg_get_note(words);   /* note field = note number / CC index */
  if (status == MIDI2_STATUS_NOTE_ON) {
    emit_catalog(index % midi2_catalog_count());
  } else if (status == MIDI2_STATUS_CC) {
    if (index == 120) s_cycle_paused = true;
    else if (index == 121) s_cycle_paused = false;
  }
}

/* midi2_proc on_sysex7 hook: a complete SysEx7 stream -> MIDI-CI responder. */
static void on_sysex7(uint8_t group, const uint8_t *data, uint16_t len, void *ctx) {
  (void) ctx;
  ci_responder_feed_sysex7(group, data, len);
}

/*--------------------------------------------------------------------+
 * midi_task: catalog cycle + inbound dispatch
 *--------------------------------------------------------------------*/
static void midi_task(void *arg) {
  (void) arg;
#ifdef STRESS_LOOPBACK
  /* Stress mode (build-time): the catalog cycle is suspended; every inbound
   * UMP is echoed back verbatim so the host harness can measure gaps, ordering
   * and throughput through the static-queue pipeline. The sequence number the
   * harness stamps into the packet rides through unchanged. */
  for (;;) {
    ump_msg_t in;
    if (xQueueReceive(rx_queue, &in, portMAX_DELAY) == pdTRUE) {
      pipeline_tx(&in);
    }
  }
#else
  uint32_t tick = 0;
  for (;;) {
    ump_msg_t in;
    if (xQueueReceive(rx_queue, &in, pdMS_TO_TICKS(SCENE_TICK_MS)) == pdTRUE) {
      midi2_proc_feed(&s_proc, in.w, in.n);
      continue;
    }
    /* timeout: advance the catalog cycle unless paused. */
    if (s_cycle_paused || !tud_midi2_n_mounted(0)) continue;

    /* JR Timestamp heartbeat on an independent slow cadence (not per entry). */
    if (++tick >= HEARTBEAT_TICKS) {
      tick = 0;
      ump_msg_t hb; hb.n = 1; hb.w[0] = midi2_msg_jr_timestamp(0);
      pipeline_tx(&hb);
    }

    emit_catalog(s_cycle_idx);
    s_cycle_idx = (s_cycle_idx + 1) % midi2_catalog_count();
  }
#endif
}

void pipeline_start(void) {
  midi2_proc_init(&s_proc, s_sysex7_buf, sizeof s_sysex7_buf, NULL, 0);
  s_proc.on_ump     = on_ump;
  s_proc.on_sysex7  = on_sysex7;
  ci_responder_init();

  rx_queue = xQueueCreateStatic(PIPE_QUEUE_DEPTH, sizeof(ump_msg_t), s_rxq_store, &s_rxq_ctrl);
  tx_queue = xQueueCreateStatic(PIPE_QUEUE_DEPTH, sizeof(ump_msg_t), s_txq_store, &s_txq_ctrl);

  xTaskCreateStatic(usb_task,  "usb",  USB_TASK_STACK,  NULL, USB_TASK_PRIO,  s_usb_stack,  &s_usb_tcb);
  xTaskCreateStatic(midi_task, "midi", MIDI_TASK_STACK, NULL, MIDI_TASK_PRIO, s_midi_stack, &s_midi_tcb);
}
