#ifndef locker_h
#define locker_h

#include "keypad.h"

#define PIN_LEN 4

/* The code itself is never stored. EEPROM holds a random salt and
 * SHA-256(salt || code), with the code as 4 ASCII digits ("1234").
 * Checking an attempt hashes it with the same salt and compares the two
 * hashes. A new salt is drawn every time the code is set.
 *
 * EEPROM layout (v2)
 *   0x000       magic byte: LOCKER_EE_MAGIC once a record has been saved
 *   0x001-0x010 salt, LOCKER_SALT_LEN bytes
 *   0x011-0x030 hash, SHA256_DIGEST_LEN bytes
 *
 * Older firmware (v1, magic LOCKER_EE_MAGIC_V1) kept the 4 digits in
 * plain text at 0x001-0x004. locker_init() converts such a record once,
 * keeping the user's code; the salt overwrites the plain-text digits.
 * A blank, half-written or unrecognised record falls back to the default
 * code 1-2-3-4.
 *
 * What this protects: anyone reading the EEPROM sees no code, and the
 * salt stops one precomputed table working on every device. What it
 * does not: a 4-digit code has only 10,000 values, so a PC can try them
 * all against a dumped salt + hash in well under a second. To stop the
 * dump itself, program the lock bits (see README). */
#define LOCKER_EE_MAGIC_ADDR 0x000
#define LOCKER_EE_SALT_ADDR  0x001
#define LOCKER_EE_HASH_ADDR  0x011
#define LOCKER_EE_MAGIC      0xA6     /* v2: salted hash */
#define LOCKER_EE_MAGIC_V1   0xA5     /* v1: plain-text digits at 0x001 */
#define LOCKER_EE_INVALID    0xFF     /* written first while saving */
#define LOCKER_SALT_LEN      16

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { LOCKER_IN_PROGRESS, LOCKER_MATCH, LOCKER_WRONG } locker_status_t;

/* Load the saved salt + hash from EEPROM, converting a v1 record or
 * writing the default code 1-2-3-4 if needed. Call once at boot, after
 * adc_init() (a new salt may read the ADC for randomness). */
void locker_init(void);

/* LOCKER_IN_PROGRESS if fewer than PIN_LEN digits have been entered;
 * otherwise hashes the attempt, compares, and clears it. A few ms. */
locker_status_t locker_check(void);
void locker_reset(void);
void locker_add_digit(key_t key);

/* Replace the code: new salt, new hash, saved to EEPROM (~170 ms of
 * EEPROM writes). `new_code` must hold exactly PIN_LEN digit keys
 * (KEY_0..KEY_9); the caller should clear its copy afterwards. */
void locker_set_code(const key_t *new_code);

#ifdef __cplusplus
}
#endif

#endif
