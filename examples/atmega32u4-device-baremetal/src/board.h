/* Board abstraction: LED mapping for the two supported ATmega32U4 boards.
 *
 * Pro Micro has no user LED on a free pin; it exposes the RX LED (PB0) and
 * TX LED (PD5), both active low. Leonardo has the D13 LED on PC7, active
 * high, plus the same RX/TX pair.
 */
#ifndef BOARD_H
#define BOARD_H

#include <avr/io.h>

#if defined(BOARD_SEL_LEONARDO)
  #define LED_INIT()  (DDRC |= _BV(7))
  #define LED_ON()    (PORTC |= _BV(7))
  #define LED_OFF()   (PORTC &= (uint8_t)~_BV(7))
  #define LED_TOGGLE()(PINC = _BV(7))
  #define BOARD_NAME  "midi2 Leonardo"
  #define BOARD_USBSTR L"midi2 Leonardo"
  #define BOARD_PIID  "LEO-40D0"
#else /* BOARD_SEL_PROMICRO (default) */
  #define LED_INIT()  (DDRB |= _BV(0))
  #define LED_ON()    (PORTB &= (uint8_t)~_BV(0))   /* active low */
  #define LED_OFF()   (PORTB |= _BV(0))
  #define LED_TOGGLE()(PINB = _BV(0))
  #define BOARD_NAME  "midi2 Pro Micro"
  #define BOARD_USBSTR L"midi2 Pro Micro"
  #define BOARD_PIID  "PM-40D0"
#endif

#endif
