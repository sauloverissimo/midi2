/* TinyUSB configuration for rp2350-device-freertos.
 * USB MIDI 2.0 device only (no CDC). Upstream TinyUSB (tud_midi2_*), no fork. */
#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/* CFG_TUSB_MCU is provided by the Pico SDK build; do not define it here. */
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#define BOARD_TUD_RHPORT      0
#define BOARD_TUD_MAX_SPEED   OPT_MODE_DEFAULT_SPEED

/* Run TinyUSB under the FreeRTOS OSAL: tud_task blocks on the USB event queue.
 * The Pico SDK already sets this to OPT_OS_FREERTOS when FreeRTOS-Kernel is
 * linked, so guard to avoid a redefinition warning. */
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_FREERTOS
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

#define CFG_TUD_ENABLED       1
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif
#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN    __attribute__((aligned(4)))
#endif
#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE  64
#endif

/* USB MIDI 2.0 device class, single bidirectional Function Block, one Group.
 * The host-shown Endpoint Name comes from CFG_TUD_MIDI2_EP_NAME: the upstream
 * built-in Stream Discovery responder returns it (default "TinyUSB MIDI 2.0"). */
#define CFG_TUD_MIDI2                    1
#define CFG_TUD_MIDI2_NUM_GROUPS         1
#define CFG_TUD_MIDI2_EP_NAME            "RP2350 FreeRTOS Bench"

/* UMP endpoint FIFO sizes. Chosen jointly with the FreeRTOS queue depth
 * (pipeline.c PIPE_QUEUE_DEPTH) as the zero-loss burst budget. */
#define CFG_TUD_MIDI2_RX_BUFSIZE         256
#define CFG_TUD_MIDI2_TX_BUFSIZE         256

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H_ */
