/*
states: unlocked | locking | locked | sleep

unlocked -> locking when lock key pressed (*)
locking -> locked when no block detected
locking -> unlock when block detected
locked -> check passcode: correct -> unlock, incorrect -> locked
sleep after 3s timeout, wake on keypress
*/

#include "fsm.h"
#include "pwm.h"
#include "keypad.h"
#include "locker.h"

typedef enum {
    STATE_LOCKED,
    STATE_UNLOCKED,
    STATE_LOCKING,
    STATE_SLEEP
} state_t;

static state_t state = STATE_LOCKED;
static state_t prev_state;  // used when waking from sleep

static unsigned long locking_start;     // used for checking for block
static unsigned long last_activity;     // for the 3 s sleep timeout

static void handle_locked(key_t key, unsigned long now) {
    if (key == KEY_NONE) {
        return;
    }

    locker_add_digit(key);
    switch (locker_check()) {
        case LOCKER_MATCH:
            setPosUnlock();
            state = STATE_UNLOCKED;
            break;
        case LOCKER_WRONG:
            /* stay locked; buffer already reset for next try */
            break;
        default: /* LOCKER_IN_PROGRESS */ break;
    }
}
static void handle_unlocked(key_t key, unsigned long now) {
    if (key == KEY_STAR) {
        setPosLock();
        locking_start = now;
        state = STATE_LOCKING;
    }
}
static void handle_locking(key_t key, unsigned long now) {
    /*
     if (now - locking_start >= LOCK_TIMEOUT) {      // ~500-1000 ms for the servo
           if (block_detected()) {
               setPosUnlock();       // door is blocked: abort and re-open
               state = STATE_UNLOCKED;
           } else {
               state = STATE_LOCKED;
           }
       }
    */
    state = STATE_LOCKED;   // placeholder
}
static void handle_sleep(key_t key, unsigned long now) {
    if (key != KEY_NONE) {
        state = prev_state;
    }
}

void fsm_init(void) {
    pwm_init();
    adc_init();
    setPosLock();     /* physically assert the locked position: the FSM
                         starts in STATE_LOCKED, and an uncommanded servo
                         holds no torque at all */
    last_activity = milliseconds();
}

void fsm_tick(void) {
    tick_update();
    key_t key = keypad_read();          // KEY_NONE if no key
    unsigned long now = milliseconds();

    // global timeout rule: 3 s of no input -> sleep
    if (key != KEY_NONE) {
        last_activity = now;
    } else if (state != STATE_SLEEP && now - last_activity >= 3000UL) {
        prev_state = state;
        state = STATE_SLEEP;
    }

    switch (state) {
        case STATE_LOCKED:   handle_locked(key, now);   break;
        case STATE_UNLOCKED: handle_unlocked(key, now); break;
        case STATE_LOCKING:  handle_locking(key, now);  break;
        case STATE_SLEEP:    handle_sleep(key, now);    break;
    }
}
