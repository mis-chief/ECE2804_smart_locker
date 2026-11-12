#ifndef fsm_h
#define fsm_h

#include "keypad.h"   /* key_t */

/* ms to let the servo finish its move before the block check. An SG90
 * takes ~0.3 s for the full lock/unlock swing; 600 ms leaves margin and
 * still answers well inside the spec's 1 s blocked-motor deadline. */
#define LOCK_TIMEOUT      600UL
#define SLEEP_TIMEOUT_MS  15000UL   /* ms of no key presses before power-down */

#ifdef __cplusplus
extern "C" {
#endif

/* What just happened, reported through fsm_on_event(). */
typedef enum {
    FSM_EVT_BOOT,              /* boot finished, system is locked */
    FSM_EVT_SLEEP_ENTERING,    /* about to power down: flush output here */
    FSM_EVT_WAKE,              /* wake button pressed, back awake */
    FSM_EVT_LOCKED,            /* servo finished moving to locked */
    FSM_EVT_BLOCKED_REVERT,    /* motor blocked: went back to unlocked */
    FSM_EVT_REPROGRAM_START,   /* entering new-code mode */
    FSM_EVT_CODE_SAVED,        /* new code written to EEPROM */
    FSM_EVT_REPROGRAM_CANCEL,  /* new-code entry abandoned */
    FSM_EVT_ENTRY_INCOMPLETE   /* '#' with fewer than PIN_LEN digits */
} fsm_event_t;

/* One-time bring-up: PWM, ADC, current sense, battery monitor, saved code
 * from EEPROM, wake button, servo supply switch; asserts the locked
 * position and starts the sleep-timeout clock.
 * Call exactly once (from setup()). */
void fsm_init(void);

/* Runs one iteration of the main loop: services the millisecond tick
 * and advances the state machine.
 * `key` is the debounced keypad event for this iteration (KEY_NONE if
 * no key). The caller owns the single keypad_read() poll per loop so
 * that edge-triggered events are not consumed twice.
 * Call repeatedly (from loop()). */
void fsm_tick(key_t key);

/* ---- user-feedback hooks ----------------------------------------------
 * The FSM calls these as things happen; the implementations live with the
 * sketch (servo_locker.ino) so the C modules stay free of serial code. */

void fsm_on_event(fsm_event_t event);

/* A digit key was accepted, in either the lock attempt or the new code.
 * digit is 0..9. */
void fsm_on_digit(unsigned char digit);

/* A full code was submitted: ok = 1 correct, 0 wrong. */
void fsm_on_password_result(unsigned char ok);

/* A new code was just saved (called after FSM_EVT_CODE_SAVED, before the
 * reprogram buffer is wiped). `code` holds PIN_LEN digit keys
 * (KEY_0..KEY_9). For debug display only; do not keep the pointer. */
void fsm_on_code_saved(const key_t *code);

#ifdef __cplusplus
}
#endif

#endif
