#ifndef power_h
#define power_h

/* Power-down sleep. The ONLY wake source is a pushbutton on D2 (PD2,
 * pin-change interrupt PCINT18); the keypad does not wake the chip.
 * Direct register access only -- no <avr/sleep.h> or <avr/interrupt.h>.
 *
 * While asleep: servo signal disconnected and its supply switched off,
 * ADC off, Timer0 stopped, unused peripheral clocks gated.
 *
 * Wake button: pushbutton from D2 to GND, internal pull-up enabled.
 *
 * Servo supply switch: LOW-SIDE logic-level N-channel (TP0606, TO-220:
 * G / D / S) in the servo's ground return:
 *
 *   servo GND --> D (drain)
 *                 G (gate) <-- 1k <-- D4 (PD4)
 *                 |              + 10k gate-to-GND (fail-safe, holds it
 *                 |                off while the MCU is unpowered)
 *                 S (source) --> board GND
 *
 * POLARITY IS INVERTED versus a P-channel high-side switch:
 *   D4 HIGH -> gate above threshold -> servo supplied
 *   D4 LOW  -> switch off           -> servo dead
 *
 * THE GROUND IS NOISY, and that is the accepted cost of this variant.
 * The servo's ground no longer sits at board ground: it rides up by
 * I_servo * Rds(on) on every current spike -- small at normal current,
 * larger at stall. The servo signal is referenced to board ground while
 * the servo's own return is not, so a heavy spike can disturb the signal.
 * If the servo ever twitches or drops out under a stall, suspect this
 * first; the fix is a P-channel high-side switch on +5 V, which needs only
 * the two branches in power_servo_supply() swapped, plus the wiring. */

#define POWER_WAKE_PIN            PD2   /* D2, PCINT18 (PORTD bit, regs.h) */

#define POWER_SERVO_CUTOFF_ENABLE 1
#define POWER_SERVO_CUTOFF_PIN    PD4   /* D4 */

/* Whole 5 V load cutoff, same polarity as the servo switch. Leave at 0
 * while the MCU itself is on the switched rail, or it can't wake itself. */
#define POWER_LOAD_CUTOFF_ENABLE  0
#define POWER_LOAD_CUTOFF_PIN     PD5   /* D5 */

/* Brown-out detector off during sleep. The clean route is the extended
 * fuse (efuse 0xFF) rather than this. */
#define POWER_BOD_DISABLE_ON_SLEEP 0

#ifdef __cplusplus
extern "C" {
#endif

/* Configure the wake button and the servo supply switch (servo ON).
 * Call once at boot. */
void power_init(void);

/* Shut everything down and enter power-down mode. Returns once the wake
 * button is pressed, with the servo signal, its supply, the ADC and the
 * clocks restored. The millisecond clock pauses while asleep. */
void power_sleep(void);

/* Servo supply switch: on=1 closes the switch and supplies the servo,
 * on=0 opens it. */
void power_servo_supply(unsigned char on);

/* Whole-load switch for the current-draw measurement. A no-op unless
 * POWER_LOAD_CUTOFF_ENABLE is set. */
void power_load_supply(unsigned char on);

#ifdef __cplusplus
}
#endif

#endif
