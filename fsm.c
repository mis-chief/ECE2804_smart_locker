/*
states: locked | unlocked | locking | reprogram | sleep

locked    -> unlocked when the correct passcode + '#' is entered
             (wrong code: stay locked)
unlocked  -> locking when lock key pressed (*)
unlocked  -> reprogram when '#' pressed
locking   -> locked when no block detected (after LOCK_TIMEOUT)
locking   -> unlocked when block detected
reprogram -> unlocked when 4 digits + '#' (new code saved to EEPROM)
             or '*' (cancelled, old code kept)
any state -> sleep (power-down) after SLEEP_TIMEOUT_MS with no key,
             except while locking
sleep     -> the state it slept from, ONLY when the wake button (D2) is
             pressed; the keypad does not wake the chip

milliseconds() counts Timer1 overflows and Timer1 is gated during
power-down, so the clock simply pauses while asleep. That is why the
timers are re-stamped on wake.

Every state change calls a status hook (fsm.h) so the user can see what
the system just did. The hooks live with the sketch, so this file has no
serial or LED code of its own.
*/

#include "fsm.h"
#include "pwm.h"
#include "keypad.h"
#include "locker.h"
#include "current.h"
#include "battery.h"
#include "power.h"

typedef enum {
    STATE_LOCKED,
    STATE_UNLOCKED,
    STATE_LOCKING,
    STATE_REPROGRAM,
    STATE_SLEEP
} state_t;

static state_t state = STATE_LOCKED;
static state_t prev_state;           /* used when waking from sleep */

static unsigned long locking_start;  /* used for checking for block */
static unsigned long last_activity;  /* for the sleep timeout */

static key_t new_code[PIN_LEN];      /* reprogram buffer */
static unsigned int new_len = 0;

/* Empty the reprogram buffer. Zeroes the digits too: once saved, only
 * the salted hash should exist, not a plain-text copy in RAM. */
static void clear_new_code(void)
{
    unsigned int i;

    for (i = 0; i < PIN_LEN; i++) {
        new_code[i] = KEY_NONE;
    }
    new_len = 0;
}

static void handle_locked(key_t key, unsigned long now)
{
    (void)now;
    if (key == KEY_NONE) {
        return;
    }
    if (key == KEY_POUND) {
        switch (locker_check()) {
            case LOCKER_MATCH:
                fsm_on_password_result(1);
                setPosUnlock();
                state = STATE_UNLOCKED;
                break;
            case LOCKER_WRONG:
                fsm_on_password_result(0);
                /* stay locked; buffer already reset for next try */
                break;
            default:
                /* LOCKER_IN_PROGRESS: '#' pressed with fewer than 4 digits.
                   Discard the partial entry so leftover digits can't poison
                   the next attempt. */
                locker_reset();
                fsm_on_event(FSM_EVT_ENTRY_INCOMPLETE);
                break;
        }
    }
    else if (key >= KEY_0 && key <= KEY_9) {
        locker_add_digit(key);
        fsm_on_digit((unsigned char)(key - KEY_0));
    }
    /* other keys (KEY_STAR) are meaningless while locked: ignore */
}

static void handle_unlocked(key_t key, unsigned long now)
{
    if (key == KEY_STAR) {
        setPosLock();
        locking_start = now;
        state = STATE_LOCKING;
    }
    else if (key == KEY_POUND) {
        new_len = 0;
        state = STATE_REPROGRAM;
        fsm_on_event(FSM_EVT_REPROGRAM_START);
    }
}

static void handle_locking(key_t key, unsigned long now)
{
    (void)key;
    if (now - locking_start >= LOCK_TIMEOUT) {
        if (block_detected()) {
            setPosUnlock();          /* door is blocked: abort and re-open */
            state = STATE_UNLOCKED;
            fsm_on_event(FSM_EVT_BLOCKED_REVERT);
        } else {
            state = STATE_LOCKED;
            fsm_on_event(FSM_EVT_LOCKED);
        }
    }
}

static void handle_reprogram(key_t key, unsigned long now)
{
    (void)now;
    if (key == KEY_NONE) {
        return;
    }
    if (key >= KEY_0 && key <= KEY_9) {
        if (new_len == PIN_LEN) {
            new_len = 0;             /* 5th digit starts the entry over */
        }
        new_code[new_len++] = key;
        fsm_on_digit((unsigned char)(key - KEY_0));
    }
    else if (key == KEY_POUND) {
        if (new_len == PIN_LEN) {
            locker_set_code(new_code);   /* persists through power loss */
            state = STATE_UNLOCKED;
            fsm_on_event(FSM_EVT_CODE_SAVED);
            fsm_on_code_saved(new_code); /* debug display, before the wipe */
            clear_new_code();
        } else {
            clear_new_code();        /* incomplete: clear and let them retype */
            fsm_on_event(FSM_EVT_ENTRY_INCOMPLETE);
        }
    }
    else if (key == KEY_STAR) {
        clear_new_code();            /* cancel: keep the old code */
        state = STATE_UNLOCKED;
        fsm_on_event(FSM_EVT_REPROGRAM_CANCEL);
    }
}

static void handle_sleep(void)
{
    unsigned long now;

    power_sleep();               /* blocks until the wake button is pressed */

    /* First conversion after re-enabling the ADC is a throwaway. */
    (void)adc_read_channel(KEYPAD_CHANNEL);

    /* The button is the only wake source, so returning from power_sleep()
     * IS the wake. Go straight back to the previous state and start a full
     * timeout window. */
    now = milliseconds();
    state = prev_state;
    last_activity = now;
    battery_alert_reset(now);    /* re-check the battery on every wake */
    fsm_on_event(FSM_EVT_WAKE);
}

void fsm_init(void)
{
    pwm_init();
    adc_init();
    current_init();
    battery_init();
    locker_init();               /* load saved code from EEPROM */
    power_init();                /* wake button on D2, servo switch on D4 */
    setPosLock(); /* physically assert the locked position: the FSM
                     starts in STATE_LOCKED, and an uncommanded servo
                     holds no torque at all */
    last_activity = milliseconds();
    fsm_on_event(FSM_EVT_BOOT);
}

void fsm_tick(key_t key)
{
    tick_update();
    unsigned long now = milliseconds();
    battery_alert_update(now);   /* low-battery check + LED flash */

    /* global timeout rule: SLEEP_TIMEOUT_MS of no input -> sleep.
       Never mid-lock: the block check still has to run. */
    if (key != KEY_NONE) {
        last_activity = now;
    } else if (state != STATE_SLEEP && state != STATE_LOCKING
               && now - last_activity >= SLEEP_TIMEOUT_MS) {
        prev_state = (state == STATE_REPROGRAM) ? STATE_UNLOCKED : state;
        clear_new_code();
        locker_reset();          /* a half-typed code must not survive the nap */
        battery_alert_reset(now);   /* LED off: power-down would hold it lit */
        state = STATE_SLEEP;
        /* the sketch flushes serial in this hook, before the USART is gated */
        fsm_on_event(FSM_EVT_SLEEP_ENTERING);
    }

    switch (state) {
        case STATE_LOCKED:    handle_locked(key, now);    break;
        case STATE_UNLOCKED:  handle_unlocked(key, now);  break;
        case STATE_LOCKING:   handle_locking(key, now);   break;
        case STATE_REPROGRAM: handle_reprogram(key, now); break;
        case STATE_SLEEP:     handle_sleep();             break;
    }
}
