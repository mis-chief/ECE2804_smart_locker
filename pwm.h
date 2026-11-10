#ifndef pwm_h
#define pwm_h

/* Timer1 fast PWM at 50 Hz on OC1A (PB1 / D9) for the servo, plus a
 * millisecond clock counted from the same timer's overflows. */

#ifdef __cplusplus
extern "C" {
#endif

/* Milliseconds since boot, NOT including time spent asleep. Timer1 is
 * gated during power-down, so this counter stops while the chip is down;
 * it is not a wall clock. Do not compare two readings taken across a
 * sleep -- the FSM re-stamps its timers on wake (see fsm.c). */
unsigned long milliseconds(void);
void tick_update(void);
void pwm_init(void);
void setPosUnlock(void);
void setPosLock(void);

#ifdef __cplusplus
}
#endif

#endif
