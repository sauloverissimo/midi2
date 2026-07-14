/* Board identity for the Arduino recipe.
 *
 * Only the UMP Endpoint Name lives here (consumed by stream_responder.c).
 * The USB descriptor strings (Manufacturer / Product) come from the Arduino
 * core build properties (USB_MANUFACTURER / USB_PRODUCT), set in
 * boards.local.txt, not from a LUFA-style wide string. The user LED is driven
 * from the sketch through LED_BUILTIN, so no register map is needed here.
 *
 * All ATmega32U4 boards (Leonardo, Pro Micro, Micro) share this one name:
 * they are the same silicon and enumerate identically. The physical board is
 * distinguished by its photos and README, not on the wire.
 */
#ifndef BOARD_H
#define BOARD_H

#define BOARD_NAME  "ATmega32U4 MIDI 2.0"

#endif
