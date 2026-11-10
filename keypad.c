#include "keypad.h"
#include "entropy.h"
#include "regs.h"

/* Configure the ADC for single-conversion (polled) mode: the ADC sits
 * idle until adc_read() explicitly starts a conversion. The caller
 * controls sampling, which also makes throttling/debouncing easy and
 * leaves the ADC free for other channels.
 *
 * - AVcc reference: the keypad is a resistor ladder, so its output is
 *   ratiometric to the supply; AVcc makes the readings immune to supply
 *   drift.
 * - Right-adjusted (ADLAR = 0): thresholds are 10-bit values, 0-1023.
 * - Prescaler /128: 16 MHz / 128 = 125 kHz, inside the 50-200 kHz range
 *   the ADC needs for full 10-bit accuracy. A conversion takes ~104 us.
 */
void adc_init(void)
{
    ADMUX  = (1 << REFS0) | KEYPAD_CHANNEL; /* reference to supply voltage
                                            since resistor ladder is tied
                                            to Vcc. Use PC0/A0 by default */

    /* no ADATE: the ADC only converts when adc_read() asks it to */
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    /* enable adc and set prescalar to /128 for a 125kHz clock */

    /* dummy conversion: the first one after enabling takes 25 cycles
     * (instead of 13) while internal circuitry initializes, and its
     * result should be discarded */
    (void)adc_read();

    /* disable the digital input buffer on A0: nothing reads it digitally
     * (wake is a button on D2), and leaving it on wastes current at the
     * ladder's mid-rail voltages. */
    DIDR0 |= (1 << ADC0D);
}

/* Start one conversion, wait for it to finish (~104 us), return the
 * 10-bit result (0-1023). */
unsigned int adc_read(void)
{
    ADCSRA |= (1 << ADSC);              /* start conversion */
    while (ADCSRA & (1 << ADSC)) { }   /* hardware clears it when done */
    return ADC;
}

/* Channel adc_init() selected; tracked so adc_read_channel() knows when
 * the multiplexer actually moves. */
static unsigned char cur_channel = KEYPAD_CHANNEL;

/* Select channel `ch` (0-7) and return one 10-bit conversion of it.
 * Every caller sharing the ADC must go through here, otherwise a read
 * could convert whichever channel the last caller left selected.
 *
 * After a channel change one conversion is thrown away: the sample-and-
 * hold capacitor still carries charge from the previous input, and the
 * keypad ladder's high source impedance can't swing it fully within one
 * sample window. Costs one extra ~104 us conversion per switch. */
unsigned int adc_read_channel(unsigned char ch)
{
    if (ch != cur_channel) {
        ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);  /* keep REFS/ADLAR, set MUX */
        cur_channel = ch;
        (void)adc_read();                      /* settle: discard */
    }
    return adc_read();
}

typedef struct {
    unsigned int lo;
    unsigned int hi;
    key_t        key;
} key_range_t;

static const key_range_t keymap[] = {
    { zero_l,  zero_h,  KEY_0 },
    { one_l,   one_h,   KEY_1 },
    { two_l,   two_h,   KEY_2 },
    { three_l, three_h, KEY_3 },
    { four_l,  four_h,  KEY_4 },
    { five_l,  five_h,  KEY_5 },
    { six_l,   six_h,   KEY_6 },
    { seven_l, seven_h, KEY_7 },
    { eight_l, eight_h, KEY_8 },
    { nine_l,  nine_h,  KEY_9 },
    { star_l,  star_h,  KEY_STAR },
    { pound_l, pound_h, KEY_POUND },
};

static key_t decode(unsigned int adc)
{
    unsigned char i;

    for (i = 0; i < sizeof keymap / sizeof keymap[0]; ++i) {
        if (adc >= keymap[i].lo && adc <= keymap[i].hi) {
            return keymap[i].key;
        }
    }
    return KEY_NONE;
}

/* Debounced key events: one event per physical press.
 *
 * Two things make a single raw reading untrustworthy:
 *   - As a key is pressed or released, the A0 voltage sweeps through other
 *     keys' ranges, so a reading taken mid-sweep can match the wrong key.
 *   - On release the contact bounces between no-key and the key.
 *
 * So a key only counts once it has read the SAME on KEYPAD_STABLE_READS
 * consecutive polls (a mid-sweep reading can't get through), and after a
 * press the pad must read idle for KEYPAD_IDLE_READS polls before another
 * press can count (bounce can't produce a double). */
static key_t        cand = KEY_NONE;              /* key being confirmed */
static unsigned int cand_count = 0;
static unsigned int idle_count = KEYPAD_IDLE_READS;
static unsigned char armed = 1;                   /* ready for a new press */

key_t keypad_read(void)
{
    key_t raw = decode(adc_read_channel(KEYPAD_CHANNEL));

    if (raw == KEY_NONE) {
        if (idle_count < KEYPAD_IDLE_READS) {
            idle_count++;
        }
        if (idle_count >= KEYPAD_IDLE_READS) {
            armed = 1;                 /* fully released: ready again */
        }
        cand = KEY_NONE;
        cand_count = 0;
        return KEY_NONE;
    }

    idle_count = 0;                    /* any key resets the idle count */
    if (!armed) {
        return KEY_NONE;               /* still the same press */
    }

    if (raw != cand) {                 /* value still moving: restart */
        cand = raw;
        cand_count = 1;
        return KEY_NONE;
    }

    if (++cand_count < KEYPAD_STABLE_READS) {
        return KEY_NONE;
    }

    armed = 0;                         /* one event per press */
    entropy_add(TCNT1);                /* press timing -> salt randomness */
    return raw;
}
