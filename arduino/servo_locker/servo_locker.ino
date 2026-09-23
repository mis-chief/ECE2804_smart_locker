/* Arduino Uno sketch: servo-actuated keypad lock.
 *
 * The Arduino core supplies main(), which calls setup() once and then
 * calls loop() forever. All real work lives in the C modules (fsm.c,
 * pwm.c, keypad.c, locker.c) next to this sketch; this file is only the
 * glue. Those modules talk to the hardware through direct ATmega328P
 * register access (see their headers) -- no Arduino/Servo libraries are
 * used, by design.
 *
 * Wiring:
 *   keypad (resistor ladder) -> A0  (ADC0 / PC0, see KEYPAD_CHANNEL)
 *   servo signal             -> D9  (OC1A / PB1)
 */

#include "fsm.h"

void setup() {
    fsm_init();
}

void loop() {
    fsm_tick();
}
