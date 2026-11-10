#ifndef ui_h
#define ui_h

#include "fsm.h"   /* fsm_event_t */

/* State LEDs and a passive piezo buzzer.
 *
 * Hardware: three LEDs, each through 1 k to GND; a passive piezo on OC2B,
 * driven by Timer2 in CTC mode. Steady LED shows the state (red locked,
 * green unlocked, yellow setting a new code); flashes and tones confirm
 * key presses and events.
 *
 * Driven entirely from servo_locker.ino through the FSM hooks, so fsm.c
 * has no LED or buzzer code. Nothing here blocks: sequences advance in
 * ui_tick(). */

#define UI_LED_RED_PIN    PD6   /* D6 (PORTD bit, regs.h) */
#define UI_LED_YELLOW_PIN PD7   /* D7 (PORTD bit) */
#define UI_LED_GREEN_PIN  PB0   /* D8 (PORTB bit) */
#define UI_BUZZER_PIN     PD3   /* D3 = OC2B (PORTD bit) */

#define UI_BLINK_MS 120UL       /* on/off half-period of an LED flash */

#ifdef __cplusplus
extern "C" {
#endif

/* Configure the LED pins and the piezo. Call BEFORE fsm_init(), because
 * fsm_init() raises FSM_EVT_BOOT, which this module displays. */
void ui_init(void);

/* Advance the sound and blink sequences. Call once per loop pass, after
 * fsm_tick(), so the two stay in step. */
void ui_tick(void);

void ui_event(fsm_event_t event);

/* A digit key was accepted. */
void ui_digit(void);

/* A full code was submitted: ok = 1 correct, 0 wrong. */
void ui_result(unsigned char ok);

#ifdef __cplusplus
}
#endif

#endif
