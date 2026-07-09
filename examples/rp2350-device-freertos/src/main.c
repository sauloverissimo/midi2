/* rp2350-device-freertos: USB MIDI 2.0 device on the Raspberry Pi Pico 2,
 * pure C99 over FreeRTOS-Kernel + TinyUSB upstream.
 *
 * main() brings up the board and USB, creates the pipeline (two static tasks
 * plus two static queues), and starts the scheduler. All MIDI logic lives in
 * pipeline.c (concurrency) and catalog.c (the enumerated UMP catalog). */
#include "bsp/board_api.h"
#include "tusb.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pipeline.h"

int main(void) {
  board_init();

  /* TinyUSB device stack on the configured root port. */
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO,
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);
  board_init_after_tusb();

  pipeline_start();
  vTaskStartScheduler();

  /* Not reached. */
  for (;;) { }
  return 0;
}
