#ifndef eeprom_h
#define eeprom_h

/* Bare-metal EEPROM access for the ATmega328P (1 KB, addresses 0-1023).
 * Direct register access only -- no <avr/eeprom.h>.
 *
 * Each cell is rated for ~100k erase/write cycles; eeprom_write() skips
 * writes that would not change the cell. A write takes ~3.4 ms and
 * finishes in the background; the next read or write waits for it. */

#ifdef __cplusplus
extern "C" {
#endif

unsigned char eeprom_read(unsigned int addr);
void eeprom_write(unsigned int addr, unsigned char data);

#ifdef __cplusplus
}
#endif

#endif
