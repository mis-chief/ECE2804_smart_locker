#ifndef battery_h
#define battery_h

/* 9 V battery monitoring through a resistor divider.
 *
 * Hardware: R1 (top) from battery+ to A3, R2 (bottom) from A2 to GND,
 * so V(A2) = V_batt * R2 / (R1 + R2). With 1 M / 1 M a fresh 9.6 V cell
 * puts 4.8 V on A2 -- just under the AVcc (5 V) reference shared with
 * the keypad, so there is little headroom.
 *
 * The 500 k source impedance is far above the ~10 k the ADC wants, so a
 * cap from A2 to GND (>= 100 nF) must supply the sample-and-hold charge.
 * RC = 500 k * 100 nF = 50 ms: readings settle ~250 ms after power-up.
 * Pin leakage through the 500 k also adds a small offset (typically tens
 * of mV); check against a multimeter with DEBUG_BATTERY.
 *
 * If the same 9 V powers the board through VIN, readings are only valid
 * while the regulator still holds 5 V (battery above ~7 V). Below that
 * the reference sags with the battery and the reading comes out high. */

#define BATTERY_CHANNEL    2       /* ADC2 = PC2 / A2 */
#define ADC2D              2       /* DIDR0 bit for the ADC2 digital input buffer */
#define BATTERY_SAMPLES    16      /* averaged per reading (~1.7 ms total) */
#define BATTERY_R1_KOHM    1000UL  /* divider top resistor (battery+ to A2) */
#define BATTERY_R2_KOHM    1000UL  /* divider bottom resistor (A2 to GND) */
#define BATTERY_LOW_MV     7000    /* placeholder: tune on hardware with DEBUG_BATTERY */

/* Low-battery alert LED: D13 / PB5 through 1 k to GND (on the Uno the
 * onboard "L" LED mirrors it). The battery is checked once per boot or
 * wake, BATTERY_CHECK_DELAY_MS after it starts; a low reading latches
 * the alert until the next battery_alert_reset(). milliseconds() moves
 * in 20 ms steps, so keep these times multiples of 20 ms. */
#define BATTERY_LED_PIN         PB5     /* PORTB bit (pwm.h) */
#define BATTERY_CHECK_DELAY_MS  500UL   /* wait out the servo's inrush current */
#define BATTERY_FLASH_PERIOD_MS 5000UL  /* start of one flash to the next; tune to awake time */
#define BATTERY_FLASH_ON_MS     20UL    /* LED on-time per flash */

#ifdef __cplusplus
extern "C" {
#endif

/* Call once after adc_init(). Also sets up the alert LED and starts the
 * first battery check. */
void battery_init(void);

/* Averaged 10-bit reading of A3, 0-1023. */
unsigned int battery_read_raw(void);

/* Averaged battery voltage in millivolts, divider ratio applied
 * (0-9990 with the default 1 M / 1 M divider). */
unsigned int battery_read_mv(void);

/* 1 if the battery voltage is below BATTERY_LOW_MV, else 0. */
unsigned char battery_low(void);

/* Clear the latched alert, turn the LED off, and check the battery again
 * BATTERY_CHECK_DELAY_MS after `now`. Call on every wake. */
void battery_alert_reset(unsigned long now);

/* Non-blocking: runs the pending check, then flashes the LED for
 * BATTERY_FLASH_ON_MS every BATTERY_FLASH_PERIOD_MS while latched.
 * Call every loop iteration, after tick_update(). */
void battery_alert_update(unsigned long now);

#ifdef __cplusplus
}
#endif

#endif
