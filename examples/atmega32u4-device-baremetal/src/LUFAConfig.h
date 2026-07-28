/* LUFA compile-time configuration: USB device only, full speed, descriptors
 * served from flash, EP0 size 64.
 */
#ifndef LUFA_CONFIG_H
#define LUFA_CONFIG_H

#define USB_DEVICE_ONLY
#define USE_STATIC_OPTIONS (USB_DEVICE_OPT_FULLSPEED | USB_OPT_REG_ENABLED | USB_OPT_AUTO_PLL)
#define USE_FLASH_DESCRIPTORS
#define FIXED_CONTROL_ENDPOINT_SIZE  64
#define FIXED_NUM_CONFIGURATIONS    1

#endif
