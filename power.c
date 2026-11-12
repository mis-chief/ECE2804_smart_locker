#include "power.h"
#include "regs.h"

/* PCINT2 (port D) is interrupt vector 5. The handler is deliberately
 * empty: the interrupt exists only to wake the CPU. It must be defined,
 * because an enabled interrupt with no handler jumps to the reset vector.
 * Declared by hand so <avr/interrupt.h> isn't needed.
 *
 * __vector_5 is reserved for this: any future port D pin-change use has
 * to share this handler rather than define a second one. */
void __vector_5(void) __attribute__((signal, used, externally_visible));
void __vector_5(void) { }

void power_init(void)
{
    /* Wake button as an input with the internal pull-up. Pressing it pulls
     * the pin to GND, a clean edge the pin-change logic always sees. */
    DDRD   &= ~(1 << POWER_WAKE_PIN);
    PORTD  |=  (1 << POWER_WAKE_PIN);
    PCMSK2 |=  (1 << PCINT18);

    PCIFR = (1 << PCIF2);            /* clear any wake flag left from boot */

#if POWER_SERVO_CUTOFF_ENABLE
    /* D4 drives the 2N3904 base of the level shifter.
     * LOW = 2N3904 off = gate pulled to 9 V by the divider = IRFZ44N ON.
     * So boot with D4 LOW: the FSM commands the lock position right after
     * this and the servo needs its supply. */
    DDRD   |=  (1 << POWER_SERVO_CUTOFF_PIN);
    PORTD  &= ~(1 << POWER_SERVO_CUTOFF_PIN);
#endif
#if POWER_LOAD_CUTOFF_ENABLE
    DDRD   |=  (1 << POWER_LOAD_CUTOFF_PIN);
    PORTD  |=  (1 << POWER_LOAD_CUTOFF_PIN);
#endif
}



void power_servo_supply(unsigned char on)
{
#if POWER_SERVO_CUTOFF_ENABLE
    if (on) {
        PORTD &= ~(1 << POWER_SERVO_CUTOFF_PIN);   /* D4 low  -> servo on  */
    } else {
        PORTD |=  (1 << POWER_SERVO_CUTOFF_PIN);   /* D4 high -> servo off */
    }
#else
    (void)on;
#endif
}

void power_load_supply(unsigned char on)
{
#if POWER_LOAD_CUTOFF_ENABLE
    if (on) {
        PORTD |=  (1 << POWER_LOAD_CUTOFF_PIN);
    } else {
        PORTD &= ~(1 << POWER_LOAD_CUTOFF_PIN);
    }
#else
    (void)on;
#endif
}

static void servo_signal_off(void)
{
    /* Let any in-flight servo pulse finish before disconnecting the pin.
     * TOV1 is set once per 20 ms servo period, so this waits at most one
     * period. */
    TIFR1 = (1 << TOV1);
    while (!(TIFR1 & (1 << TOV1))) { }
    TIFR1 = (1 << TOV1);

    TCCR1A &= ~(1 << COM1A1);   /* OC1A no longer drives PB1 */
    PORTB  &= ~(1 << PB1);      /* hold the signal low */
}

void power_sleep(void)
{
    unsigned char saved_tccr0b = TCCR0B;
    unsigned char saved_timsk0 = TIMSK0;
    unsigned char saved_prr    = PRR;

    /* 1. Servo: stop commanding it, then open its switch. */
    servo_signal_off();
    power_servo_supply(0);

    /* 2. ADC off. */
    ADCSRA &= ~(1 << ADEN);

    /* 3. Timer0 (Arduino core's millis()) would wake the CPU about every
     *    millisecond, so stop its clock and mask its interrupt. */
    TCCR0B = 0;
    TIMSK0 = 0;

    /* 4. Gate the clocks we aren't using while asleep. */
    PRR |= (1 << PRUSART0) | (1 << PRSPI) | (1 << PRTIM1);

    /* 5. Arm the button, clear any stale flag, enter power-down. sei runs
     *    one more instruction before an interrupt is taken, so a press in
     *    that window still wakes the chip. */
    PCIFR  = (1 << PCIF2);
    PCICR |= (1 << PCIE2);
    SMCR   = (1 << SM1) | (1 << SE);   /* power-down, sleep enabled */

#if POWER_BOD_DISABLE_ON_SLEEP
    __asm__ __volatile__ (
        "cli"               "\n\t"
        "in  r24, %[mcucr]" "\n\t"
        "ori r24, 0x60"     "\n\t"   /* BODS + BODSE              */
        "out %[mcucr], r24" "\n\t"
        "andi r24, 0xDF"    "\n\t"   /* BODS only, BODSE cleared  */
        "out %[mcucr], r24" "\n\t"
        "sei"               "\n\t"
        "sleep"             "\n\t"
        :
        : [mcucr] "M" (MCUCR_IO)
        : "r24", "memory"
    );
#else
    __asm__ __volatile__ ("sei" "\n\t" "sleep" ::: "memory");
#endif

    /* ---- awake ---- */

    SMCR   = 0;                        /* SE clear */
    PCICR &= ~(1 << PCIE2);            /* button only wakes while asleep */

    PRR    = saved_prr;
    TCCR0B = saved_tccr0b;
    TIMSK0 = saved_timsk0;

    ADCSRA |= (1 << ADEN);

    /* 6. Hand the pin back to Timer1, then close the servo switch, so the
     *    servo sees a valid signal as it powers up. */
    TCCR1A |= (1 << COM1A1);
    power_servo_supply(1);
}
