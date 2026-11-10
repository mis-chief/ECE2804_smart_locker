#include "locker.h"
#include "eeprom.h"
#include "sha256.h"
#include "entropy.h"

static const key_t default_code[PIN_LEN] = {KEY_1, KEY_2, KEY_3, KEY_4};

/* RAM copy of the saved record: never the code itself */
static unsigned char salt[LOCKER_SALT_LEN];
static unsigned char hash[SHA256_DIGEST_LEN];

static key_t attempt[PIN_LEN];
static unsigned int len = 0;

/* Overwrite a buffer that held code-derived data. volatile so the
 * compiler can't drop the stores as "dead". */
static void wipe(void *p, unsigned int n)
{
    volatile unsigned char *b = (volatile unsigned char *)p;

    while (n--) {
        *b++ = 0;
    }
}

static unsigned char is_digit(key_t k)
{
    return k >= KEY_0 && k <= KEY_9;
}

/* out = SHA-256(salt || digits as ASCII), e.g. salt || "1234". ASCII so
 * the result can be reproduced on a PC with sha256sum. */
static void hash_code(const key_t *digits, unsigned char *out)
{
    unsigned char buf[LOCKER_SALT_LEN + PIN_LEN];
    unsigned int i;

    for (i = 0; i < LOCKER_SALT_LEN; i++) {
        buf[i] = salt[i];
    }
    for (i = 0; i < PIN_LEN; i++) {
        buf[LOCKER_SALT_LEN + i] = (unsigned char)('0' + (digits[i] - KEY_0));
    }
    sha256(buf, sizeof buf, out);
    wipe(buf, sizeof buf);
}

void locker_set_code(const key_t *new_code)
{
    unsigned char rnd[SHA256_DIGEST_LEN];
    unsigned int i;

    entropy_get(rnd);
    for (i = 0; i < LOCKER_SALT_LEN; i++) {
        salt[i] = rnd[i];
    }
    hash_code(new_code, hash);

    /* Invalidate first, magic last: if power dies mid-save, the next
     * boot sees no valid record and falls back to the default code
     * instead of checking against a half-written hash. */
    eeprom_write(LOCKER_EE_MAGIC_ADDR, LOCKER_EE_INVALID);
    for (i = 0; i < LOCKER_SALT_LEN; i++) {
        eeprom_write(LOCKER_EE_SALT_ADDR + i, salt[i]);
    }
    for (i = 0; i < SHA256_DIGEST_LEN; i++) {
        eeprom_write(LOCKER_EE_HASH_ADDR + i, hash[i]);
    }
    eeprom_write(LOCKER_EE_MAGIC_ADDR, LOCKER_EE_MAGIC);
}

/* v1 record: 4 plain-text digits at 0x001. Re-save them as a v2 record,
 * which overwrites the plain text with the salt. Returns 0 if the old
 * record is corrupt. */
static unsigned char convert_v1(void)
{
    key_t old[PIN_LEN];
    unsigned int i;
    unsigned char valid = 1;

    for (i = 0; i < PIN_LEN; i++) {
        old[i] = (key_t)eeprom_read(LOCKER_EE_SALT_ADDR + i);
        if (!is_digit(old[i])) {
            valid = 0;
        }
    }
    if (valid) {
        locker_set_code(old);
    }
    wipe(old, sizeof old);
    return valid;
}

void locker_init(void)
{
    unsigned char magic = eeprom_read(LOCKER_EE_MAGIC_ADDR);
    unsigned int i;

    if (magic == LOCKER_EE_MAGIC) {
        for (i = 0; i < LOCKER_SALT_LEN; i++) {
            salt[i] = eeprom_read(LOCKER_EE_SALT_ADDR + i);
        }
        for (i = 0; i < SHA256_DIGEST_LEN; i++) {
            hash[i] = eeprom_read(LOCKER_EE_HASH_ADDR + i);
        }
    } else if (!(magic == LOCKER_EE_MAGIC_V1 && convert_v1())) {
        locker_set_code(default_code);   /* blank, half-written or corrupt */
    }
    locker_reset();
}

void locker_reset(void)
{
    wipe(attempt, sizeof attempt);
    len = 0;
}

void locker_add_digit(key_t key)
{
    if (len == PIN_LEN) {
        /* Buffer full but not yet submitted: locker_check() only runs
           when '#' is pressed. Without this, a 5th (6th, ...) keypress
           would be silently dropped and '#' would judge the stale
           first four digits. Start a fresh attempt with this key
           instead. */
        len = 0;
    }
    attempt[len++] = key;
}

locker_status_t locker_check(void)
{
    unsigned char h[SHA256_DIGEST_LEN];
    unsigned char diff = 0;
    unsigned int i;

    if (len != PIN_LEN) {
        return LOCKER_IN_PROGRESS;   /* not all digits entered yet */
    }

    hash_code(attempt, h);

    /* Compare every byte even after a mismatch, so timing doesn't reveal
       how much of the hash matched. */
    for (i = 0; i < SHA256_DIGEST_LEN; i++) {
        diff |= (unsigned char)(h[i] ^ hash[i]);
    }

    wipe(h, sizeof h);
    locker_reset();                  /* either way the buffer starts clean */
    return diff ? LOCKER_WRONG : LOCKER_MATCH;
}
