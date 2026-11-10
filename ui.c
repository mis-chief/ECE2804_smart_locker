#include "ui.h"
#include "pwm.h"      /* milliseconds() */
#include "regs.h"

/* Which LEDs are lit, as a mask, independent of which port each is on */
#define LED_R  0x01
#define LED_Y  0x02
#define LED_G  0x04

/* Timer2 runs at 16 MHz / 64 = 250 kHz. OC2B toggles once per period, so
 * the output frequency is 250 kHz / (2 * (OCR2A + 1)). */
#define TONE_HI   61   /* ~2.0 kHz */
#define TONE_MID  124  /* ~1.0 kHz */
#define TONE_LO   249  /* ~500 Hz  */
#define TONE_REST 0    /* silence  */

/* One step of a sound sequence: play `ocr` for `ms`, then move on. */
typedef struct {
    unsigned char ocr;
    unsigned int  ms;
} note_t;

static const note_t S_KEY[]      = { {TONE_HI,  40} };
static const note_t S_OK[]       = { {TONE_MID, 80}, {TONE_HI,  120} };
static const note_t S_WRONG[]    = { {TONE_LO,  400} };
static const note_t S_SHORT_LO[] = { {TONE_LO,  160} };
static const note_t S_LOCK[]     = { {TONE_HI,  80}, {TONE_MID, 120} };
static const note_t S_SAVED[]    = { {TONE_HI,  80}, {TONE_REST, 60}, {TONE_HI, 80} };
static const note_t S_BLOCKED[]  = { {TONE_LO,  160}, {TONE_REST, 100}, {TONE_LO, 160},
                                     {TONE_REST, 100}, {TONE_LO, 160} };
static const note_t S_WAKE[]     = { {TONE_MID, 60} };

/* sequence player */
static const note_t *seq;            /* 0 when idle */
static unsigned char seq_len;
static unsigned char seq_i;
static unsigned long seq_t;

/* LED state */
static unsigned char steady;         /* LED mask shown when not flashing */
static unsigned char flash_mask;
static unsigned char flash_left;     /* half-periods remaining */
static unsigned long flash_t;

static void leds(unsigned char m)
{
    if (m & LED_R) { PORTD |= (1 << UI_LED_RED_PIN);    } else { PORTD &= ~(1 << UI_LED_RED_PIN); }
    if (m & LED_Y) { PORTD |= (1 << UI_LED_YELLOW_PIN); } else { PORTD &= ~(1 << UI_LED_YELLOW_PIN); }
    if (m & LED_G) { PORTB |= (1 << UI_LED_GREEN_PIN);  } else { PORTB &= ~(1 << UI_LED_GREEN_PIN); }
}

static void set_steady(unsigned char m)
{
    steady = m;
    flash_left = 0;
    leds(m);
}

static void flash(unsigned char m, unsigned char times)
{
    flash_mask = m;
    flash_left = (unsigned char)(times * 2);   /* on/off per flash */
    flash_t = milliseconds();
    leds(m);
}

static void tone_on(unsigned char ocr)
{
    PRR   &= ~(1 << PRTIM2);            /* Timer2 clock back on */
    TCCR2A = (1 << COM2B0) | (1 << WGM21);
    OCR2A  = ocr;
    OCR2B  = 0;
    TCNT2  = 0;
    TCCR2B = (1 << CS22);               /* clk/64 */
}

static void tone_off(void)
{
    TCCR2B = 0;                         /* stop the timer */
    TCCR2A = 0;                         /* release OC2B */
    PORTD &= ~(1 << UI_BUZZER_PIN);     /* piezo at 0 V */
}

static void start_note(void)
{
    seq_t = milliseconds();
    if (seq[seq_i].ocr) {
        tone_on(seq[seq_i].ocr);
    } else {
        tone_off();
    }
}

static void play(const note_t *s, unsigned char n)
{
    seq     = s;
    seq_len = n;
    seq_i   = 0;
    start_note();
}

#define PLAY(s) play((s), (unsigned char)(sizeof(s) / sizeof((s)[0])))

void ui_init(void)
{
    DDRD |= (1 << UI_LED_RED_PIN) | (1 << UI_LED_YELLOW_PIN) | (1 << UI_BUZZER_PIN);
    DDRB |= (1 << UI_LED_GREEN_PIN);
    tone_off();
    set_steady(LED_R);        /* the FSM starts locked */
}

void ui_tick(void)
{
    unsigned long now = milliseconds();

    /* sound sequence */
    if (seq && (now - seq_t) >= seq[seq_i].ms) {
        if (++seq_i >= seq_len) {
            seq = 0;
            tone_off();
        } else {
            start_note();
        }
    }

    /* LED flash */
    if (flash_left && (now - flash_t) >= UI_BLINK_MS) {
        flash_t = now;
        if (--flash_left == 0) {
            leds(steady);
        } else {
            leds((flash_left & 1) ? 0 : flash_mask);
        }
    }
}

void ui_digit(void)
{
    flash(LED_Y, 1);
    PLAY(S_KEY);
}

void ui_result(unsigned char ok)
{
    if (ok) {
        set_steady(LED_G);
        PLAY(S_OK);
    } else {
        flash(LED_R, 3);
        PLAY(S_WRONG);
    }
}

void ui_event(fsm_event_t event)
{
    switch (event) {
        case FSM_EVT_BOOT:
            set_steady(LED_R);
            PLAY(S_WAKE);
            break;

        case FSM_EVT_SLEEP_ENTERING:
            /* All dark and silent before power_sleep() stops the CPU. */
            seq = 0;
            tone_off();
            flash_left = 0;
            leds(0);
            PRR |= (1 << PRTIM2);         /* gate Timer2's clock */
            break;

        case FSM_EVT_WAKE:
            leds(steady);
            PLAY(S_WAKE);
            break;

        case FSM_EVT_LOCKED:
            set_steady(LED_R);
            PLAY(S_LOCK);
            break;

        case FSM_EVT_BLOCKED_REVERT:
            steady = LED_G;               /* ends up back in unlocked */
            flash(LED_R, 3);
            PLAY(S_BLOCKED);
            break;

        case FSM_EVT_REPROGRAM_START:
            set_steady(LED_Y);
            PLAY(S_KEY);
            break;

        case FSM_EVT_CODE_SAVED:
            steady = LED_G;
            flash(LED_G, 2);
            PLAY(S_SAVED);
            break;

        case FSM_EVT_REPROGRAM_CANCEL:
            set_steady(LED_G);
            PLAY(S_SHORT_LO);
            break;

        case FSM_EVT_ENTRY_INCOMPLETE:
            flash(LED_Y, 2);
            PLAY(S_SHORT_LO);
            break;

        default:
            break;
    }
}
