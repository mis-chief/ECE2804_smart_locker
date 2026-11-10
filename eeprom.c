#include "eeprom.h"
#include "regs.h"

unsigned char eeprom_read(unsigned int addr)
{
    while (EECR & (1 << EEPE)) { }   /* wait for any write in progress */
    EEAR = addr;
    EECR |= (1 << EERE);             /* read happens immediately */
    return EEDR;
}

void eeprom_write(unsigned int addr, unsigned char data)
{
    unsigned char sreg;

    if (eeprom_read(addr) == data) {
        return;                      /* save a write cycle */
    }

    while (EECR & (1 << EEPE)) { }
    EEAR = addr;
    EEDR = data;

    /* EEPE must be set within 4 clock cycles of EEMPE. An interrupt (e.g.
     * the Arduino core's Timer0 millis() ISR) landing between the two would
     * silently cancel the write, so interrupts are held off for these two
     * instructions and then restored to whatever they were. */
    sreg = SREG;
    __asm__ __volatile__ ("cli" ::: "memory");
    EECR = (1 << EEMPE);                   /* EEPM = 00: erase + write */
    EECR = (1 << EEMPE) | (1 << EEPE);
    SREG = sreg;
}
