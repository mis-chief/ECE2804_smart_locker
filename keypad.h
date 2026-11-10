#ifndef keypad_h
#define keypad_h

/* Keypad on a resistor ladder: each key puts a different voltage on A0,
 * read by the ADC. This module also owns the ADC itself; every other
 * analog reader goes through adc_read_channel(). */

// macros for keypad input values
#define zero_l  335
#define zero_h  352
#define one_l   568
#define one_h   598
#define two_l   622
#define two_h   652
#define three_l 688
#define three_h 718
#define four_l  448
#define four_h  478
#define five_l  482
#define five_h  512
#define six_l   521
#define six_h   551
#define seven_l 374
#define seven_h 394
#define eight_l 397
#define eight_h 417
#define nine_l  423
#define nine_h  443
#define star_l  318
#define star_h  334
#define pound_l 353
#define pound_h 373

/* AVcc reference shared by every channel, in mV */
#define ADC_VREF_MV 5000UL

/* keypad hardware configuration */
#define KEYPAD_CHANNEL 0    /* ADC0 = PC0 / A0; change if keypad wired elsewhere */

/* Debounce, counted in polls (one keypad_read() per loop pass), not ms,
 * so the 20 ms clock step does not affect it. At roughly 0.1-0.15 ms per
 * poll this is about 25 ms of stable agreement and 50 ms of required
 * idle. If doubles appear, raise KEYPAD_IDLE_READS; if fast taps get
 * missed, lower KEYPAD_STABLE_READS. Anything that slows the main loop
 * (e.g. the DEBUG_* serial prints) stretches both. */
#define KEYPAD_STABLE_READS 200u   /* same key this many polls -> press */
#define KEYPAD_IDLE_READS   400u   /* no key this many polls -> released */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEY_NONE = 0,
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_STAR,
    KEY_POUND
} key_t;

void adc_init(void);
unsigned int adc_read(void);
unsigned int adc_read_channel(unsigned char ch);
key_t keypad_read(void);

#ifdef __cplusplus
}
#endif

#endif
