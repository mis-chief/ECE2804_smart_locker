#ifndef fsm_h
#define fsm_h

#ifdef __cplusplus
extern "C" {
#endif

/* One-time bring-up: initializes PWM + ADC, physically asserts the
 * locked position, and starts the sleep-timeout clock.
 * Call exactly once (from setup()). */
void fsm_init(void);

/* Runs one iteration of the main loop: services the millisecond tick,
 * polls the keypad, and advances the state machine.
 * Call repeatedly (from loop()). */
void fsm_tick(void);

#ifdef __cplusplus
}
#endif

#endif
