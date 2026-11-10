#include "entropy.h"
#include "sha256.h"
#include "keypad.h"    /* adc_read_channel(), KEYPAD_CHANNEL */
#include "current.h"   /* CURRENT_CHANNEL */
#include "battery.h"   /* BATTERY_CHANNEL */
#include "regs.h"      /* TCNT1 */

#define POOL_LEN   SHA256_DIGEST_LEN
#define SEED_READS 16   /* per analog channel, on first use */

static unsigned char pool[POOL_LEN];
static unsigned char pos;            /* next pool byte to mix into */
static unsigned long count;          /* samples + outputs so far */
static unsigned char seeded;

void entropy_add(unsigned int sample)
{
    /* XOR the two bytes in round-robin: entropy_get() hashes the whole
       pool, so this only has to keep every sample's bits, not mix them */
    pool[pos] ^= (unsigned char)sample;
    pos = (pos + 1) % POOL_LEN;
    pool[pos] ^= (unsigned char)(sample >> 8);
    pos = (pos + 1) % POOL_LEN;
    count++;
}

/* ADC readings of the analog inputs: the low bits carry some noise,
 * especially A1 while no shunt is fitted (the pin floats). Weak on its
 * own, so it only supplements the keypress timing. */
static void seed(void)
{
    static const unsigned char channels[] = {
        CURRENT_CHANNEL, BATTERY_CHANNEL, KEYPAD_CHANNEL
    };
    unsigned char c, i;

    for (c = 0; c < sizeof channels; c++) {
        for (i = 0; i < SEED_READS; i++) {
            entropy_add(adc_read_channel(channels[c]) ^ TCNT1);
        }
    }
    seeded = 1;
}

void entropy_get(unsigned char *out)
{
    unsigned char buf[POOL_LEN + 4];
    unsigned char i;

    if (!seeded) {
        seed();
    }
    entropy_add(TCNT1);   /* when the request came in */

    /* out = SHA-256(pool || count) */
    for (i = 0; i < POOL_LEN; i++) {
        buf[i] = pool[i];
    }
    buf[POOL_LEN]     = (unsigned char)(count >> 24);
    buf[POOL_LEN + 1] = (unsigned char)(count >> 16);
    buf[POOL_LEN + 2] = (unsigned char)(count >> 8);
    buf[POOL_LEN + 3] = (unsigned char)count;
    sha256(buf, sizeof buf, out);

    /* fold the output back in, so the next call starts from a new pool
       even if no samples arrive in between */
    for (i = 0; i < POOL_LEN; i++) {
        pool[i] ^= out[i];
    }
    count++;
}
