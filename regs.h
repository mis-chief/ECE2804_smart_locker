/* ATmega328P register map: every register and bit this project uses,
 * with the datasheet's names.
 *
 * unsigned char -> 8 bits; volatile because the hardware can change the
 * value behind the compiler's back. Addresses are DATA-space addresses
 * (used with a normal C load/store); I/O-space addresses for inline asm
 * are 0x20 lower and get an _IO suffix.
 *
 * Include this from .c files ONLY, never from a header. The sketch's
 * .ino includes our headers after Arduino.h has pulled in <avr/io.h>,
 * which defines these same names; keeping regs.h out of the headers keeps
 * the two sets from colliding. */

#ifndef regs_h
#define regs_h

#define REG8(a)  (*(volatile unsigned char*)(a))
#define REG16(a) (*(volatile unsigned int*)(a))

/* ---- status register ---- */
#define SREG    REG8(0x5F)  /* I bit = global interrupt enable */

/* ---- ports ---- */
#define DDRB    REG8(0x24)
#define PORTB   REG8(0x25)
#define PB0     0           /* D8:  green LED */
#define PB1     1           /* D9:  servo signal, OC1A */
#define PB5     5           /* D13: low-battery LED */

#define DDRD    REG8(0x2A)
#define PORTD   REG8(0x2B)
#define PD2     2           /* D2: wake button (PCINT18) */
#define PD3     3           /* D3: piezo, OC2B */
#define PD4     4           /* D4: servo supply switch */
#define PD5     5           /* D5: whole-load switch (unused by default) */
#define PD6     6           /* D6: red LED */
#define PD7     7           /* D7: yellow LED */

/* ---- sleep / power reduction ---- */
#define SMCR    REG8(0x53)
#define SE      0           /* sleep enable */
#define SM1     2           /* SM2..0 = 010 -> power-down */

#define MCUCR_IO 0x35       /* I/O-space address, for the BOD-disable asm */

#define PRR     REG8(0x64)
#define PRUSART0 1
#define PRSPI   2
#define PRTIM1  3
#define PRTIM2  6

/* ---- pin-change interrupts ---- */
#define PCIFR   REG8(0x3B)
#define PCIF2   2           /* port D */
#define PCICR   REG8(0x68)
#define PCIE2   2           /* port D */
#define PCMSK2  REG8(0x6D)
#define PCINT18 2           /* PD2 */

/* ---- Timer0 (Arduino core's millis()) ---- */
#define TCCR0B  REG8(0x45)
#define TIMSK0  REG8(0x6E)

/* ---- Timer1: servo PWM + millisecond tick ---- */
#define TCCR1A  REG8(0x80)
#define COM1A1  7
#define COM1A0  6
#define WGM11   1

#define TCCR1B  REG8(0x81)
#define WGM13   4
#define WGM12   3
#define CS11    1

/* 16-bit access reads TCNT1L first, which latches TCNT1H, so both bytes
 * come from the same instant */
#define TCNT1   REG16(0x84)

#define ICR1H   REG8(0x87)
#define ICR1L   REG8(0x86)

#define OCR1AH  REG8(0x89)
#define OCR1AL  REG8(0x88)

#define TIFR1   REG8(0x36)
#define TOV1    0

/* ---- Timer2: piezo tones ---- */
#define TCCR2A  REG8(0xB0)
#define COM2B0  4           /* toggle OC2B on compare match */
#define WGM21   1           /* CTC mode */

#define TCCR2B  REG8(0xB1)
#define CS22    2           /* clk/64 */

#define TCNT2   REG8(0xB2)
#define OCR2A   REG8(0xB3)
#define OCR2B   REG8(0xB4)

/* ---- ADC ---- */
#define ADMUX   REG8(0x7C)
#define REFS1   7   /* reference selection, with REFS0 */
#define REFS0   6
#define ADLAR   5   /* 0 -> right-adjusted 10-bit result */
#define MUX3    3   /* input channel selection, with MUX2..0 */
#define MUX2    2
#define MUX1    1
#define MUX0    0

#define ADCSRA  REG8(0x7A)
#define ADEN    7   /* ADC enable */
#define ADSC    6   /* start conversion; stays 1 while busy */
#define ADIF    4   /* conversion complete flag */
#define ADIE    3   /* interrupt enable (unused) */
#define ADPS2   2   /* prescaler select, with ADPS1..0 */
#define ADPS1   1
#define ADPS0   0

/* 16-bit access reads ADCL before ADCH, which the hardware requires
 * to keep the pair from the same conversion */
#define ADC     REG16(0x78)

#define DIDR0   REG8(0x7E)  /* digital input disable, one bit per ADC pin */
#define ADC0D   0           /* A0: keypad */
#define ADC1D   1           /* A1: servo current shunt */
#define ADC2D   2           /* A2: battery divider */

/* ---- EEPROM ---- */
#define EECR    REG8(0x3F)
#define EERE    0           /* read enable */
#define EEPE    1           /* write enable */
#define EEMPE   2           /* master write enable */
#define EEDR    REG8(0x40)
#define EEAR    REG16(0x41) /* EEARL/EEARH */

#endif
