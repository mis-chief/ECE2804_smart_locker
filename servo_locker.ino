/* Arduino Uno sketch: servo-actuated keypad lock.
 *
 * The Arduino core supplies main(), which calls setup() once and then
 * calls loop() forever. All real work lives in the C modules (fsm.c,
 * pwm.c, keypad.c, locker.c, current.c, battery.c, power.c, ui.c,
 * eeprom.c) next to this sketch; this file is the glue, and it also owns
 * every serial message the user sees. Those modules talk to the hardware
 * through direct ATmega328P register access (regs.h) -- no Arduino/Servo
 * libraries are used, by design.
 *
 * Wiring:
 *   keypad (resistor ladder) -> A0  (ADC0 / PC0, see KEYPAD_CHANNEL)
 *   servo current shunt      -> A1  (ADC1 / PC1, see CURRENT_CHANNEL;
 *                                    not fitted yet, detection disabled)
 *   9 V battery divider tap  -> A2  (ADC2 / PC2, see BATTERY_CHANNEL)
 *   wake button              -> D2  (PD2, to GND, internal pull-up)
 *   passive piezo buzzer     -> D3  (OC2B / PD3)
 *   servo supply switch      -> D4  (PD4, TP0606 low side, HIGH = servo on)
 *   state LEDs (1 K to GND)  -> D6 red, D7 yellow, D8 green
 *   servo signal             -> D9  (OC1A / PB1)
 *   low-battery LED (1 K)    -> D13 (PB5, see BATTERY_LED_PIN)
 */

#include "fsm.h"
#include "keypad.h"
#include "current.h"
#include "battery.h"
#include "ui.h"

/* Uncomment to print the raw keypad ADC reading twice a second, for
   checking it against the thresholds in keypad.h. */
//#define DEBUG_ADC

/* Uncomment to print the averaged shunt voltage (mV) twice a second, for
   picking BLOCK_THRESHOLD_MV in current.h. */
//#define DEBUG_CURRENT

/* Uncomment to print the averaged battery voltage (mV) twice a second, for
   checking it against a multimeter and picking BATTERY_LOW_MV in battery.h. */
//#define DEBUG_BATTERY

/* Any DEBUG_* print slows the main loop, which stretches the keypad's
   poll-count debounce (see keypad.h). Leave them off in normal use. */

/* ---- the user-facing messages ---------------------------------------- */

/* One '*' per accepted digit: the code itself never goes out over serial. */
void fsm_on_digit(unsigned char digit) {
    (void)digit;
    Serial.print('*');
    ui_digit();
}

void fsm_on_password_result(unsigned char ok) {
    ui_result(ok);
    if (ok) {
        Serial.println(F("\r\nACCESS GRANTED - unlocked. '*' locks, '#' sets a new code."));
    } else {
        Serial.println(F("\r\nACCESS DENIED - wrong passcode."));
    }
}

void fsm_on_event(fsm_event_t event) {
    ui_event(event);
    switch (event) {
        case FSM_EVT_BOOT:
            Serial.println(F("\r\n=== Personal Smart Locker ==="));
            Serial.println(F("Status: LOCKED. Enter passcode, then '#'."));
            break;
        case FSM_EVT_SLEEP_ENTERING:
            Serial.println(F("\r\nEntering power-down sleep mode..."));
            /* power_sleep() gates the USART clock next; finish sending
               first or the message is cut off mid-transmission. */
            Serial.flush();
            break;
        case FSM_EVT_WAKE:
            Serial.println(F("\r\nWoke up from sleep."));
            break;
        case FSM_EVT_LOCKED:
            Serial.println(F("\r\nLOCKED."));
            break;
        case FSM_EVT_BLOCKED_REVERT:
            Serial.println(F("\r\nBLOCKED! Motor could not turn - reverted to unlocked."));
            break;
        case FSM_EVT_REPROGRAM_START:
            Serial.println(F("\r\nSet new passcode: enter 4 digits, '#' to save, '*' to cancel."));
            break;
        case FSM_EVT_CODE_SAVED:
            Serial.println(F("\r\nNew passcode saved to EEPROM."));
            break;
        case FSM_EVT_REPROGRAM_CANCEL:
            Serial.println(F("\r\nNew passcode cancelled - old code unchanged."));
            break;
        case FSM_EVT_ENTRY_INCOMPLETE:
            Serial.println(F("\r\nIncomplete entry - cleared."));
            break;
        default:
            break;
    }
}

/* ---- setup / loop ---------------------------------------------------- */

void setup() {
    /* Serial first, so fsm_init()'s boot message has somewhere to go. */
    Serial.begin(9600);
    /* LEDs and buzzer before fsm_init(): it raises FSM_EVT_BOOT, which the
       UI displays. */
    ui_init();
    fsm_init();

    /* Boot banner: proves the MCU is running and the serial link works
       before any key is pressed. If it only shows up at 600 or 4800 baud
       the chip is on its internal oscillator (1 MHz / 8 MHz), not the
       16 MHz crystal -- see "Standalone ATmega328P" in the README. */
    Serial.println(F("servo_locker boot"));
    Serial.flush();
}

void loop() {
#ifdef DEBUG_ADC
    static unsigned long last_dump;
    if (millis() - last_dump >= 500UL) {
        last_dump = millis();
        Serial.print(F("adc="));
        Serial.println(adc_read_channel(KEYPAD_CHANNEL));
    }
#endif
#ifdef DEBUG_CURRENT
    static unsigned long last_cur;
    if (millis() - last_cur >= 500UL) {
        last_cur = millis();
        Serial.print(F("shunt_mv="));
        Serial.println(current_read_mv());
    }
#endif
#ifdef DEBUG_BATTERY
    static unsigned long last_batt;
    if (millis() - last_batt >= 500UL) {
        last_batt = millis();
        Serial.print(F("battery_mv="));
        Serial.println(battery_read_mv());
    }
#endif

    /* Poll the keypad exactly ONCE per iteration: keypad_read() is
       edge-triggered (one event per physical press), so calling it both
       here for debugging and inside fsm_tick() would swallow every
       keypress before the FSM could see it. */
    key_t key = keypad_read();

    switch (key) {
        case KEY_0:    Serial.println(F("Key Pressed: 0")); break;
        case KEY_1:    Serial.println(F("Key Pressed: 1")); break;
        case KEY_2:    Serial.println(F("Key Pressed: 2")); break;
        case KEY_3:    Serial.println(F("Key Pressed: 3")); break;
        case KEY_4:    Serial.println(F("Key Pressed: 4")); break;
        case KEY_5:    Serial.println(F("Key Pressed: 5")); break;
        case KEY_6:    Serial.println(F("Key Pressed: 6")); break;
        case KEY_7:    Serial.println(F("Key Pressed: 7")); break;
        case KEY_8:    Serial.println(F("Key Pressed: 8")); break;
        case KEY_9:    Serial.println(F("Key Pressed: 9")); break;
        case KEY_STAR: Serial.println(F("Key Pressed: *")); break;
        case KEY_POUND:Serial.println(F("Key Pressed: #")); break;
        default: break;  /* KEY_NONE */
    }

    fsm_tick(key);

    /* Advance sounds and LED flashes, after fsm_tick() so an event raised
       this pass is shown immediately. */
    ui_tick();
}
