#include "battery.h"
#include "keypad.h"   /* adc_read_channel(), ADC_VREF_MV */
#include "pwm.h"      /* milliseconds() */
#include "regs.h"

static unsigned char alert_checked;  /* this wake's reading has been taken */
static unsigned char alert_latched;  /* that reading was low */
static unsigned char led_on;
/* start of the check delay, then the time of the last LED change */
static unsigned long alert_t;

void battery_init(void)
{
    /* disable the digital input buffer on the analog pin, as for ADC0 */
    DIDR0 |= (1 << ADC2D);

    DDRB |= (1 << BATTERY_LED_PIN);   /* alert LED as output */
    battery_alert_reset(milliseconds());
}

/* Average a burst to smooth out ADC noise and load ripple on the cell. */
unsigned int battery_read_raw(void)
{
    unsigned long sum = 0;
    unsigned char i;

    for (i = 0; i < BATTERY_SAMPLES; ++i) {
        sum += adc_read_channel(BATTERY_CHANNEL);
    }
    return (unsigned int)(sum / BATTERY_SAMPLES);
}

unsigned int battery_read_mv(void)
{
    /* Two steps: 1023 * 5000 * 2000 in one go overflows 32 bits.
     * First the pin voltage (0-4995 mV), then undo the divider. */
    unsigned long pin_mv = (unsigned long)battery_read_raw() * ADC_VREF_MV / 1024;
    return (unsigned int)(pin_mv * (BATTERY_R1_KOHM + BATTERY_R2_KOHM) / BATTERY_R2_KOHM);
}

unsigned char battery_low(void)
{
    return battery_read_mv() < BATTERY_LOW_MV;
}

void battery_alert_reset(unsigned long now)
{
    alert_checked = 0;
    alert_latched = 0;
    led_on = 0;
    PORTB &= ~(1 << BATTERY_LED_PIN);
    alert_t = now;
}

/* Unsigned subtraction keeps every comparison correct across
 * milliseconds() wraparound. */
void battery_alert_update(unsigned long now)
{
    if (!alert_checked) {
        if (now - alert_t >= BATTERY_CHECK_DELAY_MS) {
            alert_checked = 1;          /* one reading per wake */
            if (battery_low()) {
                /* first flash right away: the MCU may only be up 5 s */
                alert_latched = 1;
                led_on = 1;
                PORTB |= (1 << BATTERY_LED_PIN);
                alert_t = now;
            }
        }
        return;
    }
    if (!alert_latched) {
        return;
    }

    if (led_on) {
        if (now - alert_t >= BATTERY_FLASH_ON_MS) {
            led_on = 0;
            PORTB &= ~(1 << BATTERY_LED_PIN);
            alert_t = now;
        }
    } else if (now - alert_t >= BATTERY_FLASH_PERIOD_MS - BATTERY_FLASH_ON_MS) {
        led_on = 1;
        PORTB |= (1 << BATTERY_LED_PIN);
        alert_t = now;
    }
}
