# Servo Locker — Arduino Uno sketch

Same firmware as the bare-metal C project in the repo root, packaged as
an Arduino IDE sketch. All hardware access is direct ATmega328P register
manipulation (ADC polling on the keypad's resistor ladder, Timer1 fast
PWM at 50 Hz for the servo, Timer2 tones, EEPROM, power-down sleep). No
Arduino/Servo library calls are used — the modules are plain C and never
see `Arduino.h`; only the thin `.ino` wrapper does, and it owns every
serial message.

## Files

| File            | Purpose |
|-----------------|---------|
| `servo_locker.ino` | Sketch entry point: `setup()` → `fsm_init()`, `loop()` → `fsm_tick()`; implements the FSM's user-feedback hooks (serial text) |
| `fsm.c/.h`      | State machine (was `main.c`); split into init + per-iteration tick |
| `keypad.c/.h`   | ADC + debounced keypad decoding (resistor ladder on ADC0) |
| `pwm.c/.h`      | Timer1 50 Hz servo PWM on OC1A + millisecond tick clock |
| `locker.c/.h`   | 4-digit passcode check; only a salted SHA-256 hash of the code is saved in EEPROM |
| `sha256.c/.h`   | SHA-256 (FIPS 180-4), written for this project; no includes, so it also builds on a PC for testing |
| `entropy.c/.h`  | Randomness for salts: keypress timing + ADC noise, hashed |
| `eeprom.c/.h`   | Bare-metal EEPROM read/write |
| `test/`         | Host-side tests (not built by the Arduino IDE): `bash test/run_tests.sh` |
| `current.c/.h`  | Servo current sensing (shunt on ADC1) + `block_detected()` (disabled until the shunt is fitted) |
| `battery.c/.h`  | 9 V battery voltage (divider on ADC2) + `battery_low()` + low-battery LED alert on D13 |
| `power.c/.h`    | Power-down sleep, D2 wake button, D4 servo supply switch |
| `ui.c/.h`       | State LEDs (D6/D7/D8) and piezo tones (D3) |
| `regs.h`        | ATmega328P register map. Included by `.c` files only — never from a header, so it can't collide with `<avr/io.h>` in the `.ino` |

## Using it

- **Unlock**: enter the 4-digit code, then `#`. Default code is `1 2 3 4`.
  A wrong code flashes red; `#` with fewer than 4 digits clears the entry.
- **Lock**: `*` while unlocked. If block detection is enabled and the servo
  stalls, it re-opens.
- **Change the code**: `#` while unlocked (yellow LED), 4 digits, `#` to
  save to EEPROM or `*` to cancel. The code survives power loss; a blank or
  corrupt EEPROM, or a power cut in the middle of saving, falls back to
  `1 2 3 4`.
- **Sleep**: after `SLEEP_TIMEOUT_MS` (15 s, `fsm.h`) with no key, the chip
  powers down: LEDs off, servo signal and supply cut. Only the D2 button
  wakes it; the keypad does not. It wakes into the state it slept from.

## Passcode storage

The code is never stored. Each time it is set, the firmware draws a new
16-byte random salt and saves `SHA-256(salt || code)`, with the code as 4
ASCII digits (`"1234"`). Each `#` hashes the attempt with the same salt
and compares the hashes in constant time.

| EEPROM        | Contents |
|---------------|----------|
| `0x000`       | Magic: `0xA6` = valid record. `0xA5` = old plain-text record, converted automatically on first boot (the code is kept). `0xFF` while saving. |
| `0x001–0x010` | Salt (16 bytes) |
| `0x011–0x030` | SHA-256 hash (32 bytes) |

**What this does and doesn't protect.** An EEPROM dump no longer shows the
code, and the salt stops a precomputed table working on every board. But a
4-digit code has only 10,000 values: anyone with a dump can try them all
on a PC in well under a second. To stop the dump itself, program the lock
bits after uploading (blocks reading flash and EEPROM over ISP; clearing
them needs a chip erase, which also wipes the EEPROM since EESAVE is
unprogrammed):

    avrdude -c stk500v1 -P <port> -b 19200 -p m328p -U lock:w:0x3C:m

After that the chip can only be reprogrammed after a chip erase (`-e`),
which also resets the code to `1 2 3 4`. A bootloader, if fitted, is a
separate way in and is not covered by this.

To check a board's stored record against `sha256sum` (needs `xxd`):

    avrdude -c stk500v1 -P <port> -b 19200 -p m328p -U eeprom:r:ee.bin:r
    xxd -s 0x01 -l 16 -p ee.bin                         # salt
    xxd -s 0x11 -l 32 -p ee.bin                         # stored hash
    { head -c 17 ee.bin | tail -c 16; printf 1234; } | sha256sum

The last line must print the stored hash when the code is `1234`.

## Tests

`bash test/run_tests.sh` (needs `gcc` and `sha256sum`) builds and runs, on
a PC:

- SHA-256 against the FIPS 180-4 example vectors and against `sha256sum`
  for every length from 0 to 300 bytes plus a 60,000-byte message
- the locker against a simulated EEPROM: default code, changing the code,
  converting an old plain-text record, corrupt records, and a power cut
  after every possible number of EEPROM writes during a save
- the stored hash against `sha256sum` of the salt plus `"1234"`

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
| Servo current shunt (top)  | A1      | PC1 / ADC1 (`CURRENT_CHANNEL` in `current.h`) — not fitted yet |
| 9 V battery divider tap    | A2      | PC2 / ADC2 (`BATTERY_CHANNEL` in `battery.h`) |
| Wake button (to GND)       | D2      | PD2 / PCINT18 (`POWER_WAKE_PIN` in `power.h`) |
| Passive piezo              | D3      | PD3 / OC2B (`UI_BUZZER_PIN` in `ui.h`) |
| Servo supply switch gate   | D4      | PD4 (`POWER_SERVO_CUTOFF_PIN` in `power.h`) |
| Red LED (1 k to GND)       | D6      | PD6 (`UI_LED_RED_PIN` in `ui.h`) |
| Yellow LED (1 k to GND)    | D7      | PD7 (`UI_LED_YELLOW_PIN` in `ui.h`) |
| Green LED (1 k to GND)     | D8      | PB0 (`UI_LED_GREEN_PIN` in `ui.h`) |
| Servo signal               | D9      | PB1 / OC1A |
| Low-battery LED (1 k to GND) | D13   | PB5 (`BATTERY_LED_PIN` in `battery.h`) |

The battery divider is 1 M (battery+ to A2) / 1 M (A2 to GND), with a
≥100 nF cap from A2 to GND so the ADC sees a low source impedance. A
fresh 9.6 V cell puts 4.8 V on A2, just under the 5 V reference.

The battery is checked once per boot or wake, `BATTERY_CHECK_DELAY_MS`
(500 ms) after it starts so the servo's inrush current has passed. A low
reading latches the alert: the LED flashes for `BATTERY_FLASH_ON_MS`
every `BATTERY_FLASH_PERIOD_MS` (20 ms every 5 s), starting right away,
until the device sleeps; the next wake re-checks. On the Uno the onboard
"L" LED flashes with it.

The servo supply switch is a TP0606 N-channel MOSFET in the servo's
ground return (low side): D4 → 1 k → gate, 10 k gate to GND, drain to the
servo's GND lead, source to board GND. D4 HIGH = servo powered. See
`power.h` for the trade-off (noisy servo ground) and how to switch to a
P-channel high-side part.

## Standalone ATmega328P

The firmware assumes a 16 MHz clock (50 Hz PWM, 9600 baud, ADC prescaler).
The clock source is selected by fuses, not by the code, and a factory-fresh
chip runs on its internal oscillator at 1 MHz and ignores the crystal.
"Upload Using Programmer" does not write fuses; "Burn Bootloader" does.

1. Wire the chip (DIP-28 pin numbers):

   | Pin    | Connection |
   |--------|------------|
   | 1 RESET | 10 k to 5 V (not to the Uno's RESET header) |
   | 7 VCC, 20 AVCC | 5 V, 100 nF to GND at each |
   | 8 GND, 22 GND | GND (both) |
   | 9, 10 XTAL | 16 MHz crystal across them, 22 pF from each to GND |
   | 4 PD2 | Wake button (to GND) |
   | 5 PD3 / OC2B | Passive piezo |
   | 6 PD4 | Servo supply switch gate (via 1 k) |
   | 12 PD6 | Red LED (1 k to GND) |
   | 13 PD7 | Yellow LED (1 k to GND) |
   | 14 PB0 | Green LED (1 k to GND) |
   | 15 PB1 / OC1A | Servo signal |
   | 19 PB5 | Low-battery LED (1 k to GND) |
   | 23 PC0 / ADC0 | Keypad resistor-ladder out |
   | 24 PC1 / ADC1 | Servo current shunt (top) |
   | 25 PC2 / ADC2 | 9 V battery divider tap |

2. With the Uno running ArduinoISP: Tools → Board → **Arduino Uno**,
   Programmer → **Arduino as ISP**, then **Burn Bootloader** once to set
   the fuses (low `0xFF`, high `0xDE`, extended `0xFD`). The crystal must
   already be working: once the fuses select it, the chip does not answer
   the programmer without it.
3. Sketch → **Upload Using Programmer**.

To check the fuses without changing them:

    avrdude -c stk500v1 -P <port> -b 19200 -p m328p -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h

`0x62 0xD9 0xFF` is the factory setting (1 MHz internal).

### Serial monitor through the Uno

Hold the Uno's own MCU in reset (RESET header to GND) and wire straight,
not crossed: chip pin 3 (TXD) → Uno pin 1 "TX", chip pin 2 (RXD) → Uno
pin 0 "RX", grounds common. The sketch prints `servo_locker boot` at
9600 baud on reset. If the banner only appears at 600 or 4800 baud, the
chip is on the internal oscillator at 1 MHz or 8 MHz.

If the banner prints but keys do not, uncomment `DEBUG_ADC` in
`servo_locker.ino` to print raw ADC readings and compare them with the
thresholds in `keypad.h`.

To check the battery reading, uncomment `DEBUG_BATTERY` in
`servo_locker.ino` and compare the printed `battery_mv=` with a
multimeter across the battery; tune `BATTERY_LOW_MV` in `battery.h` from
there.

To enable block detection once the shunt is fitted, set
`BLOCK_DETECT_ENABLE` to 1 in `current.h`, uncomment `DEBUG_CURRENT`, and
tune `BLOCK_THRESHOLD_MV` from idle / moving / stalled readings.

Leave all `DEBUG_*` options off in normal use: the prints slow the main
loop, and the keypad debounce is counted in loop passes (`keypad.h`).
