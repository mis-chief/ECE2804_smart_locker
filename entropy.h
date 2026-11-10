#ifndef entropy_h
#define entropy_h

/* Randomness for passcode salts. The ATmega328P has no hardware random
 * number generator, so this pools timing samples and hashes them.
 *
 * Sources:
 *   - keypress timing: keypad.c adds Timer1's counter (TCNT1, 0.5 us
 *     resolution, 20 ms period) each time a key is accepted. Human press
 *     timing varies by milliseconds, so the low bits are unpredictable.
 *   - ADC noise: on first use, a burst of readings of the analog inputs
 *     is mixed in, so there is something even before any key is pressed.
 *
 * A salt only has to be unlikely to repeat, not secret, so this is
 * plenty. It is NOT suitable for generating secret keys. */

#ifdef __cplusplus
extern "C" {
#endif

/* Mix one 16-bit sample into the pool. Cheap: no hashing here. */
void entropy_add(unsigned int sample);

/* Fill `out` with 32 bytes derived from everything added so far. Two
 * calls never return the same bytes, even with nothing added between
 * them. Takes a few ms (one SHA-256). */
void entropy_get(unsigned char *out);

#ifdef __cplusplus
}
#endif

#endif
