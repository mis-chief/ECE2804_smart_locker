# Servo Locker — Arduino Uno sketch

Same firmware as the bare-metal C project in the repo root, packaged as
an Arduino IDE sketch. The implementation is unchanged: all hardware
access is direct ATmega328P register manipulation (ADC polling on the
keypad's resistor ladder, Timer1 fast PWM at 50 Hz for the servo). No
Arduino/Servo library calls are used — the modules are plain C and never
see `Arduino.h`; only the thin `.ino` wrapper does.

## Files

| File            | Purpose |
|-----------------|---------|
| `servo_locker.ino` | Sketch entry point: `setup()` → `fsm_init()`, `loop()` → `fsm_tick()` |
| `fsm.c/.h`      | State machine (was `main.c`); split into init + per-iteration tick |
| `keypad.c/.h`   | ADC + debounced keypad decoding (resistor ladder on ADC0) |
| `pwm.c/.h`      | Timer1 50 Hz servo PWM on OC1A + millisecond tick clock |
| `locker.c/.h`   | 4-digit passcode check |

## Flashing

1. Open `servo_locker/servo_locker.ino` in the Arduino IDE (or `arduino
   --verify servo_locker/servo_locker.ino` from the CLI). The IDE
   automatically compiles and links the `.c` files in the sketch folder.
2. Tools → Board → **Arduino Uno**.
3. Connect the board, pick its serial port, and click **Upload**.

## Wiring

| Signal                     | Uno pin | MCU pin |
|----------------------------|---------|---------|
| Keypad resistor-ladder out | A0      | PC0 / ADC0 (`KEYPAD_CHANNEL` in `keypad.h`) |
| Servo signal               | D9      | PB1 / OC1A |

Default code is `1 2 3 4` (`locker.c`).
