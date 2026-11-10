/* Host-side test for ../locker.c, with the EEPROM and entropy source
 * replaced by stubs. Built and run by test/run_tests.sh:
 *   gcc -Wall -Wextra -std=c99 -o locker_test test/locker_test.c locker.c sha256.c
 *   ./locker_test           run the checks
 *   ./locker_test --dump    set code 1234, print "salt hash" in hex
 *                           (run_tests.sh checks it against sha256sum) */

#include <stdio.h>
#include <string.h>
#include "../locker.h"
#include "../eeprom.h"
#include "../entropy.h"
#include "../sha256.h"

/* ---- stubs ------------------------------------------------------------ */

static unsigned char ee[1024];
static long write_budget = -1;   /* -1 = unlimited; else writes left before "power loss" */
static unsigned char rnd_counter;

unsigned char eeprom_read(unsigned int addr) { return ee[addr]; }

void eeprom_write(unsigned int addr, unsigned char data)
{
    if (write_budget == 0) {
        return;                  /* power is gone: write never happens */
    }
    if (write_budget > 0) {
        write_budget--;
    }
    ee[addr] = data;
}

void entropy_add(unsigned int sample) { (void)sample; }

/* deterministic, but different on every call (like the real one) */
void entropy_get(unsigned char *out)
{
    unsigned char seed = ++rnd_counter;
    sha256(&seed, 1, out);
}

/* ---- helpers ---------------------------------------------------------- */

static int fails;

#define CHECK(cond, msg) do { \
    if (cond) { printf("ok   %s\n", msg); } \
    else { printf("FAIL %s (line %d)\n", msg, __LINE__); fails++; } \
} while (0)

static void erase(void) { memset(ee, 0xFF, sizeof ee); }

static void enter(const char *digits)
{
    for (; *digits; digits++) {
        locker_add_digit((key_t)(KEY_0 + (*digits - '0')));
    }
}

static locker_status_t try_code(const char *digits)
{
    locker_reset();
    enter(digits);
    return locker_check();
}

static void set_code(const char *digits)
{
    key_t k[PIN_LEN];
    int i;

    for (i = 0; i < PIN_LEN; i++) {
        k[i] = (key_t)(KEY_0 + (digits[i] - '0'));
    }
    locker_set_code(k);
}

static void hex(const unsigned char *p, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        printf("%02x", p[i]);
    }
}

/* ---- tests ------------------------------------------------------------ */

int main(int argc, char **argv)
{
    unsigned char salt1[LOCKER_SALT_LEN];
    long n;
    int i, bad;

    if (argc > 1 && strcmp(argv[1], "--dump") == 0) {
        erase();
        locker_init();                       /* writes default 1234 */
        hex(&ee[LOCKER_EE_SALT_ADDR], LOCKER_SALT_LEN);
        printf(" ");
        hex(&ee[LOCKER_EE_HASH_ADDR], SHA256_DIGEST_LEN);
        printf("\n");
        return 0;
    }

    /* blank EEPROM */
    erase();
    locker_init();
    CHECK(ee[LOCKER_EE_MAGIC_ADDR] == LOCKER_EE_MAGIC, "blank EEPROM: v2 record written");
    CHECK(try_code("1234") == LOCKER_MATCH, "blank EEPROM: default 1234 opens");
    CHECK(try_code("1235") == LOCKER_WRONG, "blank EEPROM: 1235 rejected");
    CHECK(try_code("0000") == LOCKER_WRONG, "blank EEPROM: 0000 rejected");

    /* partial / overlong entry */
    locker_reset();
    enter("123");
    CHECK(locker_check() == LOCKER_IN_PROGRESS, "3 digits: in progress");
    locker_reset();
    enter("91234");
    CHECK(locker_check() == LOCKER_IN_PROGRESS, "5 digits: 5th starts a fresh attempt");
    locker_reset();
    enter("9999");
    enter("1234");
    CHECK(locker_check() == LOCKER_MATCH, "8 digits: last 4 count");

    /* change code, then "reboot" */
    memcpy(salt1, &ee[LOCKER_EE_SALT_ADDR], LOCKER_SALT_LEN);
    set_code("5678");
    CHECK(memcmp(salt1, &ee[LOCKER_EE_SALT_ADDR], LOCKER_SALT_LEN) != 0, "new code gets a new salt");
    locker_init();
    CHECK(try_code("5678") == LOCKER_MATCH, "after reboot: new code 5678 opens");
    CHECK(try_code("1234") == LOCKER_WRONG, "after reboot: old code 1234 rejected");

    /* no plain-text digits anywhere in the record */
    bad = 0;
    for (i = 0; i + PIN_LEN <= 0x031; i++) {
        if (ee[i] == KEY_5 && ee[i + 1] == KEY_6 && ee[i + 2] == KEY_7 && ee[i + 3] == KEY_8) bad = 1;
        if (memcmp(&ee[i], "5678", 4) == 0) bad = 1;
    }
    CHECK(!bad, "EEPROM holds no plain-text 5678");

    /* v1 (plain-text) record from the old firmware */
    erase();
    ee[0] = LOCKER_EE_MAGIC_V1;
    ee[1] = KEY_5; ee[2] = KEY_6; ee[3] = KEY_7; ee[4] = KEY_8;
    locker_init();
    CHECK(ee[LOCKER_EE_MAGIC_ADDR] == LOCKER_EE_MAGIC, "v1 record converted to v2");
    CHECK(try_code("5678") == LOCKER_MATCH, "v1 record: user's code 5678 still opens");
    CHECK(try_code("1234") == LOCKER_WRONG, "v1 record: default 1234 does not");
    CHECK(!(ee[1] == KEY_5 && ee[2] == KEY_6 && ee[3] == KEY_7 && ee[4] == KEY_8),
          "v1 record: plain-text digits overwritten");
    locker_init();
    CHECK(try_code("5678") == LOCKER_MATCH, "v1 record: still 5678 after a second boot");

    /* corrupt v1 record */
    erase();
    ee[0] = LOCKER_EE_MAGIC_V1;
    ee[1] = KEY_5; ee[2] = 0x77; ee[3] = KEY_7; ee[4] = KEY_8;
    locker_init();
    CHECK(try_code("1234") == LOCKER_MATCH, "corrupt v1 record: falls back to 1234");

    /* unknown magic */
    erase();
    ee[0] = 0x00;
    locker_init();
    CHECK(try_code("1234") == LOCKER_MATCH, "unknown magic: falls back to 1234");

    /* power loss after every possible number of EEPROM writes during a
       save of 1111 over 5678: must come back as 5678 (nothing written),
       1234 (half-written) or 1111 (complete) -- never locked out */
    bad = 0;
    for (n = 0; n <= 1 + LOCKER_SALT_LEN + SHA256_DIGEST_LEN + 1; n++) {
        int old_ok, def_ok, new_ok;

        erase();
        locker_init();
        set_code("5678");
        write_budget = n;
        set_code("1111");
        write_budget = -1;
        locker_init();                       /* reboot */
        old_ok = try_code("5678") == LOCKER_MATCH;
        def_ok = try_code("1234") == LOCKER_MATCH;
        new_ok = try_code("1111") == LOCKER_MATCH;
        if (n == 0 && !old_ok) bad = 1;                       /* nothing written */
        if (n > 0 && n < 50 && !def_ok) bad = 1;              /* half-written */
        if (n == 50 && !new_ok) bad = 1;                      /* complete */
        if (old_ok + def_ok + new_ok != 1) bad = 1;
    }
    CHECK(!bad, "power loss at each of 0..50 writes: never locked out");

    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails != 0;
}
